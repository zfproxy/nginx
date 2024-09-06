
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_STREAM_VARIABLES_H_INCLUDED_
#define _NGX_STREAM_VARIABLES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>


/* 定义流模块变量值类型,与核心变量值类型相同 */
typedef ngx_variable_value_t  ngx_stream_variable_value_t;

/* 定义流模块变量的宏,用于初始化变量 */
#define ngx_stream_variable(v)     { sizeof(v) - 1, 1, 0, 0, 0, (u_char *) v }

/* 定义流模块变量结构体类型 */
typedef struct ngx_stream_variable_s  ngx_stream_variable_t;

/* 定义设置变量值的函数指针类型 */
typedef void (*ngx_stream_set_variable_pt) (ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data);

/* 定义获取变量值的函数指针类型 */
typedef ngx_int_t (*ngx_stream_get_variable_pt) (ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data);


/* 定义变量的各种标志 */
#define NGX_STREAM_VAR_CHANGEABLE   1   /* 变量值可以被改变 */
#define NGX_STREAM_VAR_NOCACHEABLE  2   /* 变量值不可被缓存 */
#define NGX_STREAM_VAR_INDEXED      4   /* 变量可以被索引 */
#define NGX_STREAM_VAR_NOHASH       8   /* 变量不需要被哈希 */
#define NGX_STREAM_VAR_WEAK         16  /* 弱引用变量 */
#define NGX_STREAM_VAR_PREFIX       32  /* 变量是前缀 */


/* 定义流模块变量结构体 */
struct ngx_stream_variable_s {
    ngx_str_t                     name;   /* 变量名，必须是第一个成员以便构建哈希 */
    ngx_stream_set_variable_pt    set_handler;  /* 设置变量值的处理函数 */
    ngx_stream_get_variable_pt    get_handler;  /* 获取变量值的处理函数 */
    uintptr_t                     data;   /* 与变量相关的数据 */
    ngx_uint_t                    flags;  /* 变量标志 */
    ngx_uint_t                    index;  /* 变量索引 */
};

/* 定义空变量的宏 */
#define ngx_stream_null_variable  { ngx_null_string, NULL, NULL, 0, 0, 0 }


/**
 * @brief 添加一个新的流模块变量
 * @param cf 配置上下文
 * @param name 变量名
 * @param flags 变量标志
 * @return 返回新添加的变量指针，如果失败则返回NULL
 */
ngx_stream_variable_t *ngx_stream_add_variable(ngx_conf_t *cf, ngx_str_t *name,
    ngx_uint_t flags);

/**
 * @brief 获取变量的索引
 * @param cf 配置上下文
 * @param name 变量名
 * @return 返回变量的索引，如果失败则返回NGX_ERROR
 */
ngx_int_t ngx_stream_get_variable_index(ngx_conf_t *cf, ngx_str_t *name);

/**
 * @brief 获取已索引的变量值
 * @param s 流会话
 * @param index 变量索引
 * @return 返回变量值，如果未找到则返回NULL
 */
ngx_stream_variable_value_t *ngx_stream_get_indexed_variable(
    ngx_stream_session_t *s, ngx_uint_t index);

/**
 * @brief 获取已刷新的变量值
 * @param s 流会话
 * @param index 变量索引
 * @return 返回刷新后的变量值，如果未找到则返回NULL
 */
ngx_stream_variable_value_t *ngx_stream_get_flushed_variable(
    ngx_stream_session_t *s, ngx_uint_t index);

/**
 * @brief 根据名称获取变量值
 * @param s 流会话
 * @param name 变量名
 * @param key 变量的哈希键值
 * @return 返回变量值，如果未找到则返回NULL
 */
ngx_stream_variable_value_t *ngx_stream_get_variable(ngx_stream_session_t *s,
    ngx_str_t *name, ngx_uint_t key);


#if (NGX_PCRE)

/**
 * @brief 流模块正则表达式变量结构体
 */
typedef struct {
    ngx_uint_t                    capture;    /**< 捕获组的索引 */
    ngx_int_t                     index;      /**< 变量的索引 */
} ngx_stream_regex_variable_t;


/**
 * @brief 流模块正则表达式结构体
 */
typedef struct {
    ngx_regex_t                  *regex;      /**< 指向编译后的正则表达式对象 */
    ngx_uint_t                    ncaptures;  /**< 捕获组的数量 */
    ngx_stream_regex_variable_t  *variables;  /**< 指向正则表达式变量数组 */
    ngx_uint_t                    nvariables; /**< 变量的数量 */
    ngx_str_t                     name;       /**< 正则表达式的名称 */
} ngx_stream_regex_t;


/**
 * @brief 流模块映射正则表达式结构体
 */
typedef struct {
    ngx_stream_regex_t           *regex;      /**< 指向正则表达式结构体 */
    void                         *value;      /**< 与正则表达式匹配的值 */
} ngx_stream_map_regex_t;


/**
 * @brief 编译流模块的正则表达式
 * @param cf 配置结构体指针
 * @param rc 正则表达式编译结构体指针
 * @return 返回编译后的流模块正则表达式结构体指针，如果编译失败则返回NULL
 */
ngx_stream_regex_t *ngx_stream_regex_compile(ngx_conf_t *cf,
    ngx_regex_compile_t *rc);

/**
 * @brief 执行流模块的正则表达式匹配
 * @param s 流会话指针
 * @param re 流模块正则表达式结构体指针
 * @param str 要匹配的字符串
 * @return 返回匹配结果，0表示匹配成功，非0表示匹配失败
 */
ngx_int_t ngx_stream_regex_exec(ngx_stream_session_t *s, ngx_stream_regex_t *re,
    ngx_str_t *str);

#endif


/**
 * @brief 流模块映射结构体
 */
typedef struct {
    ngx_hash_combined_t           hash;       /**< 组合哈希表，用于快速查找 */
#if (NGX_PCRE)
    ngx_stream_map_regex_t       *regex;      /**< 正则表达式映射数组 */
    ngx_uint_t                    nregex;     /**< 正则表达式映射数量 */
#endif
} ngx_stream_map_t;

/**
 * @brief 在映射中查找匹配项
 * @param s 流会话指针
 * @param map 映射结构体指针
 * @param match 要匹配的字符串
 * @return 返回找到的值，如果未找到则返回NULL
 */
void *ngx_stream_map_find(ngx_stream_session_t *s, ngx_stream_map_t *map,
    ngx_str_t *match);

/**
 * @brief 添加核心变量到流模块
 * @param cf 配置结构体指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_stream_variables_add_core_vars(ngx_conf_t *cf);

/**
 * @brief 初始化流模块变量
 * @param cf 配置结构体指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_stream_variables_init_vars(ngx_conf_t *cf);

/**
 * @brief 空值变量
 */
extern ngx_stream_variable_value_t  ngx_stream_variable_null_value;

/**
 * @brief 真值变量
 */
extern ngx_stream_variable_value_t  ngx_stream_variable_true_value;


#endif /* _NGX_STREAM_VARIABLES_H_INCLUDED_ */
