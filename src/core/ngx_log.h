
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_LOG_H_INCLUDED_
#define _NGX_LOG_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* 日志级别定义 */
#define NGX_LOG_STDERR            0  /* 标准错误输出 */
#define NGX_LOG_EMERG             1  /* 紧急情况 */
#define NGX_LOG_ALERT             2  /* 需要立即采取行动的严重情况 */
#define NGX_LOG_CRIT              3  /* 严重错误 */
#define NGX_LOG_ERR               4  /* 错误 */
#define NGX_LOG_WARN              5  /* 警告 */
#define NGX_LOG_NOTICE            6  /* 正常但重要的情况 */
#define NGX_LOG_INFO              7  /* 信息性消息 */
#define NGX_LOG_DEBUG             8  /* 调试信息 */

/* 调试日志类型定义 */
#define NGX_LOG_DEBUG_CORE        0x010  /* 核心模块调试 */
#define NGX_LOG_DEBUG_ALLOC       0x020  /* 内存分配调试 */
#define NGX_LOG_DEBUG_MUTEX       0x040  /* 互斥锁调试 */
#define NGX_LOG_DEBUG_EVENT       0x080  /* 事件调试 */
#define NGX_LOG_DEBUG_HTTP        0x100  /* HTTP模块调试 */
#define NGX_LOG_DEBUG_MAIL        0x200  /* 邮件模块调试 */
#define NGX_LOG_DEBUG_STREAM      0x400  /* 流处理模块调试 */

/*
 * 注意：在添加新的调试级别后，
 * 不要忘记更新 src/core/ngx_log.c 中的 debug_levels[] 数组
 */

/* 调试日志范围定义 */
#define NGX_LOG_DEBUG_FIRST       NGX_LOG_DEBUG_CORE   /* 第一个调试日志类型 */
#define NGX_LOG_DEBUG_LAST        NGX_LOG_DEBUG_STREAM /* 最后一个调试日志类型 */
#define NGX_LOG_DEBUG_CONNECTION  0x80000000  /* 连接相关调试 */
#define NGX_LOG_DEBUG_ALL         0x7ffffff0  /* 所有调试日志类型 */


/* 定义日志处理函数指针类型 */
typedef u_char *(*ngx_log_handler_pt) (ngx_log_t *log, u_char *buf, size_t len);

/* 定义日志写入函数指针类型 */
typedef void (*ngx_log_writer_pt) (ngx_log_t *log, ngx_uint_t level,
    u_char *buf, size_t len);

/* 定义日志结构体 */
/* 该结构体包含了日志系统所需的各种信息和功能 */
struct ngx_log_s {
    ngx_uint_t           log_level;    /* 日志级别 */
    ngx_open_file_t     *file;         /* 日志文件 */

    ngx_atomic_uint_t    connection;   /* 连接数，用于在日志中区分不同的连接 */

    time_t               disk_full_time;  /* 磁盘满时的时间戳 */

    ngx_log_handler_pt   handler;      /* 日志处理函数 */
    void                *data;         /* 传递给处理函数的自定义数据 */

    ngx_log_writer_pt    writer;       /* 日志写入函数 */
    void                *wdata;        /* 传递给写入函数的自定义数据 */

    /*
     * 我们将"action"声明为"char *"，因为动作通常是静态字符串
     * 如果使用"u_char *"，我们就必须经常覆盖它们的类型
     */

    char                *action;       /* 当前正在执行的动作描述 */

    ngx_log_t           *next;         /* 指向下一个日志对象，形成日志链 */
};


/* 定义最大错误字符串长度 */
#define NGX_MAX_ERROR_STR   2048


/*********************************/

/* 如果支持C99可变参数宏 */
#if (NGX_HAVE_C99_VARIADIC_MACROS)

/* 标记支持可变参数宏 */
#define NGX_HAVE_VARIADIC_MACROS  1

/* 定义日志错误宏，如果日志级别足够高则调用核心错误日志函数 */
#define ngx_log_error(level, log, ...)                                        \
    if ((log)->log_level >= level) ngx_log_error_core(level, log, __VA_ARGS__)

/* 声明核心错误日志函数 */
void ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...);

/* 定义调试日志宏，如果日志级别包含指定的调试级别则调用核心错误日志函数 */
#define ngx_log_debug(level, log, ...)                                        \
    if ((log)->log_level & level)                                             \
        ngx_log_error_core(NGX_LOG_DEBUG, log, __VA_ARGS__)

/*********************************/

#elif (NGX_HAVE_GCC_VARIADIC_MACROS)

/* 定义支持可变参数宏 */
#define NGX_HAVE_VARIADIC_MACROS  1

/* 定义错误日志宏，如果日志级别足够高则调用核心错误日志函数 */
#define ngx_log_error(level, log, args...)                                    \
    if ((log)->log_level >= level) ngx_log_error_core(level, log, args)

/* 声明核心错误日志函数 */
void ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...);

/* 定义调试日志宏，如果日志级别包含指定的调试级别则调用核心错误日志函数 */
#define ngx_log_debug(level, log, args...)                                    \
    if ((log)->log_level & level)                                             \
        ngx_log_error_core(NGX_LOG_DEBUG, log, args)

/*********************************/

#else /* 不支持可变参数宏 */

/* 定义不支持可变参数宏 */
#define NGX_HAVE_VARIADIC_MACROS  0

/* 声明错误日志函数 */
void ngx_cdecl ngx_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...);
/* 声明核心错误日志函数 */
void ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, va_list args);
/* 声明调试日志核心函数 */
void ngx_cdecl ngx_log_debug_core(ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...);


#endif /* variadic macros */


/*********************************/

#if (NGX_DEBUG)

#if (NGX_HAVE_VARIADIC_MACROS)

/* 定义不同参数数量的调试日志宏，使用可变参数宏 */

/* 无参数的调试日志宏 */
#define ngx_log_debug0(level, log, err, fmt)                                  \
        ngx_log_debug(level, log, err, fmt)

/* 一个参数的调试日志宏 */
#define ngx_log_debug1(level, log, err, fmt, arg1)                            \
        ngx_log_debug(level, log, err, fmt, arg1)

/* 两个参数的调试日志宏 */
#define ngx_log_debug2(level, log, err, fmt, arg1, arg2)                      \
        ngx_log_debug(level, log, err, fmt, arg1, arg2)

/* 三个参数的调试日志宏 */
#define ngx_log_debug3(level, log, err, fmt, arg1, arg2, arg3)                \
        ngx_log_debug(level, log, err, fmt, arg1, arg2, arg3)

/* 四个参数的调试日志宏 */
#define ngx_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4)          \
        ngx_log_debug(level, log, err, fmt, arg1, arg2, arg3, arg4)

/* 五个参数的调试日志宏 */
#define ngx_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)    \
        ngx_log_debug(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)

/* 六个参数的调试日志宏 */
#define ngx_log_debug6(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6)                    \
        ngx_log_debug(level, log, err, fmt,                                   \
                       arg1, arg2, arg3, arg4, arg5, arg6)

/* 七个参数的调试日志宏 */
#define ngx_log_debug7(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7)              \
        ngx_log_debug(level, log, err, fmt,                                   \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7)

/* 八个参数的调试日志宏 */
#define ngx_log_debug8(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)        \
        ngx_log_debug(level, log, err, fmt,                                   \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)


#else /* 不支持可变参数宏 */

/* 定义不同参数数量的调试日志宏，使用条件判断 */

/* 无参数的调试日志宏 */
#define ngx_log_debug0(level, log, err, fmt)                                  \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt)

/* 一个参数的调试日志宏 */
#define ngx_log_debug1(level, log, err, fmt, arg1)                            \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1)

/* 两个参数的调试日志宏 */
#define ngx_log_debug2(level, log, err, fmt, arg1, arg2)                      \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2)

/* 三个参数的调试日志宏 */
#define ngx_log_debug3(level, log, err, fmt, arg1, arg2, arg3)                \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2, arg3)

/* 四个参数的调试日志宏 */
#define ngx_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4)          \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2, arg3, arg4)

/* 五个参数的调试日志宏 */
#define ngx_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)    \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2, arg3, arg4, arg5)

/* 六个参数的调试日志宏 */
#define ngx_log_debug6(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6)                    \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2, arg3, arg4, arg5, arg6)

/* 七个参数的调试日志宏 */
#define ngx_log_debug7(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7)              \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt,                                     \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7)

/* 八个参数的调试日志宏 */
#define ngx_log_debug8(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)        \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt,                                     \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)

#endif

#else /* !NGX_DEBUG */

/* 在非调试模式下，将所有调试日志宏定义为空操作 */
#define ngx_log_debug0(level, log, err, fmt)
#define ngx_log_debug1(level, log, err, fmt, arg1)
#define ngx_log_debug2(level, log, err, fmt, arg1, arg2)
#define ngx_log_debug3(level, log, err, fmt, arg1, arg2, arg3)
#define ngx_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4)
#define ngx_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)
#define ngx_log_debug6(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define ngx_log_debug7(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5,    \
                       arg6, arg7)
#define ngx_log_debug8(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5,    \
                       arg6, arg7, arg8)

#endif

/*********************************/

/* 初始化日志系统，设置日志前缀和错误日志文件路径 */
ngx_log_t *ngx_log_init(u_char *prefix, u_char *error_log);

/* 记录致命错误并中止程序执行 */
void ngx_cdecl ngx_log_abort(ngx_err_t err, const char *fmt, ...);

/* 将错误信息输出到标准错误流 */
void ngx_cdecl ngx_log_stderr(ngx_err_t err, const char *fmt, ...);

/* 将错误码转换为对应的错误信息字符串 */
u_char *ngx_log_errno(u_char *buf, u_char *last, ngx_err_t err);

/* 打开默认的错误日志文件 */
ngx_int_t ngx_log_open_default(ngx_cycle_t *cycle);

/* 重定向标准错误流到日志文件 */
ngx_int_t ngx_log_redirect_stderr(ngx_cycle_t *cycle);

/* 获取日志链表中的文件日志对象 */
ngx_log_t *ngx_log_get_file_log(ngx_log_t *head);

/* 设置日志配置 */
char *ngx_log_set_log(ngx_conf_t *cf, ngx_log_t **head);


/*
 * ngx_write_stderr() cannot be implemented as macro, since
 * MSVC does not allow to use #ifdef inside macro parameters.
 *
 * ngx_write_fd() is used instead of ngx_write_console(), since
 * CharToOemBuff() inside ngx_write_console() cannot be used with
 * read only buffer as destination and CharToOemBuff() is not needed
 * for ngx_write_stderr() anyway.
 */
/* 将文本写入标准错误流 */
static ngx_inline void
ngx_write_stderr(char *text)
{
    (void) ngx_write_fd(ngx_stderr, text, ngx_strlen(text));
}


/* 将文本写入标准输出流 */
static ngx_inline void
ngx_write_stdout(char *text)
{
    (void) ngx_write_fd(ngx_stdout, text, ngx_strlen(text));
}


/* 错误日志模块 */
extern ngx_module_t  ngx_errlog_module;

/* 是否使用标准错误流的标志 */
extern ngx_uint_t    ngx_use_stderr;


#endif /* _NGX_LOG_H_INCLUDED_ */
