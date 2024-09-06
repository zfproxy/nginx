
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_connection.c - Nginx连接管理模块
 *
 * 本文件实现了Nginx的连接管理功能，负责处理网络连接的创建、维护和关闭。
 *
 * 主要支持的功能:
 * 1. 创建和初始化监听套接字
 * 2. 管理连接池，复用连接对象
 * 3. 处理新的客户端连接
 * 4. 管理空闲连接和活跃连接
 * 5. 实现连接的读写操作
 * 6. 处理连接超时和关闭
 *
 * 使用注意点:
 * - 连接对象的生命周期需要谨慎管理，避免内存泄漏
 * - 在多工作进程模式下，需要注意连接的负载均衡
 * - SSL连接需要额外的处理逻辑
 * - 连接数量会影响系统资源使用，需要根据实际情况调整
 * - 在高并发场景下，连接管理的效率对整体性能影响较大
 * - 需要注意处理各种网络异常情况，如连接重置、半开连接等
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


// 定义全局的I/O操作结构体
ngx_os_io_t  ngx_io;


// 声明静态函数，用于清理连接
static void ngx_drain_connections(ngx_cycle_t *cycle);


// 创建一个新的监听对象
ngx_listening_t *
ngx_create_listening(ngx_conf_t *cf, struct sockaddr *sockaddr,
    socklen_t socklen)
{
    size_t            len;
    ngx_listening_t  *ls;
    struct sockaddr  *sa;
    u_char            text[NGX_SOCKADDR_STRLEN];

    // 在监听数组中添加一个新元素
    ls = ngx_array_push(&cf->cycle->listening);
    if (ls == NULL) {
        return NULL;
    }

    // 初始化新的监听对象
    ngx_memzero(ls, sizeof(ngx_listening_t));

    // 分配内存并复制sockaddr结构
    sa = ngx_palloc(cf->pool, socklen);
    if (sa == NULL) {
        return NULL;
    }

    ngx_memcpy(sa, sockaddr, socklen);

    // 设置监听对象的sockaddr和socklen
    ls->sockaddr = sa;
    ls->socklen = socklen;

    // 将sockaddr转换为字符串形式
    len = ngx_sock_ntop(sa, socklen, text, NGX_SOCKADDR_STRLEN, 1);
    ls->addr_text.len = len;

    // 根据地址族设置最大地址文本长度
    switch (ls->sockaddr->sa_family) {
#if (NGX_HAVE_INET6)
    case AF_INET6:
        ls->addr_text_max_len = NGX_INET6_ADDRSTRLEN;
        break;
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
    case AF_UNIX:
        ls->addr_text_max_len = NGX_UNIX_ADDRSTRLEN;
        len++;
        break;
#endif
    case AF_INET:
        ls->addr_text_max_len = NGX_INET_ADDRSTRLEN;
        break;
    default:
        ls->addr_text_max_len = NGX_SOCKADDR_STRLEN;
        break;
    }

    // 分配内存并复制地址文本
    ls->addr_text.data = ngx_pnalloc(cf->pool, len);
    if (ls->addr_text.data == NULL) {
        return NULL;
    }

    ngx_memcpy(ls->addr_text.data, text, len);

#if !(NGX_WIN32)
    // 初始化红黑树（非Windows平台）
    ngx_rbtree_init(&ls->rbtree, &ls->sentinel, ngx_udp_rbtree_insert_value);
#endif

    // 设置默认值
    ls->fd = (ngx_socket_t) -1;
    ls->type = SOCK_STREAM;

    ls->backlog = NGX_LISTEN_BACKLOG;
    ls->rcvbuf = -1;
    ls->sndbuf = -1;

#if (NGX_HAVE_SETFIB)
    ls->setfib = -1;
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
    ls->fastopen = -1;
#endif

    return ls;
}


ngx_int_t
ngx_clone_listening(ngx_cycle_t *cycle, ngx_listening_t *ls)
{
#if (NGX_HAVE_REUSEPORT)

    ngx_int_t         n;
    ngx_core_conf_t  *ccf;
    ngx_listening_t   ols;

    // 如果不支持端口重用或者不是主进程，直接返回
    if (!ls->reuseport || ls->worker != 0) {
        return NGX_OK;
    }

    // 保存原始监听套接字的配置
    ols = *ls;

    // 获取核心配置
    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    // 为每个工作进程创建一个新的监听套接字
    for (n = 1; n < ccf->worker_processes; n++) {

        /* 为每个工作进程创建一个套接字 */

        // 在监听数组中添加新的监听套接字
        ls = ngx_array_push(&cycle->listening);
        if (ls == NULL) {
            return NGX_ERROR;
        }

        // 复制原始监听套接字的配置
        *ls = ols;
        // 设置工作进程ID
        ls->worker = n;
    }

#endif

    return NGX_OK;
}


ngx_int_t
ngx_set_inherited_sockets(ngx_cycle_t *cycle)
{
    size_t                     len;
    ngx_uint_t                 i;
    ngx_listening_t           *ls;
    socklen_t                  olen;
#if (NGX_HAVE_DEFERRED_ACCEPT || NGX_HAVE_TCP_FASTOPEN)
    ngx_err_t                  err;
#endif
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    struct accept_filter_arg   af;
#endif
#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
    int                        timeout;
#endif
#if (NGX_HAVE_REUSEPORT)
    int                        reuseport;
#endif

    // 获取监听套接字数组
    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        // 为每个监听套接字分配sockaddr结构体内存
        ls[i].sockaddr = ngx_palloc(cycle->pool, sizeof(ngx_sockaddr_t));
        if (ls[i].sockaddr == NULL) {
            return NGX_ERROR;
        }

        // 获取套接字的本地地址信息
        ls[i].socklen = sizeof(ngx_sockaddr_t);
        if (getsockname(ls[i].fd, ls[i].sockaddr, &ls[i].socklen) == -1) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_socket_errno,
                          "getsockname() of the inherited "
                          "socket #%d failed", ls[i].fd);
            ls[i].ignore = 1;
            continue;
        }

        // 确保socklen不超过ngx_sockaddr_t的大小
        if (ls[i].socklen > (socklen_t) sizeof(ngx_sockaddr_t)) {
            ls[i].socklen = sizeof(ngx_sockaddr_t);
        }

        // 根据套接字地址族设置相关参数
        switch (ls[i].sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            ls[i].addr_text_max_len = NGX_INET6_ADDRSTRLEN;
            len = NGX_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1;
            break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            ls[i].addr_text_max_len = NGX_UNIX_ADDRSTRLEN;
            len = NGX_UNIX_ADDRSTRLEN;
            break;
#endif

        case AF_INET:
            ls[i].addr_text_max_len = NGX_INET_ADDRSTRLEN;
            len = NGX_INET_ADDRSTRLEN + sizeof(":65535") - 1;
            break;

        default:
            ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_socket_errno,
                          "the inherited socket #%d has "
                          "an unsupported protocol family", ls[i].fd);
            ls[i].ignore = 1;
            continue;
        }

        // 分配内存用于存储地址文本表示
        ls[i].addr_text.data = ngx_pnalloc(cycle->pool, len);
        if (ls[i].addr_text.data == NULL) {
            return NGX_ERROR;
        }

        // 将套接字地址转换为文本形式
        len = ngx_sock_ntop(ls[i].sockaddr, ls[i].socklen,
                            ls[i].addr_text.data, len, 1);
        if (len == 0) {
            return NGX_ERROR;
        }

        ls[i].addr_text.len = len;

        // 设置默认的backlog值
        ls[i].backlog = NGX_LISTEN_BACKLOG;

        olen = sizeof(int);

        // 获取套接字类型
        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_TYPE, (void *) &ls[i].type,
                       &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_TYPE) %V failed", &ls[i].addr_text);
            ls[i].ignore = 1;
            continue;
        }

        olen = sizeof(int);

        // 获取接收缓冲区大小
        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_RCVBUF, (void *) &ls[i].rcvbuf,
                       &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_RCVBUF) %V failed, ignored",
                          &ls[i].addr_text);

            ls[i].rcvbuf = -1;
        }

        olen = sizeof(int);

        // 获取发送缓冲区大小
        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_SNDBUF, (void *) &ls[i].sndbuf,
                       &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_SNDBUF) %V failed, ignored",
                          &ls[i].addr_text);

            ls[i].sndbuf = -1;
        }

#if 0
        /* SO_SETFIB is currently a set only option */

#if (NGX_HAVE_SETFIB)

        olen = sizeof(int);

        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_SETFIB,
                       (void *) &ls[i].setfib, &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_SETFIB) %V failed, ignored",
                          &ls[i].addr_text);

            ls[i].setfib = -1;
        }

#endif
#endif

#if (NGX_HAVE_REUSEPORT)

        reuseport = 0;
        olen = sizeof(int);

#ifdef SO_REUSEPORT_LB

        // 获取SO_REUSEPORT_LB选项状态
        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_REUSEPORT_LB,
                       (void *) &reuseport, &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_REUSEPORT_LB) %V failed, ignored",
                          &ls[i].addr_text);

        } else {
            ls[i].reuseport = reuseport ? 1 : 0;
        }

#else

        // 获取SO_REUSEPORT选项状态
        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_REUSEPORT,
                       (void *) &reuseport, &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_REUSEPORT) %V failed, ignored",
                          &ls[i].addr_text);

        } else {
            ls[i].reuseport = reuseport ? 1 : 0;
        }
#endif

#endif

        // 如果不是流式套接字，跳过后续处理
        if (ls[i].type != SOCK_STREAM) {
            continue;
        }

#if (NGX_HAVE_TCP_FASTOPEN)

        olen = sizeof(int);

        // 获取TCP_FASTOPEN选项状态
        if (getsockopt(ls[i].fd, IPPROTO_TCP, TCP_FASTOPEN,
                       (void *) &ls[i].fastopen, &olen)
            == -1)
        {
            err = ngx_socket_errno;

            if (err != NGX_EOPNOTSUPP && err != NGX_ENOPROTOOPT
                && err != NGX_EINVAL)
            {
                ngx_log_error(NGX_LOG_NOTICE, cycle->log, err,
                              "getsockopt(TCP_FASTOPEN) %V failed, ignored",
                              &ls[i].addr_text);
            }

            ls[i].fastopen = -1;
        }

#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)

        ngx_memzero(&af, sizeof(struct accept_filter_arg));
        olen = sizeof(struct accept_filter_arg);

        // 获取SO_ACCEPTFILTER选项状态
        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_ACCEPTFILTER, &af, &olen)
            == -1)
        {
            err = ngx_socket_errno;

            if (err == NGX_EINVAL) {
                continue;
            }

            ngx_log_error(NGX_LOG_NOTICE, cycle->log, err,
                          "getsockopt(SO_ACCEPTFILTER) for %V failed, ignored",
                          &ls[i].addr_text);
            continue;
        }

        if (olen < sizeof(struct accept_filter_arg) || af.af_name[0] == '\0') {
            continue;
        }

        // 保存accept filter名称
        ls[i].accept_filter = ngx_palloc(cycle->pool, 16);
        if (ls[i].accept_filter == NULL) {
            return NGX_ERROR;
        }

        (void) ngx_cpystrn((u_char *) ls[i].accept_filter,
                           (u_char *) af.af_name, 16);
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)

        timeout = 0;
        olen = sizeof(int);

        // 获取TCP_DEFER_ACCEPT选项状态
        if (getsockopt(ls[i].fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &timeout, &olen)
            == -1)
        {
            err = ngx_socket_errno;

            if (err == NGX_EOPNOTSUPP) {
                continue;
            }

            ngx_log_error(NGX_LOG_NOTICE, cycle->log, err,
                          "getsockopt(TCP_DEFER_ACCEPT) for %V failed, ignored",
                          &ls[i].addr_text);
            continue;
        }

        if (olen < sizeof(int) || timeout == 0) {
            continue;
        }

        ls[i].deferred_accept = 1;
#endif
    }

    return NGX_OK;
}


ngx_int_t
ngx_open_listening_sockets(ngx_cycle_t *cycle)
{
    int               reuseaddr;
    ngx_uint_t        i, tries, failed;
    ngx_err_t         err;
    ngx_log_t        *log;
    ngx_socket_t      s;
    ngx_listening_t  *ls;

    // 设置地址重用选项
    reuseaddr = 1;
#if (NGX_SUPPRESS_WARN)
    failed = 0;
#endif

    log = cycle->log;

    // 尝试打开监听套接字的次数，默认为5次
    for (tries = 5; tries; tries--) {
        failed = 0;

        // 遍历所有的监听套接字
        ls = cycle->listening.elts;
        for (i = 0; i < cycle->listening.nelts; i++) {

            // 如果该监听套接字被标记为忽略，则跳过
            if (ls[i].ignore) {
                continue;
            }

#if (NGX_HAVE_REUSEPORT)
            // 处理端口重用的情况
            if (ls[i].add_reuseport) {
                int  reuseport = 1;

#ifdef SO_REUSEPORT_LB
                // 尝试设置SO_REUSEPORT_LB选项
                if (setsockopt(ls[i].fd, SOL_SOCKET, SO_REUSEPORT_LB,
                               (const void *) &reuseport, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                                  "setsockopt(SO_REUSEPORT_LB) %V failed, "
                                  "ignored",
                                  &ls[i].addr_text);
                }
#else
                // 尝试设置SO_REUSEPORT选项
                if (setsockopt(ls[i].fd, SOL_SOCKET, SO_REUSEPORT,
                               (const void *) &reuseport, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                                  "setsockopt(SO_REUSEPORT) %V failed, ignored",
                                  &ls[i].addr_text);
                }
#endif
                ls[i].add_reuseport = 0;
            }
#endif

            // 如果套接字已经打开，则跳过
            if (ls[i].fd != (ngx_socket_t) -1) {
                continue;
            }

            // 如果是继承的套接字，则跳过
            if (ls[i].inherited) {
                continue;
            }

            // 创建新的套接字
            s = ngx_socket(ls[i].sockaddr->sa_family, ls[i].type, 0);

            if (s == (ngx_socket_t) -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                              ngx_socket_n " %V failed", &ls[i].addr_text);
                return NGX_ERROR;
            }

            // 对于非UDP套接字或非测试配置，设置地址重用选项
            if (ls[i].type != SOCK_DGRAM || !ngx_test_config) {
                if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                               (const void *) &reuseaddr, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  "setsockopt(SO_REUSEADDR) %V failed",
                                  &ls[i].addr_text);

                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }

                    return NGX_ERROR;
                }
            }

#if (NGX_HAVE_REUSEPORT)
            // 处理端口重用的情况
            if (ls[i].reuseport && !ngx_test_config) {
                int  reuseport;

                reuseport = 1;

#ifdef SO_REUSEPORT_LB
                // 尝试设置SO_REUSEPORT_LB选项
                if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT_LB,
                               (const void *) &reuseport, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  "setsockopt(SO_REUSEPORT_LB) %V failed",
                                  &ls[i].addr_text);

                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }

                    return NGX_ERROR;
                }

#else
                // 尝试设置SO_REUSEPORT选项
                if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT,
                               (const void *) &reuseport, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  "setsockopt(SO_REUSEPORT) %V failed",
                                  &ls[i].addr_text);

                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }

                    return NGX_ERROR;
                }
#endif
            }
#endif

#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
            // 处理IPv6只监听的情况
            if (ls[i].sockaddr->sa_family == AF_INET6) {
                int  ipv6only;

                ipv6only = ls[i].ipv6only;

                if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
                               (const void *) &ipv6only, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  "setsockopt(IPV6_V6ONLY) %V failed, ignored",
                                  &ls[i].addr_text);
                }
            }
#endif
            // 设置非阻塞模式
            if (!(ngx_event_flags & NGX_USE_IOCP_EVENT)) {
                if (ngx_nonblocking(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_nonblocking_n " %V failed",
                                  &ls[i].addr_text);

                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }

                    return NGX_ERROR;
                }
            }

            ngx_log_debug2(NGX_LOG_DEBUG_CORE, log, 0,
                           "bind() %V #%d ", &ls[i].addr_text, s);

            // 绑定地址
            if (bind(s, ls[i].sockaddr, ls[i].socklen) == -1) {
                err = ngx_socket_errno;

                if (err != NGX_EADDRINUSE || !ngx_test_config) {
                    ngx_log_error(NGX_LOG_EMERG, log, err,
                                  "bind() to %V failed", &ls[i].addr_text);
                }

                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }

                if (err != NGX_EADDRINUSE) {
                    return NGX_ERROR;
                }

                if (!ngx_test_config) {
                    failed = 1;
                }

                continue;
            }

#if (NGX_HAVE_UNIX_DOMAIN)
            // 处理UNIX域套接字的权限
            if (ls[i].sockaddr->sa_family == AF_UNIX) {
                mode_t   mode;
                u_char  *name;

                name = ls[i].addr_text.data + sizeof("unix:") - 1;
                mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

                if (chmod((char *) name, mode) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                  "chmod() \"%s\" failed", name);
                }

                if (ngx_test_config) {
                    if (ngx_delete_file(name) == NGX_FILE_ERROR) {
                        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                      ngx_delete_file_n " %s failed", name);
                    }
                }
            }
#endif

            // 对于非TCP套接字，直接设置fd并继续
            if (ls[i].type != SOCK_STREAM) {
                ls[i].fd = s;
                continue;
            }

            // 开始监听
            if (listen(s, ls[i].backlog) == -1) {
                err = ngx_socket_errno;

                if (err != NGX_EADDRINUSE || !ngx_test_config) {
                    ngx_log_error(NGX_LOG_EMERG, log, err,
                                  "listen() to %V, backlog %d failed",
                                  &ls[i].addr_text, ls[i].backlog);
                }

                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }

                if (err != NGX_EADDRINUSE) {
                    return NGX_ERROR;
                }

                if (!ngx_test_config) {
                    failed = 1;
                }

                continue;
            }

            // 标记为已监听
            ls[i].listen = 1;

            ls[i].fd = s;
        }

        if (!failed) {
            break;
        }

        // 如果失败，等待500ms后重试
        ngx_log_error(NGX_LOG_NOTICE, log, 0,
                      "try again to bind() after 500ms");

        ngx_msleep(500);
    }

    if (failed) {
        ngx_log_error(NGX_LOG_EMERG, log, 0, "still could not bind()");
        return NGX_ERROR;
    }

    return NGX_OK;
}


void
ngx_configure_listening_sockets(ngx_cycle_t *cycle)
{
    int                        value;
    ngx_uint_t                 i;
    ngx_listening_t           *ls;

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    struct accept_filter_arg   af;
#endif

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        ls[i].log = *ls[i].logp;

        if (ls[i].rcvbuf != -1) {
            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_RCVBUF,
                           (const void *) &ls[i].rcvbuf, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_RCVBUF, %d) %V failed, ignored",
                              ls[i].rcvbuf, &ls[i].addr_text);
            }
        }

        if (ls[i].sndbuf != -1) {
            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_SNDBUF,
                           (const void *) &ls[i].sndbuf, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_SNDBUF, %d) %V failed, ignored",
                              ls[i].sndbuf, &ls[i].addr_text);
            }
        }

        if (ls[i].keepalive) {
            value = (ls[i].keepalive == 1) ? 1 : 0;

            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_KEEPALIVE,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_KEEPALIVE, %d) %V failed, ignored",
                              value, &ls[i].addr_text);
            }
        }

#if (NGX_HAVE_KEEPALIVE_TUNABLE)

        if (ls[i].keepidle) {
            value = ls[i].keepidle;

#if (NGX_KEEPALIVE_FACTOR)
            value *= NGX_KEEPALIVE_FACTOR;
#endif

            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_KEEPIDLE,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_KEEPIDLE, %d) %V failed, ignored",
                              value, &ls[i].addr_text);
            }
        }

        if (ls[i].keepintvl) {
            value = ls[i].keepintvl;

#if (NGX_KEEPALIVE_FACTOR)
            value *= NGX_KEEPALIVE_FACTOR;
#endif

            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_KEEPINTVL,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                             "setsockopt(TCP_KEEPINTVL, %d) %V failed, ignored",
                             value, &ls[i].addr_text);
            }
        }

        if (ls[i].keepcnt) {
            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_KEEPCNT,
                           (const void *) &ls[i].keepcnt, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_KEEPCNT, %d) %V failed, ignored",
                              ls[i].keepcnt, &ls[i].addr_text);
            }
        }

#endif

#if (NGX_HAVE_SETFIB)
        if (ls[i].setfib != -1) {
            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_SETFIB,
                           (const void *) &ls[i].setfib, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_SETFIB, %d) %V failed, ignored",
                              ls[i].setfib, &ls[i].addr_text);
            }
        }
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
        if (ls[i].fastopen != -1) {
            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_FASTOPEN,
                           (const void *) &ls[i].fastopen, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_FASTOPEN, %d) %V failed, ignored",
                              ls[i].fastopen, &ls[i].addr_text);
            }
        }
#endif

#if 0
        if (1) {
            int tcp_nodelay = 1;

            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_NODELAY,
                       (const void *) &tcp_nodelay, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_NODELAY) %V failed, ignored",
                              &ls[i].addr_text);
            }
        }
#endif

        if (ls[i].listen) {

            /* change backlog via listen() */

            if (listen(ls[i].fd, ls[i].backlog) == -1) {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "listen() to %V, backlog %d failed, ignored",
                              &ls[i].addr_text, ls[i].backlog);
            }
        }

        /*
         * setting deferred mode should be last operation on socket,
         * because code may prematurely continue cycle on failure
         */

#if (NGX_HAVE_DEFERRED_ACCEPT)

#ifdef SO_ACCEPTFILTER

        if (ls[i].delete_deferred) {
            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_ACCEPTFILTER, NULL, 0)
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_ACCEPTFILTER, NULL) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);

                if (ls[i].accept_filter) {
                    ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                                  "could not change the accept filter "
                                  "to \"%s\" for %V, ignored",
                                  ls[i].accept_filter, &ls[i].addr_text);
                }

                continue;
            }

            ls[i].deferred_accept = 0;
        }

        if (ls[i].add_deferred) {
            ngx_memzero(&af, sizeof(struct accept_filter_arg));
            (void) ngx_cpystrn((u_char *) af.af_name,
                               (u_char *) ls[i].accept_filter, 16);

            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_ACCEPTFILTER,
                           &af, sizeof(struct accept_filter_arg))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_ACCEPTFILTER, \"%s\") "
                              "for %V failed, ignored",
                              ls[i].accept_filter, &ls[i].addr_text);
                continue;
            }

            ls[i].deferred_accept = 1;
        }

#endif

#ifdef TCP_DEFER_ACCEPT

        if (ls[i].add_deferred || ls[i].delete_deferred) {

            if (ls[i].add_deferred) {
                /*
                 * There is no way to find out how long a connection was
                 * in queue (and a connection may bypass deferred queue at all
                 * if syncookies were used), hence we use 1 second timeout
                 * here.
                 */
                value = 1;

            } else {
                value = 0;
            }

            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_DEFER_ACCEPT,
                           &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_DEFER_ACCEPT, %d) for %V failed, "
                              "ignored",
                              value, &ls[i].addr_text);

                continue;
            }
        }

        if (ls[i].add_deferred) {
            ls[i].deferred_accept = 1;
        }

#endif

#endif /* NGX_HAVE_DEFERRED_ACCEPT */

#if (NGX_HAVE_IP_RECVDSTADDR)

        if (ls[i].wildcard
            && ls[i].type == SOCK_DGRAM
            && ls[i].sockaddr->sa_family == AF_INET)
        {
            value = 1;

            if (setsockopt(ls[i].fd, IPPROTO_IP, IP_RECVDSTADDR,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IP_RECVDSTADDR) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#elif (NGX_HAVE_IP_PKTINFO)

        if (ls[i].wildcard
            && ls[i].type == SOCK_DGRAM
            && ls[i].sockaddr->sa_family == AF_INET)
        {
            value = 1;

            if (setsockopt(ls[i].fd, IPPROTO_IP, IP_PKTINFO,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IP_PKTINFO) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#endif

#if (NGX_HAVE_INET6 && NGX_HAVE_IPV6_RECVPKTINFO)

        if (ls[i].wildcard
            && ls[i].type == SOCK_DGRAM
            && ls[i].sockaddr->sa_family == AF_INET6)
        {
            value = 1;

            if (setsockopt(ls[i].fd, IPPROTO_IPV6, IPV6_RECVPKTINFO,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IPV6_RECVPKTINFO) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#endif

#if (NGX_HAVE_IP_MTU_DISCOVER)

        if (ls[i].quic && ls[i].sockaddr->sa_family == AF_INET) {
            value = IP_PMTUDISC_DO;

            if (setsockopt(ls[i].fd, IPPROTO_IP, IP_MTU_DISCOVER,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IP_MTU_DISCOVER) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#elif (NGX_HAVE_IP_DONTFRAG)

        if (ls[i].quic && ls[i].sockaddr->sa_family == AF_INET) {
            value = 1;

            if (setsockopt(ls[i].fd, IPPROTO_IP, IP_DONTFRAG,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IP_DONTFRAG) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#endif

#if (NGX_HAVE_INET6)

#if (NGX_HAVE_IPV6_MTU_DISCOVER)

        if (ls[i].quic && ls[i].sockaddr->sa_family == AF_INET6) {
            value = IPV6_PMTUDISC_DO;

            if (setsockopt(ls[i].fd, IPPROTO_IPV6, IPV6_MTU_DISCOVER,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IPV6_MTU_DISCOVER) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#elif (NGX_HAVE_IP_DONTFRAG)

        if (ls[i].quic && ls[i].sockaddr->sa_family == AF_INET6) {
            value = 1;

            if (setsockopt(ls[i].fd, IPPROTO_IPV6, IPV6_DONTFRAG,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IPV6_DONTFRAG) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#endif

#endif
    }

    return;
}


void
ngx_close_listening_sockets(ngx_cycle_t *cycle)
{
    ngx_uint_t         i;
    ngx_listening_t   *ls;
    ngx_connection_t  *c;

    // 如果使用IOCP事件模型,直接返回
    if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
        return;
    }

    // 重置accept mutex相关标志
    ngx_accept_mutex_held = 0;
    ngx_use_accept_mutex = 0;

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

#if (NGX_QUIC)
        // 跳过QUIC监听socket
        if (ls[i].quic) {
            continue;
        }
#endif

        c = ls[i].connection;

        if (c) {
            if (c->read->active) {
                if (ngx_event_flags & NGX_USE_EPOLL_EVENT) {

                    /*
                     * 在Linux-2.6.x OpenVZ上,需要显式删除已关闭共享监听socket的事件,
                     * 否则可能会继续收到事件
                     */

                    ngx_del_event(c->read, NGX_READ_EVENT, 0);

                } else {
                    ngx_del_event(c->read, NGX_READ_EVENT, NGX_CLOSE_EVENT);
                }
            }

            // 释放连接
            ngx_free_connection(c);

            c->fd = (ngx_socket_t) -1;
        }

        // 记录关闭监听socket的调试日志
        ngx_log_debug2(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                       "close listening %V #%d ", &ls[i].addr_text, ls[i].fd);

        // 关闭socket
        if (ngx_close_socket(ls[i].fd) == -1) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno,
                          ngx_close_socket_n " %V failed", &ls[i].addr_text);
        }

#if (NGX_HAVE_UNIX_DOMAIN)

        // 对于UNIX域socket,在特定条件下删除socket文件
        if (ls[i].sockaddr->sa_family == AF_UNIX
            && ngx_process <= NGX_PROCESS_MASTER
            && ngx_new_binary == 0
            && (!ls[i].inherited || ngx_getppid() != ngx_parent))
        {
            u_char *name = ls[i].addr_text.data + sizeof("unix:") - 1;

            if (ngx_delete_file(name) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno,
                              ngx_delete_file_n " %s failed", name);
            }
        }

#endif

        // 将socket标记为已关闭
        ls[i].fd = (ngx_socket_t) -1;
    }

    // 重置监听socket数量
    cycle->listening.nelts = 0;
}


ngx_connection_t *
ngx_get_connection(ngx_socket_t s, ngx_log_t *log)
{
    ngx_uint_t         instance;
    ngx_event_t       *rev, *wev;
    ngx_connection_t  *c;

    /* disable warning: Win32 SOCKET is u_int while UNIX socket is int */

    // 检查文件描述符是否超出了可用文件数量的范围
    if (ngx_cycle->files && (ngx_uint_t) s >= ngx_cycle->files_n) {
        ngx_log_error(NGX_LOG_ALERT, log, 0,
                      "the new socket has number %d, "
                      "but only %ui files are available",
                      s, ngx_cycle->files_n);
        return NULL;
    }

    // 尝试释放一些空闲连接
    ngx_drain_connections((ngx_cycle_t *) ngx_cycle);

    // 获取一个空闲连接
    c = ngx_cycle->free_connections;

    // 如果没有可用的空闲连接，返回NULL
    if (c == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0,
                      "%ui worker_connections are not enough",
                      ngx_cycle->connection_n);

        return NULL;
    }

    // 更新空闲连接链表和计数
    ngx_cycle->free_connections = c->data;
    ngx_cycle->free_connection_n--;

    // 如果文件描述符数组存在且该位置为空，将连接赋值给对应位置
    if (ngx_cycle->files && ngx_cycle->files[s] == NULL) {
        ngx_cycle->files[s] = c;
    }

    // 保存读写事件指针
    rev = c->read;
    wev = c->write;

    // 清零连接结构体
    ngx_memzero(c, sizeof(ngx_connection_t));

    // 恢复读写事件指针和其他必要信息
    c->read = rev;
    c->write = wev;
    c->fd = s;
    c->log = log;

    // 保存实例标志
    instance = rev->instance;

    // 清零读写事件结构体
    ngx_memzero(rev, sizeof(ngx_event_t));
    ngx_memzero(wev, sizeof(ngx_event_t));

    // 设置新的实例标志
    rev->instance = !instance;
    wev->instance = !instance;

    // 初始化事件索引
    rev->index = NGX_INVALID_INDEX;
    wev->index = NGX_INVALID_INDEX;

    // 关联事件和连接
    rev->data = c;
    wev->data = c;

    // 标记写事件
    wev->write = 1;

    // 返回初始化好的连接
    return c;
}


void
ngx_free_connection(ngx_connection_t *c)
{
    // 将连接添加到空闲连接链表的头部
    c->data = ngx_cycle->free_connections;
    ngx_cycle->free_connections = c;

    // 增加空闲连接数量
    ngx_cycle->free_connection_n++;

    // 如果存在文件描述符数组，并且该连接对应的文件描述符指向当前连接
    // 则将该文件描述符对应的连接指针置为NULL
    if (ngx_cycle->files && ngx_cycle->files[c->fd] == c) {
        ngx_cycle->files[c->fd] = NULL;
    }
}


void
ngx_close_connection(ngx_connection_t *c)
{
    ngx_err_t     err;
    ngx_uint_t    log_error, level;
    ngx_socket_t  fd;

    // 检查连接是否已经关闭
    if (c->fd == (ngx_socket_t) -1) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0, "connection already closed");
        return;
    }

    // 删除读事件的定时器
    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    // 删除写事件的定时器
    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    if (!c->shared) {
        // 如果存在ngx_del_conn函数，则调用它
        if (ngx_del_conn) {
            ngx_del_conn(c, NGX_CLOSE_EVENT);

        } else {
            // 否则，手动删除读写事件
            if (c->read->active || c->read->disabled) {
                ngx_del_event(c->read, NGX_READ_EVENT, NGX_CLOSE_EVENT);
            }

            if (c->write->active || c->write->disabled) {
                ngx_del_event(c->write, NGX_WRITE_EVENT, NGX_CLOSE_EVENT);
            }
        }
    }

    // 从posted队列中删除读事件
    if (c->read->posted) {
        ngx_delete_posted_event(c->read);
    }

    // 从posted队列中删除写事件
    if (c->write->posted) {
        ngx_delete_posted_event(c->write);
    }

    // 标记读写事件为已关闭
    c->read->closed = 1;
    c->write->closed = 1;

    // 将连接标记为不可重用
    ngx_reusable_connection(c, 0);

    // 保存日志错误级别
    log_error = c->log_error;

    // 释放连接
    ngx_free_connection(c);

    // 保存文件描述符并将连接的fd设置为-1
    fd = c->fd;
    c->fd = (ngx_socket_t) -1;

    // 如果是共享连接，直接返回
    if (c->shared) {
        return;
    }

    // 关闭socket
    if (ngx_close_socket(fd) == -1) {

        err = ngx_socket_errno;

        // 根据错误类型和日志级别设置日志级别
        if (err == NGX_ECONNRESET || err == NGX_ENOTCONN) {

            switch (log_error) {

            case NGX_ERROR_INFO:
                level = NGX_LOG_INFO;
                break;

            case NGX_ERROR_ERR:
                level = NGX_LOG_ERR;
                break;

            default:
                level = NGX_LOG_CRIT;
            }

        } else {
            level = NGX_LOG_CRIT;
        }

        // 记录关闭socket失败的日志
        ngx_log_error(level, c->log, err, ngx_close_socket_n " %d failed", fd);
    }
}


void
ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable)
{
    // 记录调试日志，显示连接是否可重用
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "reusable connection: %ui", reusable);

    // 如果连接当前是可重用的
    if (c->reusable) {
        // 从可重用连接队列中移除
        ngx_queue_remove(&c->queue);
        // 减少可重用连接计数
        ngx_cycle->reusable_connections_n--;

#if (NGX_STAT_STUB)
        // 更新等待连接的统计数据
        (void) ngx_atomic_fetch_add(ngx_stat_waiting, -1);
#endif
    }

    // 设置连接的可重用状态
    c->reusable = reusable;

    // 如果设置为可重用
    if (reusable) {
        /* 需要类型转换，因为ngx_cycle是volatile的 */

        // 将连接插入到可重用连接队列的头部
        ngx_queue_insert_head(
            (ngx_queue_t *) &ngx_cycle->reusable_connections_queue, &c->queue);
        // 增加可重用连接计数
        ngx_cycle->reusable_connections_n++;

#if (NGX_STAT_STUB)
        // 更新等待连接的统计数据
        (void) ngx_atomic_fetch_add(ngx_stat_waiting, 1);
#endif
    }
}


// 释放可重用连接
static void
ngx_drain_connections(ngx_cycle_t *cycle)
{
    ngx_uint_t         i, n;
    ngx_queue_t       *q;
    ngx_connection_t  *c;

    // 如果空闲连接数量足够多或没有可重用连接，则直接返回
    if (cycle->free_connection_n > cycle->connection_n / 16
        || cycle->reusable_connections_n == 0)
    {
        return;
    }

    // 如果当前时间与上次重用连接的时间不同，记录警告日志
    if (cycle->connections_reuse_time != ngx_time()) {
        cycle->connections_reuse_time = ngx_time();

        ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                      "%ui worker_connections are not enough, "
                      "reusing connections",
                      cycle->connection_n);
    }

    c = NULL;
    // 计算本次要释放的连接数量，最少1个，最多32个
    n = ngx_max(ngx_min(32, cycle->reusable_connections_n / 8), 1);

    // 循环释放连接
    for (i = 0; i < n; i++) {
        // 如果可重用连接队列为空，则退出循环
        if (ngx_queue_empty(&cycle->reusable_connections_queue)) {
            break;
        }

        // 获取队列尾部的连接
        q = ngx_queue_last(&cycle->reusable_connections_queue);
        c = ngx_queue_data(q, ngx_connection_t, queue);

        // 记录调试日志
        ngx_log_debug0(NGX_LOG_DEBUG_CORE, c->log, 0,
                       "reusing connection");

        // 标记连接为关闭状态，并调用读事件处理函数
        c->close = 1;
        c->read->handler(c->read);
    }

    // 如果没有空闲连接，且最后一个处理的连接仍然可重用，则再次尝试释放它
    if (cycle->free_connection_n == 0 && c && c->reusable) {

        /*
         * 如果没有连接被释放，尝试再次重用最后一个连接：
         * 这应该能释放它，只要之前的重用将它移到了延迟关闭状态
         */

        ngx_log_debug0(NGX_LOG_DEBUG_CORE, c->log, 0,
                       "reusing connection again");

        c->close = 1;
        c->read->handler(c->read);
    }
}


void
ngx_close_idle_connections(ngx_cycle_t *cycle)
{
    ngx_uint_t         i;
    ngx_connection_t  *c;

    // 获取连接数组的起始地址
    c = cycle->connections;

    // 遍历所有连接
    for (i = 0; i < cycle->connection_n; i++) {

        /* THREAD: lock */

        // 检查连接是否有效且处于空闲状态
        if (c[i].fd != (ngx_socket_t) -1 && c[i].idle) {
            // 标记连接为关闭状态
            c[i].close = 1;
            // 调用读事件处理函数，通常会关闭连接
            c[i].read->handler(c[i].read);
        }
    }
}


ngx_int_t
ngx_connection_local_sockaddr(ngx_connection_t *c, ngx_str_t *s,
    ngx_uint_t port)
{
    socklen_t             len;
    ngx_uint_t            addr;
    ngx_sockaddr_t        sa;
    struct sockaddr_in   *sin;
#if (NGX_HAVE_INET6)
    ngx_uint_t            i;
    struct sockaddr_in6  *sin6;
#endif

    addr = 0;

    // 如果已经有本地套接字地址信息
    if (c->local_socklen) {
        // 根据套接字地址族进行处理
        switch (c->local_sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            // 处理IPv6地址
            sin6 = (struct sockaddr_in6 *) c->local_sockaddr;

            // 检查IPv6地址是否为全0（未指定地址）
            for (i = 0; addr == 0 && i < 16; i++) {
                addr |= sin6->sin6_addr.s6_addr[i];
            }

            break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            // Unix域套接字，设置addr为1
            addr = 1;
            break;
#endif

        default: /* AF_INET */
            // 处理IPv4地址
            sin = (struct sockaddr_in *) c->local_sockaddr;
            addr = sin->sin_addr.s_addr;
            break;
        }
    }

    // 如果没有有效的地址信息
    if (addr == 0) {

        len = sizeof(ngx_sockaddr_t);

        // 获取套接字的本地地址
        if (getsockname(c->fd, &sa.sockaddr, &len) == -1) {
            ngx_connection_error(c, ngx_socket_errno, "getsockname() failed");
            return NGX_ERROR;
        }

        // 分配内存存储本地套接字地址
        c->local_sockaddr = ngx_palloc(c->pool, len);
        if (c->local_sockaddr == NULL) {
            return NGX_ERROR;
        }

        // 复制获取到的地址信息
        ngx_memcpy(c->local_sockaddr, &sa, len);

        c->local_socklen = len;
    }

    // 如果不需要返回字符串形式的地址
    if (s == NULL) {
        return NGX_OK;
    }

    // 将套接字地址转换为字符串形式
    s->len = ngx_sock_ntop(c->local_sockaddr, c->local_socklen,
                           s->data, s->len, port);

    return NGX_OK;
}


// 设置TCP连接的NODELAY选项
ngx_int_t
ngx_tcp_nodelay(ngx_connection_t *c)
{
    int  tcp_nodelay;

    // 如果已经设置过TCP_NODELAY,直接返回
    if (c->tcp_nodelay != NGX_TCP_NODELAY_UNSET) {
        return NGX_OK;
    }

    // 记录调试日志
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, c->log, 0, "tcp_nodelay");

    // 设置TCP_NODELAY选项值为1
    tcp_nodelay = 1;

    // 调用setsockopt设置TCP_NODELAY选项
    if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                   (const void *) &tcp_nodelay, sizeof(int))
        == -1)
    {
#if (NGX_SOLARIS)
        // Solaris特殊处理
        if (c->log_error == NGX_ERROR_INFO) {

            /* Solaris returns EINVAL if a socket has been shut down */
            c->log_error = NGX_ERROR_IGNORE_EINVAL;

            ngx_connection_error(c, ngx_socket_errno,
                                 "setsockopt(TCP_NODELAY) failed");

            c->log_error = NGX_ERROR_INFO;

            return NGX_ERROR;
        }
#endif

        // 设置失败,记录错误日志
        ngx_connection_error(c, ngx_socket_errno,
                             "setsockopt(TCP_NODELAY) failed");
        return NGX_ERROR;
    }

    // 标记TCP_NODELAY已设置
    c->tcp_nodelay = NGX_TCP_NODELAY_SET;

    return NGX_OK;
}


// 处理连接错误
ngx_int_t
ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text)
{
    ngx_uint_t  level;

    /* Winsock may return NGX_ECONNABORTED instead of NGX_ECONNRESET */

    // 如果是连接重置错误且设置了忽略该错误,则直接返回
    if ((err == NGX_ECONNRESET
#if (NGX_WIN32)
         || err == NGX_ECONNABORTED
#endif
        ) && c->log_error == NGX_ERROR_IGNORE_ECONNRESET)
    {
        return 0;
    }

#if (NGX_SOLARIS)
    // Solaris系统下,如果是无效参数错误且设置了忽略该错误,则直接返回
    if (err == NGX_EINVAL && c->log_error == NGX_ERROR_IGNORE_EINVAL) {
        return 0;
    }
#endif

    // 如果是消息过长错误且设置了忽略该错误,则直接返回
    if (err == NGX_EMSGSIZE && c->log_error == NGX_ERROR_IGNORE_EMSGSIZE) {
        return 0;
    }

    // 根据错误类型设置日志级别
    if (err == 0
        || err == NGX_ECONNRESET
#if (NGX_WIN32)
        || err == NGX_ECONNABORTED
#else
        || err == NGX_EPIPE
#endif
        || err == NGX_ENOTCONN
        || err == NGX_ETIMEDOUT
        || err == NGX_ECONNREFUSED
        || err == NGX_ENETDOWN
        || err == NGX_ENETUNREACH
        || err == NGX_EHOSTDOWN
        || err == NGX_EHOSTUNREACH)
    {
        switch (c->log_error) {

        case NGX_ERROR_IGNORE_EMSGSIZE:
        case NGX_ERROR_IGNORE_EINVAL:
        case NGX_ERROR_IGNORE_ECONNRESET:
        case NGX_ERROR_INFO:
            level = NGX_LOG_INFO;  // 设置为信息级别
            break;

        default:
            level = NGX_LOG_ERR;   // 设置为错误级别
        }

    } else {
        level = NGX_LOG_ALERT;     // 设置为警告级别
    }

    // 记录错误日志
    ngx_log_error(level, c->log, err, text);

    return NGX_ERROR;
}
