
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONFIG_H_INCLUDED_
#define _NGX_CONFIG_H_INCLUDED_


#include <ngx_auto_headers.h>


#if defined __DragonFly__ && !defined __FreeBSD__
#define __FreeBSD__        4
#define __FreeBSD_version  480101
#endif


#if (NGX_FREEBSD)
#include <ngx_freebsd_config.h>


#elif (NGX_LINUX)
#include <ngx_linux_config.h>


#elif (NGX_SOLARIS)
#include <ngx_solaris_config.h>


#elif (NGX_DARWIN)
#include <ngx_darwin_config.h>


#elif (NGX_WIN32)
#include <ngx_win32_config.h>


#else /* POSIX */
#include <ngx_posix_config.h>

#endif


#ifndef NGX_HAVE_SO_SNDLOWAT
#define NGX_HAVE_SO_SNDLOWAT     1
#endif


#if !(NGX_WIN32)

/* 定义信号宏 */
#define ngx_signal_helper(n)     SIG##n
#define ngx_signal_value(n)      ngx_signal_helper(n)

/* 定义随机函数 */
#define ngx_random               random

/* TODO: #ifndef */
/* 定义各种信号 */
#define NGX_SHUTDOWN_SIGNAL      QUIT    /* 关闭信号 */
#define NGX_TERMINATE_SIGNAL     TERM    /* 终止信号 */
#define NGX_NOACCEPT_SIGNAL      WINCH   /* 不接受新连接信号 */
#define NGX_RECONFIGURE_SIGNAL   HUP     /* 重新加载配置信号 */

#if (NGX_LINUXTHREADS)
#define NGX_REOPEN_SIGNAL        INFO    /* 重新打开日志文件信号（Linux线程） */
#define NGX_CHANGEBIN_SIGNAL     XCPU    /* 平滑升级信号（Linux线程） */
#else
#define NGX_REOPEN_SIGNAL        USR1    /* 重新打开日志文件信号 */
#define NGX_CHANGEBIN_SIGNAL     USR2    /* 平滑升级信号 */
#endif

/* 定义C函数调用约定 */
#define ngx_cdecl
#define ngx_libc_cdecl

#endif

/* 定义整数类型 */
typedef intptr_t        ngx_int_t;    /* 定义有符号整数类型 */
typedef uintptr_t       ngx_uint_t;   /* 定义无符号整数类型 */
typedef intptr_t        ngx_flag_t;   /* 定义标志类型，通常用于布尔值 */

/* 定义整数长度常量 */
/* 定义32位整数的最大长度（包括负号） */
#define NGX_INT32_LEN   (sizeof("-2147483648") - 1)

/* 定义64位整数的最大长度（包括负号） */
#define NGX_INT64_LEN   (sizeof("-9223372036854775808") - 1)

#if (NGX_PTR_SIZE == 4)
#define NGX_INT_T_LEN   NGX_INT32_LEN
#define NGX_MAX_INT_T_VALUE  2147483647

#else
#define NGX_INT_T_LEN   NGX_INT64_LEN
#define NGX_MAX_INT_T_VALUE  9223372036854775807
#endif

/* 定义内存对齐 */
#ifndef NGX_ALIGNMENT
#define NGX_ALIGNMENT   sizeof(unsigned long)    /* 平台字长 */
#endif

/* 内存对齐宏 */
#define ngx_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define ngx_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

/* 定义中止函数 */
#define ngx_abort       abort

/* TODO: 平台特定：array[NGX_INVALID_ARRAY_INDEX] 必须导致 SIGSEGV */
#define NGX_INVALID_ARRAY_INDEX 0x80000000

/* TODO: 自动配置：ngx_inline   inline __inline __inline__ */
#ifndef ngx_inline
#define ngx_inline      inline
#endif

/* 定义无效IP地址 */
#ifndef INADDR_NONE  /* Solaris */
#define INADDR_NONE  ((unsigned int) -1)
#endif

/* 定义最大主机名长度 */
#ifdef MAXHOSTNAMELEN
#define NGX_MAXHOSTNAMELEN  MAXHOSTNAMELEN
#else
#define NGX_MAXHOSTNAMELEN  256
#endif

/* 定义最大32位无符号整数和有符号整数值 */
#define NGX_MAX_UINT32_VALUE  (uint32_t) 0xffffffff
#define NGX_MAX_INT32_VALUE   (uint32_t) 0x7fffffff

/* 兼容性宏定义 */
#if (NGX_COMPAT)

#define NGX_COMPAT_BEGIN(slots)  uint64_t spare[slots];
#define NGX_COMPAT_END

#else

#define NGX_COMPAT_BEGIN(slots)
#define NGX_COMPAT_END

#endif


#endif /* _NGX_CONFIG_H_INCLUDED_ */
