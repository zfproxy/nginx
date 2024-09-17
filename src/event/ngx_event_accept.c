
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_event_accept.c
 *
 * 该文件实现了Nginx的事件接受功能，主要处理新连接的接受和初始化。
 *
 * 支持的功能:
 * 1. 接受新的TCP连接
 * 2. 非阻塞I/O
 * 3. 多路复用(epoll, kqueue等)
 * 4. 负载均衡
 * 5. 连接限制
 *
 * 使用注意点:
 * 1. 合理配置worker进程数，避免过多竞争
 * 2. 适当调整accept_mutex_delay，平衡接受新连接和处理已有连接
 * 3. 在高并发场景下，考虑开启reuseport
 * 4. 监控accept队列溢出情况，适时调整backlog参数
 * 5. 注意文件描述符限制，合理设置worker_connections
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


static ngx_int_t ngx_disable_accept_events(ngx_cycle_t *cycle, ngx_uint_t all);
#if (NGX_HAVE_EPOLLEXCLUSIVE)
static void ngx_reorder_accept_events(ngx_listening_t *ls);
#endif
static void ngx_close_accepted_connection(ngx_connection_t *c);


/**
 * @brief 处理新连接接受事件的函数
 *
 * @param ev 触发接受事件的事件结构体指针
 *
 * @details 该函数负责接受新的客户端连接，并进行初始化设置。
 * 它会处理以下任务：
 * 1. 接受新的TCP连接
 * 2. 为新连接分配ngx_connection_t结构体
 * 3. 设置新连接的各种属性（如非阻塞模式）
 * 4. 初始化新连接的读写事件
 * 5. 调用监听对象的处理函数来进一步处理新连接
 * 
 * 该函数支持多个连接的批量接受（通过multi_accept配置）
 * 并处理各种错误情况，如连接数达到上限、资源不足等。
 */
void
ngx_event_accept(ngx_event_t *ev)
{
    socklen_t          socklen;
    ngx_err_t          err;
    ngx_log_t         *log;
    ngx_uint_t         level;
    ngx_socket_t       s;
    ngx_event_t       *rev, *wev;
    ngx_sockaddr_t     sa;
    ngx_listening_t   *ls;
    ngx_connection_t  *c, *lc;
    ngx_event_conf_t  *ecf;
#if (NGX_HAVE_ACCEPT4)
    static ngx_uint_t  use_accept4 = 1;
#endif

    // 检查事件是否超时
    if (ev->timedout) {
        // 如果超时，重新启用accept事件
        if (ngx_enable_accept_events((ngx_cycle_t *) ngx_cycle) != NGX_OK) {
            return;
        }

        ev->timedout = 0;
    }

    // 获取事件核心模块的配置
    ecf = ngx_event_get_conf(ngx_cycle->conf_ctx, ngx_event_core_module);

    // 设置可用连接数，除非使用kqueue
    if (!(ngx_event_flags & NGX_USE_KQUEUE_EVENT)) {
        ev->available = ecf->multi_accept;
    }

    lc = ev->data;
    ls = lc->listening;
    ev->ready = 0;

    // 记录调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "accept on %V, ready: %d", &ls->addr_text, ev->available);

    do {
        socklen = sizeof(ngx_sockaddr_t);

#if (NGX_HAVE_ACCEPT4)
        // 使用accept4函数（如果可用）
        if (use_accept4) {
            s = accept4(lc->fd, &sa.sockaddr, &socklen, SOCK_NONBLOCK);
        } else {
            s = accept(lc->fd, &sa.sockaddr, &socklen);
        }
#else
        // 使用标准accept函数
        s = accept(lc->fd, &sa.sockaddr, &socklen);
#endif

        // 处理accept失败的情况
        if (s == (ngx_socket_t) -1) {
            err = ngx_socket_errno;

            if (err == NGX_EAGAIN) {
                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, ev->log, err,
                               "accept() not ready");
                return;
            }

            level = NGX_LOG_ALERT;

            if (err == NGX_ECONNABORTED) {
                level = NGX_LOG_ERR;

            } else if (err == NGX_EMFILE || err == NGX_ENFILE) {
                level = NGX_LOG_CRIT;
            }

#if (NGX_HAVE_ACCEPT4)
            ngx_log_error(level, ev->log, err,
                          use_accept4 ? "accept4() failed" : "accept() failed");

            if (use_accept4 && err == NGX_ENOSYS) {
                use_accept4 = 0;
                ngx_inherited_nonblocking = 0;
                continue;
            }
#else
            ngx_log_error(level, ev->log, err, "accept() failed");
#endif

            if (err == NGX_ECONNABORTED) {
                if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
                    ev->available--;
                }

                if (ev->available) {
                    continue;
                }
            }

            // 处理文件描述符耗尽的情况
            if (err == NGX_EMFILE || err == NGX_ENFILE) {
                if (ngx_disable_accept_events((ngx_cycle_t *) ngx_cycle, 1)
                    != NGX_OK)
                {
                    return;
                }

                if (ngx_use_accept_mutex) {
                    if (ngx_accept_mutex_held) {
                        ngx_shmtx_unlock(&ngx_accept_mutex);
                        ngx_accept_mutex_held = 0;
                    }

                    ngx_accept_disabled = 1;

                } else {
                    ngx_add_timer(ev, ecf->accept_mutex_delay);
                }
            }

            return;
        }

#if (NGX_STAT_STUB)
        // 更新统计信息
        (void) ngx_atomic_fetch_add(ngx_stat_accepted, 1);
#endif

        // 计算是否需要暂时禁用accept
        ngx_accept_disabled = ngx_cycle->connection_n / 8
                              - ngx_cycle->free_connection_n;

        // 获取新的连接结构
        c = ngx_get_connection(s, ev->log);

        if (c == NULL) {
            if (ngx_close_socket(s) == -1) {
                ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_socket_errno,
                              ngx_close_socket_n " failed");
            }

            return;
        }

        c->type = SOCK_STREAM;

#if (NGX_STAT_STUB)
        // 更新活跃连接统计
        (void) ngx_atomic_fetch_add(ngx_stat_active, 1);
#endif

        // 为新连接创建内存池
        c->pool = ngx_create_pool(ls->pool_size, ev->log);
        if (c->pool == NULL) {
            ngx_close_accepted_connection(c);
            return;
        }

        // 确保socklen不超过ngx_sockaddr_t的大小
        if (socklen > (socklen_t) sizeof(ngx_sockaddr_t)) {
            socklen = sizeof(ngx_sockaddr_t);
        }

        // 为新连接分配sockaddr内存
        c->sockaddr = ngx_palloc(c->pool, socklen);
        if (c->sockaddr == NULL) {
            ngx_close_accepted_connection(c);
            return;
        }

        ngx_memcpy(c->sockaddr, &sa, socklen);

        // 为新连接创建日志结构
        log = ngx_palloc(c->pool, sizeof(ngx_log_t));
        if (log == NULL) {
            ngx_close_accepted_connection(c);
            return;
        }

        // 设置连接的阻塞/非阻塞模式

        if (ngx_inherited_nonblocking) {
            if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
                if (ngx_blocking(s) == -1) {
                    ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_socket_errno,
                                  ngx_blocking_n " failed");
                    ngx_close_accepted_connection(c);
                    return;
                }
            }

        } else {
            if (!(ngx_event_flags & NGX_USE_IOCP_EVENT)) {
                if (ngx_nonblocking(s) == -1) {
                    ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_socket_errno,
                                  ngx_nonblocking_n " failed");
                    ngx_close_accepted_connection(c);
                    return;
                }
            }
        }

        *log = ls->log;

        // 设置连接的I/O操作函数
        c->recv = ngx_recv;
        c->send = ngx_send;
        c->recv_chain = ngx_recv_chain;
        c->send_chain = ngx_send_chain;

        c->log = log;
        c->pool->log = log;

        c->socklen = socklen;
        c->listening = ls;
        c->local_sockaddr = ls->sockaddr;
        c->local_socklen = ls->socklen;

#if (NGX_HAVE_UNIX_DOMAIN)
        // 对Unix域套接字进行特殊处理
        if (c->sockaddr->sa_family == AF_UNIX) {
            c->tcp_nopush = NGX_TCP_NOPUSH_DISABLED;
            c->tcp_nodelay = NGX_TCP_NODELAY_DISABLED;
#if (NGX_SOLARIS)
            // Solaris的sendfilev()支持AF_NCA、AF_INET和AF_INET6
            c->sendfile = 0;
#endif
        }
#endif

        rev = c->read;
        wev = c->write;

        wev->ready = 1;

        // 对IOCP事件进行特殊处理
        if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
            rev->ready = 1;
        }

        // 处理延迟接受
        if (ev->deferred_accept) {
            rev->ready = 1;
#if (NGX_HAVE_KQUEUE || NGX_HAVE_EPOLLRDHUP)
            rev->available = 1;
#endif
        }

        rev->log = log;
        wev->log = log;

        // 分配连接ID
        c->number = ngx_atomic_fetch_add(ngx_connection_counter, 1);

        c->start_time = ngx_current_msec;

#if (NGX_STAT_STUB)
        // 更新已处理连接统计
        (void) ngx_atomic_fetch_add(ngx_stat_handled, 1);
#endif

        // 设置客户端地址文本表示
        if (ls->addr_ntop) {
            c->addr_text.data = ngx_pnalloc(c->pool, ls->addr_text_max_len);
            if (c->addr_text.data == NULL) {
                ngx_close_accepted_connection(c);
                return;
            }

            c->addr_text.len = ngx_sock_ntop(c->sockaddr, c->socklen,
                                             c->addr_text.data,
                                             ls->addr_text_max_len, 0);
            if (c->addr_text.len == 0) {
                ngx_close_accepted_connection(c);
                return;
            }
        }

#if (NGX_DEBUG)
        {
        ngx_str_t  addr;
        u_char     text[NGX_SOCKADDR_STRLEN];

        ngx_debug_accepted_connection(ecf, c);

        if (log->log_level & NGX_LOG_DEBUG_EVENT) {
            addr.data = text;
            addr.len = ngx_sock_ntop(c->sockaddr, c->socklen, text,
                                     NGX_SOCKADDR_STRLEN, 1);

            ngx_log_debug3(NGX_LOG_DEBUG_EVENT, log, 0,
                           "*%uA accept: %V fd:%d", c->number, &addr, s);
        }

        }
#endif

        // 将连接添加到事件处理机制中
        if (ngx_add_conn && (ngx_event_flags & NGX_USE_EPOLL_EVENT) == 0) {
            if (ngx_add_conn(c) == NGX_ERROR) {
                ngx_close_accepted_connection(c);
                return;
            }
        }

        log->data = NULL;
        log->handler = NULL;

        // 调用监听对象的处理函数
        ls->handler(c);

        if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
            ev->available--;
        }

    } while (ev->available);

#if (NGX_HAVE_EPOLLEXCLUSIVE)
    // 重新排序accept事件（仅适用于EPOLLEXCLUSIVE）
    ngx_reorder_accept_events(ls);
#endif
}


/**
 * @brief 尝试获取accept互斥锁
 *
 * @param cycle Nginx核心循环结构体指针
 * @return NGX_OK 成功获取锁或处理完成
 * @return NGX_ERROR 出现错误
 *
 * @details 该函数尝试获取accept互斥锁，用于控制多个worker进程接受新连接。
 * 如果成功获取锁，它会启用accept事件；如果未能获取锁，它会禁用accept事件。
 * 这个机制用于在多个worker进程之间平衡新连接的接受。
 */
ngx_int_t
ngx_trylock_accept_mutex(ngx_cycle_t *cycle)
{
    // 尝试获取accept互斥锁
    if (ngx_shmtx_trylock(&ngx_accept_mutex)) {

        // 记录日志，表示成功获取锁
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                       "accept mutex locked");

        // 如果已持有锁且没有待处理的accept事件，直接返回
        if (ngx_accept_mutex_held && ngx_accept_events == 0) {
            return NGX_OK;
        }

        // 启用accept事件，如果失败则解锁并返回错误
        if (ngx_enable_accept_events(cycle) == NGX_ERROR) {
            ngx_shmtx_unlock(&ngx_accept_mutex);
            return NGX_ERROR;
        }

        // 重置accept事件计数器，标记已持有锁
        ngx_accept_events = 0;
        ngx_accept_mutex_held = 1;

        return NGX_OK;
    }

    // 记录日志，表示获取锁失败
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                   "accept mutex lock failed: %ui", ngx_accept_mutex_held);

    // 如果之前持有锁，现在获取失败，需要禁用accept事件
    if (ngx_accept_mutex_held) {
        if (ngx_disable_accept_events(cycle, 0) == NGX_ERROR) {
            return NGX_ERROR;
        }

        // 标记不再持有锁
        ngx_accept_mutex_held = 0;
    }

    return NGX_OK;
}


/**
 * @brief 启用所有监听套接字的接受事件
 *
 * @param cycle Nginx核心循环结构体指针
 * @return NGX_OK 成功启用所有接受事件
 * @return NGX_ERROR 启用过程中出现错误
 *
 * @details 该函数遍历所有的监听套接字，为每个尚未激活读事件的连接添加NGX_READ_EVENT。
 * 这通常在获取accept mutex后调用，以允许工作进程开始接受新的连接。
 */
ngx_int_t
ngx_enable_accept_events(ngx_cycle_t *cycle)
{
    ngx_uint_t         i;
    ngx_listening_t   *ls;
    ngx_connection_t  *c;

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        c = ls[i].connection;

        if (c == NULL || c->read->active) {
            continue;
        }

        if (ngx_add_event(c->read, NGX_READ_EVENT, 0) == NGX_ERROR) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_disable_accept_events(ngx_cycle_t *cycle, ngx_uint_t all)
{
    ngx_uint_t         i;
    ngx_listening_t   *ls;
    ngx_connection_t  *c;

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        c = ls[i].connection;

        if (c == NULL || !c->read->active) {
            continue;
        }

#if (NGX_HAVE_REUSEPORT)

        /*
         * do not disable accept on worker's own sockets
         * when disabling accept events due to accept mutex
         */

        if (ls[i].reuseport && !all) {
            continue;
        }

#endif

        if (ngx_del_event(c->read, NGX_READ_EVENT, NGX_DISABLE_EVENT)
            == NGX_ERROR)
        {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


#if (NGX_HAVE_EPOLLEXCLUSIVE)

static void
ngx_reorder_accept_events(ngx_listening_t *ls)
{
    ngx_connection_t  *c;

    /*
     * Linux with EPOLLEXCLUSIVE usually notifies only the process which
     * was first to add the listening socket to the epoll instance.  As
     * a result most of the connections are handled by the first worker
     * process.  To fix this, we re-add the socket periodically, so other
     * workers will get a chance to accept connections.
     */

    if (!ngx_use_exclusive_accept) {
        return;
    }

#if (NGX_HAVE_REUSEPORT)

    if (ls->reuseport) {
        return;
    }

#endif

    c = ls->connection;

    if (c->requests++ % 16 != 0
        && ngx_accept_disabled <= 0)
    {
        return;
    }

    if (ngx_del_event(c->read, NGX_READ_EVENT, NGX_DISABLE_EVENT)
        == NGX_ERROR)
    {
        return;
    }

    if (ngx_add_event(c->read, NGX_READ_EVENT, NGX_EXCLUSIVE_EVENT)
        == NGX_ERROR)
    {
        return;
    }
}

#endif


static void
ngx_close_accepted_connection(ngx_connection_t *c)
{
    ngx_socket_t  fd;

    ngx_free_connection(c);

    fd = c->fd;
    c->fd = (ngx_socket_t) -1;

    if (ngx_close_socket(fd) == -1) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_socket_errno,
                      ngx_close_socket_n " failed");
    }

    if (c->pool) {
        ngx_destroy_pool(c->pool);
    }

#if (NGX_STAT_STUB)
    (void) ngx_atomic_fetch_add(ngx_stat_active, -1);
#endif
}


u_char *
ngx_accept_log_error(ngx_log_t *log, u_char *buf, size_t len)
{
    return ngx_snprintf(buf, len, " while accepting new connection on %V",
                        log->data);
}


#if (NGX_DEBUG)

void
ngx_debug_accepted_connection(ngx_event_conf_t *ecf, ngx_connection_t *c)
{
    struct sockaddr_in   *sin;
    ngx_cidr_t           *cidr;
    ngx_uint_t            i;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6  *sin6;
    ngx_uint_t            n;
#endif

    cidr = ecf->debug_connection.elts;
    for (i = 0; i < ecf->debug_connection.nelts; i++) {
        if (cidr[i].family != (ngx_uint_t) c->sockaddr->sa_family) {
            goto next;
        }

        switch (cidr[i].family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            sin6 = (struct sockaddr_in6 *) c->sockaddr;
            for (n = 0; n < 16; n++) {
                if ((sin6->sin6_addr.s6_addr[n]
                    & cidr[i].u.in6.mask.s6_addr[n])
                    != cidr[i].u.in6.addr.s6_addr[n])
                {
                    goto next;
                }
            }
            break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            break;
#endif

        default: /* AF_INET */
            sin = (struct sockaddr_in *) c->sockaddr;
            if ((sin->sin_addr.s_addr & cidr[i].u.in.mask)
                != cidr[i].u.in.addr)
            {
                goto next;
            }
            break;
        }

        c->log->log_level = NGX_LOG_DEBUG_CONNECTION|NGX_LOG_DEBUG_ALL;
        break;

    next:
        continue;
    }
}

#endif
