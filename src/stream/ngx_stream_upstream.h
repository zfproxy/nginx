
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_STREAM_UPSTREAM_H_INCLUDED_
#define _NGX_STREAM_UPSTREAM_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>
#include <ngx_event_connect.h>


/* 定义上游服务器的各种标志位 */
#define NGX_STREAM_UPSTREAM_CREATE        0x0001  /* 创建上游服务器 */
#define NGX_STREAM_UPSTREAM_WEIGHT        0x0002  /* 设置上游服务器权重 */
#define NGX_STREAM_UPSTREAM_MAX_FAILS     0x0004  /* 设置最大失败次数 */
#define NGX_STREAM_UPSTREAM_FAIL_TIMEOUT  0x0008  /* 设置失败超时时间 */
#define NGX_STREAM_UPSTREAM_DOWN          0x0010  /* 标记上游服务器为离线状态 */
#define NGX_STREAM_UPSTREAM_BACKUP        0x0020  /* 标记上游服务器为备用服务器 */
#define NGX_STREAM_UPSTREAM_MAX_CONNS     0x0100  /* 设置最大连接数 */


/* 定义上游服务器连接通知标志 */
#define NGX_STREAM_UPSTREAM_NOTIFY_CONNECT     0x1  /* 连接建立时通知 */


/* 定义上游主配置结构体 */
typedef struct {
    ngx_array_t                        upstreams;
                                           /* ngx_stream_upstream_srv_conf_t */
} ngx_stream_upstream_main_conf_t;


/**
 * @brief 上游服务器配置结构体的前向声明
 */
typedef struct ngx_stream_upstream_srv_conf_s  ngx_stream_upstream_srv_conf_t;

/**
 * @brief 上游初始化函数指针类型
 * @param cf 配置上下文
 * @param us 上游服务器配置
 * @return ngx_int_t 初始化结果
 */
typedef ngx_int_t (*ngx_stream_upstream_init_pt)(ngx_conf_t *cf,
    ngx_stream_upstream_srv_conf_t *us);

/**
 * @brief 上游初始化对等点函数指针类型
 * @param s 流会话
 * @param us 上游服务器配置
 * @return ngx_int_t 初始化结果
 */
typedef ngx_int_t (*ngx_stream_upstream_init_peer_pt)(ngx_stream_session_t *s,
    ngx_stream_upstream_srv_conf_t *us);


/**
 * @brief 上游对等点结构体
 */
typedef struct {
    ngx_stream_upstream_init_pt        init_upstream;  /**< 初始化上游的函数指针 */
    ngx_stream_upstream_init_peer_pt   init;           /**< 初始化对等点的函数指针 */
    void                              *data;           /**< 用户自定义数据 */
} ngx_stream_upstream_peer_t;


/**
 * @brief 上游服务器结构体
 */
typedef struct {
    ngx_str_t                          name;           /**< 服务器名称 */
    ngx_addr_t                        *addrs;          /**< 服务器地址数组 */
    ngx_uint_t                         naddrs;         /**< 地址数量 */
    ngx_uint_t                         weight;         /**< 服务器权重 */
    ngx_uint_t                         max_conns;      /**< 最大连接数 */
    ngx_uint_t                         max_fails;      /**< 最大失败次数 */
    time_t                             fail_timeout;   /**< 失败超时时间 */
    ngx_msec_t                         slow_start;     /**< 慢启动时间 */
    ngx_uint_t                         down;           /**< 服务器是否下线 */

    unsigned                           backup:1;       /**< 是否为备用服务器 */

    NGX_COMPAT_BEGIN(4)
    NGX_COMPAT_END
} ngx_stream_upstream_server_t;


/**
 * @brief 上游服务器配置结构体
 */
struct ngx_stream_upstream_srv_conf_s {
    ngx_stream_upstream_peer_t         peer;           /**< 上游对等点配置 */
    void                             **srv_conf;       /**< 服务器配置数组 */

    ngx_array_t                       *servers;        /**< 上游服务器数组 */
                                              /* ngx_stream_upstream_server_t */

    ngx_uint_t                         flags;          /**< 配置标志 */
    ngx_str_t                          host;           /**< 主机名 */
    u_char                            *file_name;      /**< 配置文件名 */
    ngx_uint_t                         line;           /**< 配置行号 */
    in_port_t                          port;           /**< 端口号 */
    ngx_uint_t                         no_port;        /**< 是否没有指定端口 */
                                                       /* unsigned no_port:1 */

#if (NGX_STREAM_UPSTREAM_ZONE)
    ngx_shm_zone_t                    *shm_zone;       /**< 共享内存区域 */
#endif
};

/**
 * @brief 上游连接状态结构体
 */
typedef struct {
    ngx_msec_t                         response_time;   /**< 响应时间 */
    ngx_msec_t                         connect_time;    /**< 连接时间 */
    ngx_msec_t                         first_byte_time; /**< 首字节时间 */
    off_t                              bytes_sent;      /**< 发送字节数 */
    off_t                              bytes_received;  /**< 接收字节数 */

    ngx_str_t                         *peer;            /**< 对等点信息 */
} ngx_stream_upstream_state_t;


/**
 * @brief 已解析的上游服务器信息结构体
 */
typedef struct {
    ngx_str_t                          host;            /**< 主机名 */
    in_port_t                          port;            /**< 端口号 */
    ngx_uint_t                         no_port;         /**< 是否没有指定端口 */

    ngx_uint_t                         naddrs;          /**< 地址数量 */
    ngx_resolver_addr_t               *addrs;           /**< 解析后的地址数组 */

    struct sockaddr                   *sockaddr;        /**< 套接字地址 */
    socklen_t                          socklen;         /**< 套接字地址长度 */
    ngx_str_t                          name;            /**< 服务器名称 */

    ngx_resolver_ctx_t                *ctx;             /**< 解析器上下文 */
} ngx_stream_upstream_resolved_t;


/**
 * @brief 上游连接结构体
 */
typedef struct {
    ngx_peer_connection_t              peer;            /**< 对等连接 */

    ngx_buf_t                          downstream_buf;  /**< 下游缓冲区 */
    ngx_buf_t                          upstream_buf;    /**< 上游缓冲区 */

    ngx_chain_t                       *free;            /**< 空闲缓冲链 */
    ngx_chain_t                       *upstream_out;    /**< 上游输出链 */
    ngx_chain_t                       *upstream_busy;   /**< 上游忙碌链 */
    ngx_chain_t                       *downstream_out;  /**< 下游输出链 */
    ngx_chain_t                       *downstream_busy; /**< 下游忙碌链 */

    off_t                              received;        /**< 接收的字节数 */
    time_t                             start_sec;       /**< 开始时间（秒） */
    ngx_uint_t                         requests;        /**< 请求数 */
    ngx_uint_t                         responses;       /**< 响应数 */
    ngx_msec_t                         start_time;      /**< 开始时间（毫秒） */

    size_t                             upload_rate;     /**< 上传速率 */
    size_t                             download_rate;   /**< 下载速率 */

    ngx_str_t                          ssl_name;        /**< SSL名称 */

    ngx_stream_upstream_srv_conf_t    *upstream;        /**< 上游服务器配置 */
    ngx_stream_upstream_resolved_t    *resolved;        /**< 已解析的上游信息 */
    ngx_stream_upstream_state_t       *state;           /**< 上游连接状态 */
    unsigned                           connected:1;     /**< 是否已连接 */
    unsigned                           proxy_protocol:1;/**< 是否使用代理协议 */
    unsigned                           half_closed:1;   /**< 是否半关闭 */
} ngx_stream_upstream_t;


/**
 * @brief 添加上游服务器配置
 * @param cf 配置结构体
 * @param u URL结构体
 * @param flags 标志
 * @return 上游服务器配置结构体指针
 */
ngx_stream_upstream_srv_conf_t *ngx_stream_upstream_add(ngx_conf_t *cf,
    ngx_url_t *u, ngx_uint_t flags);


/**
 * @brief 获取上游服务器配置宏
 * @param uscf 上游服务器配置结构体
 * @param module 模块
 */
#define ngx_stream_conf_upstream_srv_conf(uscf, module)                       \
    uscf->srv_conf[module.ctx_index]


/**
 * @brief 上游模块声明
 */
extern ngx_module_t  ngx_stream_upstream_module;


#endif /* _NGX_STREAM_UPSTREAM_H_INCLUDED_ */
