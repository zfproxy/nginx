
/*
 * Copyright (C) Roman Arutyunyan
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_V3_PARSE_H_INCLUDED_
#define _NGX_HTTP_V3_PARSE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


// 用于解析可变长度整数的结构体
typedef struct {
    ngx_uint_t                      state;  // 当前解析状态
    uint64_t                        value;  // 解析得到的值
} ngx_http_v3_parse_varlen_int_t;


// 用于解析前缀整数的结构体
typedef struct {
    ngx_uint_t                      state;  // 当前解析状态
    ngx_uint_t                      shift;  // 位移量
    uint64_t                        value;  // 解析得到的值
} ngx_http_v3_parse_prefix_int_t;


// 用于解析设置帧的结构体
typedef struct {
    ngx_uint_t                      state;  // 当前解析状态
    uint64_t                        id;     // 设置项ID
    ngx_http_v3_parse_varlen_int_t  vlint;  // 可变长度整数解析器
} ngx_http_v3_parse_settings_t;


// 用于解析字段部分前缀的结构体
typedef struct {
    ngx_uint_t                      state;       // 当前解析状态
    ngx_uint_t                      insert_count;// 插入计数
    ngx_uint_t                      delta_base;  // 基数增量
    ngx_uint_t                      sign;        // 符号
    ngx_uint_t                      base;        // 基数
    ngx_http_v3_parse_prefix_int_t  pint;        // 前缀整数解析器
} ngx_http_v3_parse_field_section_prefix_t;


// 用于解析字面值的结构体
typedef struct {
    ngx_uint_t                      state;    // 当前解析状态
    ngx_uint_t                      length;   // 字面值长度
    ngx_uint_t                      huffman;  // 是否使用Huffman编码
    ngx_str_t                       value;    // 解析得到的值
    u_char                         *last;     // 最后解析位置
    u_char                          huffstate;// Huffman解码状态
} ngx_http_v3_parse_literal_t;


// 用于解析字段的结构体
typedef struct {
    ngx_uint_t                      state;   // 当前解析状态
    ngx_uint_t                      index;   // 索引
    ngx_uint_t                      base;    // 基数
    ngx_uint_t                      dynamic; // 是否为动态表

    ngx_str_t                       name;    // 字段名
    ngx_str_t                       value;   // 字段值

    ngx_http_v3_parse_prefix_int_t  pint;    // 前缀整数解析器
    ngx_http_v3_parse_literal_t     literal; // 字面值解析器
} ngx_http_v3_parse_field_t;


// 用于解析字段表示的结构体
typedef struct {
    ngx_uint_t                      state; // 当前解析状态
    ngx_http_v3_parse_field_t       field; // 字段解析器
} ngx_http_v3_parse_field_rep_t;


// 用于解析头部的结构体
typedef struct {
    ngx_uint_t                      state;    // 当前解析状态
    ngx_uint_t                      type;     // 类型
    ngx_uint_t                      length;   // 长度
    ngx_http_v3_parse_varlen_int_t  vlint;    // 可变长度整数解析器
    ngx_http_v3_parse_field_section_prefix_t  prefix;   // 字段部分前缀解析器
    ngx_http_v3_parse_field_rep_t   field_rep;// 字段表示解析器
} ngx_http_v3_parse_headers_t;


// 用于解析编码器指令的结构体
typedef struct {
    ngx_uint_t                      state; // 当前解析状态
    ngx_http_v3_parse_field_t       field; // 字段解析器
    ngx_http_v3_parse_prefix_int_t  pint;  // 前缀整数解析器
} ngx_http_v3_parse_encoder_t;


// 用于解析解码器指令的结构体
typedef struct {
    ngx_uint_t                      state; // 当前解析状态
    ngx_http_v3_parse_prefix_int_t  pint;  // 前缀整数解析器
} ngx_http_v3_parse_decoder_t;


// 用于解析控制流的结构体
typedef struct {
    ngx_uint_t                      state;    // 当前解析状态
    ngx_uint_t                      type;     // 类型
    ngx_uint_t                      length;   // 长度
    ngx_http_v3_parse_varlen_int_t  vlint;    // 可变长度整数解析器
    ngx_http_v3_parse_settings_t    settings; // 设置解析器
} ngx_http_v3_parse_control_t;


// 用于解析单向流的结构体
typedef struct {
    ngx_uint_t                      state; // 当前解析状态
    ngx_http_v3_parse_varlen_int_t  vlint; // 可变长度整数解析器
    union {
        ngx_http_v3_parse_encoder_t  encoder; // 编码器解析器
        ngx_http_v3_parse_decoder_t  decoder; // 解码器解析器
        ngx_http_v3_parse_control_t  control; // 控制流解析器
    } u;
} ngx_http_v3_parse_uni_t;


// 用于解析数据的结构体
typedef struct {
    ngx_uint_t                      state;  // 当前解析状态
    ngx_uint_t                      type;   // 类型
    ngx_uint_t                      length; // 长度
    ngx_http_v3_parse_varlen_int_t  vlint;  // 可变长度整数解析器
} ngx_http_v3_parse_data_t;


/*
 * 解析函数返回码:
 *   NGX_DONE - 解析完成
 *   NGX_OK - 子元素解析完成
 *   NGX_AGAIN - 需要更多数据
 *   NGX_BUSY - 等待外部事件
 *   NGX_ERROR - 内部错误
 *   NGX_HTTP_V3_ERROR_XXX - HTTP/3 或 QPACK 错误
 */

// 解析头部
ngx_int_t ngx_http_v3_parse_headers(ngx_connection_t *c,
    ngx_http_v3_parse_headers_t *st, ngx_buf_t *b);
// 解析数据
ngx_int_t ngx_http_v3_parse_data(ngx_connection_t *c,
    ngx_http_v3_parse_data_t *st, ngx_buf_t *b);
// 解析单向流
ngx_int_t ngx_http_v3_parse_uni(ngx_connection_t *c,
    ngx_http_v3_parse_uni_t *st, ngx_buf_t *b);


#endif /* _NGX_HTTP_V3_PARSE_H_INCLUDED_ */
