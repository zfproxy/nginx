
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MAIL_POP3_MODULE_H_INCLUDED_
#define _NGX_MAIL_POP3_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_mail.h>


// POP3服务器配置结构体
typedef struct {
    ngx_str_t    capability;                // 基本能力字符串
    ngx_str_t    starttls_capability;       // STARTTLS能力字符串
    ngx_str_t    starttls_only_capability;  // 仅STARTTLS能力字符串
    ngx_str_t    auth_capability;           // 认证能力字符串

    ngx_uint_t   auth_methods;              // 支持的认证方法

    ngx_array_t  capabilities;              // 额外的能力列表
} ngx_mail_pop3_srv_conf_t;


// 初始化POP3会话
void ngx_mail_pop3_init_session(ngx_mail_session_t *s, ngx_connection_t *c);

// 初始化POP3协议
void ngx_mail_pop3_init_protocol(ngx_event_t *rev);

// 处理POP3认证状态
void ngx_mail_pop3_auth_state(ngx_event_t *rev);

// 解析POP3命令
ngx_int_t ngx_mail_pop3_parse_command(ngx_mail_session_t *s);


// POP3模块声明
extern ngx_module_t  ngx_mail_pop3_module;


#endif /* _NGX_MAIL_POP3_MODULE_H_INCLUDED_ */
