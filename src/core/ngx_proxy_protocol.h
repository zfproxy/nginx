
/*
 * Copyright (C) Roman Arutyunyan
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PROXY_PROTOCOL_H_INCLUDED_
#define _NGX_PROXY_PROTOCOL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* PROXY协议v1版本的最大头部长度 */
#define NGX_PROXY_PROTOCOL_V1_MAX_HEADER  107

/* PROXY协议的最大头部长度（适用于v1和v2版本） */
#define NGX_PROXY_PROTOCOL_MAX_HEADER     4096


/* PROXY协议信息结构体 */
struct ngx_proxy_protocol_s {
    ngx_str_t           src_addr;  /* 源地址 */
    ngx_str_t           dst_addr;  /* 目标地址 */
    in_port_t           src_port;  /* 源端口 */
    in_port_t           dst_port;  /* 目标端口 */
    ngx_str_t           tlvs;      /* TLV（类型-长度-值）扩展字段 */
};


/* 读取PROXY协议头部信息 */
u_char *ngx_proxy_protocol_read(ngx_connection_t *c, u_char *buf,
    u_char *last);

/* 写入PROXY协议头部信息 */
u_char *ngx_proxy_protocol_write(ngx_connection_t *c, u_char *buf,
    u_char *last);

/* 获取PROXY协议中的TLV值 */
ngx_int_t ngx_proxy_protocol_get_tlv(ngx_connection_t *c, ngx_str_t *name,
    ngx_str_t *value);


#endif /* _NGX_PROXY_PROTOCOL_H_INCLUDED_ */
