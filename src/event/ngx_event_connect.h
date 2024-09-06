
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_CONNECT_H_INCLUDED_
#define _NGX_EVENT_CONNECT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


/* 定义对等连接的状态标志 */
#define NGX_PEER_KEEPALIVE           1  /* 保持连接活跃 */
#define NGX_PEER_NEXT                2  /* 尝试下一个对等点 */
#define NGX_PEER_FAILED              4  /* 连接失败 */


/* 对等连接结构体的前向声明 */
typedef struct ngx_peer_connection_s  ngx_peer_connection_t;

/* 获取对等点的函数指针类型 */
typedef ngx_int_t (*ngx_event_get_peer_pt)(ngx_peer_connection_t *pc,
    void *data);

/* 释放对等点的函数指针类型 */
typedef void (*ngx_event_free_peer_pt)(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t state);

/* 通知对等点事件的函数指针类型 */
typedef void (*ngx_event_notify_peer_pt)(ngx_peer_connection_t *pc,
    void *data, ngx_uint_t type);

/* 设置对等点会话的函数指针类型 */
typedef ngx_int_t (*ngx_event_set_peer_session_pt)(ngx_peer_connection_t *pc,
    void *data);

/* 保存对等点会话的函数指针类型 */
typedef void (*ngx_event_save_peer_session_pt)(ngx_peer_connection_t *pc,
    void *data);

/**
 * @brief 对等连接结构体
 *
 * 该结构体包含了与对等连接相关的所有信息和操作函数指针。
 * 它用于管理和控制与上游服务器或其他对等节点的连接。
 */
struct ngx_peer_connection_s {
    ngx_connection_t                *connection;  /* 与对等点的连接对象 */

    struct sockaddr                 *sockaddr;    /* 对等点的套接字地址 */
    socklen_t                        socklen;     /* 套接字地址长度 */
    ngx_str_t                       *name;        /* 对等点的名称 */

    ngx_uint_t                       tries;       /* 尝试连接的次数，用于重试逻辑 */
    ngx_msec_t                       start_time;  /* 开始连接的时间戳，用于计算连接时间 */

    ngx_event_get_peer_pt            get;         /* 获取对等点的函数指针，用于负载均衡 */
    ngx_event_free_peer_pt           free;        /* 释放对等点的函数指针，用于连接池管理 */
    ngx_event_notify_peer_pt         notify;      /* 通知对等点事件的函数指针，用于状态更新 */
    void                            *data;        /* 用户自定义数据，可用于存储额外信息 */

#if (NGX_SSL || NGX_COMPAT)
    ngx_event_set_peer_session_pt    set_session; /* 设置SSL会话的函数指针，用于SSL会话复用 */
    ngx_event_save_peer_session_pt   save_session;/* 保存SSL会话的函数指针，用于SSL会话保存 */
#endif

    ngx_addr_t                      *local;       /* 本地地址信息，用于绑定特定本地IP */

    int                              type;        /* 连接类型，如TCP、UDP等 */
    int                              rcvbuf;      /* 接收缓冲区大小，用于调整套接字参数 */

    ngx_log_t                       *log;         /* 日志对象，用于记录连接相关的日志 */

    unsigned                         cached:1;    /* 是否使用缓存的连接，用于连接池优化 */
    unsigned                         transparent:1; /* 是否使用透明代理，影响网络层行为 */
    unsigned                         so_keepalive:1; /* 是否启用SO_KEEPALIVE，用于保持连接活跃 */
    unsigned                         down:1;      /* 对等点是否已下线，用于标记不可用的节点 */

                                     /* ngx_connection_log_error_e */
    unsigned                         log_error:2; /* 日志错误级别，用于控制错误日志的详细程度 */

    NGX_COMPAT_BEGIN(2)
    NGX_COMPAT_END
};


ngx_int_t ngx_event_connect_peer(ngx_peer_connection_t *pc);  /* 连接对等点的函数 */
ngx_int_t ngx_event_get_peer(ngx_peer_connection_t *pc, void *data);  /* 获取对等点的函数 */


#endif /* _NGX_EVENT_CONNECT_H_INCLUDED_ */
