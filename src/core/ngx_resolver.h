
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef _NGX_RESOLVER_H_INCLUDED_
#define _NGX_RESOLVER_H_INCLUDED_


/* DNS记录类型定义 */
#define NGX_RESOLVE_A         1   /* IPv4地址记录 */
#define NGX_RESOLVE_CNAME     5   /* 规范名称记录 */
#define NGX_RESOLVE_PTR       12  /* 指针记录(反向DNS查询) */
#define NGX_RESOLVE_MX        15  /* 邮件交换记录 */
#define NGX_RESOLVE_TXT       16  /* 文本记录 */
#if (NGX_HAVE_INET6)
#define NGX_RESOLVE_AAAA      28  /* IPv6地址记录 */
#endif
#define NGX_RESOLVE_SRV       33  /* 服务定位记录 */
#define NGX_RESOLVE_DNAME     39  /* 委托名称记录 */

/* DNS响应码定义 */
#define NGX_RESOLVE_FORMERR   1   /* 格式错误 */
#define NGX_RESOLVE_SERVFAIL  2   /* 服务器失败 */
#define NGX_RESOLVE_NXDOMAIN  3   /* 不存在的域名 */
#define NGX_RESOLVE_NOTIMP    4   /* 未实现 */
#define NGX_RESOLVE_REFUSED   5   /* 查询被拒绝 */
#define NGX_RESOLVE_TIMEDOUT  NGX_ETIMEDOUT  /* 查询超时 */

/* 特殊值定义 */
#define NGX_NO_RESOLVER       (void *) -1  /* 表示没有可用的解析器 */

/* 解析器最大递归深度 */
#define NGX_RESOLVER_MAX_RECURSION    50


/* 前向声明解析器结构体 */
typedef struct ngx_resolver_s  ngx_resolver_t;


/* 定义解析器连接结构体 */
typedef struct {
    ngx_connection_t         *udp;        /* UDP连接 */
    ngx_connection_t         *tcp;        /* TCP连接 */
    struct sockaddr          *sockaddr;   /* 服务器地址 */
    socklen_t                 socklen;    /* 地址长度 */
    ngx_str_t                 server;     /* 服务器名称 */
    ngx_log_t                 log;        /* 日志对象 */
    ngx_buf_t                *read_buf;   /* 读缓冲区 */
    ngx_buf_t                *write_buf;  /* 写缓冲区 */
    ngx_resolver_t           *resolver;   /* 指向关联的解析器 */
} ngx_resolver_connection_t;


/* 前向声明解析器上下文结构体 */
typedef struct ngx_resolver_ctx_s  ngx_resolver_ctx_t;

/* 定义解析器处理函数的函数指针类型 */
typedef void (*ngx_resolver_handler_pt)(ngx_resolver_ctx_t *ctx);


/* 定义解析器地址结构体 */
typedef struct {
    struct sockaddr          *sockaddr;   /* 套接字地址 */
    socklen_t                 socklen;    /* 地址长度 */
    ngx_str_t                 name;       /* 主机名 */
    u_short                   priority;   /* 优先级 */
    u_short                   weight;     /* 权重 */
} ngx_resolver_addr_t;


/* 定义SRV记录结构体 */
typedef struct {
    ngx_str_t                 name;       /* 服务名称 */
    u_short                   priority;   /* 优先级 */
    u_short                   weight;     /* 权重 */
    u_short                   port;       /* 端口号 */
} ngx_resolver_srv_t;


/* 定义SRV名称结构体，用于存储和管理SRV记录相关信息 */
typedef struct {
    ngx_str_t                 name;       /* 服务名称 */
    u_short                   priority;   /* 优先级 */
    u_short                   weight;     /* 权重 */
    u_short                   port;       /* 端口号 */

    ngx_resolver_ctx_t       *ctx;        /* 解析器上下文 */
    ngx_int_t                 state;      /* 当前状态 */

    ngx_uint_t                naddrs;     /* 地址数量 */
    ngx_addr_t               *addrs;      /* 地址数组 */
} ngx_resolver_srv_name_t;


typedef struct {
    ngx_rbtree_node_t         node;       /* 红黑树节点 */
    ngx_queue_t               queue;      /* 队列节点 */

    /* PTR: resolved name, A: name to resolve */
    u_char                   *name;       /* 解析的名称或待解析的名称 */

#if (NGX_HAVE_INET6)
    /* PTR: IPv6 address to resolve (IPv4 address is in rbtree node key) */
    struct in6_addr           addr6;      /* IPv6地址（用于PTR查询） */
#endif

    u_short                   nlen;       /* 名称长度 */
    u_short                   qlen;       /* 查询长度 */

    u_char                   *query;      /* 查询内容 */
#if (NGX_HAVE_INET6)
    u_char                   *query6;     /* IPv6查询内容 */
#endif

    union {
        in_addr_t             addr;       /* IPv4地址 */
        in_addr_t            *addrs;      /* IPv4地址数组 */
        u_char               *cname;      /* 规范名称 */
        ngx_resolver_srv_t   *srvs;       /* SRV记录 */
    } u;

    u_char                    code;       /* 响应代码 */
    u_short                   naddrs;     /* 地址数量 */
    u_short                   nsrvs;      /* SRV记录数量 */
    u_short                   cnlen;      /* 规范名称长度 */

#if (NGX_HAVE_INET6)
    union {
        struct in6_addr       addr6;      /* IPv6地址 */
        struct in6_addr      *addrs6;     /* IPv6地址数组 */
    } u6;

    u_short                   naddrs6;    /* IPv6地址数量 */
#endif

    time_t                    expire;     /* 记录过期时间 */
    time_t                    valid;      /* 记录有效期 */
    uint32_t                  ttl;        /* DNS记录的生存时间 */

    unsigned                  tcp:1;      /* 是否使用TCP进行查询 */
#if (NGX_HAVE_INET6)
    unsigned                  tcp6:1;     /* 是否使用IPv6 TCP进行查询 */
#endif

    ngx_uint_t                last_connection;  /* 最后使用的DNS服务器连接索引 */

    ngx_resolver_ctx_t       *waiting;    /* 等待此节点解析完成的上下文链表 */
} ngx_resolver_node_t;


/* 解析器节点结构体定义 */
struct ngx_resolver_s {
    /* 必须是指针，因为是"不完整类型" */
    ngx_event_t              *event;
    void                     *dummy;
    ngx_log_t                *log;

    /* 事件标识必须在3个指针之后，与ngx_connection_t结构保持一致 */
    ngx_int_t                 ident;

    /* 简单的轮询DNS服务器负载均衡器 */
    ngx_array_t               connections;
    ngx_uint_t                last_connection;

    /* 用于存储域名解析结果的红黑树 */
    ngx_rbtree_t              name_rbtree;
    ngx_rbtree_node_t         name_sentinel;

    /* 用于存储SRV记录的红黑树 */
    ngx_rbtree_t              srv_rbtree;
    ngx_rbtree_node_t         srv_sentinel;

    /* 用于存储地址解析结果的红黑树 */
    ngx_rbtree_t              addr_rbtree;
    ngx_rbtree_node_t         addr_sentinel;

    /* 用于重新发送查询的队列 */
    ngx_queue_t               name_resend_queue;
    ngx_queue_t               srv_resend_queue;
    ngx_queue_t               addr_resend_queue;

    /* 用于处理过期记录的队列 */
    ngx_queue_t               name_expire_queue;
    ngx_queue_t               srv_expire_queue;
    ngx_queue_t               addr_expire_queue;

    /* 是否支持IPv4 */
    unsigned                  ipv4:1;

#if (NGX_HAVE_INET6)
    /* 是否支持IPv6 */
    unsigned                  ipv6:1;
    /* IPv6地址解析相关的数据结构 */
    ngx_rbtree_t              addr6_rbtree;        /* IPv6地址解析结果的红黑树 */
    ngx_rbtree_node_t         addr6_sentinel;      /* IPv6地址红黑树的哨兵节点 */
    ngx_queue_t               addr6_resend_queue;  /* IPv6地址重新发送查询的队列 */
    ngx_queue_t               addr6_expire_queue;  /* IPv6地址过期记录的队列 */
#endif

    /* 重新发送查询的超时时间 */
    time_t                    resend_timeout;
    /* TCP连接超时时间 */
    time_t                    tcp_timeout;
    /* 记录过期时间 */
    time_t                    expire;
    /* 记录有效期 */
    time_t                    valid;

    /* 日志级别 */
    ngx_uint_t                log_level;
};


struct ngx_resolver_ctx_s {
    ngx_resolver_ctx_t       *next;
    ngx_resolver_t           *resolver;
    ngx_resolver_node_t      *node;

    /* event ident must be after 3 pointers as in ngx_connection_t */
    ngx_int_t                 ident;

    ngx_int_t                 state;
    ngx_str_t                 name;
    ngx_str_t                 service;

    time_t                    valid;
    ngx_uint_t                naddrs;
    ngx_resolver_addr_t      *addrs;
    ngx_resolver_addr_t       addr;
    struct sockaddr_in        sin;

    ngx_uint_t                count;
    ngx_uint_t                nsrvs;
    ngx_resolver_srv_name_t  *srvs;

    ngx_resolver_handler_pt   handler;
    void                     *data;
    ngx_msec_t                timeout;

    unsigned                  quick:1;
    unsigned                  async:1;
    unsigned                  cancelable:1;
    ngx_uint_t                recursion;
    ngx_event_t              *event;
};


/* 创建一个新的解析器实例 */
ngx_resolver_t *ngx_resolver_create(ngx_conf_t *cf, ngx_str_t *names,
    ngx_uint_t n);

/* 开始一个新的解析上下文 */
ngx_resolver_ctx_t *ngx_resolve_start(ngx_resolver_t *r,
    ngx_resolver_ctx_t *temp);

/* 解析域名 */
ngx_int_t ngx_resolve_name(ngx_resolver_ctx_t *ctx);

/* 完成域名解析 */
void ngx_resolve_name_done(ngx_resolver_ctx_t *ctx);

/* 解析IP地址 */
ngx_int_t ngx_resolve_addr(ngx_resolver_ctx_t *ctx);

/* 完成IP地址解析 */
void ngx_resolve_addr_done(ngx_resolver_ctx_t *ctx);

/* 获取解析器错误的字符串描述 */
char *ngx_resolver_strerror(ngx_int_t err);


#endif /* _NGX_RESOLVER_H_INCLUDED_ */
