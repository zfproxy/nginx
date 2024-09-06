
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_linux_sendfile_chain.c
 *
 * 该文件实现了Nginx在Linux系统上使用sendfile()系统调用发送文件的功能。
 *
 * 支持的功能:
 * 1. 使用sendfile()系统调用零拷贝发送文件
 * 2. 处理大文件(>2GB)的发送
 * 3. 支持多线程发送文件(如果启用NGX_THREADS)
 * 4. 处理TCP_NODELAY选项
 * 5. 链式发送多个文件和内存块
 *
 * 使用注意点:
 * 1. 仅适用于Linux系统，其他系统可能需要不同的实现
 * 2. 对于32位系统，文件偏移量有2GB限制
 * 3. 即使在64位系统上，单次sendfile()调用也有最大传输量限制
 * 4. 多线程模式下需要注意线程安全性
 * 5. 大文件传输时要注意内存使用和性能优化
 * 6. 错误处理需要考虑EAGAIN等特殊情况
 * 7. 与TCP_NODELAY等套接字选项配合使用时需要特别注意
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


// 使用sendfile系统调用发送文件
static ssize_t ngx_linux_sendfile(ngx_connection_t *c, ngx_buf_t *file,
    size_t size);

#if (NGX_THREADS)
#include <ngx_thread_pool.h>

#if !(NGX_HAVE_SENDFILE64)
#error sendfile64() is required!
#endif

// 在线程中使用sendfile发送文件
static ssize_t ngx_linux_sendfile_thread(ngx_connection_t *c, ngx_buf_t *file,
    size_t size);

// 线程池中sendfile操作的处理函数
static void ngx_linux_sendfile_thread_handler(void *data, ngx_log_t *log);
#endif


/*
 * Linux 2.4.21之前的版本中，sendfile()系统调用(系统调用号187)只能处理32位偏移量。
 * 如果off_t是64位的，包含<sys/sendfile.h>会导致编译错误。
 * 因此我们使用自定义的sendfile()定义，其中offset参数是int32_t类型，
 * 并且只对小于2G的文件部分使用sendfile()。
 * 详见src/os/unix/ngx_linux_config.h
 *
 * Linux 2.4.21引入了新的sendfile64()系统调用(系统调用号239)。
 *
 * 在Linux 2.6.16及之前的版本中，即使在64位平台上，sendfile()也不允许count参数超过2G-1字节，
 * 否则会返回EINVAL错误。因此我们将其限制为2G-1字节。
 *
 * 在Linux 2.6.16及之后的版本中，sendfile()会默默地将count参数限制为2G减去页面大小，
 * 即使在64位平台上也是如此。
 */

// 定义sendfile()可以处理的最大文件大小为2G-1字节
#define NGX_SENDFILE_MAXSIZE  2147483647L


ngx_chain_t *
ngx_linux_sendfile_chain(ngx_connection_t *c, ngx_chain_t *in, off_t limit)
{
    int            tcp_nodelay;
    off_t          send, prev_send;
    size_t         file_size, sent;
    ssize_t        n;
    ngx_err_t      err;
    ngx_buf_t     *file;
    ngx_event_t   *wev;
    ngx_chain_t   *cl;
    ngx_iovec_t    header;
    struct iovec   headers[NGX_IOVS_PREALLOCATE];

    wev = c->write;

    if (!wev->ready) {
        return in;
    }


    /* the maximum limit size is 2G-1 - the page size */

    if (limit == 0 || limit > (off_t) (NGX_SENDFILE_MAXSIZE - ngx_pagesize)) {
        limit = NGX_SENDFILE_MAXSIZE - ngx_pagesize;
    }


    send = 0;

    header.iovs = headers;
    header.nalloc = NGX_IOVS_PREALLOCATE;

    for ( ;; ) {
        prev_send = send;

        /* create the iovec and coalesce the neighbouring bufs */

        cl = ngx_output_chain_to_iovec(&header, in, limit - send, c->log);

        if (cl == NGX_CHAIN_ERROR) {
            return NGX_CHAIN_ERROR;
        }

        send += header.size;

        /* set TCP_CORK if there is a header before a file */

        if (c->tcp_nopush == NGX_TCP_NOPUSH_UNSET
            && header.count != 0
            && cl
            && cl->buf->in_file)
        {
            /* the TCP_CORK and TCP_NODELAY are mutually exclusive */

            if (c->tcp_nodelay == NGX_TCP_NODELAY_SET) {

                tcp_nodelay = 0;

                if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                               (const void *) &tcp_nodelay, sizeof(int)) == -1)
                {
                    err = ngx_socket_errno;

                    /*
                     * there is a tiny chance to be interrupted, however,
                     * we continue a processing with the TCP_NODELAY
                     * and without the TCP_CORK
                     */

                    if (err != NGX_EINTR) {
                        wev->error = 1;
                        ngx_connection_error(c, err,
                                             "setsockopt(TCP_NODELAY) failed");
                        return NGX_CHAIN_ERROR;
                    }

                } else {
                    c->tcp_nodelay = NGX_TCP_NODELAY_UNSET;

                    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0,
                                   "no tcp_nodelay");
                }
            }

            if (c->tcp_nodelay == NGX_TCP_NODELAY_UNSET) {

                if (ngx_tcp_nopush(c->fd) == -1) {
                    err = ngx_socket_errno;

                    /*
                     * there is a tiny chance to be interrupted, however,
                     * we continue a processing without the TCP_CORK
                     */

                    if (err != NGX_EINTR) {
                        wev->error = 1;
                        ngx_connection_error(c, err,
                                             ngx_tcp_nopush_n " failed");
                        return NGX_CHAIN_ERROR;
                    }

                } else {
                    c->tcp_nopush = NGX_TCP_NOPUSH_SET;

                    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0,
                                   "tcp_nopush");
                }
            }
        }

        /* get the file buf */

        if (header.count == 0 && cl && cl->buf->in_file && send < limit) {
            file = cl->buf;

            /* coalesce the neighbouring file bufs */

            file_size = (size_t) ngx_chain_coalesce_file(&cl, limit - send);

            send += file_size;
#if 1
            if (file_size == 0) {
                ngx_debug_point();
                return NGX_CHAIN_ERROR;
            }
#endif

            n = ngx_linux_sendfile(c, file, file_size);

            if (n == NGX_ERROR) {
                return NGX_CHAIN_ERROR;
            }

            if (n == NGX_DONE) {
                /* thread task posted */
                return in;
            }

            sent = (n == NGX_AGAIN) ? 0 : n;

        } else {
            n = ngx_writev(c, &header);

            if (n == NGX_ERROR) {
                return NGX_CHAIN_ERROR;
            }

            sent = (n == NGX_AGAIN) ? 0 : n;
        }

        c->sent += sent;

        in = ngx_chain_update_sent(in, sent);

        if (n == NGX_AGAIN) {
            wev->ready = 0;
            return in;
        }

        if ((size_t) (send - prev_send) != sent) {

            /*
             * sendfile() on Linux 4.3+ might be interrupted at any time,
             * and provides no indication if it was interrupted or not,
             * so we have to retry till an explicit EAGAIN
             *
             * sendfile() in threads can also report less bytes written
             * than we are prepared to send now, since it was started in
             * some point in the past, so we again have to retry
             */

            send = prev_send + sent;
        }

        if (send >= limit || in == NULL) {
            return in;
        }
    }
}


static ssize_t
ngx_linux_sendfile(ngx_connection_t *c, ngx_buf_t *file, size_t size)
{
#if (NGX_HAVE_SENDFILE64)
    off_t      offset;
#else
    int32_t    offset;
#endif
    ssize_t    n;
    ngx_err_t  err;

#if (NGX_THREADS)

    if (file->file->thread_handler) {
        return ngx_linux_sendfile_thread(c, file, size);
    }

#endif

#if (NGX_HAVE_SENDFILE64)
    offset = file->file_pos;
#else
    offset = (int32_t) file->file_pos;
#endif

eintr:

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   "sendfile: @%O %uz", file->file_pos, size);

    n = sendfile(c->fd, file->file->fd, &offset, size);

    if (n == -1) {
        err = ngx_errno;

        switch (err) {
        case NGX_EAGAIN:
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "sendfile() is not ready");
            return NGX_AGAIN;

        case NGX_EINTR:
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "sendfile() was interrupted");
            goto eintr;

        default:
            c->write->error = 1;
            ngx_connection_error(c, err, "sendfile() failed");
            return NGX_ERROR;
        }
    }

    if (n == 0) {
        /*
         * if sendfile returns zero, then someone has truncated the file,
         * so the offset became beyond the end of the file
         */

        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "sendfile() reported that \"%s\" was truncated at %O",
                      file->file->name.data, file->file_pos);

        return NGX_ERROR;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0, "sendfile: %z of %uz @%O",
                   n, size, file->file_pos);

    return n;
}


#if (NGX_THREADS)

typedef struct {
    ngx_buf_t     *file;
    ngx_socket_t   socket;
    size_t         size;

    size_t         sent;
    ngx_err_t      err;
} ngx_linux_sendfile_ctx_t;


static ssize_t
ngx_linux_sendfile_thread(ngx_connection_t *c, ngx_buf_t *file, size_t size)
{
    ngx_event_t               *wev;
    ngx_thread_task_t         *task;
    ngx_linux_sendfile_ctx_t  *ctx;

    ngx_log_debug3(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "linux sendfile thread: %d, %uz, %O",
                   file->file->fd, size, file->file_pos);

    task = c->sendfile_task;

    if (task == NULL) {
        task = ngx_thread_task_alloc(c->pool, sizeof(ngx_linux_sendfile_ctx_t));
        if (task == NULL) {
            return NGX_ERROR;
        }

        task->event.log = c->log;
        task->handler = ngx_linux_sendfile_thread_handler;

        c->sendfile_task = task;
    }

    ctx = task->ctx;
    wev = c->write;

    if (task->event.complete) {
        task->event.complete = 0;

        if (ctx->err == NGX_EAGAIN) {
            /*
             * if wev->complete is set, this means that a write event
             * happened while we were waiting for the thread task, so
             * we have to retry sending even on EAGAIN
             */

            if (wev->complete) {
                return 0;
            }

            return NGX_AGAIN;
        }

        if (ctx->err) {
            wev->error = 1;
            ngx_connection_error(c, ctx->err, "sendfile() failed");
            return NGX_ERROR;
        }

        if (ctx->sent == 0) {
            /*
             * if sendfile returns zero, then someone has truncated the file,
             * so the offset became beyond the end of the file
             */

            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "sendfile() reported that \"%s\" was truncated at %O",
                          file->file->name.data, file->file_pos);

            return NGX_ERROR;
        }

        return ctx->sent;
    }

    ctx->file = file;
    ctx->socket = c->fd;
    ctx->size = size;

    wev->complete = 0;

    if (file->file->thread_handler(task, file->file) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_DONE;
}


static void
ngx_linux_sendfile_thread_handler(void *data, ngx_log_t *log)
{
    ngx_linux_sendfile_ctx_t *ctx = data;

    off_t       offset;
    ssize_t     n;
    ngx_buf_t  *file;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, log, 0, "linux sendfile thread handler");

    file = ctx->file;
    offset = file->file_pos;

again:

    n = sendfile(ctx->socket, file->file->fd, &offset, ctx->size);

    if (n == -1) {
        ctx->err = ngx_errno;

    } else {
        ctx->sent = n;
        ctx->err = 0;
    }

#if 0
    ngx_time_update();
#endif

    ngx_log_debug4(NGX_LOG_DEBUG_EVENT, log, 0,
                   "sendfile: %z (err: %d) of %uz @%O",
                   n, ctx->err, ctx->size, file->file_pos);

    if (ctx->err == NGX_EINTR) {
        goto again;
    }
}

#endif /* NGX_THREADS */
