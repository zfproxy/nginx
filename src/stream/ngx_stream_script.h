
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_STREAM_SCRIPT_H_INCLUDED_
#define _NGX_STREAM_SCRIPT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>


/**
 * @brief 流模块脚本引擎结构体
 */
typedef struct {
    u_char                       *ip;        /**< 指令指针 */
    u_char                       *pos;       /**< 当前位置指针 */
    ngx_stream_variable_value_t  *sp;        /**< 变量值指针 */

    ngx_str_t                     buf;       /**< 缓冲区 */
    ngx_str_t                     line;      /**< 行缓冲区 */

    unsigned                      flushed:1; /**< 是否已刷新标志 */
    unsigned                      skip:1;    /**< 是否跳过标志 */

    ngx_stream_session_t         *session;   /**< 流会话指针 */
} ngx_stream_script_engine_t;


/**
 * @brief 流模块脚本编译结构体
 */
typedef struct {
    ngx_conf_t                   *cf;        /**< 配置上下文 */
    ngx_str_t                    *source;    /**< 源代码字符串 */

    ngx_array_t                 **flushes;   /**< 刷新操作数组 */
    ngx_array_t                 **lengths;   /**< 长度操作数组 */
    ngx_array_t                 **values;    /**< 值操作数组 */

    ngx_uint_t                    variables; /**< 变量数量 */
    ngx_uint_t                    ncaptures; /**< 捕获组数量 */
    ngx_uint_t                    size;      /**< 大小 */

    void                         *main;      /**< 主要数据指针 */

    unsigned                      complete_lengths:1; /**< 长度操作是否完成 */
    unsigned                      complete_values:1;  /**< 值操作是否完成 */
    unsigned                      zero:1;             /**< 是否为零 */
    unsigned                      conf_prefix:1;      /**< 是否使用配置前缀 */
    unsigned                      root_prefix:1;      /**< 是否使用根前缀 */
} ngx_stream_script_compile_t;


/**
 * @brief 流模块复杂值结构体
 */
typedef struct {
    ngx_str_t                     value;         /**< 值字符串 */
    ngx_uint_t                   *flushes;       /**< 刷新操作数组 */
    void                         *lengths;       /**< 长度操作数组 */
    void                         *values;        /**< 值操作数组 */

    union {
        size_t                    size;          /**< 大小 */
    } u;
} ngx_stream_complex_value_t;


/**
 * @brief 流模块编译复杂值结构体
 */
typedef struct {
    ngx_conf_t                   *cf;            /**< 配置上下文 */
    ngx_str_t                    *value;         /**< 值字符串指针 */
    ngx_stream_complex_value_t   *complex_value; /**< 复杂值结构体指针 */

    unsigned                      zero:1;        /**< 是否为零标志 */
    unsigned                      conf_prefix:1; /**< 是否使用配置前缀标志 */
    unsigned                      root_prefix:1; /**< 是否使用根前缀标志 */
} ngx_stream_compile_complex_value_t;


/**
 * @brief 流模块脚本代码函数指针类型
 */
typedef void (*ngx_stream_script_code_pt) (ngx_stream_script_engine_t *e);

/**
 * @brief 流模块脚本长度代码函数指针类型
 */
typedef size_t (*ngx_stream_script_len_code_pt) (ngx_stream_script_engine_t *e);


/**
 * @brief 流模块脚本复制代码结构体
 */
typedef struct {
    ngx_stream_script_code_pt     code;  /**< 脚本代码函数指针 */
    uintptr_t                     len;   /**< 长度 */
} ngx_stream_script_copy_code_t;


/**
 * @brief 流模块脚本变量代码结构体
 */
typedef struct {
    ngx_stream_script_code_pt     code;  /**< 脚本代码函数指针 */
    uintptr_t                     index; /**< 索引 */
} ngx_stream_script_var_code_t;


/**
 * @brief 流模块脚本复制捕获代码结构体
 */
typedef struct {
    ngx_stream_script_code_pt     code;  /**< 脚本代码函数指针 */
    uintptr_t                     n;     /**< 捕获数量 */
} ngx_stream_script_copy_capture_code_t;


/**
 * @brief 流模块脚本完整名称代码结构体
 */
typedef struct {
    ngx_stream_script_code_pt     code;        /**< 脚本代码函数指针 */
    uintptr_t                     conf_prefix; /**< 配置前缀标志 */
} ngx_stream_script_full_name_code_t;


/**
 * @brief 刷新复杂值
 * @param s 流会话
 * @param val 复杂值结构体
 */
void ngx_stream_script_flush_complex_value(ngx_stream_session_t *s,
    ngx_stream_complex_value_t *val);

/**
 * @brief 计算复杂值
 * @param s 流会话
 * @param val 复杂值结构体
 * @param value 计算结果
 * @return ngx_int_t 计算结果状态
 */
ngx_int_t ngx_stream_complex_value(ngx_stream_session_t *s,
    ngx_stream_complex_value_t *val, ngx_str_t *value);

/**
 * @brief 计算复杂值的大小
 * @param s 流会话
 * @param val 复杂值结构体
 * @param default_value 默认值
 * @return size_t 计算结果大小
 */
size_t ngx_stream_complex_value_size(ngx_stream_session_t *s,
    ngx_stream_complex_value_t *val, size_t default_value);

/**
 * @brief 编译复杂值
 * @param ccv 复杂值编译结构体
 * @return ngx_int_t 编译结果状态
 */
ngx_int_t ngx_stream_compile_complex_value(
    ngx_stream_compile_complex_value_t *ccv);

/**
 * @brief 设置复杂值槽
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 设置结果
 */
char *ngx_stream_set_complex_value_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 设置复杂值为零的槽
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 设置结果
 */
char *ngx_stream_set_complex_value_zero_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 设置复杂值大小槽
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 设置结果
 */
char *ngx_stream_set_complex_value_size_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


/**
 * @brief 计算字符串中变量的数量
 * @param value 输入字符串
 * @return 变量数量
 */
ngx_uint_t ngx_stream_script_variables_count(ngx_str_t *value);

/**
 * @brief 编译脚本
 * @param sc 脚本编译上下文
 * @return 编译结果
 */
ngx_int_t ngx_stream_script_compile(ngx_stream_script_compile_t *sc);

/**
 * @brief 运行脚本
 * @param s 流会话
 * @param value 输入值
 * @param code_lengths 代码长度
 * @param reserved 保留参数
 * @param code_values 代码值
 * @return 运行结果
 */
u_char *ngx_stream_script_run(ngx_stream_session_t *s, ngx_str_t *value,
    void *code_lengths, size_t reserved, void *code_values);

/**
 * @brief 刷新不可缓存的变量
 * @param s 流会话
 * @param indices 索引数组
 */
void ngx_stream_script_flush_no_cacheable_variables(ngx_stream_session_t *s,
    ngx_array_t *indices);

/**
 * @brief 添加代码到数组
 * @param codes 代码数组
 * @param size 代码大小
 * @param code 代码指针
 * @return 添加的代码指针
 */
void *ngx_stream_script_add_code(ngx_array_t *codes, size_t size, void *code);

/**
 * @brief 获取复制长度代码的大小
 * @param e 脚本引擎
 * @return 代码大小
 */
size_t ngx_stream_script_copy_len_code(ngx_stream_script_engine_t *e);

/**
 * @brief 执行复制代码
 * @param e 脚本引擎
 */
void ngx_stream_script_copy_code(ngx_stream_script_engine_t *e);

/**
 * @brief 获取复制变量长度代码的大小
 * @param e 脚本引擎
 * @return 代码大小
 */
size_t ngx_stream_script_copy_var_len_code(ngx_stream_script_engine_t *e);

/**
 * @brief 执行复制变量代码
 * @param e 脚本引擎
 */
void ngx_stream_script_copy_var_code(ngx_stream_script_engine_t *e);

/**
 * @brief 获取复制捕获长度代码的大小
 * @param e 脚本引擎
 * @return 代码大小
 */
size_t ngx_stream_script_copy_capture_len_code(ngx_stream_script_engine_t *e);

/**
 * @brief 执行复制捕获代码
 * @param e 脚本引擎
 */
void ngx_stream_script_copy_capture_code(ngx_stream_script_engine_t *e);

#endif /* _NGX_STREAM_SCRIPT_H_INCLUDED_ */
