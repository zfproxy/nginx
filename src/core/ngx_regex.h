
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_REGEX_H_INCLUDED_
#define _NGX_REGEX_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#if (NGX_PCRE2)

// 定义PCRE2的代码单元宽度为8位
#define PCRE2_CODE_UNIT_WIDTH  8
#include <pcre2.h>

// 定义未匹配的错误码
#define NGX_REGEX_NO_MATCHED   PCRE2_ERROR_NOMATCH   /* -1 */

// 使用PCRE2的pcre2_code作为ngx_regex_t类型
typedef pcre2_code  ngx_regex_t;

#else

#include <pcre.h>

// 定义未匹配的错误码
#define NGX_REGEX_NO_MATCHED   PCRE_ERROR_NOMATCH    /* -1 */

// 定义ngx_regex_t结构体，包含PCRE的代码和额外数据
typedef struct {
    pcre        *code;
    pcre_extra  *extra;
} ngx_regex_t;

#endif


// 定义正则表达式选项
#define NGX_REGEX_CASELESS     0x00000001  // 不区分大小写
#define NGX_REGEX_MULTILINE    0x00000002  // 多行模式


// 定义正则表达式编译结构体
typedef struct {
    ngx_str_t     pattern;      // 正则表达式模式
    ngx_pool_t   *pool;         // 内存池
    ngx_uint_t    options;      // 编译选项

    ngx_regex_t  *regex;        // 编译后的正则表达式
    int           captures;     // 捕获组数量
    int           named_captures; // 命名捕获组数量
    int           name_size;    // 命名捕获组名称大小
    u_char       *names;        // 命名捕获组名称
    ngx_str_t     err;          // 错误信息
} ngx_regex_compile_t;


// 定义正则表达式元素结构体
typedef struct {
    ngx_regex_t  *regex;        // 正则表达式
    u_char       *name;         // 名称
} ngx_regex_elt_t;


// 初始化正则表达式
void ngx_regex_init(void);
// 编译正则表达式
ngx_int_t ngx_regex_compile(ngx_regex_compile_t *rc);

// 执行正则表达式匹配
ngx_int_t ngx_regex_exec(ngx_regex_t *re, ngx_str_t *s, int *captures,
    ngx_uint_t size);

#if (NGX_PCRE2)
#define ngx_regex_exec_n       "pcre2_match()"
#else
#define ngx_regex_exec_n       "pcre_exec()"
#endif

// 执行正则表达式数组匹配
ngx_int_t ngx_regex_exec_array(ngx_array_t *a, ngx_str_t *s, ngx_log_t *log);


#endif /* _NGX_REGEX_H_INCLUDED_ */
