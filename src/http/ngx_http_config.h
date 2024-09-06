
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_CONFIG_H_INCLUDED_
#define _NGX_HTTP_CONFIG_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/*
 * 定义HTTP配置上下文结构体
 * 这个结构体用于存储HTTP模块的各级配置信息
 * 包括主配置、服务器配置和位置配置
 */
typedef struct {
    void        **main_conf;  /* HTTP主配置指针数组 */
    void        **srv_conf;   /* HTTP服务器配置指针数组 */
    void        **loc_conf;   /* HTTP位置配置指针数组 */
} ngx_http_conf_ctx_t;


/*
 * 定义HTTP模块结构体
 * 这个结构体包含了HTTP模块的各种配置和初始化函数指针
 * 每个HTTP模块都需要实现这个结构体来定义其行为和配置方法
 */
typedef struct {
    /* 预配置函数，在创建和初始化配置之前调用 */
    ngx_int_t   (*preconfiguration)(ngx_conf_t *cf);
    
    /* 后配置函数，在所有模块配置完成后调用 */
    ngx_int_t   (*postconfiguration)(ngx_conf_t *cf);

    /* 创建主配置结构体的函数 */
    void       *(*create_main_conf)(ngx_conf_t *cf);
    
    /* 初始化主配置的函数 */
    char       *(*init_main_conf)(ngx_conf_t *cf, void *conf);

    /* 创建服务器配置结构体的函数 */
    void       *(*create_srv_conf)(ngx_conf_t *cf);
    
    /* 合并服务器配置的函数，用于继承上层配置 */
    char       *(*merge_srv_conf)(ngx_conf_t *cf, void *prev, void *conf);

    /* 创建位置配置结构体的函数 */
    void       *(*create_loc_conf)(ngx_conf_t *cf);
    
    /* 合并位置配置的函数，用于继承上层配置 */
    char       *(*merge_loc_conf)(ngx_conf_t *cf, void *prev, void *conf);
} ngx_http_module_t;


/**
 * @brief 定义HTTP模块的标识符
 *
 * 这个宏定义了HTTP模块的唯一标识符。
 * 值0x50545448是ASCII字符串"HTTP"的十六进制表示:
 * 'H' = 0x48
 * 'T' = 0x54
 * 'T' = 0x54
 * 'P' = 0x50
 * 
 * 这个标识符用于在Nginx的模块系统中识别和区分HTTP模块。
 * 它可能在模块的初始化、配置解析和其他内部操作中被使用。
 */
#define NGX_HTTP_MODULE           0x50545448   /* "HTTP" */

/**
 * @brief 定义HTTP主配置标志
 *
 * 这个宏定义了HTTP主配置的标志位。
 * 值0x02000000表示二进制中的第26位被设置。
 * 
 * 这个标志用于:
 * 1. 标识配置指令属于HTTP主配置块
 * 2. 在解析配置时确定当前上下文
 * 3. 在模块开发中指定配置项的作用域
 *
 * 通过位运算，可以快速检查和设置配置的类型。
 */
#define NGX_HTTP_MAIN_CONF        0x02000000
/**
 * @brief 定义HTTP服务器配置标志
 *
 * 这个宏定义了HTTP服务器配置的标志位。
 * 值0x04000000表示二进制中的第27位被设置。
 * 
 * 这个标志用于:
 * 1. 标识配置指令属于HTTP服务器配置块
 * 2. 在解析配置时确定当前上下文是服务器级别
 * 3. 在模块开发中指定配置项适用于服务器级别的作用域
 *
 * 通过位运算，可以快速检查和设置配置的类型。
 * 这有助于Nginx在处理配置时正确识别和应用服务器级别的设置。
 */
#define NGX_HTTP_SRV_CONF         0x04000000
/**
 * @brief 定义HTTP位置配置标志
 *
 * 这个宏定义了HTTP位置配置的标志位。
 * 值0x08000000表示二进制中的第28位被设置。
 * 
 * 这个标志用于:
 * 1. 标识配置指令属于HTTP位置配置块
 * 2. 在解析配置时确定当前上下文是位置级别
 * 3. 在模块开发中指定配置项适用于位置级别的作用域
 *
 * 通过位运算，可以快速检查和设置配置的类型。
 * 这有助于Nginx在处理配置时正确识别和应用位置级别的设置，
 * 例如在处理特定URI路径的配置时。
 */
#define NGX_HTTP_LOC_CONF         0x08000000
/**
 * @brief 定义HTTP上游配置标志
 *
 * 这个宏定义了HTTP上游配置的标志位。
 * 值0x10000000表示二进制中的第29位被设置。
 * 
 * 这个标志用于:
 * 1. 标识配置指令属于HTTP上游配置块
 * 2. 在解析配置时确定当前上下文是上游服务器级别
 * 3. 在模块开发中指定配置项适用于上游服务器的作用域
 *
 * 通过位运算，可以快速检查和设置配置的类型。
 * 这有助于Nginx在处理配置时正确识别和应用上游服务器的设置，
 * 例如在配置负载均衡或反向代理时。
 */
#define NGX_HTTP_UPS_CONF         0x10000000
/**
 * @brief 定义HTTP服务器内部if配置标志
 *
 * 这个宏定义了HTTP服务器内部if配置的标志位。
 * 值0x20000000表示二进制中的第30位被设置。
 * 
 * 这个标志用于:
 * 1. 标识配置指令属于HTTP服务器内部if配置块
 * 2. 在解析配置时确定当前上下文是服务器内部if级别
 * 3. 在模块开发中指定配置项适用于服务器内部if条件的作用域
 *
 * 通过位运算，可以快速检查和设置配置的类型。
 * 这有助于Nginx在处理配置时正确识别和应用服务器内部if条件的设置，
 * 例如在服务器配置中使用if指令时。
 */
#define NGX_HTTP_SIF_CONF         0x20000000
/**
 * @brief 定义HTTP位置内部if配置标志
 *
 * 这个宏定义了HTTP位置内部if配置的标志位。
 * 值0x40000000表示二进制中的第31位被设置。
 * 
 * 这个标志用于:
 * 1. 标识配置指令属于HTTP位置内部if配置块
 * 2. 在解析配置时确定当前上下文是位置内部if级别
 * 3. 在模块开发中指定配置项适用于位置内部if条件的作用域
 *
 * 通过位运算，可以快速检查和设置配置的类型。
 * 这有助于Nginx在处理配置时正确识别和应用位置内部if条件的设置，
 * 例如在location块内使用if指令时。
 */
#define NGX_HTTP_LIF_CONF         0x40000000
/**
 * @brief 定义HTTP限制配置标志
 *
 * 这个宏定义了HTTP限制配置的标志位。
 * 值0x80000000表示二进制中的第32位（最高位）被设置。
 * 
 * 这个标志用于:
 * 1. 标识配置指令属于HTTP限制配置块
 * 2. 在解析配置时确定当前上下文是限制级别
 * 3. 在模块开发中指定配置项适用于限制条件的作用域
 *
 * 通过位运算，可以快速检查和设置配置的类型。
 * 这有助于Nginx在处理配置时正确识别和应用限制条件的设置，
 * 例如在配置请求速率限制或连接数限制时。
 */
#define NGX_HTTP_LMT_CONF         0x80000000


/*
 * 定义HTTP主配置的偏移量
 * 使用offsetof宏获取ngx_http_conf_ctx_t结构体中main_conf成员的偏移量
 * 这个宏用于在运行时快速定位和访问HTTP主配置
 */
#define NGX_HTTP_MAIN_CONF_OFFSET  offsetof(ngx_http_conf_ctx_t, main_conf)
/*
 * 定义HTTP服务器配置的偏移量
 * 使用offsetof宏获取ngx_http_conf_ctx_t结构体中srv_conf成员的偏移量
 * 这个宏用于在运行时快速定位和访问HTTP服务器级别的配置
 */
#define NGX_HTTP_SRV_CONF_OFFSET   offsetof(ngx_http_conf_ctx_t, srv_conf)
/*
 * 定义HTTP位置配置的偏移量
 * 使用offsetof宏获取ngx_http_conf_ctx_t结构体中loc_conf成员的偏移量
 * 这个宏用于在运行时快速定位和访问HTTP位置级别的配置
 */
#define NGX_HTTP_LOC_CONF_OFFSET   offsetof(ngx_http_conf_ctx_t, loc_conf)


/*
 * 获取HTTP请求的主配置
 * 参数:
 *   r: HTTP请求结构体指针
 *   module: 模块结构体
 * 返回值:
 *   指向特定模块的主配置的指针
 * 说明:
 *   通过请求结构体和模块的上下文索引，获取该模块的主配置
 */
#define ngx_http_get_module_main_conf(r, module)                             \
    (r)->main_conf[module.ctx_index]
/*
 * 获取HTTP请求的服务器配置
 * 参数:
 *   r: HTTP请求结构体指针
 *   module: 模块结构体
 * 返回值:
 *   指向特定模块的服务器配置的指针
 * 说明:
 *   通过请求结构体和模块的上下文索引，获取该模块的服务器级别配置
 */
#define ngx_http_get_module_srv_conf(r, module)  (r)->srv_conf[module.ctx_index]
/*
 * 获取HTTP请求的局部配置
 * 参数:
 *   r: HTTP请求结构体指针
 *   module: 模块结构体
 * 返回值:
 *   指向特定模块的局部配置的指针
 * 说明:
 *   通过请求结构体和模块的上下文索引，获取该模块的局部配置
 */
#define ngx_http_get_module_loc_conf(r, module)  (r)->loc_conf[module.ctx_index]


/*
 * 获取HTTP模块的主配置
 * 参数:
 *   cf: 配置结构体指针
 *   module: 模块结构体
 * 返回值:
 *   指向特定模块的主配置的指针
 * 说明:
 *   通过配置结构体和模块的上下文索引，获取该模块的主配置
 */
#define ngx_http_conf_get_module_main_conf(cf, module)                        \
    ((ngx_http_conf_ctx_t *) cf->ctx)->main_conf[module.ctx_index]

/*
 * 获取HTTP模块的服务器配置
 * 参数:
 *   cf: 配置结构体指针
 *   module: 模块结构体
 * 返回值:
 *   指向特定模块的服务器配置的指针
 * 说明:
 *   通过配置结构体和模块的上下文索引，获取该模块的服务器级别配置
 */
#define ngx_http_conf_get_module_srv_conf(cf, module)                         \
    ((ngx_http_conf_ctx_t *) cf->ctx)->srv_conf[module.ctx_index]
/*
 * 获取HTTP模块的局部配置
 * 参数:
 *   cf: 配置结构体指针
 *   module: 模块结构体
 * 返回值:
 *   指向特定模块的局部配置的指针
 * 说明:
 *   通过配置结构体和模块的上下文索引，获取该模块的局部配置
 */
#define ngx_http_conf_get_module_loc_conf(cf, module)                         \
    ((ngx_http_conf_ctx_t *) cf->ctx)->loc_conf[module.ctx_index]

/*
 * 获取HTTP模块在当前运行周期中的主配置
 * 参数:
 *   cycle: 当前运行周期结构体指针
 *   module: 模块结构体
 * 返回值:
 *   指向特定模块在当前运行周期中的主配置的指针，如果不存在则返回NULL
 * 说明:
 *   通过当前运行周期结构体和模块的上下文索引，获取该模块在当前运行周期中的主配置
 */
#define ngx_http_cycle_get_module_main_conf(cycle, module)                    \
    (cycle->conf_ctx[ngx_http_module.index] ?                                 \
        ((ngx_http_conf_ctx_t *) cycle->conf_ctx[ngx_http_module.index])      \
            ->main_conf[module.ctx_index]:                                    \
        NULL)


#endif /* _NGX_HTTP_CONFIG_H_INCLUDED_ */
