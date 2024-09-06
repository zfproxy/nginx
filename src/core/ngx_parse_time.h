
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PARSE_TIME_H_INCLUDED_
#define _NGX_PARSE_TIME_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * 解析HTTP时间字符串为time_t类型
 * @param value 时间字符串
 * @param len 字符串长度
 * @return 解析后的time_t时间值
 */
time_t ngx_parse_http_time(u_char *value, size_t len);

/* 兼容性定义，保留旧的函数名 */
#define ngx_http_parse_time(value, len)  ngx_parse_http_time(value, len)


#endif /* _NGX_PARSE_TIME_H_INCLUDED_ */
