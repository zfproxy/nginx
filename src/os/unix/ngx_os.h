
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


/**
 * @file ngx_os.h
 * @brief Nginx操作系统相关头文件
 */

#ifndef _NGX_OS_H_INCLUDED_
#define _NGX_OS_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_IO_SENDFILE    1


/**
 * @brief 接收数据的函数指针类型
 */
typedef ssize_t (*ngx_recv_pt)(ngx_connection_t *c, u_char *buf, size_t size);


/**
 * @brief 接收链式数据的函数指针类型
 * @param c 连接对象指针
 * @param in 输入链表指针
 * @param limit 接收数据的限制大小
 * @return 接收的字节数，错误时返回负值
 */
typedef ssize_t (*ngx_recv_chain_pt)(ngx_connection_t *c, ngx_chain_t *in,
    off_t limit);

/**
 * @brief 发送数据的函数指针类型
 */
typedef ssize_t (*ngx_send_pt)(ngx_connection_t *c, u_char *buf, size_t size);

/**
 * @brief 发送链式数据的函数指针类型
 */
typedef ngx_chain_t *(*ngx_send_chain_pt)(ngx_connection_t *c, ngx_chain_t *in,
    off_t limit);

/**
 * @brief Nginx I/O操作结构体
 */
typedef struct {
    ngx_recv_pt        recv;              /**< TCP接收函数 */
    ngx_recv_chain_pt  recv_chain;        /**< TCP链式接收函数 */
    ngx_recv_pt        udp_recv;          /**< UDP接收函数 */
    ngx_send_pt        send;              /**< TCP发送函数 */
    ngx_send_pt        udp_send;          /**< UDP发送函数 */
    ngx_send_chain_pt  udp_send_chain;    /**< UDP链式发送函数 */
    ngx_send_chain_pt  send_chain;        /**< TCP链式发送函数 */
    ngx_uint_t         flags;             /**< I/O操作标志 */
} ngx_os_io_t;


/**
 * @brief 初始化操作系统相关的功能
 * @param log 日志对象指针
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_os_init(ngx_log_t *log);

/**
 * @brief 输出操作系统相关的状态信息
 * @param log 日志对象指针
 */
void ngx_os_status(ngx_log_t *log);

/**
 * @brief 初始化特定操作系统的功能
 * @param log 日志对象指针
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_os_specific_init(ngx_log_t *log);

/**
 * @brief 输出特定操作系统的状态信息
 * @param log 日志对象指针
 */
void ngx_os_specific_status(ngx_log_t *log);

/**
 * @brief 将进程转为守护进程
 * @param log 日志对象指针
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_daemon(ngx_log_t *log);

/**
 * @brief 向指定进程发送信号
 * @param cycle Nginx核心循环对象指针
 * @param sig 信号字符串
 * @param pid 目标进程ID
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_os_signal_process(ngx_cycle_t *cycle, char *sig, ngx_pid_t pid);

/**
 * @brief Unix系统接收数据
 * @param c 连接对象指针
 * @param buf 接收缓冲区
 * @param size 接收缓冲区大小
 * @return 接收的字节数，错误时返回负值
 */
ssize_t ngx_unix_recv(ngx_connection_t *c, u_char *buf, size_t size);

/**
 * @brief 读取链式数据
 * @param c 连接对象指针
 * @param entry 链表入口
 * @param limit 读取限制
 * @return 读取的字节数，错误时返回负值
 */
ssize_t ngx_readv_chain(ngx_connection_t *c, ngx_chain_t *entry, off_t limit);

/**
 * @brief Unix系统UDP接收数据
 * @param c 连接对象指针
 * @param buf 接收缓冲区
 * @param size 接收缓冲区大小
 * @return 接收的字节数，错误时返回负值
 */
ssize_t ngx_udp_unix_recv(ngx_connection_t *c, u_char *buf, size_t size);

/**
 * @brief Unix系统发送数据
 * @param c 连接对象指针
 * @param buf 发送缓冲区
 * @param size 发送数据大小
 * @return 发送的字节数，错误时返回负值
 */
ssize_t ngx_unix_send(ngx_connection_t *c, u_char *buf, size_t size);

/**
 * @brief 写入链式数据
 * @param c 连接对象指针
 * @param in 输入链表
 * @param limit 写入限制
 * @return 剩余未写入的链表，NULL表示全部写入完成，NGX_CHAIN_ERROR表示错误
 */
ngx_chain_t *ngx_writev_chain(ngx_connection_t *c, ngx_chain_t *in,
    off_t limit);

/**
 * @brief Unix系统UDP发送数据
 * @param c 连接对象指针
 * @param buf 发送缓冲区
 * @param size 发送数据大小
 * @return 发送的字节数，错误时返回负值
 */
ssize_t ngx_udp_unix_send(ngx_connection_t *c, u_char *buf, size_t size);

/**
 * @brief Unix系统UDP发送链式数据
 * @param c 连接对象指针
 * @param in 输入链表
 * @param limit 发送限制
 * @return 剩余未发送的链表，NULL表示全部发送完成，NGX_CHAIN_ERROR表示错误
 */
ngx_chain_t *ngx_udp_unix_sendmsg_chain(ngx_connection_t *c, ngx_chain_t *in,
    off_t limit);


/**
 * @brief 预分配的iovec结构体数量
 */
#if (IOV_MAX > 64)
#define NGX_IOVS_PREALLOCATE  64
#else
#define NGX_IOVS_PREALLOCATE  IOV_MAX
#endif


/**
 * @brief Nginx iovec结构体
 */
typedef struct {
    struct iovec  *iovs;      /**< iovec数组 */
    ngx_uint_t     count;     /**< iovec数量 */
    size_t         size;      /**< 总大小 */
    ngx_uint_t     nalloc;    /**< 已分配的iovec数量 */
} ngx_iovec_t;

ngx_chain_t *ngx_output_chain_to_iovec(ngx_iovec_t *vec, ngx_chain_t *in,
    size_t limit, ngx_log_t *log);


/**
 * @brief 使用writev系统调用发送数据
 * @param c 连接对象指针
 * @param vec iovec结构体指针，包含要发送的数据
 * @return 成功发送的字节数，错误时返回负值
 */
ssize_t ngx_writev(ngx_connection_t *c, ngx_iovec_t *vec);


extern ngx_os_io_t  ngx_os_io;        /**< 操作系统I/O操作函数集 */
extern ngx_int_t    ngx_ncpu;         /**< CPU核心数 */
extern ngx_int_t    ngx_max_sockets;  /**< 系统支持的最大套接字数 */
extern ngx_uint_t   ngx_inherited_nonblocking;  /**< 继承的非阻塞标志 */
extern ngx_uint_t   ngx_tcp_nodelay_and_tcp_nopush;  /**< TCP_NODELAY和TCP_NOPUSH同时启用的标志 */


#if (NGX_FREEBSD)
#include <ngx_freebsd.h>


#elif (NGX_LINUX)
#include <ngx_linux.h>


#elif (NGX_SOLARIS)
#include <ngx_solaris.h>


#elif (NGX_DARWIN)
#include <ngx_darwin.h>
#endif

#endif /* _NGX_OS_H_INCLUDED_ */
