
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_SSI_FILTER_H_INCLUDED_
#define _NGX_HTTP_SSI_FILTER_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/* SSI命令的最大参数数量 */
#define NGX_HTTP_SSI_MAX_PARAMS       16

/* SSI命令的最大长度 */
#define NGX_HTTP_SSI_COMMAND_LEN      32
/* SSI参数的最大长度 */
#define NGX_HTTP_SSI_PARAM_LEN        32
/* 预分配的SSI参数数量 */
#define NGX_HTTP_SSI_PARAMS_N         4


/* SSI条件指令：IF */
#define NGX_HTTP_SSI_COND_IF          1
/* SSI条件指令：ELSE */
#define NGX_HTTP_SSI_COND_ELSE        2


/* SSI编码类型：无编码 */
#define NGX_HTTP_SSI_NO_ENCODING      0
/* SSI编码类型：URL编码 */
#define NGX_HTTP_SSI_URL_ENCODING     1
/* SSI编码类型：实体编码 */
#define NGX_HTTP_SSI_ENTITY_ENCODING  2

/* SSI主配置结构体 */
typedef struct {
    ngx_hash_t                hash;        /* 用于存储SSI命令的哈希表 */
    ngx_hash_keys_arrays_t    commands;    /* 用于初始化哈希表的键值数组 */
} ngx_http_ssi_main_conf_t;


/* SSI上下文结构体 */
typedef struct {
    ngx_buf_t                *buf;         /* 用于存储处理中的数据的缓冲区 */

    u_char                   *pos;         /* 当前处理位置 */
    u_char                   *copy_start;  /* 复制开始位置 */
    u_char                   *copy_end;    /* 复制结束位置 */

    ngx_uint_t                key;         /* 当前命令的哈希键 */
    ngx_str_t                 command;     /* 当前SSI命令 */
    ngx_array_t               params;      /* 命令参数数组 */
    ngx_table_elt_t          *param;       /* 当前参数 */
    ngx_table_elt_t           params_array[NGX_HTTP_SSI_PARAMS_N];  /* 预分配的参数数组 */

    ngx_chain_t              *in;          /* 输入链 */
    ngx_chain_t              *out;         /* 输出链 */
    ngx_chain_t             **last_out;    /* 指向输出链最后一个元素的指针 */
    ngx_chain_t              *busy;        /* 正在使用的缓冲区链 */
    ngx_chain_t              *free;        /* 空闲缓冲区链 */

    ngx_uint_t                state;       /* 当前状态 */
    ngx_uint_t                saved_state; /* 保存的状态 */
    size_t                    saved;       /* 已保存的数据大小 */
    size_t                    looked;      /* 已查看的数据大小 */

    size_t                    value_len;   /* 值的长度 */

    ngx_list_t               *variables;   /* SSI变量列表 */
    ngx_array_t              *blocks;      /* SSI块数组 */

#if (NGX_PCRE)
    ngx_uint_t                ncaptures;   /* 正则表达式捕获数量 */
    int                      *captures;    /* 正则表达式捕获结果 */
    u_char                   *captures_data; /* 正则表达式捕获数据 */
#endif

    unsigned                  shared:1;    /* 是否共享 */
    unsigned                  conditional:2; /* 条件状态 */
    unsigned                  encoding:2;  /* 编码类型 */
    unsigned                  block:1;     /* 是否在块内 */
    unsigned                  output:1;    /* 是否输出 */
    unsigned                  output_chosen:1; /* 是否已选择输出 */

    ngx_http_request_t       *wait;        /* 等待的请求 */
    void                     *value_buf;   /* 值缓冲区 */
    ngx_str_t                 timefmt;     /* 时间格式 */
    ngx_str_t                 errmsg;      /* 错误消息 */
} ngx_http_ssi_ctx_t;


/* SSI命令处理函数的函数指针类型定义 */
typedef ngx_int_t (*ngx_http_ssi_command_pt) (ngx_http_request_t *r,
    ngx_http_ssi_ctx_t *ctx, ngx_str_t **);


/* SSI参数结构体 */
typedef struct {
    ngx_str_t                 name;        /* 参数名称 */
    ngx_uint_t                index;       /* 参数索引 */

    unsigned                  mandatory:1; /* 是否为必需参数 */
    unsigned                  multiple:1;  /* 是否允许多次出现 */
} ngx_http_ssi_param_t;


/* SSI命令结构体 */
typedef struct {
    ngx_str_t                 name;        /* 命令名称 */
    ngx_http_ssi_command_pt   handler;     /* 命令处理函数 */
    ngx_http_ssi_param_t     *params;      /* 命令参数列表 */

    unsigned                  conditional:2; /* 条件类型（0：非条件，1：if，2：elif，3：else） */
    unsigned                  block:1;     /* 是否为块命令 */
    unsigned                  flush:1;     /* 是否需要立即刷新输出 */
} ngx_http_ssi_command_t;


/* 声明SSI过滤模块 */
extern ngx_module_t  ngx_http_ssi_filter_module;


#endif /* _NGX_HTTP_SSI_FILTER_H_INCLUDED_ */
