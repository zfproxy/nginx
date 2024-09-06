
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_send.c - Nginx发送数据模块
 *
 * 本文件实现了Nginx在Unix系统上发送数据的相关功能。
 *
 * 支持的功能:
 * 1. 通过send()系统调用发送数据
 * 2. 处理部分发送的情况
 * 3. 处理发送错误和中断
 * 4. 支持非阻塞I/O
 * 5. 与kqueue事件机制的集成(如果可用)
 *
 * 使用注意点:
 * - 确保在调用ngx_unix_send()之前已正确初始化连接结构体
 * - 注意处理返回值,尤其是部分发送和错误情况
 * - 在非阻塞模式下,需要正确处理EAGAIN错误
 * - 当与事件机制配合使用时,需要正确设置和检查相关的事件标志
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


ssize_t
ngx_unix_send(ngx_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    ngx_err_t     err;
    ngx_event_t  *wev;

    wev = c->write;

#if (NGX_HAVE_KQUEUE)

    if ((ngx_event_flags & NGX_USE_KQUEUE_EVENT) && wev->pending_eof) {
        (void) ngx_connection_error(c, wev->kq_errno,
                               "kevent() reported about an closed connection");
        wev->error = 1;
        return NGX_ERROR;
    }

#endif

    for ( ;; ) {
        n = send(c->fd, buf, size, 0);

        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "send: fd:%d %z of %uz", c->fd, n, size);

        if (n > 0) {
            if (n < (ssize_t) size) {
                wev->ready = 0;
            }

            c->sent += n;

            return n;
        }

        err = ngx_socket_errno;

        if (n == 0) {
            ngx_log_error(NGX_LOG_ALERT, c->log, err, "send() returned zero");
            wev->ready = 0;
            return n;
        }

        if (err == NGX_EAGAIN || err == NGX_EINTR) {
            wev->ready = 0;

            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "send() not ready");

            if (err == NGX_EAGAIN) {
                return NGX_AGAIN;
            }

        } else {
            wev->error = 1;
            (void) ngx_connection_error(c, err, "send() failed");
            return NGX_ERROR;
        }
    }
}
