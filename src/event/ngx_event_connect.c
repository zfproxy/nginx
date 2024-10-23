
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_event_connect.c
 *
 * 该文件实现了Nginx的事件连接功能，主要处理与上游服务器的连接建立和管理。
 *
 * 支持的功能:
 * 1. 建立TCP连接
 * 2. 非阻塞I/O
 * 3. 连接超时处理
 * 4. 连接失败重试
 * 5. 支持SSL/TLS连接
 * 6. 支持Unix域套接字连接
 * 7. 支持透明代理
 *
 * 支持的指令:
 * - proxy_connect_timeout
 * - proxy_ssl_certificate
 * - proxy_ssl_certificate_key
 * - proxy_ssl_protocols
 * - proxy_ssl_ciphers
 *
 * 相关变量:
 * - ngx_event_connect_peer
 * - ngx_peer_connection_t
 * - ngx_connection_t
 *
 * 使用注意点:
 * 1. 合理设置连接超时时间，避免长时间阻塞
 * 2. 注意处理连接失败的情况，实现合适的重试策略
 * 3. 使用SSL/TLS连接时，确保正确配置证书和密钥
 * 4. 在高并发场景下，注意控制同时建立的连接数
 * 5. 使用透明代理功能时，需要确保系统和网络环境支持
 * 6. 注意处理DNS解析失败的情况
 * 7. 合理配置错误日志级别，便于问题排查
 * 8. 在使用Unix域套接字时，注意文件权限设置
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>


#if (NGX_HAVE_TRANSPARENT_PROXY)
static ngx_int_t ngx_event_connect_set_transparent(ngx_peer_connection_t *pc,
    ngx_socket_t s);
#endif


/*
 * 连接到上游服务器的函数
 * 
 * 参数:
 * pc: 上游服务器连接对象
 * 
 * 返回值:
 * NGX_OK: 连接成功
 * NGX_ERROR: 连接失败
 * NGX_BUSY: 连接忙
 * NGX_DECLINED: 连接被拒绝
 */
ngx_int_t
ngx_event_connect_peer(ngx_peer_connection_t *pc)
{
    int                rc, type, value;
#if (NGX_HAVE_IP_BIND_ADDRESS_NO_PORT || NGX_LINUX)
    in_port_t          port;
#endif
    // 事件类型
    ngx_int_t          event;
    // 错误代码
    ngx_err_t          err;
    // 日志级别
    ngx_uint_t         level;
    // 套接字
    ngx_socket_t       s;
    // 读事件
    ngx_event_t       *rev;
    // 写事件
    ngx_event_t       *wev;
    // 连接对象
    ngx_connection_t  *c;

    // 调用pc对象的get方法，参数为pc对象本身和pc对象的data成员
    rc = pc->get(pc, pc->data);
    if (rc != NGX_OK) {
        return rc;
    }

    type = (pc->type ? pc->type : SOCK_STREAM);

    s = ngx_socket(pc->sockaddr->sa_family, type, 0);

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, pc->log, 0, "%s socket %d",
                   (type == SOCK_STREAM) ? "stream" : "dgram", s);

    if (s == (ngx_socket_t) -1) {
        ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                      ngx_socket_n " failed");
        return NGX_ERROR;
    }


    c = ngx_get_connection(s, pc->log);

    if (c == NULL) {
        if (ngx_close_socket(s) == -1) {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          ngx_close_socket_n " failed");
        }

        return NGX_ERROR;
    }

    c->type = type;

    if (pc->rcvbuf) {
        if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
                       (const void *) &pc->rcvbuf, sizeof(int)) == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          "setsockopt(SO_RCVBUF) failed");
            goto failed;
        }
    }

    if (pc->so_keepalive) {
        value = 1;

        if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,
                       (const void *) &value, sizeof(int))
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          "setsockopt(SO_KEEPALIVE) failed, ignored");
        }
    }

    if (ngx_nonblocking(s) == -1) {
        ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                      ngx_nonblocking_n " failed");

        goto failed;
    }

    if (pc->local) {

#if (NGX_HAVE_TRANSPARENT_PROXY)
        if (pc->transparent) {
            if (ngx_event_connect_set_transparent(pc, s) != NGX_OK) {
                goto failed;
            }
        }
#endif

#if (NGX_HAVE_IP_BIND_ADDRESS_NO_PORT || NGX_LINUX)
        port = ngx_inet_get_port(pc->local->sockaddr);
#endif

#if (NGX_HAVE_IP_BIND_ADDRESS_NO_PORT)

        if (pc->sockaddr->sa_family != AF_UNIX && port == 0) {
            static int  bind_address_no_port = 1;

            if (bind_address_no_port) {
                if (setsockopt(s, IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT,
                               (const void *) &bind_address_no_port,
                               sizeof(int)) == -1)
                {
                    err = ngx_socket_errno;

                    if (err != NGX_EOPNOTSUPP && err != NGX_ENOPROTOOPT) {
                        ngx_log_error(NGX_LOG_ALERT, pc->log, err,
                                      "setsockopt(IP_BIND_ADDRESS_NO_PORT) "
                                      "failed, ignored");

                    } else {
                        bind_address_no_port = 0;
                    }
                }
            }
        }

#endif

#if (NGX_LINUX)

        if (pc->type == SOCK_DGRAM && port != 0) {
            int  reuse_addr = 1;

            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                           (const void *) &reuse_addr, sizeof(int))
                 == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                              "setsockopt(SO_REUSEADDR) failed");
                goto failed;
            }
        }

#endif

        // 尝试绑定套接字
        if (bind(s, pc->local->sockaddr, pc->local->socklen) == -1) {
            // 如果绑定失败，记录错误日志
            ngx_log_error(NGX_LOG_CRIT, pc->log, ngx_socket_errno,
                          "bind(%V) failed", &pc->local->name);

            // 跳转到失败处理
            goto failed;
        }
    }

    // 根据套接字类型设置连接的接收和发送函数
    if (type == SOCK_STREAM) {
        c->recv = ngx_recv; // 设置流式套接字的接收函数
        c->send = ngx_send; // 设置流式套接字的发送函数
        c->recv_chain = ngx_recv_chain; // 设置流式套接字的链式接收函数
        c->send_chain = ngx_send_chain; // 设置流式套接字的链式发送函数

        c->sendfile = 1; // 默认启用发送文件功能

        // 如果套接字地址族为AF_UNIX（即Unix域套接字）
        if (pc->sockaddr->sa_family == AF_UNIX) {
            c->tcp_nopush = NGX_TCP_NOPUSH_DISABLED; // 禁用TCP_NOPUSH
            c->tcp_nodelay = NGX_TCP_NODELAY_DISABLED; // 禁用TCP_NODELAY

            // 如果是Solaris系统
#if (NGX_SOLARIS)
            /* Solaris的sendfilev()支持AF_NCA, AF_INET, 和AF_INET6 */
            c->sendfile = 0; // 对于Solaris系统，禁用发送文件功能
#endif
        }

    } else { /* type == SOCK_DGRAM */
        c->recv = ngx_udp_recv; // 设置数据报套接字的接收函数
        c->send = ngx_send; // 设置数据报套接字的发送函数
        c->send_chain = ngx_udp_send_chain; // 设置数据报套接字的链式发送函数

        c->need_flush_buf = 1; // 需要刷新缓冲区
    }

    c->log_error = pc->log_error;

    rev = c->read;
    wev = c->write;

    rev->log = pc->log;
    wev->log = pc->log;

    pc->connection = c;

    c->number = ngx_atomic_fetch_add(ngx_connection_counter, 1);

    c->start_time = ngx_current_msec;

    if (ngx_add_conn) {
        if (ngx_add_conn(c) == NGX_ERROR) {
            goto failed;
        }
    }

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, pc->log, 0,
                   "connect to %V, fd:%d #%uA", pc->name, s, c->number);

    rc = connect(s, pc->sockaddr, pc->socklen);

    if (rc == -1) {
        err = ngx_socket_errno;


        if (err != NGX_EINPROGRESS
#if (NGX_WIN32)
            /* Winsock returns WSAEWOULDBLOCK (NGX_EAGAIN) */
            && err != NGX_EAGAIN
#endif
            )
        {
            if (err == NGX_ECONNREFUSED
#if (NGX_LINUX)
                /*
                 * Linux returns EAGAIN instead of ECONNREFUSED
                 * for unix sockets if listen queue is full
                 */
                || err == NGX_EAGAIN
#endif
                || err == NGX_ECONNRESET
                || err == NGX_ENETDOWN
                || err == NGX_ENETUNREACH
                || err == NGX_EHOSTDOWN
                || err == NGX_EHOSTUNREACH)
            {
                level = NGX_LOG_ERR;

            } else {
                level = NGX_LOG_CRIT;
            }

            ngx_log_error(level, c->log, err, "connect() to %V failed",
                          pc->name);

            ngx_close_connection(c);
            pc->connection = NULL;

            return NGX_DECLINED;
        }
    }

    if (ngx_add_conn) {
        if (rc == -1) {

            /* NGX_EINPROGRESS */

            return NGX_AGAIN;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, pc->log, 0, "connected");

        wev->ready = 1;

        return NGX_OK;
    }

    if (ngx_event_flags & NGX_USE_IOCP_EVENT) {

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, pc->log, ngx_socket_errno,
                       "connect(): %d", rc);

        if (ngx_blocking(s) == -1) {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          ngx_blocking_n " failed");
            goto failed;
        }

        /*
         * FreeBSD's aio allows to post an operation on non-connected socket.
         * NT does not support it.
         *
         * TODO: check in Win32, etc. As workaround we can use NGX_ONESHOT_EVENT
         */

        rev->ready = 1;
        wev->ready = 1;

        return NGX_OK;
    }

    if (ngx_event_flags & NGX_USE_CLEAR_EVENT) {

        /* kqueue */

        event = NGX_CLEAR_EVENT;

    } else {

        /* select, poll, /dev/poll */

        event = NGX_LEVEL_EVENT;
    }

    if (ngx_add_event(rev, NGX_READ_EVENT, event) != NGX_OK) {
        goto failed;
    }

    if (rc == -1) {

        /* NGX_EINPROGRESS */

        if (ngx_add_event(wev, NGX_WRITE_EVENT, event) != NGX_OK) {
            goto failed;
        }

        return NGX_AGAIN;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, pc->log, 0, "connected");

    wev->ready = 1;

    return NGX_OK;

failed:

    ngx_close_connection(c);
    pc->connection = NULL;

    return NGX_ERROR;
}


#if (NGX_HAVE_TRANSPARENT_PROXY)

static ngx_int_t
ngx_event_connect_set_transparent(ngx_peer_connection_t *pc, ngx_socket_t s)
{
    int  value;

    value = 1;

#if defined(SO_BINDANY)

    if (setsockopt(s, SOL_SOCKET, SO_BINDANY,
                   (const void *) &value, sizeof(int)) == -1)
    {
        ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                      "setsockopt(SO_BINDANY) failed");
        return NGX_ERROR;
    }

#else

    switch (pc->local->sockaddr->sa_family) {

    case AF_INET:

#if defined(IP_TRANSPARENT)

        if (setsockopt(s, IPPROTO_IP, IP_TRANSPARENT,
                       (const void *) &value, sizeof(int)) == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          "setsockopt(IP_TRANSPARENT) failed");
            return NGX_ERROR;
        }

#elif defined(IP_BINDANY)

        if (setsockopt(s, IPPROTO_IP, IP_BINDANY,
                       (const void *) &value, sizeof(int)) == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          "setsockopt(IP_BINDANY) failed");
            return NGX_ERROR;
        }

#endif

        break;

#if (NGX_HAVE_INET6)

    case AF_INET6:

#if defined(IPV6_TRANSPARENT)

        if (setsockopt(s, IPPROTO_IPV6, IPV6_TRANSPARENT,
                       (const void *) &value, sizeof(int)) == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          "setsockopt(IPV6_TRANSPARENT) failed");
            return NGX_ERROR;
        }

#elif defined(IPV6_BINDANY)

        if (setsockopt(s, IPPROTO_IPV6, IPV6_BINDANY,
                       (const void *) &value, sizeof(int)) == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          "setsockopt(IPV6_BINDANY) failed");
            return NGX_ERROR;
        }

#else

        ngx_log_error(NGX_LOG_ALERT, pc->log, 0,
                      "could not enable transparent proxying for IPv6 "
                      "on this platform");

        return NGX_ERROR;

#endif

        break;

#endif /* NGX_HAVE_INET6 */

    }

#endif /* SO_BINDANY */

    return NGX_OK;
}

#endif


ngx_int_t
ngx_event_get_peer(ngx_peer_connection_t *pc, void *data)
{
    return NGX_OK;
}
