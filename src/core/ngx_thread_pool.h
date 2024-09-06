
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 */


#ifndef _NGX_THREAD_POOL_H_INCLUDED_
#define _NGX_THREAD_POOL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


/**
 * @brief 线程任务结构体
 */
struct ngx_thread_task_s {
    ngx_thread_task_t   *next;    /**< 指向下一个任务的指针，用于任务队列 */
    ngx_uint_t           id;      /**< 任务ID */
    void                *ctx;     /**< 任务上下文 */
    void               (*handler)(void *data, ngx_log_t *log);  /**< 任务处理函数 */
    ngx_event_t          event;   /**< 与任务相关的事件 */
};


/**
 * @brief 线程池结构体
 */
typedef struct ngx_thread_pool_s  ngx_thread_pool_t;


/**
 * @brief 添加线程池
 * @param cf 配置结构体指针
 * @param name 线程池名称
 * @return 返回创建的线程池指针
 */
ngx_thread_pool_t *ngx_thread_pool_add(ngx_conf_t *cf, ngx_str_t *name);

/**
 * @brief 获取线程池
 * @param cycle Nginx核心周期结构体指针
 * @param name 线程池名称
 * @return 返回找到的线程池指针
 */
ngx_thread_pool_t *ngx_thread_pool_get(ngx_cycle_t *cycle, ngx_str_t *name);

/**
 * @brief 分配线程任务
 * @param pool 内存池指针
 * @param size 任务大小
 * @return 返回分配的线程任务指针
 */
ngx_thread_task_t *ngx_thread_task_alloc(ngx_pool_t *pool, size_t size);

/**
 * @brief 提交线程任务到线程池
 * @param tp 线程池指针
 * @param task 要提交的任务指针
 * @return 返回提交结果的状态码
 */
ngx_int_t ngx_thread_task_post(ngx_thread_pool_t *tp, ngx_thread_task_t *task);


#endif /* _NGX_THREAD_POOL_H_INCLUDED_ */
