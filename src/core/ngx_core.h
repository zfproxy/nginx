
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CORE_H_INCLUDED_
#define _NGX_CORE_H_INCLUDED_


#include <ngx_config.h>


/* 定义模块结构体类型 */
typedef struct ngx_module_s          ngx_module_t;
/* 定义配置结构体类型 */
typedef struct ngx_conf_s            ngx_conf_t;
/* 定义cycle结构体类型，用于存储Nginx运行时的核心数据 */
typedef struct ngx_cycle_s           ngx_cycle_t;
/* 定义内存池结构体类型 */
typedef struct ngx_pool_s            ngx_pool_t;
/* 定义缓冲区链结构体类型 */
typedef struct ngx_chain_s           ngx_chain_t;
/* 定义日志结构体类型 */
typedef struct ngx_log_s             ngx_log_t;
/* 定义打开文件结构体类型 */
typedef struct ngx_open_file_s       ngx_open_file_t;
/* 定义命令结构体类型，用于解析配置指令 */
typedef struct ngx_command_s         ngx_command_t;
/* 定义文件结构体类型 */
typedef struct ngx_file_s            ngx_file_t;
/* 定义事件结构体类型 */
typedef struct ngx_event_s           ngx_event_t;
/* 定义异步I/O事件结构体类型 */
typedef struct ngx_event_aio_s       ngx_event_aio_t;
/* 定义连接结构体类型 */
typedef struct ngx_connection_s      ngx_connection_t;
/* 定义线程任务结构体类型 */
typedef struct ngx_thread_task_s     ngx_thread_task_t;
/* 定义SSL结构体类型 */
typedef struct ngx_ssl_s             ngx_ssl_t;
/* 定义代理协议结构体类型 */
typedef struct ngx_proxy_protocol_s  ngx_proxy_protocol_t;
/* 定义QUIC流结构体类型 */
typedef struct ngx_quic_stream_s     ngx_quic_stream_t;
/* 定义SSL连接结构体类型 */
typedef struct ngx_ssl_connection_s  ngx_ssl_connection_t;
/* 定义UDP连接结构体类型 */
typedef struct ngx_udp_connection_s  ngx_udp_connection_t;

/* 定义事件处理函数指针类型 */
typedef void (*ngx_event_handler_pt)(ngx_event_t *ev);
/* 定义连接处理函数指针类型 */
typedef void (*ngx_connection_handler_pt)(ngx_connection_t *c);


/* 操作成功 */
#define  NGX_OK          0
/* 发生错误 */
#define  NGX_ERROR      -1
/* 资源暂时不可用，需要重试 */
#define  NGX_AGAIN      -2
/* 系统繁忙 */
#define  NGX_BUSY       -3
/* 操作已完成 */
#define  NGX_DONE       -4
/* 请求被拒绝 */
#define  NGX_DECLINED   -5
/* 操作中止 */
#define  NGX_ABORT      -6


#include <ngx_errno.h>
#include <ngx_atomic.h>
#include <ngx_thread.h>
#include <ngx_rbtree.h>
#include <ngx_time.h>
#include <ngx_socket.h>
#include <ngx_string.h>
#include <ngx_files.h>
#include <ngx_shmem.h>
#include <ngx_process.h>
#include <ngx_user.h>
#include <ngx_dlopen.h>
#include <ngx_parse.h>
#include <ngx_parse_time.h>
#include <ngx_log.h>
#include <ngx_alloc.h>
#include <ngx_palloc.h>
#include <ngx_buf.h>
#include <ngx_queue.h>
#include <ngx_array.h>
#include <ngx_list.h>
#include <ngx_hash.h>
#include <ngx_file.h>
#include <ngx_crc.h>
#include <ngx_crc32.h>
#include <ngx_murmurhash.h>
#if (NGX_PCRE)
#include <ngx_regex.h>
#endif
#include <ngx_radix_tree.h>
#include <ngx_times.h>
#include <ngx_rwlock.h>
#include <ngx_shmtx.h>
#include <ngx_slab.h>
#include <ngx_inet.h>
#include <ngx_cycle.h>
#include <ngx_resolver.h>
#if (NGX_OPENSSL)
#include <ngx_event_openssl.h>
#if (NGX_QUIC)
#include <ngx_event_quic.h>
#endif
#endif
#include <ngx_process_cycle.h>
#include <ngx_conf_file.h>
#include <ngx_module.h>
#include <ngx_open_file_cache.h>
#include <ngx_os.h>
#include <ngx_connection.h>
#include <ngx_syslog.h>
#include <ngx_proxy_protocol.h>
#if (NGX_HAVE_BPF)
#include <ngx_bpf.h>
#endif


/* 定义换行符 */
#define LF     (u_char) '\n'
/* 定义回车符 */
#define CR     (u_char) '\r'
/* 定义回车换行符 */
#define CRLF   "\r\n"


/* 计算绝对值 */
#define ngx_abs(value)       (((value) >= 0) ? (value) : - (value))
/* 返回两个值中的较大值 */
#define ngx_max(val1, val2)  ((val1 < val2) ? (val2) : (val1))
/* 返回两个值中的较小值 */
#define ngx_min(val1, val2)  ((val1 > val2) ? (val2) : (val1))

/* 获取CPU信息的函数声明 */
void ngx_cpuinfo(void);

#if (NGX_HAVE_OPENAT)
/* 禁用符号链接选项：关闭 */
#define NGX_DISABLE_SYMLINKS_OFF        0
/* 禁用符号链接选项：开启 */
#define NGX_DISABLE_SYMLINKS_ON         1
/* 禁用符号链接选项：非所有者 */
#define NGX_DISABLE_SYMLINKS_NOTOWNER   2
#endif

#endif /* _NGX_CORE_H_INCLUDED_ */
