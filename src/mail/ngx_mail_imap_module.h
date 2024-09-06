
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MAIL_IMAP_MODULE_H_INCLUDED_
#define _NGX_MAIL_IMAP_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_mail.h>


/**
 * @brief IMAP服务器配置结构体
 */
typedef struct {
    size_t       client_buffer_size;        /* 客户端缓冲区大小 */

    ngx_str_t    capability;                /* IMAP能力字符串 */
    ngx_str_t    starttls_capability;       /* 支持STARTTLS时的能力字符串 */
    ngx_str_t    starttls_only_capability;  /* 仅支持STARTTLS时的能力字符串 */

    ngx_uint_t   auth_methods;              /* 支持的认证方法 */

    ngx_array_t  capabilities;              /* 额外的IMAP能力列表 */
} ngx_mail_imap_srv_conf_t;

/**
 * @brief 初始化IMAP会话
 * @param s IMAP会话结构指针
 * @param c 连接结构指针
 */
void ngx_mail_imap_init_session(ngx_mail_session_t *s, ngx_connection_t *c);

/**
 * @brief 初始化IMAP协议
 * @param rev 读事件结构指针
 */
void ngx_mail_imap_init_protocol(ngx_event_t *rev);

/**
 * @brief 处理IMAP认证状态
 * @param rev 读事件结构指针
 */
void ngx_mail_imap_auth_state(ngx_event_t *rev);

/**
 * @brief 解析IMAP命令
 * @param s IMAP会话结构指针
 * @return 解析结果，成功返回NGX_OK，失败返回错误码
 */
ngx_int_t ngx_mail_imap_parse_command(ngx_mail_session_t *s);

/**
 * @brief IMAP模块声明
 */
extern ngx_module_t  ngx_mail_imap_module;


#endif /* _NGX_MAIL_IMAP_MODULE_H_INCLUDED_ */
