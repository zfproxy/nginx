
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONNECTION_H_INCLUDED_
#define _NGX_CONNECTION_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 定义ngx_listening_t类型
 *
 * 这是一个类型定义，将struct ngx_listening_s结构体定义为ngx_listening_t类型。
 * ngx_listening_t用于表示Nginx中的监听套接字，包含了与监听相关的所有信息。
 * 这个类型在Nginx的网络模块中广泛使用，用于管理服务器的监听端口和地址。
 */
typedef struct ngx_listening_s  ngx_listening_t;

/**
 * @brief 定义监听套接字结构体
 *
 * 这个结构体包含了与监听套接字相关的所有信息，
 * 包括套接字描述符、地址信息、配置参数等。
 * 它在Nginx的网络处理中扮演着重要角色，用于管理所有的监听端口。
 */
struct ngx_listening_s {
    ngx_socket_t        fd;                  /* 监听套接字描述符 */

    struct sockaddr    *sockaddr;            /* 监听地址结构指针 */
    socklen_t           socklen;             /* sockaddr 结构体大小 */
    size_t              addr_text_max_len;   /* 地址文本的最大长度 */
    ngx_str_t           addr_text;           /* 地址的文本表示 */

    int                 type;                /* 套接字类型 */

    int                 backlog;             /* 监听队列长度 */
    int                 rcvbuf;              /* 接收缓冲区大小 */
    int                 sndbuf;              /* 发送缓冲区大小 */
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int                 keepidle;            /* TCP keepalive 空闲时间 */
    int                 keepintvl;           /* TCP keepalive 间隔时间 */
    int                 keepcnt;             /* TCP keepalive 探测次数 */
#endif

    /* handler of accepted connection */
    ngx_connection_handler_pt   handler;     /* 接受连接后的处理函数 */

    void               *servers;             /* 服务器数组，例如 ngx_http_in_addr_t */

    ngx_log_t           log;                 /* 日志对象 */
    ngx_log_t          *logp;                /* 日志对象指针 */

    size_t              pool_size;           /* 内存池大小 */
    /* should be here because of the AcceptEx() preread */
    size_t              post_accept_buffer_size;  /* 接受连接后的缓冲区大小 */

    ngx_listening_t    *previous;            /* 前一个监听对象 */
    ngx_connection_t   *connection;          /* 关联的连接对象 */

    ngx_rbtree_t        rbtree;              /* 红黑树，用于快速查找 */
    ngx_rbtree_node_t   sentinel;            /* 红黑树哨兵节点 */

    ngx_uint_t          worker;              /* 工作进程 ID */

    unsigned            open:1;              /* 套接字是否打开 */
    unsigned            remain:1;            /* 是否保持打开状态 */
    unsigned            ignore:1;            /* 是否忽略 */

    unsigned            bound:1;             /* 是否已绑定 */
    unsigned            inherited:1;         /* 是否从上一个进程继承 */
    unsigned            nonblocking_accept:1;/* 是否使用非阻塞接受 */
    unsigned            listen:1;            /* 是否正在监听 */
    unsigned            nonblocking:1;       /* 是否为非阻塞模式 */
    unsigned            shared:1;            /* 是否在线程或进程间共享 */
    unsigned            addr_ntop:1;         /* 地址是否已转换为文本 */
    unsigned            wildcard:1;          /* 是否为通配符地址 */

#if (NGX_HAVE_INET6)
    unsigned            ipv6only:1;          /* 是否仅支持 IPv6 */
#endif
    unsigned            reuseport:1;         /* 是否启用端口重用 */
    unsigned            add_reuseport:1;     /* 是否添加端口重用 */
    unsigned            keepalive:2;         /* keepalive 状态 */
    unsigned            quic:1;              /* 是否支持 QUIC 协议 */

    unsigned            deferred_accept:1;   /* 是否使用延迟接受 */
    unsigned            delete_deferred:1;   /* 是否删除延迟接受 */
    unsigned            add_deferred:1;      /* 是否添加延迟接受 */
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    char               *accept_filter;       /* 接受过滤器名称 */
#endif
#if (NGX_HAVE_SETFIB)
    int                 setfib;              /* 转发信息库 ID */
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
    int                 fastopen;            /* TCP Fast Open 队列长度 */
#endif

};


/**
 * @brief 连接日志错误枚举
 *
 * 该枚举定义了不同级别的连接日志错误类型。
 * 这些错误类型用于在处理网络连接时对不同情况进行分类和处理。
 */
typedef enum {
    NGX_ERROR_ALERT = 0,              /* 严重错误，需要立即处理 */
    NGX_ERROR_ERR,                    /* 一般错误 */
    NGX_ERROR_INFO,                   /* 信息性错误，通常不需要特别处理 */
    NGX_ERROR_IGNORE_ECONNRESET,      /* 忽略连接重置错误 */
    NGX_ERROR_IGNORE_EINVAL,          /* 忽略无效参数错误 */
    NGX_ERROR_IGNORE_EMSGSIZE         /* 忽略消息大小错误 */
} ngx_connection_log_error_e;


/**
 * @brief TCP NODELAY 选项枚举
 *
 * 该枚举定义了 TCP NODELAY 选项的不同状态。
 * TCP NODELAY 选项用于控制是否启用 Nagle 算法。
 */
typedef enum {
    NGX_TCP_NODELAY_UNSET = 0,    /* TCP NODELAY 选项未设置 */
    NGX_TCP_NODELAY_SET,          /* TCP NODELAY 选项已设置 */
    NGX_TCP_NODELAY_DISABLED      /* TCP NODELAY 选项已禁用 */
} ngx_connection_tcp_nodelay_e;


/**
 * @brief TCP NOPUSH 选项枚举
 *
 * 该枚举定义了 TCP NOPUSH 选项的不同状态。
 * TCP NOPUSH 选项用于控制是否启用 TCP 的 NOPUSH 功能，
 * 该功能可以在某些情况下优化数据传输。
 */
typedef enum {
    NGX_TCP_NOPUSH_UNSET = 0,    /* TCP NOPUSH 选项未设置 */
    NGX_TCP_NOPUSH_SET,          /* TCP NOPUSH 选项已设置 */
    NGX_TCP_NOPUSH_DISABLED      /* TCP NOPUSH 选项已禁用 */
} ngx_connection_tcp_nopush_e;


/**
 * @brief 低级缓冲标志
 *
 * 该宏定义了低级缓冲的标志值。
 * 它用于表示连接的缓冲状态，特别是在低级别的网络操作中。
 * 值0x0f（二进制为1111）允许使用低4位来表示不同的缓冲状态。
 */
#define NGX_LOWLEVEL_BUFFERED  0x0f
/**
 * @brief SSL缓冲标志
 *
 * 该宏定义了SSL缓冲的标志值。
 * 它用于表示SSL连接的缓冲状态。
 * 值0x01（二进制为0001）表示SSL连接正在使用缓冲。
 * 这个标志通常用于SSL握手过程中或者处理加密数据时。
 */
#define NGX_SSL_BUFFERED       0x01
/**
 * @brief HTTP/2 缓冲标志
 *
 * 该宏定义了 HTTP/2 缓冲的标志值。
 * 它用于表示 HTTP/2 连接的缓冲状态。
 * 值 0x02（二进制为 0010）表示 HTTP/2 连接正在使用缓冲。
 * 这个标志通常用于 HTTP/2 帧处理或流量控制过程中。
 */
#define NGX_HTTP_V2_BUFFERED   0x02


/**
 * @brief 连接结构体
 *
 * ngx_connection_s 结构体表示 Nginx 中的一个网络连接。
 * 它包含了与连接相关的所有重要信息，如文件描述符、读写事件、缓冲区等。
 * 这个结构体是 Nginx 网络处理的核心，用于管理客户端和上游服务器的连接。
 */
struct ngx_connection_s {
    void               *data;            /* 用于存储自定义数据的指针 */
    ngx_event_t        *read;            /* 读事件 */
    ngx_event_t        *write;           /* 写事件 */

    ngx_socket_t        fd;              /* 文件描述符 */

    ngx_recv_pt         recv;            /* 接收数据的函数指针 */
    ngx_send_pt         send;            /* 发送数据的函数指针 */
    ngx_recv_chain_pt   recv_chain;      /* 接收链式数据的函数指针 */
    ngx_send_chain_pt   send_chain;      /* 发送链式数据的函数指针 */

    ngx_listening_t    *listening;       /* 关联的监听对象 */

    off_t               sent;            /* 已发送的字节数 */

    ngx_log_t          *log;             /* 日志对象 */

    ngx_pool_t         *pool;            /* 内存池 */

    int                 type;            /* 连接类型 */

    struct sockaddr    *sockaddr;        /* 远端地址 */
    socklen_t           socklen;         /* 远端地址长度 */
    ngx_str_t           addr_text;       /* 远端地址的文本表示 */

    ngx_proxy_protocol_t  *proxy_protocol; /* 代理协议信息 */

#if (NGX_QUIC || NGX_COMPAT)
    ngx_quic_stream_t     *quic;         /* QUIC 流信息 */
#endif

#if (NGX_SSL || NGX_COMPAT)
    ngx_ssl_connection_t  *ssl;          /* SSL 连接信息 */
#endif

    ngx_udp_connection_t  *udp;          /* UDP 连接信息 */

    struct sockaddr    *local_sockaddr;  /* 本地地址 */
    socklen_t           local_socklen;   /* 本地地址长度 */

    ngx_buf_t          *buffer;          /* 缓冲区 */

    ngx_queue_t         queue;           /* 连接队列 */

    ngx_atomic_uint_t   number;          /* 连接序号 */

    ngx_msec_t          start_time;      /* 连接开始时间 */
    ngx_uint_t          requests;        /* 请求数 */

    unsigned            buffered:8;      /* 缓冲状态 */

    unsigned            log_error:3;     /* 日志错误级别 */

    unsigned            timedout:1;      /* 超时标志 */
    unsigned            error:1;         /* 错误标志 */
    unsigned            destroyed:1;     /* 销毁标志 */
    unsigned            pipeline:1;      /* 管道标志 */

    unsigned            idle:1;          /* 空闲标志 */
    unsigned            reusable:1;      /* 可重用标志 */
    unsigned            close:1;         /* 关闭标志 */
    unsigned            shared:1;        /* 共享标志 */

    unsigned            sendfile:1;      /* sendfile 标志 */
    unsigned            sndlowat:1;      /* 发送低水位标志 */
    unsigned            tcp_nodelay:2;   /* TCP_NODELAY 选项 */
    unsigned            tcp_nopush:2;    /* TCP_NOPUSH 选项 */

    unsigned            need_last_buf:1; /* 需要最后缓冲区标志 */
    unsigned            need_flush_buf:1; /* 需要刷新缓冲区标志 */

#if (NGX_HAVE_SENDFILE_NODISKIO || NGX_COMPAT)
    unsigned            busy_count:2;    /* 忙计数 */
#endif

#if (NGX_THREADS || NGX_COMPAT)
    ngx_thread_task_t  *sendfile_task;   /* sendfile 任务 */
#endif
};


/**
 * @brief 设置连接的日志参数
 *
 * 这个宏用于设置一个连接的日志参数。它将日志对象 l 的属性复制到连接 c 的日志对象中。
 *
 * @param c 要设置日志参数的连接对象
 * @param l 源日志对象，其参数将被复制到连接的日志对象中
 */
#define ngx_set_connection_log(c, l)                                         \
                                                                             \
    c->log->file = l->file;                                                  \
    c->log->next = l->next;                                                  \
    c->log->writer = l->writer;                                              \
    c->log->wdata = l->wdata;                                                \
    if (!(c->log->log_level & NGX_LOG_DEBUG_CONNECTION)) {                   \
        c->log->log_level = l->log_level;                                    \
    }


/**
 * @brief 创建一个新的监听对象
 *
 * 该函数用于创建一个新的ngx_listening_t对象，用于表示一个监听套接字。
 * 它设置了监听套接字的地址和其他初始属性。
 *
 * @param cf Nginx配置对象指针，用于内存分配和错误日志
 * @param sockaddr 指向sockaddr结构的指针，包含要监听的地址信息
 * @return 成功时返回新创建的ngx_listening_t对象指针，失败时返回NULL
 */
ngx_listening_t *ngx_create_listening(ngx_conf_t *cf, struct sockaddr *sockaddr,
    socklen_t socklen);
/**
 * @brief 克隆一个监听对象
 *
 * 该函数用于克隆一个已存在的监听对象。它会创建一个新的监听对象，
 * 并复制原监听对象的属性。这在需要基于现有监听配置创建新的监听时很有用。
 *
 * @param cycle 指向当前 Nginx 周期结构的指针
 * @param ls 要克隆的原始监听对象
 * @return 成功时返回 NGX_OK，失败时返回 NGX_ERROR
 */
ngx_int_t ngx_clone_listening(ngx_cycle_t *cycle, ngx_listening_t *ls);
/**
 * @brief 设置继承的套接字
 *
 * 该函数用于设置从父进程继承的套接字。在Nginx进行平滑升级时，
 * 新的worker进程会从旧的master进程继承已打开的监听套接字。
 * 这个函数负责处理这些继承的套接字，并将它们集成到新的进程中。
 *
 * @param cycle 指向当前Nginx周期结构的指针
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_set_inherited_sockets(ngx_cycle_t *cycle);
/**
 * @brief 打开监听套接字
 *
 * 该函数用于打开所有配置的监听套接字。它会遍历cycle中的listening数组，
 * 为每个监听对象创建并初始化相应的套接字。这个过程包括设置套接字选项、
 * 绑定地址、开始监听等操作。
 *
 * @param cycle 指向当前Nginx周期结构的指针，包含了所有的监听配置信息
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_open_listening_sockets(ngx_cycle_t *cycle);
/**
 * @brief 配置监听套接字
 *
 * 该函数用于配置所有已打开的监听套接字。它会遍历cycle中的listening数组，
 * 对每个监听套接字进行必要的配置，如设置套接字选项、调整缓冲区大小等。
 * 这个步骤通常在套接字打开后、开始接受连接之前执行。
 *
 * @param cycle 指向当前Nginx周期结构的指针，包含了所有的监听配置信息
 */
void ngx_configure_listening_sockets(ngx_cycle_t *cycle);
/**
 * @brief 关闭所有监听套接字
 *
 * 该函数用于关闭当前Nginx周期中所有的监听套接字。在Nginx需要停止服务或
 * 重新加载配置时，会调用此函数来清理所有打开的监听套接字。这个过程包括
 * 遍历cycle中的listening数组，关闭每个监听对象对应的套接字。
 *
 * @param cycle 指向当前Nginx周期结构的指针，包含了所有的监听配置信息
 */
void ngx_close_listening_sockets(ngx_cycle_t *cycle);
/**
 * @brief 关闭连接
 *
 * 该函数用于关闭一个Nginx连接。它会执行必要的清理操作，
 * 包括关闭套接字、释放相关资源等。这个函数通常在连接
 * 出错、超时或完成其任务时被调用。
 *
 * @param c 指向要关闭的ngx_connection_t结构的指针
 */
void ngx_close_connection(ngx_connection_t *c);
/**
 * @brief 关闭空闲连接
 *
 * 该函数用于关闭Nginx中所有空闲的连接。在某些情况下，如服务器需要释放资源或
 * 进行清理操作时，会调用此函数来关闭那些当前没有活跃使用的连接。这有助于
 * 优化资源使用，并可能提高服务器的整体性能。
 *
 * @param cycle 指向当前Nginx周期结构的指针，包含了所有的连接信息
 */
void ngx_close_idle_connections(ngx_cycle_t *cycle);
/**
 * @brief 获取连接的本地套接字地址
 *
 * 该函数用于获取指定连接的本地套接字地址信息。它将地址信息填充到提供的字符串中，
 * 并可选择性地包含端口号。这对于日志记录、调试或需要知道本地端点信息的场景很有用。
 *
 * @param c 指向ngx_connection_t结构的指针，表示要获取地址的连接
 * @param s 指向ngx_str_t结构的指针，用于存储获取到的地址信息
 * @param port 标志位，指示是否在地址字符串中包含端口号
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_connection_local_sockaddr(ngx_connection_t *c, ngx_str_t *s,
    ngx_uint_t port);
/**
 * @brief 设置TCP连接的NODELAY选项
 *
 * 该函数用于为指定的TCP连接设置NODELAY选项。TCP_NODELAY选项禁用Nagle算法，
 * 允许小包立即发送，而不是等待更多数据累积。这通常用于需要低延迟的场景，
 * 但可能会增加网络流量。
 *
 * @param c 指向ngx_connection_t结构的指针，表示要设置NODELAY选项的TCP连接
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_tcp_nodelay(ngx_connection_t *c);
/**
 * @brief 处理连接错误
 *
 * 该函数用于处理连接过程中发生的错误。它会记录错误信息，并可能执行一些清理操作。
 * 这个函数通常在连接建立、数据传输或其他网络操作出现问题时被调用。
 *
 * @param c 指向ngx_connection_t结构的指针，表示发生错误的连接
 * @param err 错误码，通常是系统errno或Nginx定义的错误码
 * @param text 错误描述文本，用于提供额外的错误上下文信息
 * @return 返回NGX_ERROR表示错误已被处理，其他值可能表示特定的错误处理结果
 */
ngx_int_t ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text);

/**
 * @brief 获取一个新的连接对象
 *
 * 该函数用于从连接池中获取一个新的ngx_connection_t对象。这个对象将被用于
 * 管理一个新的网络连接。函数会初始化连接对象的各个字段，并将其与给定的
 * 套接字和日志对象关联起来。
 *
 * @param s 与新连接关联的套接字描述符
 * @param log 用于记录该连接相关日志的日志对象指针
 * @return 成功时返回指向新分配的ngx_connection_t对象的指针，失败时返回NULL
 */
ngx_connection_t *ngx_get_connection(ngx_socket_t s, ngx_log_t *log);
/**
 * @brief 释放一个连接对象
 *
 * 该函数用于将一个不再使用的连接对象归还到连接池中。这个操作通常在连接关闭或
 * 不再需要时执行，以便连接对象可以被重新利用，从而提高资源利用效率。
 *
 * @param c 指向要释放的ngx_connection_t结构的指针
 */
void ngx_free_connection(ngx_connection_t *c);

/**
 * @brief 设置连接的可重用状态
 *
 * 该函数用于设置一个连接是否可以被重用。在Nginx中，连接重用是一种优化技术，
 * 可以减少创建和销毁连接的开销，特别是在处理大量短期连接的场景中。
 *
 * @param c 指向ngx_connection_t结构的指针，表示要设置重用状态的连接
 * @param reusable 一个无符号整数，表示是否将连接标记为可重用
 *                 非零值表示连接可重用，0表示连接不可重用
 */
void ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable);

#endif /* _NGX_CONNECTION_H_INCLUDED_ */
