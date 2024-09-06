
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_udp_send.c
 *
 * 本文件实现了Nginx在Unix系统上的UDP发送功能。
 *
 * 支持的功能:
 * 1. 非阻塞UDP数据发送
 * 2. 错误处理和日志记录
 * 3. 与事件模块的集成
 * 4. 支持部分发送的情况处理
 *
 * 使用注意点:
 * 1. 确保在调用ngx_udp_unix_send()前已正确初始化连接结构体
 * 2. 需要正确处理返回的错误码，特别是NGX_AGAIN
 * 3. 在高并发环境下，注意处理可能的缓冲区满的情况
 * 4. 对于大数据包，可能需要多次调用才能完成发送
 * 5. 注意处理EINTR（系统调用中断）的情况
 * 6. 发送完成后，更新连接的sent字段以记录已发送的数据量
 * 7. 在处理不完整发送时，需要正确设置错误标志和日志
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


ssize_t
ngx_udp_unix_send(ngx_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    ngx_err_t     err;
    ngx_event_t  *wev;

    wev = c->write;

    for ( ;; ) {
        n = sendto(c->fd, buf, size, 0, c->sockaddr, c->socklen);

        ngx_log_debug4(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "sendto: fd:%d %z of %uz to \"%V\"",
                       c->fd, n, size, &c->addr_text);

        if (n >= 0) {
            if ((size_t) n != size) {
                wev->error = 1;
                (void) ngx_connection_error(c, 0, "sendto() incomplete");
                return NGX_ERROR;
            }

            c->sent += n;

            return n;
        }

        err = ngx_socket_errno;

        if (err == NGX_EAGAIN) {
            wev->ready = 0;
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, NGX_EAGAIN,
                           "sendto() not ready");
            return NGX_AGAIN;
        }

        if (err != NGX_EINTR) {
            wev->error = 1;
            (void) ngx_connection_error(c, err, "sendto() failed");
            return NGX_ERROR;
        }
    }
}
