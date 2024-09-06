
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Maxim Dounin
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MODULE_H_INCLUDED_
#define _NGX_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx.h>


/* 定义未设置模块索引的值
 * 将其设置为无符号整型的最大值，通常用于表示模块索引未被初始化或设置
 */
#define NGX_MODULE_UNSET_INDEX  (ngx_uint_t) -1


/* 以下是一系列用于构建模块签名的宏定义 */

/* 定义基本的系统信息，包括指针大小、原子操作大小和时间类型大小 */
#define NGX_MODULE_SIGNATURE_0                                                \
    ngx_value(NGX_PTR_SIZE) ","                                               \
    ngx_value(NGX_SIG_ATOMIC_T_SIZE) ","                                      \
    ngx_value(NGX_TIME_T_SIZE) ","

/* 检查是否支持kqueue */
#if (NGX_HAVE_KQUEUE)
#define NGX_MODULE_SIGNATURE_1   "1"
#else
#define NGX_MODULE_SIGNATURE_1   "0"
#endif

/* 检查是否支持IOCP */
#if (NGX_HAVE_IOCP)
#define NGX_MODULE_SIGNATURE_2   "1"
#else
#define NGX_MODULE_SIGNATURE_2   "0"
#endif

/* 检查是否支持文件AIO或兼容模式 */
#if (NGX_HAVE_FILE_AIO || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_3   "1"
#else
#define NGX_MODULE_SIGNATURE_3   "0"
#endif

/* 检查是否支持无磁盘I/O的sendfile或兼容模式 */
#if (NGX_HAVE_SENDFILE_NODISKIO || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_4   "1"
#else
#define NGX_MODULE_SIGNATURE_4   "0"
#endif

/* 检查是否支持eventfd */
#if (NGX_HAVE_EVENTFD)
#define NGX_MODULE_SIGNATURE_5   "1"
#else
#define NGX_MODULE_SIGNATURE_5   "0"
#endif

/* 检查是否支持epoll */
#if (NGX_HAVE_EPOLL)
#define NGX_MODULE_SIGNATURE_6   "1"
#else
#define NGX_MODULE_SIGNATURE_6   "0"
#endif

/* 检查是否支持可调节的keepalive */
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
#define NGX_MODULE_SIGNATURE_7   "1"
#else
#define NGX_MODULE_SIGNATURE_7   "0"
#endif

/* 检查是否支持IPv6 */
#if (NGX_HAVE_INET6)
#define NGX_MODULE_SIGNATURE_8   "1"
#else
#define NGX_MODULE_SIGNATURE_8   "0"
#endif

/* 这两个签名始终为1，可能表示某些始终支持的功能 */
#define NGX_MODULE_SIGNATURE_9   "1"
#define NGX_MODULE_SIGNATURE_10  "1"

/* 检查是否支持延迟接受连接和SO_ACCEPTFILTER */
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
#define NGX_MODULE_SIGNATURE_11  "1"
#else
#define NGX_MODULE_SIGNATURE_11  "0"
#endif

/* 这个签名始终为1，可能表示某个始终支持的功能 */
#define NGX_MODULE_SIGNATURE_12  "1"

/* 检查是否支持setfib */
#if (NGX_HAVE_SETFIB)
#define NGX_MODULE_SIGNATURE_13  "1"
#else
#define NGX_MODULE_SIGNATURE_13  "0"
#endif

/* 检查是否支持TCP快速打开 */
#if (NGX_HAVE_TCP_FASTOPEN)
#define NGX_MODULE_SIGNATURE_14  "1"
#else
#define NGX_MODULE_SIGNATURE_14  "0"
#endif

/* 检查是否支持Unix域套接字 */
#if (NGX_HAVE_UNIX_DOMAIN)
#define NGX_MODULE_SIGNATURE_15  "1"
#else
#define NGX_MODULE_SIGNATURE_15  "0"
#endif

/* 检查是否支持可变参数宏 */
#if (NGX_HAVE_VARIADIC_MACROS)
#define NGX_MODULE_SIGNATURE_16  "1"
#else
#define NGX_MODULE_SIGNATURE_16  "0"
#endif

/* 这个签名始终为0，可能表示某个未使用或保留的功能 */
#define NGX_MODULE_SIGNATURE_17  "0"

/* 检查是否支持QUIC或兼容模式 */
#if (NGX_QUIC || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_18  "1"
#else
#define NGX_MODULE_SIGNATURE_18  "0"
#endif

/* 检查是否支持openat函数 */
#if (NGX_HAVE_OPENAT)
#define NGX_MODULE_SIGNATURE_19  "1"
#else
#define NGX_MODULE_SIGNATURE_19  "0"
#endif

/* 检查是否支持原子操作 */
#if (NGX_HAVE_ATOMIC_OPS)
#define NGX_MODULE_SIGNATURE_20  "1"
#else
#define NGX_MODULE_SIGNATURE_20  "0"
#endif

/* 检查是否支持POSIX信号量 */
#if (NGX_HAVE_POSIX_SEM)
#define NGX_MODULE_SIGNATURE_21  "1"
#else
#define NGX_MODULE_SIGNATURE_21  "0"
#endif

/* 检查是否支持线程或兼容模式 */
#if (NGX_THREADS || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_22  "1"
#else
#define NGX_MODULE_SIGNATURE_22  "0"
#endif

/* 检查是否支持PCRE */
#if (NGX_PCRE)
#define NGX_MODULE_SIGNATURE_23  "1"
#else
#define NGX_MODULE_SIGNATURE_23  "0"
#endif

/* 检查是否支持HTTP SSL或兼容模式 */
#if (NGX_HTTP_SSL || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_24  "1"
#else
#define NGX_MODULE_SIGNATURE_24  "0"
#endif

/* 这个签名始终为1，可能表示某个始终支持的功能 */
#define NGX_MODULE_SIGNATURE_25  "1"

/* 检查是否支持HTTP GZIP压缩 */
#if (NGX_HTTP_GZIP)
#define NGX_MODULE_SIGNATURE_26  "1"
#else
#define NGX_MODULE_SIGNATURE_26  "0"
#endif

/* 这个签名始终为1，可能表示某个始终支持的功能 */
#define NGX_MODULE_SIGNATURE_27  "1"

/* 检查是否支持X-Forwarded-For头部 */
#if (NGX_HTTP_X_FORWARDED_FOR)
#define NGX_MODULE_SIGNATURE_28  "1"
#else
#define NGX_MODULE_SIGNATURE_28  "0"
#endif

/* 检查是否支持实际IP */
#if (NGX_HTTP_REALIP)
#define NGX_MODULE_SIGNATURE_29  "1"
#else
#define NGX_MODULE_SIGNATURE_29  "0"
#endif

/* 检查是否支持HTTP头部模块 */
#if (NGX_HTTP_HEADERS)
#define NGX_MODULE_SIGNATURE_30  "1"
#else
#define NGX_MODULE_SIGNATURE_30  "0"
#endif

/* 检查是否支持HTTP DAV模块 */
#if (NGX_HTTP_DAV)
#define NGX_MODULE_SIGNATURE_31  "1"
#else
#define NGX_MODULE_SIGNATURE_31  "0"
#endif

/* 检查是否支持HTTP缓存 */
#if (NGX_HTTP_CACHE)
#define NGX_MODULE_SIGNATURE_32  "1"
#else
#define NGX_MODULE_SIGNATURE_32  "0"
#endif

/* 检查是否支持上游区域 */
#if (NGX_HTTP_UPSTREAM_ZONE)
#define NGX_MODULE_SIGNATURE_33  "1"
#else
#define NGX_MODULE_SIGNATURE_33  "0"
#endif

/* 检查是否启用兼容模式 */
#if (NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_34  "1"
#else
#define NGX_MODULE_SIGNATURE_34  "0"
#endif

/**
 * @brief 定义Nginx模块签名宏
 *
 * 这个宏用于定义Nginx模块的签名。签名是一个由多个部分组成的字符串，
 * 每个部分代表了Nginx的不同特性或模块的启用状态。
 * 这个签名用于在运行时验证模块的兼容性和配置。
 *
 * 宏展开后将包含NGX_MODULE_SIGNATURE_0到NGX_MODULE_SIGNATURE_34的所有部分，
 * 每个部分通常是"0"或"1"，表示相应特性是否启用。
 */
#define NGX_MODULE_SIGNATURE                                                  \
    NGX_MODULE_SIGNATURE_0 NGX_MODULE_SIGNATURE_1 NGX_MODULE_SIGNATURE_2      \
    NGX_MODULE_SIGNATURE_3 NGX_MODULE_SIGNATURE_4 NGX_MODULE_SIGNATURE_5      \
    NGX_MODULE_SIGNATURE_6 NGX_MODULE_SIGNATURE_7 NGX_MODULE_SIGNATURE_8      \
    NGX_MODULE_SIGNATURE_9 NGX_MODULE_SIGNATURE_10 NGX_MODULE_SIGNATURE_11    \
    NGX_MODULE_SIGNATURE_12 NGX_MODULE_SIGNATURE_13 NGX_MODULE_SIGNATURE_14   \
    NGX_MODULE_SIGNATURE_15 NGX_MODULE_SIGNATURE_16 NGX_MODULE_SIGNATURE_17   \
    NGX_MODULE_SIGNATURE_18 NGX_MODULE_SIGNATURE_19 NGX_MODULE_SIGNATURE_20   \
    NGX_MODULE_SIGNATURE_21 NGX_MODULE_SIGNATURE_22 NGX_MODULE_SIGNATURE_23   \
    NGX_MODULE_SIGNATURE_24 NGX_MODULE_SIGNATURE_25 NGX_MODULE_SIGNATURE_26   \
    NGX_MODULE_SIGNATURE_27 NGX_MODULE_SIGNATURE_28 NGX_MODULE_SIGNATURE_29   \
    NGX_MODULE_SIGNATURE_30 NGX_MODULE_SIGNATURE_31 NGX_MODULE_SIGNATURE_32   \
    NGX_MODULE_SIGNATURE_33 NGX_MODULE_SIGNATURE_34


/**
 * @brief 定义Nginx模块版本1的宏
 *
 * 这个宏用于定义Nginx模块的版本1结构。它包含了模块的基本信息，
 * 如索引、版本号、签名等。这个宏通常用于初始化ngx_module_t结构体。
 *
 * 宏展开后包括以下字段:
 * - 模块索引（初始设置为未设置状态）
 * - 模块版本号
 * - 模块签名
 * - 其他模块相关的元数据
 */
#define NGX_MODULE_V1                                                         \
    NGX_MODULE_UNSET_INDEX, NGX_MODULE_UNSET_INDEX,                           \
    NULL, 0, 0, nginx_version, NGX_MODULE_SIGNATURE

/**
 * @brief 定义Nginx模块版本1的填充宏
 *
 * 这个宏用于为ngx_module_s结构体的末尾提供8个额外的填充字段。
 * 这些填充字段允许在未来版本中扩展模块结构，而不破坏二进制兼容性。
 * 每个0代表一个未使用的spare_hook字段。
 */
#define NGX_MODULE_V1_PADDING  0, 0, 0, 0, 0, 0, 0, 0


/**
 * @brief Nginx模块结构体
 *
 * 这个结构体定义了Nginx模块的基本结构和属性。
 * 每个Nginx模块都应该实现这个结构体，以便于Nginx核心识别和管理。
 */
struct ngx_module_s {
    ngx_uint_t            ctx_index;    /* 模块上下文索引 */
    ngx_uint_t            index;        /* 模块索引 */

    char                 *name;         /* 模块名称 */

    ngx_uint_t            spare0;       /* 备用字段0 */
    ngx_uint_t            spare1;       /* 备用字段1 */

    ngx_uint_t            version;      /* 模块版本 */
    const char           *signature;    /* 模块签名 */

    void                 *ctx;          /* 模块上下文 */
    ngx_command_t        *commands;     /* 模块命令 */
    ngx_uint_t            type;         /* 模块类型 */

    ngx_int_t           (*init_master)(ngx_log_t *log);  /* 主进程初始化函数 */

    ngx_int_t           (*init_module)(ngx_cycle_t *cycle);  /* 模块初始化函数 */

    ngx_int_t           (*init_process)(ngx_cycle_t *cycle);  /* 进程初始化函数 */
    ngx_int_t           (*init_thread)(ngx_cycle_t *cycle);   /* 线程初始化函数 */
    void                (*exit_thread)(ngx_cycle_t *cycle);   /* 线程退出函数 */
    void                (*exit_process)(ngx_cycle_t *cycle);  /* 进程退出函数 */

    void                (*exit_master)(ngx_cycle_t *cycle);   /* 主进程退出函数 */

    uintptr_t             spare_hook0;  /* 备用钩子0 */
    uintptr_t             spare_hook1;  /* 备用钩子1 */
    uintptr_t             spare_hook2;  /* 备用钩子2 */
    uintptr_t             spare_hook3;  /* 备用钩子3 */
    uintptr_t             spare_hook4;
    uintptr_t             spare_hook5;
    uintptr_t             spare_hook6;
    uintptr_t             spare_hook7;
};


/**
 * @brief 核心模块结构体
 *
 * 这个结构体定义了Nginx核心模块的基本结构和属性。
 * 核心模块是Nginx的基础组件，负责处理核心功能和配置。
 */
typedef struct {
    ngx_str_t             name;
    void               *(*create_conf)(ngx_cycle_t *cycle);
    char               *(*init_conf)(ngx_cycle_t *cycle, void *conf);
} ngx_core_module_t;


/**
 * @brief 预初始化所有模块
 *
 * 这个函数负责在Nginx启动时预初始化所有已加载的模块。
 * 它通常在配置解析之前被调用，为模块的后续初始化和配置做准备。
 *
 * @return ngx_int_t 返回NGX_OK表示成功，NGX_ERROR表示失败
 */
ngx_int_t ngx_preinit_modules(void);
/**
 * @brief 初始化当前周期的模块
 *
 * 这个函数负责初始化当前Nginx周期中的所有模块。
 * 它会遍历所有已加载的模块，并为每个模块分配必要的资源和初始化其配置。
 * 这个过程通常在Nginx启动或重新加载配置时执行。
 *
 * @param cycle 指向当前Nginx周期结构的指针
 * @return ngx_int_t 返回NGX_OK表示成功，NGX_ERROR表示失败
 */
ngx_int_t ngx_cycle_modules(ngx_cycle_t *cycle);
/**
 * @brief 初始化所有模块
 *
 * 这个函数负责初始化当前Nginx周期中的所有模块。
 * 它会遍历所有已加载的模块，并调用每个模块的初始化函数。
 * 这个过程通常在Nginx启动时执行，确保所有模块都正确初始化。
 *
 * @param cycle 指向当前Nginx周期结构的指针
 * @return ngx_int_t 返回NGX_OK表示成功，NGX_ERROR表示失败
 */
ngx_int_t ngx_init_modules(ngx_cycle_t *cycle);
/**
 * @brief 计算指定类型的模块数量
 *
 * 这个函数用于计算当前Nginx周期中指定类型的模块数量。
 * 它遍历所有已加载的模块，统计符合指定类型的模块个数。
 *
 * @param cycle 指向当前Nginx周期结构的指针
 * @param type 要统计的模块类型
 * @return ngx_int_t 返回指定类型的模块数量
 */
ngx_int_t ngx_count_modules(ngx_cycle_t *cycle, ngx_uint_t type);


/**
 * @brief 添加一个新模块
 *
 * 这个函数用于向Nginx配置中添加一个新的模块。
 * 它通常在动态加载模块或处理配置文件中的模块指令时被调用。
 *
 * @param cf 指向Nginx配置结构的指针
 * @param file 指向包含模块文件名的ngx_str_t结构的指针
 * @param module 指向要添加的模块结构的指针
 * @param order 指向字符串数组的指针，用于指定模块的加载顺序
 * @return ngx_int_t 返回NGX_OK表示成功，NGX_ERROR表示失败
 */
ngx_int_t ngx_add_module(ngx_conf_t *cf, ngx_str_t *file,
    ngx_module_t *module, char **order);


/**
 * @brief 全局模块数组
 *
 * 这个数组包含了所有已加载的Nginx模块的指针。
 * 每个元素都是一个指向ngx_module_t结构的指针，代表一个Nginx模块。
 * 这个数组在Nginx启动时被初始化，并在整个运行过程中用于访问和管理模块。
 */
extern ngx_module_t  *ngx_modules[];
/**
 * @brief 最大模块数量
 *
 * 这个变量存储了Nginx中允许的最大模块数量。
 * 它用于限制可以加载的模块总数，以防止系统资源过度消耗。
 * 在Nginx初始化过程中，这个值会被设置，并在后续的模块管理中使用。
 */
extern ngx_uint_t     ngx_max_module;

/**
 * @brief 模块名称数组
 *
 * 这个数组存储了所有已加载Nginx模块的名称。
 * 每个元素都是一个指向字符串的指针，对应于ngx_modules数组中同位置模块的名称。
 * 这个数组主要用于调试和日志记录目的，允许通过名称引用模块。
 */
extern char          *ngx_module_names[];


#endif /* _NGX_MODULE_H_INCLUDED_ */
