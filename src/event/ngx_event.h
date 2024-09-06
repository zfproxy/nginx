
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_H_INCLUDED_
#define _NGX_EVENT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* 定义无效索引的宏 */
#define NGX_INVALID_INDEX  0xd0d0d0d0


#if (NGX_HAVE_IOCP)

/* IOCP事件重叠结构 */
typedef struct {
    WSAOVERLAPPED    ovlp;
    ngx_event_t     *event;
    int              error;
} ngx_event_ovlp_t;

#endif


/* 事件结构体定义 */
struct ngx_event_s {
    void            *data;

    unsigned         write:1;

    unsigned         accept:1;

    /* 用于检测kqueue和epoll中的过期事件 */
    unsigned         instance:1;

    /*
     * 事件已经或将要被传递给内核;
     * 在aio模式下 - 操作已被发布。
     */
    unsigned         active:1;

    unsigned         disabled:1;

    /* 就绪事件; 在aio模式下0表示没有操作可以被发布 */
    unsigned         ready:1;

    unsigned         oneshot:1;

    /* aio操作已完成 */
    unsigned         complete:1;

    unsigned         eof:1;
    unsigned         error:1;

    unsigned         timedout:1;
    unsigned         timer_set:1;

    unsigned         delayed:1;

    unsigned         deferred_accept:1;

    /* kqueue、epoll或aio链操作中报告的待处理EOF */
    unsigned         pending_eof:1;

    unsigned         posted:1;

    unsigned         closed:1;

    /* 用于在worker退出时测试 */
    unsigned         channel:1;
    unsigned         resolver:1;

    unsigned         cancelable:1;

#if (NGX_HAVE_KQUEUE)
    unsigned         kq_vnode:1;

    /* kqueue报告的待处理errno */
    int              kq_errno;
#endif

    /*
     * kqueue专用:
     *   accept:     等待被接受的套接字数量
     *   read:       事件就绪时要读取的字节数
     *               或者当事件设置了NGX_LOWAT_EVENT标志时的低水位标记
     *   write:      事件就绪时缓冲区中的可用空间
     *               或者当事件设置了NGX_LOWAT_EVENT标志时的低水位标记
     *
     * iocp: 待实现
     *
     * 其他情况:
     *   accept:     如果接受多个则为1，否则为0
     *   read:       事件就绪时要读取的字节数，如果未知则为-1
     */

    int              available;

    ngx_event_handler_pt  handler;


#if (NGX_HAVE_IOCP)
    ngx_event_ovlp_t ovlp;
#endif

    ngx_uint_t       index;

    ngx_log_t       *log;

    ngx_rbtree_node_t   timer;

    /* 已发布队列 */
    ngx_queue_t      queue;

#if 0

    /* 线程支持 */

    /*
     * 事件线程上下文，如果$(CC)不理解__thread声明
     * 且pthread_getspecific()代价太高，我们将其存储在这里
     */

    void            *thr_ctx;

#if (NGX_EVENT_T_PADDING)

    /* 事件在SMP中不应跨越缓存行 */

    uint32_t         padding[NGX_EVENT_T_PADDING];
#endif
#endif
};


#if (NGX_HAVE_FILE_AIO)

/* 文件AIO事件结构 */
struct ngx_event_aio_s {
    void                      *data;
    ngx_event_handler_pt       handler;
    ngx_file_t                *file;

    ngx_fd_t                   fd;

#if (NGX_HAVE_EVENTFD)
    int64_t                    res;
#endif

#if !(NGX_HAVE_EVENTFD) || (NGX_TEST_BUILD_EPOLL)
    ngx_err_t                  err;
    size_t                     nbytes;
#endif

    ngx_aiocb_t                aiocb;
    ngx_event_t                event;
};

#endif


/* 事件操作结构体 */
typedef struct {
    ngx_int_t  (*add)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);
    ngx_int_t  (*del)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);

    ngx_int_t  (*enable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);
    ngx_int_t  (*disable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);

    ngx_int_t  (*add_conn)(ngx_connection_t *c);
    ngx_int_t  (*del_conn)(ngx_connection_t *c, ngx_uint_t flags);

    ngx_int_t  (*notify)(ngx_event_handler_pt handler);

    ngx_int_t  (*process_events)(ngx_cycle_t *cycle, ngx_msec_t timer,
                                 ngx_uint_t flags);

    ngx_int_t  (*init)(ngx_cycle_t *cycle, ngx_msec_t timer);
    void       (*done)(ngx_cycle_t *cycle);
} ngx_event_actions_t;


extern ngx_event_actions_t   ngx_event_actions;
#if (NGX_HAVE_EPOLLRDHUP)
extern ngx_uint_t            ngx_use_epoll_rdhup;
#endif


/*
 * 事件过滤器需要读/写全部数据:
 * select, poll, /dev/poll, kqueue, epoll.
 */
#define NGX_USE_LEVEL_EVENT      0x00000001

/*
 * 事件过滤器在通知后无需额外系统调用即可删除:
 * kqueue, epoll.
 */
#define NGX_USE_ONESHOT_EVENT    0x00000002

/*
 * 事件过滤器只通知变化和初始级别:
 * kqueue, epoll.
 */
#define NGX_USE_CLEAR_EVENT      0x00000004

/*
 * 事件过滤器具有kqueue特性: eof标志, errno,
 * 可用数据等.
 */
#define NGX_USE_KQUEUE_EVENT     0x00000008

/*
 * 事件过滤器支持低水位标记: kqueue的NOTE_LOWAT.
 * FreeBSD 4.1-4.2中的kqueue没有NOTE_LOWAT，所以我们需要一个单独的标志。
 */
#define NGX_USE_LOWAT_EVENT      0x00000010

/*
 * 事件过滤器要求执行I/O操作直到EAGAIN: epoll.
 */
#define NGX_USE_GREEDY_EVENT     0x00000020

/*
 * 事件过滤器是epoll.
 */
#define NGX_USE_EPOLL_EVENT      0x00000040

/*
 * 已过时.
 */
#define NGX_USE_RTSIG_EVENT      0x00000080

/*
 * 已过时.
 */
#define NGX_USE_AIO_EVENT        0x00000100

/*
 * 只需添加一次套接字或句柄: I/O完成端口.
 */
#define NGX_USE_IOCP_EVENT       0x00000200

/*
 * 事件过滤器没有不透明数据，需要文件描述符表:
 * poll, /dev/poll.
 */
#define NGX_USE_FD_EVENT         0x00000400

/*
 * 事件模块自行处理周期性或绝对定时器事件:
 * FreeBSD 4.4, NetBSD 2.0, 和MacOSX 10.4中的kqueue, Solaris 10的事件端口.
 */
#define NGX_USE_TIMER_EVENT      0x00000800

/*
 * 通知后删除文件描述符上的所有事件过滤器:
 * Solaris 10的事件端口.
 */
#define NGX_USE_EVENTPORT_EVENT  0x00001000

/*
 * 事件过滤器支持vnode通知: kqueue.
 */
#define NGX_USE_VNODE_EVENT      0x00002000


/*
 * 事件过滤器在关闭文件之前被删除。
 * 对select和poll没有意义。
 * kqueue, epoll, eventport:         允许避免显式删除，
 *                                   因为过滤器在文件关闭时自动删除，
 *
 * /dev/poll:                        我们需要在关闭文件之前
 *                                   刷新POLLREMOVE事件。
 */
#define NGX_CLOSE_EVENT    1

/*
 * 临时禁用事件过滤器，这可能避免内核malloc()/free()中的锁:
 * kqueue.
 */
#define NGX_DISABLE_EVENT  2

/*
 * 事件必须立即传递给内核，不要等待批处理。
 */
#define NGX_FLUSH_EVENT    4


/* 这些标志仅对kqueue有意义 */
#define NGX_LOWAT_EVENT    0
#define NGX_VNODE_EVENT    0


#if (NGX_HAVE_EPOLL) && !(NGX_HAVE_EPOLLRDHUP)
#define EPOLLRDHUP         0
#endif


#if (NGX_HAVE_KQUEUE)

#define NGX_READ_EVENT     EVFILT_READ
#define NGX_WRITE_EVENT    EVFILT_WRITE

#undef  NGX_VNODE_EVENT
#define NGX_VNODE_EVENT    EVFILT_VNODE

/*
 * NGX_CLOSE_EVENT, NGX_LOWAT_EVENT, 和 NGX_FLUSH_EVENT 是模块标志
 * 它们不能进入内核，所以我们需要选择一个不会干扰任何现有和未来kqueue标志的值。
 * kqueue有这样的值 - EV_FLAG1, EV_EOF, 和 EV_ERROR:
 * 它们是保留的，在进入内核时被清除。
 */
#undef  NGX_CLOSE_EVENT
#define NGX_CLOSE_EVENT    EV_EOF

#undef  NGX_LOWAT_EVENT
#define NGX_LOWAT_EVENT    EV_FLAG1

#undef  NGX_FLUSH_EVENT
#define NGX_FLUSH_EVENT    EV_ERROR

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  EV_ONESHOT
#define NGX_CLEAR_EVENT    EV_CLEAR

#undef  NGX_DISABLE_EVENT
#define NGX_DISABLE_EVENT  EV_DISABLE


#elif (NGX_HAVE_DEVPOLL && !(NGX_TEST_BUILD_DEVPOLL)) \
      || (NGX_HAVE_EVENTPORT && !(NGX_TEST_BUILD_EVENTPORT))

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#elif (NGX_HAVE_EPOLL) && !(NGX_TEST_BUILD_EPOLL)

#define NGX_READ_EVENT     (EPOLLIN|EPOLLRDHUP)
#define NGX_WRITE_EVENT    EPOLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_CLEAR_EVENT    EPOLLET
#define NGX_ONESHOT_EVENT  0x70000000
#if 0
#define NGX_ONESHOT_EVENT  EPOLLONESHOT
#endif

#if (NGX_HAVE_EPOLLEXCLUSIVE)
#define NGX_EXCLUSIVE_EVENT  EPOLLEXCLUSIVE
#endif

#elif (NGX_HAVE_POLL)

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#else /* select */

#define NGX_READ_EVENT     0
#define NGX_WRITE_EVENT    1

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1

#endif /* NGX_HAVE_KQUEUE */


#if (NGX_HAVE_IOCP)
#define NGX_IOCP_ACCEPT      0
#define NGX_IOCP_IO          1
#define NGX_IOCP_CONNECT     2
#endif


#if (NGX_TEST_BUILD_EPOLL)
#define NGX_EXCLUSIVE_EVENT  0
#endif


#ifndef NGX_CLEAR_EVENT
#define NGX_CLEAR_EVENT    0    /* 虚拟声明 */
#endif


/* 定义事件处理函数宏 */
#define ngx_process_events   ngx_event_actions.process_events
#define ngx_done_events      ngx_event_actions.done

/* 定义事件操作函数宏 */
#define ngx_add_event        ngx_event_actions.add
#define ngx_del_event        ngx_event_actions.del
#define ngx_add_conn         ngx_event_actions.add_conn
#define ngx_del_conn         ngx_event_actions.del_conn

/* 定义通知函数宏 */
#define ngx_notify           ngx_event_actions.notify

/* 定义定时器操作函数宏 */
#define ngx_add_timer        ngx_event_add_timer
#define ngx_del_timer        ngx_event_del_timer


/* 声明外部I/O操作结构体 */
extern ngx_os_io_t  ngx_io;

/* 定义I/O操作函数宏 */
/* 定义接收数据的函数宏，对应于ngx_io结构体中的recv函数 */
#define ngx_recv             ngx_io.recv

/* 定义链式接收数据的函数宏，对应于ngx_io结构体中的recv_chain函数 */
#define ngx_recv_chain       ngx_io.recv_chain

/* 定义UDP接收数据的函数宏，对应于ngx_io结构体中的udp_recv函数 */
#define ngx_udp_recv         ngx_io.udp_recv

/* 定义发送数据的函数宏，对应于ngx_io结构体中的send函数 */
#define ngx_send             ngx_io.send

/* 定义链式发送数据的函数宏，对应于ngx_io结构体中的send_chain函数 */
#define ngx_send_chain       ngx_io.send_chain

/* 定义UDP发送数据的函数宏，对应于ngx_io结构体中的udp_send函数 */
#define ngx_udp_send         ngx_io.udp_send

/* 定义UDP链式发送数据的函数宏，对应于ngx_io结构体中的udp_send_chain函数 */
#define ngx_udp_send_chain   ngx_io.udp_send_chain


/* 定义事件模块标识 */
#define NGX_EVENT_MODULE      0x544E5645  /* "EVNT" */
#define NGX_EVENT_CONF        0x02000000


/* 事件模块配置结构 */
typedef struct {
    ngx_uint_t    connections;       /* 连接数 */
    ngx_uint_t    use;               /* 使用的事件模型 */

    ngx_flag_t    multi_accept;      /* 是否一次接受多个连接 */
    ngx_flag_t    accept_mutex;      /* 是否使用accept互斥锁 */

    ngx_msec_t    accept_mutex_delay; /* accept互斥锁延迟时间 */

    u_char       *name;              /* 事件模型名称 */

#if (NGX_DEBUG)
    ngx_array_t   debug_connection;  /* 调试连接数组 */
#endif
} ngx_event_conf_t;


/* 事件模块结构 */
typedef struct {
    ngx_str_t              *name;    /* 模块名称 */

    void                 *(*create_conf)(ngx_cycle_t *cycle);  /* 创建配置函数 */
    char                 *(*init_conf)(ngx_cycle_t *cycle, void *conf);  /* 初始化配置函数 */

    ngx_event_actions_t     actions;  /* 事件操作函数集 */
} ngx_event_module_t;


/* 声明外部变量 */
/* 连接计数器，用于跟踪当前活跃连接数 */
extern ngx_atomic_t          *ngx_connection_counter;

/* accept互斥锁指针 */
extern ngx_atomic_t          *ngx_accept_mutex_ptr;
/* accept互斥锁 */
extern ngx_shmtx_t            ngx_accept_mutex;
/* 是否使用accept互斥锁 */
extern ngx_uint_t             ngx_use_accept_mutex;
/* accept事件数 */
extern ngx_uint_t             ngx_accept_events;
/* accept互斥锁是否被持有 */
extern ngx_uint_t             ngx_accept_mutex_held;
/* accept互斥锁延迟时间 */
extern ngx_msec_t             ngx_accept_mutex_delay;
/* accept是否被禁用 */
extern ngx_int_t              ngx_accept_disabled;
/* 是否使用独占accept */
extern ngx_uint_t             ngx_use_exclusive_accept;


#if (NGX_STAT_STUB)

/* 以下是统计相关的原子计数器 */
extern ngx_atomic_t  *ngx_stat_accepted;  /* 已接受的连接数 */
extern ngx_atomic_t  *ngx_stat_handled;   /* 已处理的连接数 */
extern ngx_atomic_t  *ngx_stat_requests;  /* 请求总数 */
extern ngx_atomic_t  *ngx_stat_active;    /* 当前活跃连接数 */
extern ngx_atomic_t  *ngx_stat_reading;   /* 正在读取的连接数 */
extern ngx_atomic_t  *ngx_stat_writing;   /* 正在写入的连接数 */
extern ngx_atomic_t  *ngx_stat_waiting;   /* 等待中的连接数 */

#endif


/* 定义事件处理标志 */
#define NGX_UPDATE_TIME         1  /* 更新时间标志 */
#define NGX_POST_EVENTS         2  /* 延迟处理事件标志 */


/* 声明外部变量和模块 */
extern sig_atomic_t           ngx_event_timer_alarm;  /* 定时器报警标志 */
extern ngx_uint_t             ngx_event_flags;        /* 事件处理标志 */
extern ngx_module_t           ngx_events_module;      /* 事件模块 */
extern ngx_module_t           ngx_event_core_module;  /* 事件核心模块 */


/* 获取事件模块配置的宏 */
#define ngx_event_get_conf(conf_ctx, module)                                  \
             (*(ngx_get_conf(conf_ctx, ngx_events_module))) [module.ctx_index]



/* 处理接受新连接的事件 */
void ngx_event_accept(ngx_event_t *ev);

/* 尝试获取accept互斥锁 */
ngx_int_t ngx_trylock_accept_mutex(ngx_cycle_t *cycle);

/* 启用accept事件 */
ngx_int_t ngx_enable_accept_events(ngx_cycle_t *cycle);

/* 记录accept错误日志 */
u_char *ngx_accept_log_error(ngx_log_t *log, u_char *buf, size_t len);

#if (NGX_DEBUG)
/* 调试模式下，记录已接受的连接信息 */
void ngx_debug_accepted_connection(ngx_event_conf_t *ecf, ngx_connection_t *c);
#endif

/* 处理事件和定时器 */
void ngx_process_events_and_timers(ngx_cycle_t *cycle);

/* 处理读事件 */
ngx_int_t ngx_handle_read_event(ngx_event_t *rev, ngx_uint_t flags);

/* 处理写事件 */
ngx_int_t ngx_handle_write_event(ngx_event_t *wev, size_t lowat);

#if (NGX_WIN32)
/* Windows平台下的AcceptEx事件处理 */
void ngx_event_acceptex(ngx_event_t *ev);

/* 发布AcceptEx事件 */
ngx_int_t ngx_event_post_acceptex(ngx_listening_t *ls, ngx_uint_t n);

/* 记录AcceptEx错误日志 */
u_char *ngx_acceptex_log_error(ngx_log_t *log, u_char *buf, size_t len);
#endif

/* 设置发送低水位标记 */
ngx_int_t ngx_send_lowat(ngx_connection_t *c, size_t lowat);


/* 用于ngx_log_debugX()中的事件标识宏 */
#define ngx_event_ident(p)  ((ngx_connection_t *) (p))->fd


#include <ngx_event_timer.h>
#include <ngx_event_posted.h>
#include <ngx_event_udp.h>

#if (NGX_WIN32)
#include <ngx_iocp_module.h>
#endif


#endif /* _NGX_EVENT_H_INCLUDED_ */
