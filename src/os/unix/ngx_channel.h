
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CHANNEL_H_INCLUDED_
#define _NGX_CHANNEL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


/**
 * @brief 通道结构体，用于进程间通信
 */
typedef struct {
    ngx_uint_t  command;  /**< 命令类型 */
    ngx_pid_t   pid;      /**< 进程ID */
    ngx_int_t   slot;     /**< 槽位号 */
    ngx_fd_t    fd;       /**< 文件描述符 */
} ngx_channel_t;


/**
 * @brief 向通道写入数据
 * @param s 套接字
 * @param ch 通道结构体指针
 * @param size 数据大小
 * @param log 日志对象
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_write_channel(ngx_socket_t s, ngx_channel_t *ch, size_t size,
    ngx_log_t *log);

/**
 * @brief 从通道读取数据
 * @param s 套接字
 * @param ch 通道结构体指针
 * @param size 数据大小
 * @param log 日志对象
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_read_channel(ngx_socket_t s, ngx_channel_t *ch, size_t size,
    ngx_log_t *log);

/**
 * @brief 添加通道事件
 * @param cycle Nginx核心结构体
 * @param fd 文件描述符
 * @param event 事件类型
 * @param handler 事件处理函数
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_add_channel_event(ngx_cycle_t *cycle, ngx_fd_t fd,
    ngx_int_t event, ngx_event_handler_pt handler);

/**
 * @brief 关闭通道
 * @param fd 文件描述符指针
 * @param log 日志对象
 */
void ngx_close_channel(ngx_fd_t *fd, ngx_log_t *log);


#endif /* _NGX_CHANNEL_H_INCLUDED_ */
