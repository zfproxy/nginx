
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_udp_recv.c
 *
 * 本文件实现了Nginx中UDP接收相关的功能。
 *
 * 支持的功能:
 * 1. 非阻塞UDP数据接收
 * 2. 错误处理和日志记录
 * 3. 与事件模块的集成
 * 4. 支持kqueue事件机制(如果可用)
 *
 * 使用注意点:
 * - 确保在调用ngx_udp_unix_recv()前已正确初始化连接结构体
 * - 需要正确处理返回的错误码，特别是NGX_AGAIN
 * - 在高并发环境下，注意处理可能的缓冲区溢出情况
 * - 对于kqueue事件机制，需要注意available字段的处理
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


ssize_t
ngx_udp_unix_recv(ngx_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    ngx_err_t     err;
    ngx_event_t  *rev;

    rev = c->read;

    do {
        n = recv(c->fd, buf, size, 0);

        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "recv: fd:%d %z of %uz", c->fd, n, size);

        if (n >= 0) {

#if (NGX_HAVE_KQUEUE)

            if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
                rev->available -= n;

                /*
                 * rev->available may be negative here because some additional
                 * bytes may be received between kevent() and recv()
                 */

                if (rev->available <= 0) {
                    rev->ready = 0;
                    rev->available = 0;
                }
            }

#endif

            return n;
        }

        err = ngx_socket_errno;

        if (err == NGX_EAGAIN || err == NGX_EINTR) {
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "recv() not ready");
            n = NGX_AGAIN;

        } else {
            n = ngx_connection_error(c, err, "recv() failed");
            break;
        }

    } while (err == NGX_EINTR);

    rev->ready = 0;

    if (n == NGX_ERROR) {
        rev->error = 1;
    }

    return n;
}
