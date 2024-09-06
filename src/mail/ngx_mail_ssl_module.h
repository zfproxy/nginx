
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MAIL_SSL_H_INCLUDED_
#define _NGX_MAIL_SSL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_mail.h>


// STARTTLS 配置选项
#define NGX_MAIL_STARTTLS_OFF   0  // 禁用 STARTTLS
#define NGX_MAIL_STARTTLS_ON    1  // 启用 STARTTLS，但允许非加密连接
#define NGX_MAIL_STARTTLS_ONLY  2  // 仅允许 STARTTLS 连接

// SSL 配置结构体
typedef struct {
    ngx_flag_t       prefer_server_ciphers;  // 是否优先使用服务器密码套件

    ngx_ssl_t        ssl;  // SSL 上下文

    ngx_uint_t       starttls;  // STARTTLS 配置
    ngx_uint_t       listen;    // 监听配置
    ngx_uint_t       protocols; // 支持的 SSL/TLS 协议版本

    ngx_uint_t       verify;       // 客户端证书验证级别
    ngx_uint_t       verify_depth; // 证书链验证深度

    ssize_t          builtin_session_cache;  // 内置会话缓存大小

    time_t           session_timeout;  // SSL 会话超时时间

    ngx_array_t     *certificates;     // 服务器证书列表
    ngx_array_t     *certificate_keys; // 服务器私钥列表

    ngx_str_t        dhparam;             // DH 参数文件路径
    ngx_str_t        ecdh_curve;          // ECDH 曲线名称
    ngx_str_t        client_certificate;  // 客户端证书文件路径
    ngx_str_t        trusted_certificate; // 受信任的 CA 证书文件路径
    ngx_str_t        crl;                 // 证书吊销列表文件路径

    ngx_str_t        ciphers;  // 密码套件列表

    ngx_array_t     *passwords;     // 私钥密码列表
    ngx_array_t     *conf_commands; // SSL 配置命令列表

    ngx_shm_zone_t  *shm_zone;  // 共享内存区域，用于存储 SSL 会话

    ngx_flag_t       session_tickets;     // 是否启用会话票证
    ngx_array_t     *session_ticket_keys; // 会话票证密钥列表

    u_char          *file;  // 配置文件名
    ngx_uint_t       line;  // 配置所在行号
} ngx_mail_ssl_conf_t;

// 声明 ngx_mail_ssl_module 模块
extern ngx_module_t  ngx_mail_ssl_module;


#endif /* _NGX_MAIL_SSL_H_INCLUDED_ */
