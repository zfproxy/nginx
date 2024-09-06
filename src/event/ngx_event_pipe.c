
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_event_pipe.c - Nginx事件管道模块
 *
 * 该模块实现了Nginx的事件驱动管道处理机制,主要用于处理上游和下游之间的数据传输。
 *
 * 支持的功能:
 * - 从上游读取数据
 * - 向下游写入数据  
 * - 临时文件缓存
 * - 链式缓冲区管理
 *
 * 主要指令:
 * - ngx_event_pipe: 核心处理函数
 * - ngx_event_pipe_read_upstream: 从上游读取数据
 * - ngx_event_pipe_write_to_downstream: 向下游写入数据
 *
 * 重要变量:
 * - ngx_event_pipe_t: 事件管道结构体
 * - p->upstream: 上游连接
 * - p->downstream: 下游连接
 * - p->free_raw_bufs: 空闲原始缓冲区链表
 * - p->in: 输入缓冲区链表
 * - p->out: 输出缓冲区链表
 *
 * 使用注意点:
 * 1. 需要正确初始化ngx_event_pipe_t结构体
 * 2. 调用ngx_event_pipe函数时需要谨慎处理返回值
 * 3. 临时文件的使用需要注意磁盘空间
 * 4. 大量数据传输时需要注意内存使用
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_pipe.h>


// 从上游读取数据的函数
static ngx_int_t ngx_event_pipe_read_upstream(ngx_event_pipe_t *p);

// 向下游写入数据的函数
static ngx_int_t ngx_event_pipe_write_to_downstream(ngx_event_pipe_t *p);

// 将链式缓冲区写入临时文件的函数
static ngx_int_t ngx_event_pipe_write_chain_to_temp_file(ngx_event_pipe_t *p);

// 移除缓冲区的影子链接的内联函数
static ngx_inline void ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf);

// 清空链式缓冲区的函数
static ngx_int_t ngx_event_pipe_drain_chains(ngx_event_pipe_t *p);


ngx_int_t
ngx_event_pipe(ngx_event_pipe_t *p, ngx_int_t do_write)
{
    ngx_int_t     rc;
    ngx_uint_t    flags;
    ngx_event_t  *rev, *wev;

    for ( ;; ) {
        // 如果需要写入数据
        if (do_write) {
            p->log->action = "sending to client";

            // 向下游写入数据
            rc = ngx_event_pipe_write_to_downstream(p);

            if (rc == NGX_ABORT) {
                return NGX_ABORT;
            }

            if (rc == NGX_BUSY) {
                return NGX_OK;
            }
        }

        // 重置读取和上游阻塞标志
        p->read = 0;
        p->upstream_blocked = 0;

        p->log->action = "reading upstream";

        // 从上游读取数据
        if (ngx_event_pipe_read_upstream(p) == NGX_ABORT) {
            return NGX_ABORT;
        }

        // 如果没有读取到数据且上游未阻塞，则退出循环
        if (!p->read && !p->upstream_blocked) {
            break;
        }

        do_write = 1;
    }

    // 处理上游读事件
    if (p->upstream
        && p->upstream->fd != (ngx_socket_t) -1)
    {
        rev = p->upstream->read;

        flags = (rev->eof || rev->error) ? NGX_CLOSE_EVENT : 0;

        if (ngx_handle_read_event(rev, flags) != NGX_OK) {
            return NGX_ABORT;
        }

        // 设置或删除读事件的定时器
        if (!rev->delayed) {
            if (rev->active && !rev->ready) {
                ngx_add_timer(rev, p->read_timeout);

            } else if (rev->timer_set) {
                ngx_del_timer(rev);
            }
        }
    }

    // 处理下游写事件
    if (p->downstream->fd != (ngx_socket_t) -1
        && p->downstream->data == p->output_ctx)
    {
        wev = p->downstream->write;
        if (ngx_handle_write_event(wev, p->send_lowat) != NGX_OK) {
            return NGX_ABORT;
        }

        // 设置或删除写事件的定时器
        if (!wev->delayed) {
            if (wev->active && !wev->ready) {
                ngx_add_timer(wev, p->send_timeout);

            } else if (wev->timer_set) {
                ngx_del_timer(wev);
            }
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_event_pipe_read_upstream(ngx_event_pipe_t *p)
{
    off_t         limit;
    ssize_t       n, size;
    ngx_int_t     rc;
    ngx_buf_t    *b;
    ngx_msec_t    delay;
    ngx_chain_t  *chain, *cl, *ln;

    // 如果上游已经结束、出错或完成，或者上游连接不存在，则直接返回
    if (p->upstream_eof || p->upstream_error || p->upstream_done
        || p->upstream == NULL)
    {
        return NGX_OK;
    }

#if (NGX_THREADS)

    // 如果正在进行异步I/O操作，则返回NGX_AGAIN
    if (p->aio) {
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe read upstream: aio");
        return NGX_AGAIN;
    }

    // 如果正在写入临时文件，则先完成写入操作
    if (p->writing) {
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe read upstream: writing");

        rc = ngx_event_pipe_write_chain_to_temp_file(p);

        if (rc != NGX_OK) {
            return rc;
        }
    }

#endif

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                   "pipe read upstream: %d", p->upstream->read->ready);

    for ( ;; ) {

        // 检查上游状态
        if (p->upstream_eof || p->upstream_error || p->upstream_done) {
            break;
        }

        // 如果没有预读数据且上游未准备好，则退出循环
        if (p->preread_bufs == NULL && !p->upstream->read->ready) {
            break;
        }

        if (p->preread_bufs) {

            // 使用预读的缓冲区
            chain = p->preread_bufs;
            p->preread_bufs = NULL;
            n = p->preread_size;

            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "pipe preread: %z", n);

            if (n) {
                p->read = 1;
            }

        } else {

#if (NGX_HAVE_KQUEUE)

            // kqueue特定的EOF和错误处理
            if (p->upstream->read->available == 0
                && p->upstream->read->pending_eof
#if (NGX_SSL)
                && !p->upstream->ssl
#endif
                )
            {
                p->upstream->read->ready = 0;
                p->upstream->read->eof = 1;
                p->upstream_eof = 1;
                p->read = 1;

                if (p->upstream->read->kq_errno) {
                    p->upstream->read->error = 1;
                    p->upstream_error = 1;
                    p->upstream_eof = 0;

                    ngx_log_error(NGX_LOG_ERR, p->log,
                                  p->upstream->read->kq_errno,
                                  "kevent() reported that upstream "
                                  "closed connection");
                }

                break;
            }
#endif

            // 处理限速
            if (p->limit_rate) {
                if (p->upstream->read->delayed) {
                    break;
                }

                limit = (off_t) p->limit_rate * (ngx_time() - p->start_sec + 1)
                        - p->read_length;

                if (limit <= 0) {
                    p->upstream->read->delayed = 1;
                    delay = (ngx_msec_t) (- limit * 1000 / p->limit_rate + 1);
                    ngx_add_timer(p->upstream->read, delay);
                    break;
                }

            } else {
                limit = 0;
            }

            // 获取或分配缓冲区
            if (p->free_raw_bufs) {

                // 使用空闲的缓冲区
                chain = p->free_raw_bufs;
                if (p->single_buf) {
                    p->free_raw_bufs = p->free_raw_bufs->next;
                    chain->next = NULL;
                } else {
                    p->free_raw_bufs = NULL;
                }

            } else if (p->allocated < p->bufs.num) {

                // 分配新的缓冲区
                b = ngx_create_temp_buf(p->pool, p->bufs.size);
                if (b == NULL) {
                    return NGX_ABORT;
                }

                p->allocated++;

                chain = ngx_alloc_chain_link(p->pool);
                if (chain == NULL) {
                    return NGX_ABORT;
                }

                chain->buf = b;
                chain->next = NULL;

            } else if (!p->cacheable
                       && p->downstream->data == p->output_ctx
                       && p->downstream->write->ready
                       && !p->downstream->write->delayed)
            {
                // 如果下游准备好接收数据，则暂停读取上游数据
                p->upstream_blocked = 1;

                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe downstream ready");

                break;

            } else if (p->cacheable
                       || p->temp_file->offset < p->max_temp_file_size)
            {

                // 将数据写入临时文件
                rc = ngx_event_pipe_write_chain_to_temp_file(p);

                ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe temp offset: %O", p->temp_file->offset);

                if (rc == NGX_BUSY) {
                    break;
                }

                if (rc != NGX_OK) {
                    return rc;
                }

                chain = p->free_raw_bufs;
                if (p->single_buf) {
                    p->free_raw_bufs = p->free_raw_bufs->next;
                    chain->next = NULL;
                } else {
                    p->free_raw_bufs = NULL;
                }

            } else {

                // 没有可用的缓冲区
                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "no pipe bufs to read in");

                break;
            }

            // 从上游接收数据
            n = p->upstream->recv_chain(p->upstream, chain, limit);

            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "pipe recv chain: %z", n);

            if (p->free_raw_bufs) {
                chain->next = p->free_raw_bufs;
            }
            p->free_raw_bufs = chain;

            if (n == NGX_ERROR) {
                p->upstream_error = 1;
                break;
            }

            if (n == NGX_AGAIN) {
                if (p->single_buf) {
                    ngx_event_pipe_remove_shadow_links(chain->buf);
                }

                break;
            }

            p->read = 1;

            if (n == 0) {
                p->upstream_eof = 1;
                break;
            }
        }

        // 处理限速延迟
        delay = p->limit_rate ? (ngx_msec_t) n * 1000 / p->limit_rate : 0;

        p->read_length += n;
        cl = chain;
        p->free_raw_bufs = NULL;

        // 处理接收到的数据
        while (cl && n > 0) {

            ngx_event_pipe_remove_shadow_links(cl->buf);

            size = cl->buf->end - cl->buf->last;

            if (n >= size) {
                cl->buf->last = cl->buf->end;

                /* STUB */ cl->buf->num = p->num++;

                if (p->input_filter(p, cl->buf) == NGX_ERROR) {
                    return NGX_ABORT;
                }

                n -= size;
                ln = cl;
                cl = cl->next;
                ngx_free_chain(p->pool, ln);

            } else {
                cl->buf->last += n;
                n = 0;
            }
        }

        if (cl) {
            for (ln = cl; ln->next; ln = ln->next) { /* void */ }

            ln->next = p->free_raw_bufs;
            p->free_raw_bufs = cl;
        }

        if (delay > 0) {
            p->upstream->read->delayed = 1;
            ngx_add_timer(p->upstream->read, delay);
            break;
        }
    }

#if (NGX_DEBUG)

    // 调试日志：输出各种缓冲区链的状态
    for (cl = p->busy; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf busy s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %O",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }

    for (cl = p->out; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf out  s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %O",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }

    for (cl = p->in; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf in   s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %O",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }

    for (cl = p->free_raw_bufs; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf free s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %O",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                   "pipe length: %O", p->length);

#endif

    // 处理剩余的空闲缓冲区
    if (p->free_raw_bufs && p->length != -1) {
        cl = p->free_raw_bufs;

        if (cl->buf->last - cl->buf->pos >= p->length) {

            p->free_raw_bufs = cl->next;

            /* STUB */ cl->buf->num = p->num++;

            if (p->input_filter(p, cl->buf) == NGX_ERROR) {
                return NGX_ABORT;
            }

            ngx_free_chain(p->pool, cl);
        }
    }

    // 检查是否已读取完所有数据
    if (p->length == 0) {
        p->upstream_done = 1;
        p->read = 1;
    }

    // 处理上游EOF或错误情况
    if ((p->upstream_eof || p->upstream_error) && p->free_raw_bufs) {

        /* STUB */ p->free_raw_bufs->buf->num = p->num++;

        if (p->input_filter(p, p->free_raw_bufs->buf) == NGX_ERROR) {
            return NGX_ABORT;
        }

        p->free_raw_bufs = p->free_raw_bufs->next;

        if (p->free_bufs && p->buf_to_file == NULL) {
            for (cl = p->free_raw_bufs; cl; cl = cl->next) {
                if (cl->buf->shadow == NULL) {
                    ngx_pfree(p->pool, cl->buf->start);
                }
            }
        }
    }

    // 如果需要缓存且有数据待写入，则写入临时文件
    if (p->cacheable && (p->in || p->buf_to_file)) {

        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe write chain");

        rc = ngx_event_pipe_write_chain_to_temp_file(p);

        if (rc != NGX_OK) {
            return rc;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_event_pipe_write_to_downstream(ngx_event_pipe_t *p)
{
    u_char            *prev;
    size_t             bsize;
    ngx_int_t          rc;
    ngx_uint_t         flush, flushed, prev_last_shadow;
    ngx_chain_t       *out, **ll, *cl;
    ngx_connection_t  *downstream;

    downstream = p->downstream;

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                   "pipe write downstream: %d", downstream->write->ready);

#if (NGX_THREADS)

    // 如果正在写入，则尝试将链写入临时文件
    if (p->writing) {
        rc = ngx_event_pipe_write_chain_to_temp_file(p);

        if (rc == NGX_ABORT) {
            return NGX_ABORT;
        }
    }

#endif

    flushed = 0;

    for ( ;; ) {
        // 检查下游是否出错
        if (p->downstream_error) {
            return ngx_event_pipe_drain_chains(p);
        }

        // 处理上游EOF、错误或完成的情况
        if (p->upstream_eof || p->upstream_error || p->upstream_done) {

            // 将p->out和p->in链传递给输出过滤器

            // 标记busy链中的缓冲区为非回收
            for (cl = p->busy; cl; cl = cl->next) {
                cl->buf->recycled = 0;
            }

            // 处理out链
            if (p->out) {
                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe write downstream flush out");

                for (cl = p->out; cl; cl = cl->next) {
                    cl->buf->recycled = 0;
                }

                rc = p->output_filter(p->output_ctx, p->out);

                if (rc == NGX_ERROR) {
                    p->downstream_error = 1;
                    return ngx_event_pipe_drain_chains(p);
                }

                p->out = NULL;
            }

            if (p->writing) {
                break;
            }

            // 处理in链
            if (p->in) {
                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe write downstream flush in");

                for (cl = p->in; cl; cl = cl->next) {
                    cl->buf->recycled = 0;
                }

                rc = p->output_filter(p->output_ctx, p->in);

                if (rc == NGX_ERROR) {
                    p->downstream_error = 1;
                    return ngx_event_pipe_drain_chains(p);
                }

                p->in = NULL;
            }

            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "pipe write downstream done");

            // TODO: 释放未使用的缓冲区

            p->downstream_done = 1;
            break;
        }

        // 检查下游连接状态
        if (downstream->data != p->output_ctx
            || !downstream->write->ready
            || downstream->write->delayed)
        {
            break;
        }

        // 计算busy链中可回收缓冲区的大小

        prev = NULL;
        bsize = 0;

        for (cl = p->busy; cl; cl = cl->next) {

            if (cl->buf->recycled) {
                if (prev == cl->buf->start) {
                    continue;
                }

                bsize += cl->buf->end - cl->buf->start;
                prev = cl->buf->start;
            }
        }

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe write busy: %uz", bsize);

        out = NULL;

        // 如果busy链大小超过阈值，设置flush标志
        if (bsize >= (size_t) p->busy_size) {
            flush = 1;
            goto flush;
        }

        flush = 0;
        ll = NULL;
        prev_last_shadow = 1;

        // 构建输出链
        for ( ;; ) {
            if (p->out) {
                cl = p->out;

                if (cl->buf->recycled) {
                    ngx_log_error(NGX_LOG_ALERT, p->log, 0,
                                  "recycled buffer in pipe out chain");
                }

                p->out = p->out->next;

            } else if (!p->cacheable && !p->writing && p->in) {
                cl = p->in;

                ngx_log_debug3(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe write buf ls:%d %p %z",
                               cl->buf->last_shadow,
                               cl->buf->pos,
                               cl->buf->last - cl->buf->pos);

                if (cl->buf->recycled && prev_last_shadow) {
                    if (bsize + cl->buf->end - cl->buf->start > p->busy_size) {
                        flush = 1;
                        break;
                    }

                    bsize += cl->buf->end - cl->buf->start;
                }

                prev_last_shadow = cl->buf->last_shadow;

                p->in = p->in->next;

            } else {
                break;
            }

            cl->next = NULL;

            if (out) {
                *ll = cl;
            } else {
                out = cl;
            }
            ll = &cl->next;
        }

    flush:

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe write: out:%p, f:%ui", out, flush);

        if (out == NULL) {

            if (!flush) {
                break;
            }

            // AIO的一个解决方案
            if (flushed++ > 10) {
                return NGX_BUSY;
            }
        }

        // 调用输出过滤器
        rc = p->output_filter(p->output_ctx, out);

        // 更新链
        ngx_chain_update_chains(p->pool, &p->free, &p->busy, &out, p->tag);

        if (rc == NGX_ERROR) {
            p->downstream_error = 1;
            return ngx_event_pipe_drain_chains(p);
        }

        // 处理空闲链
        for (cl = p->free; cl; cl = cl->next) {

            if (cl->buf->temp_file) {
                if (p->cacheable || !p->cyclic_temp_file) {
                    continue;
                }

                // 如果所有缓冲区都已发送，重置p->temp_offset

                if (cl->buf->file_last == p->temp_file->offset) {
                    p->temp_file->offset = 0;
                }
            }

            // TODO: 如果p->free_bufs且上游完成，则释放buf

            // 将空闲的shadow raw buf添加到p->free_raw_bufs

            if (cl->buf->last_shadow) {
                if (ngx_event_pipe_add_free_buf(p, cl->buf->shadow) != NGX_OK) {
                    return NGX_ABORT;
                }

                cl->buf->last_shadow = 0;
            }

            cl->buf->shadow = NULL;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_event_pipe_write_chain_to_temp_file(ngx_event_pipe_t *p)
{
    ssize_t       size, bsize, n;
    ngx_buf_t    *b;
    ngx_uint_t    prev_last_shadow;
    ngx_chain_t  *cl, *tl, *next, *out, **ll, **last_out, **last_free;

#if (NGX_THREADS)

    // 如果正在写入,检查是否需要等待异步I/O完成
    if (p->writing) {

        if (p->aio) {
            return NGX_AGAIN;
        }

        out = p->writing;
        p->writing = NULL;

        // 继续写入临时文件
        n = ngx_write_chain_to_temp_file(p->temp_file, NULL);

        if (n == NGX_ERROR) {
            return NGX_ABORT;
        }

        goto done;
    }

#endif

    // 如果有待写入的缓冲区,将其添加到输出链的开头
    if (p->buf_to_file) {
        out = ngx_alloc_chain_link(p->pool);
        if (out == NULL) {
            return NGX_ABORT;
        }

        out->buf = p->buf_to_file;
        out->next = p->in;

    } else {
        out = p->in;
    }

    // 如果不可缓存,限制写入大小
    if (!p->cacheable) {

        size = 0;
        cl = out;
        ll = NULL;
        prev_last_shadow = 1;

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe offset: %O", p->temp_file->offset);

        // 计算可写入的数据大小
        do {
            bsize = cl->buf->last - cl->buf->pos;

            ngx_log_debug4(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "pipe buf ls:%d %p, pos %p, size: %z",
                           cl->buf->last_shadow, cl->buf->start,
                           cl->buf->pos, bsize);

            // 检查是否超过单次写入或最大文件大小限制
            if (prev_last_shadow
                && ((size + bsize > p->temp_file_write_size)
                    || (p->temp_file->offset + size + bsize
                        > p->max_temp_file_size)))
            {
                break;
            }

            prev_last_shadow = cl->buf->last_shadow;

            size += bsize;
            ll = &cl->next;
            cl = cl->next;

        } while (cl);

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0, "size: %z", size);

        if (ll == NULL) {
            return NGX_BUSY;
        }

        // 更新输入链
        if (cl) {
            p->in = cl;
            *ll = NULL;

        } else {
            p->in = NULL;
            p->last_in = &p->in;
        }

    } else {
        // 如果可缓存,清空输入链
        p->in = NULL;
        p->last_in = &p->in;
    }

#if (NGX_THREADS)
    // 设置线程相关参数
    if (p->thread_handler) {
        p->temp_file->thread_write = 1;
        p->temp_file->file.thread_task = p->thread_task;
        p->temp_file->file.thread_handler = p->thread_handler;
        p->temp_file->file.thread_ctx = p->thread_ctx;
    }
#endif

    // 写入临时文件
    n = ngx_write_chain_to_temp_file(p->temp_file, out);

    if (n == NGX_ERROR) {
        return NGX_ABORT;
    }

#if (NGX_THREADS)

    // 如果是异步写入,设置相关参数并返回
    if (n == NGX_AGAIN) {
        p->writing = out;
        p->thread_task = p->temp_file->file.thread_task;
        return NGX_AGAIN;
    }

done:

#endif

    // 处理待写入的缓冲区
    if (p->buf_to_file) {
        p->temp_file->offset = p->buf_to_file->last - p->buf_to_file->pos;
        n -= p->buf_to_file->last - p->buf_to_file->pos;
        p->buf_to_file = NULL;
        out = out->next;
    }

    // 如果成功写入数据
    if (n > 0) {
        // 更新现有缓冲区或添加新缓冲区

        if (p->out) {
            // 查找最后一个输出缓冲区
            for (cl = p->out; cl->next; cl = cl->next) { /* void */ }

            b = cl->buf;

            // 如果可以直接追加到最后一个缓冲区
            if (b->file_last == p->temp_file->offset) {
                p->temp_file->offset += n;
                b->file_last = p->temp_file->offset;
                goto free;
            }

            last_out = &cl->next;

        } else {
            last_out = &p->out;
        }

        // 分配新的缓冲区
        cl = ngx_chain_get_free_buf(p->pool, &p->free);
        if (cl == NULL) {
            return NGX_ABORT;
        }

        b = cl->buf;

        ngx_memzero(b, sizeof(ngx_buf_t));

        b->tag = p->tag;

        b->file = &p->temp_file->file;
        b->file_pos = p->temp_file->offset;
        p->temp_file->offset += n;
        b->file_last = p->temp_file->offset;

        b->in_file = 1;
        b->temp_file = 1;

        *last_out = cl;
    }

free:

    // 查找最后一个空闲原始缓冲区
    for (last_free = &p->free_raw_bufs;
         *last_free != NULL;
         last_free = &(*last_free)->next)
    {
        /* void */
    }

    // 释放已处理的缓冲区
    for (cl = out; cl; cl = next) {
        next = cl->next;

        cl->next = p->free;
        p->free = cl;

        b = cl->buf;

        // 处理shadow缓冲区
        if (b->last_shadow) {

            tl = ngx_alloc_chain_link(p->pool);
            if (tl == NULL) {
                return NGX_ABORT;
            }

            tl->buf = b->shadow;
            tl->next = NULL;

            *last_free = tl;
            last_free = &tl->next;

            b->shadow->pos = b->shadow->start;
            b->shadow->last = b->shadow->start;

            ngx_event_pipe_remove_shadow_links(b->shadow);
        }
    }

    return NGX_OK;
}


/* the copy input filter */

ngx_int_t
ngx_event_pipe_copy_input_filter(ngx_event_pipe_t *p, ngx_buf_t *buf)
{
    ngx_buf_t    *b;
    ngx_chain_t  *cl;

    // 如果缓冲区为空，直接返回
    if (buf->pos == buf->last) {
        return NGX_OK;
    }

    // 如果上游已经完成，记录日志并返回
    if (p->upstream_done) {
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "input data after close");
        return NGX_OK;
    }

    // 如果长度为0，标记上游完成并记录警告日志
    if (p->length == 0) {
        p->upstream_done = 1;

        ngx_log_error(NGX_LOG_WARN, p->log, 0,
                      "upstream sent more data than specified in "
                      "\"Content-Length\" header");

        return NGX_OK;
    }

    // 从内存池中获取一个空闲的缓冲区链
    cl = ngx_chain_get_free_buf(p->pool, &p->free);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;

    // 复制缓冲区信息并设置相关属性
    ngx_memcpy(b, buf, sizeof(ngx_buf_t));
    b->shadow = buf;
    b->tag = p->tag;
    b->last_shadow = 1;
    b->recycled = 1;
    buf->shadow = b;

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0, "input buf #%d", b->num);

    // 将新的缓冲区链接到输入链的末尾
    if (p->in) {
        *p->last_in = cl;
    } else {
        p->in = cl;
    }
    p->last_in = &cl->next;

    // 如果长度为-1，表示不限长度，直接返回
    if (p->length == -1) {
        return NGX_OK;
    }

    // 检查接收的数据是否超过指定的Content-Length
    if (b->last - b->pos > p->length) {

        ngx_log_error(NGX_LOG_WARN, p->log, 0,
                      "upstream sent more data than specified in "
                      "\"Content-Length\" header");

        // 调整缓冲区大小并标记上游完成
        b->last = b->pos + p->length;
        p->upstream_done = 1;

        return NGX_OK;
    }

    // 更新剩余长度
    p->length -= b->last - b->pos;

    return NGX_OK;
}


static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    ngx_buf_t  *b, *next;

    // 获取buf的影子缓冲区
    b = buf->shadow;

    // 如果没有影子缓冲区，直接返回
    if (b == NULL) {
        return;
    }

    // 遍历影子链，直到最后一个影子
    while (!b->last_shadow) {
        next = b->shadow;

        // 重置影子缓冲区的属性
        b->temporary = 0;
        b->recycled = 0;

        // 断开影子链接
        b->shadow = NULL;
        b = next;
    }

    // 处理最后一个影子缓冲区
    b->temporary = 0;
    b->recycled = 0;
    b->last_shadow = 0;

    // 断开最后一个影子的链接
    b->shadow = NULL;

    // 断开原始缓冲区的影子链接
    buf->shadow = NULL;
}


ngx_int_t
ngx_event_pipe_add_free_buf(ngx_event_pipe_t *p, ngx_buf_t *b)
{
    ngx_chain_t  *cl;

    // 分配一个新的链表节点
    cl = ngx_alloc_chain_link(p->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    // 如果缓冲区是待写入文件的缓冲区，则调整位置指针
    if (p->buf_to_file && b->start == p->buf_to_file->start) {
        b->pos = p->buf_to_file->last;
        b->last = p->buf_to_file->last;
    } else {
        // 否则重置缓冲区位置指针
        b->pos = b->start;
        b->last = b->start;
    }

    // 清除影子缓冲区
    b->shadow = NULL;

    // 将缓冲区与链表节点关联
    cl->buf = b;

    // 如果空闲原始缓冲区链表为空，直接添加
    if (p->free_raw_bufs == NULL) {
        p->free_raw_bufs = cl;
        cl->next = NULL;
        return NGX_OK;
    }

    // 如果第一个空闲原始缓冲区是空的
    if (p->free_raw_bufs->buf->pos == p->free_raw_bufs->buf->last) {
        // 将新的空闲缓冲区添加到链表头部
        cl->next = p->free_raw_bufs;
        p->free_raw_bufs = cl;
        return NGX_OK;
    }

    // 如果第一个空闲原始缓冲区部分填充，则将新的空闲缓冲区添加到其后
    cl->next = p->free_raw_bufs->next;
    p->free_raw_bufs->next = cl;

    return NGX_OK;
}


static ngx_int_t
ngx_event_pipe_drain_chains(ngx_event_pipe_t *p)
{
    ngx_chain_t  *cl, *tl;

    for ( ;; ) {
        // 检查并处理busy、out和in链表
        if (p->busy) {
            cl = p->busy;
            p->busy = NULL;

        } else if (p->out) {
            cl = p->out;
            p->out = NULL;

        } else if (p->in) {
            cl = p->in;
            p->in = NULL;

        } else {
            // 所有链表都为空，完成处理
            return NGX_OK;
        }

        // 遍历当前链表
        while (cl) {
            // 处理last_shadow缓冲区
            if (cl->buf->last_shadow) {
                // 将shadow缓冲区添加到空闲缓冲区列表
                if (ngx_event_pipe_add_free_buf(p, cl->buf->shadow) != NGX_OK) {
                    return NGX_ABORT;
                }

                cl->buf->last_shadow = 0;
            }

            // 清除shadow指针
            cl->buf->shadow = NULL;
            
            // 将当前链表节点添加到空闲链表
            tl = cl->next;
            cl->next = p->free;
            p->free = cl;
            cl = tl;
        }
    }
}
