
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MAIL_SMTP_MODULE_H_INCLUDED_
#define _NGX_MAIL_SMTP_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_mail.h>
#include <ngx_mail_smtp_module.h>


// SMTP服务器配置结构体
typedef struct {
    ngx_msec_t   greeting_delay;          // 问候延迟时间
    
    size_t       client_buffer_size;      // 客户端缓冲区大小
    
    ngx_str_t    capability;              // 基本能力字符串
    ngx_str_t    starttls_capability;     // STARTTLS能力字符串
    ngx_str_t    starttls_only_capability;// 仅STARTTLS能力字符串
    
    ngx_str_t    server_name;             // 服务器名称
    ngx_str_t    greeting;                // 问候语
    
    ngx_uint_t   auth_methods;            // 支持的认证方法
    
    ngx_array_t  capabilities;            // 额外的能力列表
} ngx_mail_smtp_srv_conf_t;


// 初始化SMTP会话
void ngx_mail_smtp_init_session(ngx_mail_session_t *s, ngx_connection_t *c);

// 初始化SMTP协议
void ngx_mail_smtp_init_protocol(ngx_event_t *rev);

// 处理SMTP认证状态
void ngx_mail_smtp_auth_state(ngx_event_t *rev);

// 解析SMTP命令
ngx_int_t ngx_mail_smtp_parse_command(ngx_mail_session_t *s);


// SMTP模块声明
extern ngx_module_t  ngx_mail_smtp_module;


#endif /* _NGX_MAIL_SMTP_MODULE_H_INCLUDED_ */
