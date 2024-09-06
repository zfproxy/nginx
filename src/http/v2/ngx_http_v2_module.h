
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 */


#ifndef _NGX_HTTP_V2_MODULE_H_INCLUDED_
#define _NGX_HTTP_V2_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


// HTTP/2主配置结构体
typedef struct {
    size_t                          recv_buffer_size;  // 接收缓冲区大小
    u_char                         *recv_buffer;       // 接收缓冲区指针
} ngx_http_v2_main_conf_t;


// HTTP/2位置配置结构体
typedef struct {
    size_t                          chunk_size;        // 数据块大小
} ngx_http_v2_loc_conf_t;


#endif /* _NGX_HTTP_V2_MODULE_H_INCLUDED_ */
