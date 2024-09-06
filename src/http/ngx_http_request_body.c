
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_request_body.c
 *
 * 该文件实现了Nginx处理HTTP请求体的功能。
 *
 * 支持的功能:
 * 1. 读取客户端请求体
 * 2. 处理大型请求体的分块读取
 * 3. 处理管道化请求
 * 4. 请求体的临时存储和缓冲
 * 5. 请求体的丢弃处理
 * 6. 支持请求体到文件的写入
 * 7. 处理请求体读取超时
 * 8. 处理请求体大小限制
 *
 * 支持的指令:
 * - client_body_buffer_size: 设置读取客户端请求体的缓冲区大小
 *   语法: client_body_buffer_size size;
 *   默认值: client_body_buffer_size 8k|16k;
 *   上下文: http, server, location
 *
 * - client_body_timeout: 定义读取客户端请求体的超时时间
 *   语法: client_body_timeout time;
 *   默认值: client_body_timeout 60s;
 *   上下文: http, server, location
 *
 * - client_max_body_size: 设置客户端请求体的最大允许大小
 *   语法: client_max_body_size size;
 *   默认值: client_max_body_size 1m;
 *   上下文: http, server, location
 *
 * - client_body_temp_path: 定义存储客户端请求体的临时文件路径
 *   语法: client_body_temp_path path [level1 [level2 [level3]]];
 *   默认值: client_body_temp_path client_body_temp;
 *   上下文: http, server, location
 *
 * 支持的变量:
 * - $request_body: 客户端请求体
 * - $request_body_file: 存储了客户端请求体的临时文件名
 *
 * 使用注意点:
 * 1. 合理设置client_body_buffer_size，避免频繁的磁盘I/O
 * 2. 注意client_body_timeout的设置，防止长时间占用连接
 * 3. 根据应用需求适当设置client_max_body_size，防止过大的请求体导致服务器负载过高
 * 4. 确保client_body_temp_path指向的路径有足够的磁盘空间和适当的权限
 * 5. 处理大文件上传时，考虑使用流式处理而非完全缓冲
 * 6. 在处理请求体时注意内存使用，避免内存泄漏
 * 7. 对于敏感数据，确保临时文件被正确删除
 * 8. 在高并发环境下，注意临时文件的创建和删除可能对性能造成的影响
 * 9. 使用$request_body变量时要注意，它可能会占用大量内存
 * 10. 在反向代理场景中，注意请求体的转发和可能的修改
 */



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/* 
 * 处理客户端请求体读取的处理函数
 * 参数:
 *   r: HTTP请求结构体指针
 */
static void ngx_http_read_client_request_body_handler(ngx_http_request_t *r);
/*
 * 执行读取客户端请求体的函数
 * 参数:
 *   r: HTTP请求结构体指针
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 *   NGX_AGAIN: 需要继续读取
 */
static ngx_int_t ngx_http_do_read_client_request_body(ngx_http_request_t *r);
/*
 * 复制管道化请求头
 * 参数:
 *   r: HTTP请求结构体指针
 *   buf: 缓冲区指针
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 */
static ngx_int_t ngx_http_copy_pipelined_header(ngx_http_request_t *r,
    ngx_buf_t *buf);
/*
 * 写入请求体
 * 参数:
 *   r: HTTP请求结构体指针
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 */
static ngx_int_t ngx_http_write_request_body(ngx_http_request_t *r);
/*
 * 读取丢弃的请求体
 * 参数:
 *   r: HTTP请求结构体指针
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 */
static ngx_int_t ngx_http_read_discarded_request_body(ngx_http_request_t *r);
/*
 * 丢弃请求体过滤器
 * 参数:
 *   r: HTTP请求结构体指针
 *   b: 缓冲区指针
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 */
static ngx_int_t ngx_http_discard_request_body_filter(ngx_http_request_t *r,
    ngx_buf_t *b);
/*
 * 测试期望
 * 参数:
 *   r: HTTP请求结构体指针
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 */
static ngx_int_t ngx_http_test_expect(ngx_http_request_t *r);

/*
 * 请求体过滤器
 * 参数:
 *   r: HTTP请求结构体指针
 *   in: 输入链表指针
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 */
static ngx_int_t ngx_http_request_body_filter(ngx_http_request_t *r,
    ngx_chain_t *in);
/*
 * 请求体长度过滤器
 * 参数:
 *   r: HTTP请求结构体指针
 *   in: 输入链表指针
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 */
static ngx_int_t ngx_http_request_body_length_filter(ngx_http_request_t *r,
    ngx_chain_t *in);
/*
 * 请求体分块过滤器
 * 参数:
 *   r: HTTP请求结构体指针
 *   in: 输入链表指针
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 */
static ngx_int_t ngx_http_request_body_chunked_filter(ngx_http_request_t *r,
    ngx_chain_t *in);


/**
 * 读取客户端请求体
 * 
 * @param r HTTP请求结构体指针
 * @param post_handler 读取完成后的回调处理函数
 * @return NGX_OK 成功, NGX_ERROR 错误, 或其他特定的HTTP状态码
 */
ngx_int_t
ngx_http_read_client_request_body(ngx_http_request_t *r,
    ngx_http_client_body_handler_pt post_handler)
{
    size_t                     preread;
    ssize_t                    size;
    ngx_int_t                  rc;
    ngx_buf_t                 *b;
    ngx_chain_t                out;
    ngx_http_request_body_t   *rb;
    ngx_http_core_loc_conf_t  *clcf;

    // 增加主请求计数
    r->main->count++;

    // 检查是否为主请求，是否已有请求体或需要丢弃请求体
    if (r != r->main || r->request_body || r->discard_body) {
        r->request_body_no_buffering = 0;
        post_handler(r);
        return NGX_OK;
    }

    // 测试Expect头
    if (ngx_http_test_expect(r) != NGX_OK) {
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto done;
    }

    // 分配请求体结构体内存
    rb = ngx_pcalloc(r->pool, sizeof(ngx_http_request_body_t));
    if (rb == NULL) {
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto done;
    }

    /*
     * ngx_pcalloc()已经设置了以下字段:
     *
     *     rb->temp_file = NULL;
     *     rb->bufs = NULL;
     *     rb->buf = NULL;
     *     rb->free = NULL;
     *     rb->busy = NULL;
     *     rb->chunked = NULL;
     *     rb->received = 0;
     *     rb->filter_need_buffering = 0;
     *     rb->last_sent = 0;
     *     rb->last_saved = 0;
     */

    rb->rest = -1;
    rb->post_handler = post_handler;

    r->request_body = rb;

    // 检查是否有Content-Length或使用分块传输编码
    if (r->headers_in.content_length_n < 0 && !r->headers_in.chunked) {
        r->request_body_no_buffering = 0;
        post_handler(r);
        return NGX_OK;
    }

#if (NGX_HTTP_V2)
    // 处理HTTP/2请求
    if (r->stream) {
        rc = ngx_http_v2_read_request_body(r);
        goto done;
    }
#endif

#if (NGX_HTTP_V3)
    // 处理HTTP/3请求
    if (r->http_version == NGX_HTTP_VERSION_30) {
        rc = ngx_http_v3_read_request_body(r);
        goto done;
    }
#endif

    // 计算预读取的数据量
    preread = r->header_in->last - r->header_in->pos;

    if (preread) {
        // 处理预读取的请求体部分
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http client request body preread %uz", preread);

        out.buf = r->header_in;
        out.next = NULL;

        rc = ngx_http_request_body_filter(r, &out);

        if (rc != NGX_OK) {
            goto done;
        }

        r->request_length += preread - (r->header_in->last - r->header_in->pos);

        // 检查是否可以将整个请求体放入r->header_in
        if (!r->headers_in.chunked
            && rb->rest > 0
            && rb->rest <= (off_t) (r->header_in->end - r->header_in->last))
        {
            b = ngx_calloc_buf(r->pool);
            if (b == NULL) {
                rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
                goto done;
            }

            b->temporary = 1;
            b->start = r->header_in->pos;
            b->pos = r->header_in->pos;
            b->last = r->header_in->last;
            b->end = r->header_in->end;

            rb->buf = b;

            r->read_event_handler = ngx_http_read_client_request_body_handler;
            r->write_event_handler = ngx_http_request_empty_handler;

            rc = ngx_http_do_read_client_request_body(r);
            goto done;
        }

    } else {
        // 设置rb->rest
        rc = ngx_http_request_body_filter(r, NULL);

        if (rc != NGX_OK) {
            goto done;
        }
    }

    // 检查是否已经读取完整个请求体
    if (rb->rest == 0 && rb->last_saved) {
        r->request_body_no_buffering = 0;
        post_handler(r);
        return NGX_OK;
    }

    if (rb->rest < 0) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "negative request body rest");
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto done;
    }

    // 获取配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 计算缓冲区大小
    size = clcf->client_body_buffer_size;
    size += size >> 2;

    // TODO: 处理r->request_body_in_single_buf

    // 调整缓冲区大小
    if (!r->headers_in.chunked && rb->rest < size) {
        size = (ssize_t) rb->rest;

        if (r->request_body_in_single_buf) {
            size += preread;
        }

        if (size == 0) {
            size++;
        }

    } else {
        size = clcf->client_body_buffer_size;
    }

    // 创建临时缓冲区
    rb->buf = ngx_create_temp_buf(r->pool, size);
    if (rb->buf == NULL) {
        rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto done;
    }

    // 设置读写事件处理函数
    r->read_event_handler = ngx_http_read_client_request_body_handler;
    r->write_event_handler = ngx_http_request_empty_handler;

    // 读取请求体
    rc = ngx_http_do_read_client_request_body(r);

done:

    // 处理无缓冲读取的情况
    if (r->request_body_no_buffering
        && (rc == NGX_OK || rc == NGX_AGAIN))
    {
        if (rc == NGX_OK) {
            r->request_body_no_buffering = 0;

        } else {
            /* rc == NGX_AGAIN */
            r->reading_body = 1;
        }

        r->read_event_handler = ngx_http_block_reading;
        post_handler(r);
    }

    // 减少主请求计数
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        r->main->count--;
    }

    return rc;
}


/**
 * @brief 读取未缓冲的请求体
 * 
 * @param r HTTP请求结构体指针
 * @return ngx_int_t 返回读取结果
 */
ngx_int_t
ngx_http_read_unbuffered_request_body(ngx_http_request_t *r)
{
    ngx_int_t  rc;

#if (NGX_HTTP_V2)
    // 处理HTTP/2请求
    if (r->stream) {
        rc = ngx_http_v2_read_unbuffered_request_body(r);

        if (rc == NGX_OK) {
            r->reading_body = 0;
        }

        return rc;
    }
#endif

#if (NGX_HTTP_V3)
    // 处理HTTP/3请求
    if (r->http_version == NGX_HTTP_VERSION_30) {
        rc = ngx_http_v3_read_unbuffered_request_body(r);

        if (rc == NGX_OK) {
            r->reading_body = 0;
        }

        return rc;
    }
#endif

    // 检查连接是否超时
    if (r->connection->read->timedout) {
        r->connection->timedout = 1;
        return NGX_HTTP_REQUEST_TIME_OUT;
    }

    // 读取客户端请求体
    rc = ngx_http_do_read_client_request_body(r);

    if (rc == NGX_OK) {
        r->reading_body = 0;
    }

    return rc;
}

/**
 * @brief 读取客户端请求体的处理函数
 * 
 * @param r HTTP请求结构体指针
 */
static void
ngx_http_read_client_request_body_handler(ngx_http_request_t *r)
{
    ngx_int_t  rc;

    // 检查连接是否超时
    if (r->connection->read->timedout) {
        r->connection->timedout = 1;
        ngx_http_finalize_request(r, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    // 读取客户端请求体
    rc = ngx_http_do_read_client_request_body(r);

    // 如果返回特殊响应，结束请求
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        ngx_http_finalize_request(r, rc);
    }
}


/**
 * @brief 读取客户端请求体的主要处理函数
 * 
 * @param r HTTP请求结构体指针
 * @return ngx_int_t 返回处理结果
 */
static ngx_int_t
ngx_http_do_read_client_request_body(ngx_http_request_t *r)
{
    off_t                      rest;
    size_t                     size;
    ssize_t                    n;
    ngx_int_t                  rc;
    ngx_uint_t                 flush;
    ngx_chain_t                out;
    ngx_connection_t          *c;
    ngx_http_request_body_t   *rb;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    rb = r->request_body;
    flush = 1;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http read client request body");

    for ( ;; ) {
        for ( ;; ) {
            // 检查是否已读取完所有请求体
            if (rb->rest == 0) {
                break;
            }

            // 检查缓冲区是否已满
            if (rb->buf->last == rb->buf->end) {

                // 更新链表
                rc = ngx_http_request_body_filter(r, NULL);

                if (rc != NGX_OK) {
                    return rc;
                }

                // 处理忙碌的缓冲区
                if (rb->busy != NULL) {
                    if (r->request_body_no_buffering) {
                        // 处理无缓冲模式
                        if (c->read->timer_set) {
                            ngx_del_timer(c->read);
                        }

                        if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
                            return NGX_HTTP_INTERNAL_SERVER_ERROR;
                        }

                        return NGX_AGAIN;
                    }

                    if (rb->filter_need_buffering) {
                        // 处理需要缓冲的情况
                        clcf = ngx_http_get_module_loc_conf(r,
                                                         ngx_http_core_module);
                        ngx_add_timer(c->read, clcf->client_body_timeout);

                        if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
                            return NGX_HTTP_INTERNAL_SERVER_ERROR;
                        }

                        return NGX_AGAIN;
                    }

                    ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                                  "busy buffers after request body flush");

                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                // 重置缓冲区
                flush = 0;
                rb->buf->pos = rb->buf->start;
                rb->buf->last = rb->buf->start;
            }

            // 计算可读取的数据大小
            size = rb->buf->end - rb->buf->last;
            rest = rb->rest - (rb->buf->last - rb->buf->pos);

            if ((off_t) size > rest) {
                size = (size_t) rest;
            }

            if (size == 0) {
                break;
            }

            // 从连接中读取数据
            n = c->recv(c, rb->buf->last, size);

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http client request body recv %z", n);

            if (n == NGX_AGAIN) {
                break;
            }

            if (n == 0) {
                ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "client prematurely closed connection");
            }

            if (n == 0 || n == NGX_ERROR) {
                c->error = 1;
                return NGX_HTTP_BAD_REQUEST;
            }

            // 更新缓冲区和请求长度
            rb->buf->last += n;
            r->request_length += n;

            // 将缓冲区传递给请求体过滤器链
            flush = 0;
            out.buf = rb->buf;
            out.next = NULL;

            rc = ngx_http_request_body_filter(r, &out);

            if (rc != NGX_OK) {
                return rc;
            }

            if (rb->rest == 0) {
                break;
            }

            if (rb->buf->last < rb->buf->end) {
                break;
            }
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http client request body rest %O", rb->rest);

        // 刷新请求体过滤器
        if (flush) {
            rc = ngx_http_request_body_filter(r, NULL);

            if (rc != NGX_OK) {
                return rc;
            }
        }

        // 检查是否已读取完所有数据
        if (rb->rest == 0 && rb->last_saved) {
            break;
        }

        // 处理读取事件
        if (!c->read->ready || rb->rest == 0) {

            clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
            ngx_add_timer(c->read, clcf->client_body_timeout);

            if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            return NGX_AGAIN;
        }
    }

    // 处理管道化的请求头
    if (ngx_http_copy_pipelined_header(r, rb->buf) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 清理定时器
    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    // 设置读取事件处理器
    if (!r->request_body_no_buffering) {
        r->read_event_handler = ngx_http_block_reading;
        rb->post_handler(r);
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_copy_pipelined_header(ngx_http_request_t *r, ngx_buf_t *buf)
{
    size_t                     n;
    ngx_buf_t                 *b;
    ngx_chain_t               *cl;
    ngx_http_connection_t     *hc;
    ngx_http_core_srv_conf_t  *cscf;

    b = r->header_in;
    n = buf->last - buf->pos;

    // 如果缓冲区相同或没有数据需要复制，直接返回
    if (buf == b || n == 0) {
        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http body pipelined header: %uz", n);

    /*
     * 如果客户端请求体缓冲区中有管道化的请求，
     * 将其复制到 r->header_in 缓冲区（如果有足够空间），
     * 或者分配一个大的客户端头部缓冲区
     */

    // 检查 r->header_in 缓冲区是否有足够空间
    if (n > (size_t) (b->end - b->last)) {

        hc = r->http_connection;

        // 尝试从空闲链表中获取缓冲区
        if (hc->free) {
            cl = hc->free;
            hc->free = cl->next;

            b = cl->buf;

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http large header free: %p %uz",
                           b->pos, b->end - b->last);

        } else {
            // 如果没有空闲缓冲区，创建一个新的临时缓冲区
            cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

            b = ngx_create_temp_buf(r->connection->pool,
                                    cscf->large_client_header_buffers.size);
            if (b == NULL) {
                return NGX_ERROR;
            }

            cl = ngx_alloc_chain_link(r->connection->pool);
            if (cl == NULL) {
                return NGX_ERROR;
            }

            cl->buf = b;

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http large header alloc: %p %uz",
                           b->pos, b->end - b->last);
        }

        // 将新分配的缓冲区添加到忙碌链表中
        cl->next = hc->busy;
        hc->busy = cl;
        hc->nbusy++;

        r->header_in = b;

        // 再次检查新分配的缓冲区是否足够大
        if (n > (size_t) (b->end - b->last)) {
            ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                          "too large pipelined header after reading body");
            return NGX_ERROR;
        }
    }

    // 复制管道化的请求头数据
    ngx_memcpy(b->last, buf->pos, n);

    // 更新缓冲区指针和请求长度
    b->last += n;
    r->request_length -= n;

    return NGX_OK;
}


/**
 * @brief 将请求体写入临时文件
 * 
 * @param r HTTP请求结构体指针
 * @return ngx_int_t 返回写入结果
 */
static ngx_int_t
ngx_http_write_request_body(ngx_http_request_t *r)
{
    ssize_t                    n;
    ngx_chain_t               *cl, *ln;
    ngx_temp_file_t           *tf;
    ngx_http_request_body_t   *rb;
    ngx_http_core_loc_conf_t  *clcf;

    rb = r->request_body;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http write client request body, bufs %p", rb->bufs);

    // 如果临时文件还未创建，则创建一个新的临时文件
    if (rb->temp_file == NULL) {
        tf = ngx_pcalloc(r->pool, sizeof(ngx_temp_file_t));
        if (tf == NULL) {
            return NGX_ERROR;
        }

        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        // 初始化临时文件结构
        tf->file.fd = NGX_INVALID_FILE;
        tf->file.log = r->connection->log;
        tf->path = clcf->client_body_temp_path;
        tf->pool = r->pool;
        tf->warn = "a client request body is buffered to a temporary file";
        tf->log_level = r->request_body_file_log_level;
        tf->persistent = r->request_body_in_persistent_file;
        tf->clean = r->request_body_in_clean_file;

        // 设置文件访问权限
        if (r->request_body_file_group_access) {
            tf->access = 0660;
        }

        rb->temp_file = tf;

        // 如果没有缓冲区，但要求将请求体保存到文件中
        if (rb->bufs == NULL) {
            if (ngx_create_temp_file(&tf->file, tf->path, tf->pool,
                                     tf->persistent, tf->clean, tf->access)
                != NGX_OK)
            {
                return NGX_ERROR;
            }

            return NGX_OK;
        }
    }

    // 如果没有缓冲区，直接返回成功
    if (rb->bufs == NULL) {
        return NGX_OK;
    }

    // 将缓冲区中的数据写入临时文件
    n = ngx_write_chain_to_temp_file(rb->temp_file, rb->bufs);

    /* TODO: n == 0 or not complete and level event */

    if (n == NGX_ERROR) {
        return NGX_ERROR;
    }

    // 更新临时文件的偏移量
    rb->temp_file->offset += n;

    // 标记所有缓冲区为已写入，并释放链表节点
    for (cl = rb->bufs; cl; /* void */) {
        cl->buf->pos = cl->buf->last;

        ln = cl;
        cl = cl->next;
        ngx_free_chain(r->pool, ln);
    }

    // 清空缓冲区链表
    rb->bufs = NULL;

    return NGX_OK;
}


ngx_int_t
ngx_http_discard_request_body(ngx_http_request_t *r)
{
    ssize_t       size;
    ngx_int_t     rc;
    ngx_event_t  *rev;

    // 如果不是主请求，或者已经设置了丢弃请求体，或者已经有请求体，则直接返回
    if (r != r->main || r->discard_body || r->request_body) {
        return NGX_OK;
    }

#if (NGX_HTTP_V2)
    // 对于HTTP/2请求，设置跳过数据标志并返回
    if (r->stream) {
        r->stream->skip_data = 1;
        return NGX_OK;
    }
#endif

#if (NGX_HTTP_V3)
    // 对于HTTP/3请求，直接返回
    if (r->http_version == NGX_HTTP_VERSION_30) {
        return NGX_OK;
    }
#endif

    // 检查Expect头部
    if (ngx_http_test_expect(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    rev = r->connection->read;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0, "http set discard body");

    // 如果设置了读事件定时器，则删除它
    if (rev->timer_set) {
        ngx_del_timer(rev);
    }

    // 如果没有请求体或不是分块传输，直接返回
    if (r->headers_in.content_length_n <= 0 && !r->headers_in.chunked) {
        return NGX_OK;
    }

    // 计算请求头中剩余的数据大小
    size = r->header_in->last - r->header_in->pos;

    // 如果有剩余数据或是分块传输，则进行请求体过滤
    if (size || r->headers_in.chunked) {
        rc = ngx_http_discard_request_body_filter(r, r->header_in);

        if (rc != NGX_OK) {
            return rc;
        }

        if (r->headers_in.content_length_n == 0) {
            return NGX_OK;
        }
    }

    // 读取并丢弃请求体
    rc = ngx_http_read_discarded_request_body(r);

    if (rc == NGX_OK) {
        r->lingering_close = 0;
        return NGX_OK;
    }

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    // 如果需要继续读取（rc == NGX_AGAIN）
    r->read_event_handler = ngx_http_discarded_request_body_handler;

    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 增加请求引用计数，设置丢弃请求体标志
    r->count++;
    r->discard_body = 1;

    return NGX_OK;
}


void
ngx_http_discarded_request_body_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_msec_t                 timer;
    ngx_event_t               *rev;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    rev = c->read;

    // 检查读事件是否超时
    if (rev->timedout) {
        c->timedout = 1;
        c->error = 1;
        ngx_http_finalize_request(r, NGX_ERROR);
        return;
    }

    // 处理lingering close
    if (r->lingering_time) {
        timer = (ngx_msec_t) r->lingering_time - (ngx_msec_t) ngx_time();

        if ((ngx_msec_int_t) timer <= 0) {
            r->discard_body = 0;
            r->lingering_close = 0;
            ngx_http_finalize_request(r, NGX_ERROR);
            return;
        }

    } else {
        timer = 0;
    }

    // 读取并丢弃请求体
    rc = ngx_http_read_discarded_request_body(r);

    if (rc == NGX_OK) {
        // 请求体已完全丢弃
        r->discard_body = 0;
        r->lingering_close = 0;
        r->lingering_time = 0;
        ngx_http_finalize_request(r, NGX_DONE);
        return;
    }

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        c->error = 1;
        ngx_http_finalize_request(r, NGX_ERROR);
        return;
    }

    // 如果需要继续读取（rc == NGX_AGAIN）
    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        c->error = 1;
        ngx_http_finalize_request(r, NGX_ERROR);
        return;
    }

    // 设置lingering close定时器
    if (timer) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        timer *= 1000;

        if (timer > clcf->lingering_timeout) {
            timer = clcf->lingering_timeout;
        }

        ngx_add_timer(rev, timer);
    }
}


static ngx_int_t
ngx_http_read_discarded_request_body(ngx_http_request_t *r)
{
    size_t     size;
    ssize_t    n;
    ngx_int_t  rc;
    ngx_buf_t  b;
    u_char     buffer[NGX_HTTP_DISCARD_BUFFER_SIZE];

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http read discarded body");

    ngx_memzero(&b, sizeof(ngx_buf_t));

    b.temporary = 1;

    for ( ;; ) {
        // 如果请求体已全部读取，退出循环
        if (r->headers_in.content_length_n == 0) {
            break;
        }

        // 如果连接不可读，返回NGX_AGAIN
        if (!r->connection->read->ready) {
            return NGX_AGAIN;
        }

        // 计算本次要读取的数据大小
        size = (size_t) ngx_min(r->headers_in.content_length_n,
                                NGX_HTTP_DISCARD_BUFFER_SIZE);

        // 从连接中读取数据
        n = r->connection->recv(r->connection, buffer, size);

        if (n == NGX_ERROR) {
            r->connection->error = 1;
            return NGX_OK;
        }

        if (n == NGX_AGAIN) {
            return NGX_AGAIN;
        }

        if (n == 0) {
            return NGX_OK;
        }

        // 设置缓冲区指针
        b.pos = buffer;
        b.last = buffer + n;

        // 调用请求体过滤器
        rc = ngx_http_discard_request_body_filter(r, &b);

        if (rc != NGX_OK) {
            return rc;
        }
    }

    // 处理管道化的请求头
    if (ngx_http_copy_pipelined_header(r, &b) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 设置读事件处理函数为阻塞读取
    r->read_event_handler = ngx_http_block_reading;

    return NGX_OK;
}


static ngx_int_t
ngx_http_discard_request_body_filter(ngx_http_request_t *r, ngx_buf_t *b)
{
    size_t                     size;
    ngx_int_t                  rc;
    ngx_http_request_body_t   *rb;
    ngx_http_core_srv_conf_t  *cscf;

    // 如果请求头中指定了分块传输编码
    if (r->headers_in.chunked) {

        rb = r->request_body;

        // 如果请求体还未初始化，则进行初始化
        if (rb == NULL) {

            rb = ngx_pcalloc(r->pool, sizeof(ngx_http_request_body_t));
            if (rb == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            rb->chunked = ngx_pcalloc(r->pool, sizeof(ngx_http_chunked_t));
            if (rb->chunked == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            r->request_body = rb;
        }

        // 循环处理分块数据
        for ( ;; ) {

            // 解析分块数据
            rc = ngx_http_parse_chunked(r, b, rb->chunked);

            if (rc == NGX_OK) {

                // 成功解析一个分块

                size = b->last - b->pos;

                // 更新缓冲区位置和剩余大小
                if ((off_t) size > rb->chunked->size) {
                    b->pos += (size_t) rb->chunked->size;
                    rb->chunked->size = 0;

                } else {
                    rb->chunked->size -= size;
                    b->pos = b->last;
                }

                continue;
            }

            if (rc == NGX_DONE) {

                // 整个响应已成功解析

                r->headers_in.content_length_n = 0;
                break;
            }

            if (rc == NGX_AGAIN) {

                // 设置下次想要看到的数据量

                cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

                r->headers_in.content_length_n = ngx_max(rb->chunked->length,
                               (off_t) cscf->large_client_header_buffers.size);
                break;
            }

            // 无效的分块数据

            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "client sent invalid chunked body");

            return NGX_HTTP_BAD_REQUEST;
        }

    } else {
        // 非分块传输的情况
        size = b->last - b->pos;

        // 更新缓冲区位置和剩余内容长度
        if ((off_t) size > r->headers_in.content_length_n) {
            b->pos += (size_t) r->headers_in.content_length_n;
            r->headers_in.content_length_n = 0;

        } else {
            b->pos = b->last;
            r->headers_in.content_length_n -= size;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_test_expect(ngx_http_request_t *r)
{
    ngx_int_t   n;
    ngx_str_t  *expect;

    // 检查是否需要处理Expect头
    if (r->expect_tested
        || r->headers_in.expect == NULL
        || r->http_version < NGX_HTTP_VERSION_11
#if (NGX_HTTP_V2)
        || r->stream != NULL
#endif
#if (NGX_HTTP_V3)
        || r->connection->quic != NULL
#endif
       )
    {
        return NGX_OK;
    }

    r->expect_tested = 1;

    expect = &r->headers_in.expect->value;

    // 检查Expect头的值是否为"100-continue"
    if (expect->len != sizeof("100-continue") - 1
        || ngx_strncasecmp(expect->data, (u_char *) "100-continue",
                           sizeof("100-continue") - 1)
           != 0)
    {
        return NGX_OK;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "send 100 Continue");

    // 发送"100 Continue"响应
    n = r->connection->send(r->connection,
                            (u_char *) "HTTP/1.1 100 Continue" CRLF CRLF,
                            sizeof("HTTP/1.1 100 Continue" CRLF CRLF) - 1);

    if (n == sizeof("HTTP/1.1 100 Continue" CRLF CRLF) - 1) {
        return NGX_OK;
    }

    // 假设这样小的数据包应该能成功发送，如果失败则标记连接错误

    r->connection->error = 1;

    return NGX_ERROR;
}


/**
 * 请求体过滤器
 * 根据请求头中是否有chunked编码来选择相应的过滤器
 */
static ngx_int_t
ngx_http_request_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    // 如果请求头中指定了chunked编码
    if (r->headers_in.chunked) {
        // 使用chunked过滤器
        return ngx_http_request_body_chunked_filter(r, in);

    } else {
        // 否则使用普通的长度过滤器
        return ngx_http_request_body_length_filter(r, in);
    }
}


/**
 * 请求体长度过滤器
 * 处理具有固定长度的请求体
 */
static ngx_int_t
ngx_http_request_body_length_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    size_t                     size;
    ngx_int_t                  rc;
    ngx_buf_t                 *b;
    ngx_chain_t               *cl, *tl, *out, **ll;
    ngx_http_request_body_t   *rb;

    rb = r->request_body;

    out = NULL;
    ll = &out;

    // 如果是第一次调用此过滤器
    if (rb->rest == -1) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http request body content length filter");

        // 设置剩余需要读取的长度
        rb->rest = r->headers_in.content_length_n;

        // 如果请求体长度为0，创建一个空的缓冲区
        if (rb->rest == 0) {
            tl = ngx_chain_get_free_buf(r->pool, &rb->free);
            if (tl == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            b = tl->buf;

            ngx_memzero(b, sizeof(ngx_buf_t));

            b->last_buf = 1;

            *ll = tl;
            ll = &tl->next;
        }
    }

    // 遍历输入链
    for (cl = in; cl; cl = cl->next) {

        if (rb->rest == 0) {
            break;
        }

        // 获取一个新的缓冲区
        tl = ngx_chain_get_free_buf(r->pool, &rb->free);
        if (tl == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b = tl->buf;

        ngx_memzero(b, sizeof(ngx_buf_t));

        b->temporary = 1;
        b->tag = (ngx_buf_tag_t) &ngx_http_read_client_request_body;
        b->start = cl->buf->pos;
        b->pos = cl->buf->pos;
        b->last = cl->buf->last;
        b->end = cl->buf->end;
        b->flush = r->request_body_no_buffering;

        size = cl->buf->last - cl->buf->pos;

        // 如果当前缓冲区的数据小于剩余需要读取的长度
        if ((off_t) size < rb->rest) {
            cl->buf->pos = cl->buf->last;
            rb->rest -= size;

        } else {
            // 如果当前缓冲区的数据大于或等于剩余需要读取的长度
            cl->buf->pos += (size_t) rb->rest;
            rb->rest = 0;
            b->last = cl->buf->pos;
            b->last_buf = 1;
        }

        *ll = tl;
        ll = &tl->next;
    }

    // 调用顶层请求体过滤器
    rc = ngx_http_top_request_body_filter(r, out);

    // 更新链表
    ngx_chain_update_chains(r->pool, &rb->free, &rb->busy, &out,
                            (ngx_buf_tag_t) &ngx_http_read_client_request_body);

    return rc;
}


/**
 * 请求体chunked过滤器
 * 处理使用chunked编码的请求体
 */
static ngx_int_t
ngx_http_request_body_chunked_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    size_t                     size;
    ngx_int_t                  rc;
    ngx_buf_t                 *b;
    ngx_chain_t               *cl, *out, *tl, **ll;
    ngx_http_request_body_t   *rb;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;

    rb = r->request_body;

    out = NULL;
    ll = &out;

    // 如果是第一次调用此过滤器
    if (rb->rest == -1) {

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http request body chunked filter");

        // 初始化chunked结构
        rb->chunked = ngx_pcalloc(r->pool, sizeof(ngx_http_chunked_t));
        if (rb->chunked == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

        r->headers_in.content_length_n = 0;
        rb->rest = cscf->large_client_header_buffers.size;
    }

    // 遍历输入链
    for (cl = in; cl; cl = cl->next) {

        b = NULL;

        for ( ;; ) {

            ngx_log_debug7(NGX_LOG_DEBUG_EVENT, r->connection->log, 0,
                           "http body chunked buf "
                           "t:%d f:%d %p, pos %p, size: %z file: %O, size: %O",
                           cl->buf->temporary, cl->buf->in_file,
                           cl->buf->start, cl->buf->pos,
                           cl->buf->last - cl->buf->pos,
                           cl->buf->file_pos,
                           cl->buf->file_last - cl->buf->file_pos);

            // 解析chunked编码
            rc = ngx_http_parse_chunked(r, cl->buf, rb->chunked);

            if (rc == NGX_OK) {
                // 成功解析一个chunk

                clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

                // 检查请求体大小是否超过限制
                if (clcf->client_max_body_size
                    && clcf->client_max_body_size
                       - r->headers_in.content_length_n < rb->chunked->size)
                {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "client intended to send too large chunked "
                                  "body: %O+%O bytes",
                                  r->headers_in.content_length_n,
                                  rb->chunked->size);

                    r->lingering_close = 1;

                    return NGX_HTTP_REQUEST_ENTITY_TOO_LARGE;
                }

                // 处理小型chunk
                if (b
                    && rb->chunked->size <= 128
                    && cl->buf->last - cl->buf->pos >= rb->chunked->size)
                {
                    r->headers_in.content_length_n += rb->chunked->size;

                    if (rb->chunked->size < 8) {
                        // 对于非常小的chunk，逐字节复制
                        while (rb->chunked->size) {
                            *b->last++ = *cl->buf->pos++;
                            rb->chunked->size--;
                        }

                    } else {
                        // 对于较大的chunk，使用内存复制
                        ngx_memmove(b->last, cl->buf->pos, rb->chunked->size);
                        b->last += rb->chunked->size;
                        cl->buf->pos += rb->chunked->size;
                        rb->chunked->size = 0;
                    }

                    continue;
                }

                // 获取新的缓冲区
                tl = ngx_chain_get_free_buf(r->pool, &rb->free);
                if (tl == NULL) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                b = tl->buf;

                ngx_memzero(b, sizeof(ngx_buf_t));

                b->temporary = 1;
                b->tag = (ngx_buf_tag_t) &ngx_http_read_client_request_body;
                b->start = cl->buf->pos;
                b->pos = cl->buf->pos;
                b->last = cl->buf->last;
                b->end = cl->buf->end;
                b->flush = r->request_body_no_buffering;

                *ll = tl;
                ll = &tl->next;

                size = cl->buf->last - cl->buf->pos;

                // 处理chunk数据
                if ((off_t) size > rb->chunked->size) {
                    cl->buf->pos += (size_t) rb->chunked->size;
                    r->headers_in.content_length_n += rb->chunked->size;
                    rb->chunked->size = 0;

                } else {
                    rb->chunked->size -= size;
                    r->headers_in.content_length_n += size;
                    cl->buf->pos = cl->buf->last;
                }

                b->last = cl->buf->pos;

                continue;
            }

            if (rc == NGX_DONE) {
                // 整个请求体已成功解析

                rb->rest = 0;

                // 添加最后一个缓冲区
                tl = ngx_chain_get_free_buf(r->pool, &rb->free);
                if (tl == NULL) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }

                b = tl->buf;

                ngx_memzero(b, sizeof(ngx_buf_t));

                b->last_buf = 1;

                *ll = tl;
                ll = &tl->next;

                break;
            }

            if (rc == NGX_AGAIN) {
                // 需要更多数据

                cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

                rb->rest = ngx_max(rb->chunked->length,
                               (off_t) cscf->large_client_header_buffers.size);

                break;
            }

            // 无效的chunked编码
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "client sent invalid chunked body");

            return NGX_HTTP_BAD_REQUEST;
        }
    }

    // 调用顶层请求体过滤器
    rc = ngx_http_top_request_body_filter(r, out);

    // 更新链表
    ngx_chain_update_chains(r->pool, &rb->free, &rb->busy, &out,
                            (ngx_buf_tag_t) &ngx_http_read_client_request_body);

    return rc;
}


ngx_int_t
ngx_http_request_body_save_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_buf_t                 *b;
    ngx_chain_t               *cl, *tl, **ll;
    ngx_http_request_body_t   *rb;

    // 获取请求体结构
    rb = r->request_body;

    // 初始化链表指针
    ll = &rb->bufs;

    // 遍历已存在的缓冲区链表
    for (cl = rb->bufs; cl; cl = cl->next) {
        // 调试日志（已注释）
#if 0
        ngx_log_debug7(NGX_LOG_DEBUG_EVENT, r->connection->log, 0,
                       "http body old buf t:%d f:%d %p, pos %p, size: %z "
                       "file: %O, size: %O",
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
#endif

        ll = &cl->next;
    }

    // 遍历新的输入缓冲区链表
    for (cl = in; cl; cl = cl->next) {
        // 记录新缓冲区的调试信息
        ngx_log_debug7(NGX_LOG_DEBUG_EVENT, r->connection->log, 0,
                       "http body new buf t:%d f:%d %p, pos %p, size: %z "
                       "file: %O, size: %O",
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);

        // 检查是否为最后一个缓冲区
        if (cl->buf->last_buf) {
            // 如果已经标记为最后保存的缓冲区，则报错
            if (rb->last_saved) {
                ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                              "duplicate last buf in save filter");
                *ll = NULL;
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            // 标记为最后保存的缓冲区
            rb->last_saved = 1;
        }

        // 分配新的链表节点
        tl = ngx_alloc_chain_link(r->pool);
        if (tl == NULL) {
            *ll = NULL;
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        // 将新缓冲区添加到链表中
        tl->buf = cl->buf;
        *ll = tl;
        ll = &tl->next;
    }

    // 链表结束
    *ll = NULL;

    // 如果不需要缓冲，直接返回
    if (r->request_body_no_buffering) {
        return NGX_OK;
    }

    // 如果还有剩余数据需要处理
    if (rb->rest > 0) {
        // 如果缓冲区已满，尝试写入请求体
        if (rb->bufs && rb->buf && rb->buf->last == rb->buf->end
            && ngx_http_write_request_body(r) != NGX_OK)
        {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        return NGX_OK;
    }

    // 如果还未保存最后的缓冲区，返回
    if (!rb->last_saved) {
        return NGX_OK;
    }

    // 处理需要写入文件的情况
    if (rb->temp_file || r->request_body_in_file_only) {
        // 检查是否已经在文件中
        if (rb->bufs && rb->bufs->buf->in_file) {
            ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                          "body already in file");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        // 写入请求体到文件
        if (ngx_http_write_request_body(r) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        // 如果文件不为空，创建一个表示文件内容的缓冲区
        if (rb->temp_file->file.offset != 0) {
            cl = ngx_chain_get_free_buf(r->pool, &rb->free);
            if (cl == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            b = cl->buf;

            ngx_memzero(b, sizeof(ngx_buf_t));

            b->in_file = 1;
            b->file_last = rb->temp_file->file.offset;
            b->file = &rb->temp_file->file;

            rb->bufs = cl;
        }
    }

    return NGX_OK;
}
