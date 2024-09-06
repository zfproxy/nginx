
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_STREAM_SSL_H_INCLUDED_
#define _NGX_STREAM_SSL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>


typedef struct {
    ngx_msec_t       handshake_timeout;        /* SSL握手超时时间 */

    ngx_flag_t       prefer_server_ciphers;    /* 是否优先使用服务器端密码套件 */
    ngx_flag_t       reject_handshake;         /* 是否拒绝SSL握手 */

    ngx_ssl_t        ssl;                      /* SSL配置 */

    ngx_uint_t       protocols;                /* 支持的SSL/TLS协议版本 */

    ngx_uint_t       verify;                   /* 客户端证书验证模式 */
    ngx_uint_t       verify_depth;             /* 客户端证书验证深度 */

    ssize_t          builtin_session_cache;    /* 内置会话缓存大小 */

    time_t           session_timeout;          /* SSL会话超时时间 */

    ngx_array_t     *certificates;             /* 服务器证书列表 */
    ngx_array_t     *certificate_keys;         /* 服务器证书私钥列表 */

    ngx_array_t     *certificate_values;       /* 服务器证书值列表 */
    ngx_array_t     *certificate_key_values;   /* 服务器证书私钥值列表 */

    ngx_str_t        dhparam;                  /* DH参数文件路径 */
    ngx_str_t        ecdh_curve;               /* ECDH曲线名称 */
    ngx_str_t        client_certificate;       /* 客户端证书文件路径 */
    ngx_str_t        trusted_certificate;      /* 受信任的CA证书文件路径 */
    ngx_str_t        crl;                      /* 证书吊销列表文件路径 */
    ngx_str_t        alpn;                     /* ALPN协议列表 */

    ngx_str_t        ciphers;                  /* 密码套件列表 */

    ngx_array_t     *passwords;                /* 私钥密码列表 */
    ngx_array_t     *conf_commands;            /* SSL配置命令列表 */

    ngx_shm_zone_t  *shm_zone;                 /* 共享内存区域 */

    ngx_flag_t       session_tickets;          /* 是否启用会话票证 */
    ngx_array_t     *session_ticket_keys;      /* 会话票证密钥列表 */

    ngx_uint_t       ocsp;                     /* OCSP配置 */
    ngx_str_t        ocsp_responder;           /* OCSP响应者URL */
    ngx_shm_zone_t  *ocsp_cache_zone;          /* OCSP缓存共享内存区域 */

    ngx_flag_t       stapling;                 /* 是否启用OCSP装订 */
    ngx_flag_t       stapling_verify;          /* 是否验证OCSP响应 */
    ngx_str_t        stapling_file;            /* OCSP响应文件路径 */
    ngx_str_t        stapling_responder;       /* OCSP装订响应者URL */
} ngx_stream_ssl_srv_conf_t;


extern ngx_module_t  ngx_stream_ssl_module;    /* 声明SSL模块 */


#endif /* _NGX_STREAM_SSL_H_INCLUDED_ */
