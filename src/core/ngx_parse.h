
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PARSE_H_INCLUDED_
#define _NGX_PARSE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


// 解析大小字符串,返回字节数
ssize_t ngx_parse_size(ngx_str_t *line);

// 解析偏移量字符串,返回字节偏移
off_t ngx_parse_offset(ngx_str_t *line);

// 解析时间字符串,返回毫秒数或秒数
// is_sec为1时返回秒数,否则返回毫秒数
ngx_int_t ngx_parse_time(ngx_str_t *line, ngx_uint_t is_sec);


#endif /* _NGX_PARSE_H_INCLUDED_ */
