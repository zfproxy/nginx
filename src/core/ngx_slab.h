
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SLAB_H_INCLUDED_
#define _NGX_SLAB_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


// 定义slab页面结构体
typedef struct ngx_slab_page_s  ngx_slab_page_t;

struct ngx_slab_page_s {
    uintptr_t         slab;    // slab标志位
    ngx_slab_page_t  *next;    // 指向下一个页面
    uintptr_t         prev;    // 指向前一个页面
};


// slab统计信息结构体
typedef struct {
    ngx_uint_t        total;   // 总内存
    ngx_uint_t        used;    // 已使用内存

    ngx_uint_t        reqs;    // 请求次数
    ngx_uint_t        fails;   // 失败次数
} ngx_slab_stat_t;


// slab内存池结构体
typedef struct {
    ngx_shmtx_sh_t    lock;    // 共享内存锁

    size_t            min_size;   // 最小分配大小
    size_t            min_shift;  // 最小分配大小的位移值

    ngx_slab_page_t  *pages;   // 页面列表
    ngx_slab_page_t  *last;    // 最后一个页面
    ngx_slab_page_t   free;    // 空闲页面

    ngx_slab_stat_t  *stats;   // 统计信息
    ngx_uint_t        pfree;   // 部分空闲页面数

    u_char           *start;   // 内存池起始地址
    u_char           *end;     // 内存池结束地址

    ngx_shmtx_t       mutex;   // 互斥锁

    u_char           *log_ctx; // 日志上下文
    u_char            zero;    // 零字节

    unsigned          log_nomem:1; // 是否记录内存不足日志

    void             *data;    // 用户数据
    void             *addr;    // 内存池地址
} ngx_slab_pool_t;


// 初始化slab大小
void ngx_slab_sizes_init(void);
// 初始化slab内存池
void ngx_slab_init(ngx_slab_pool_t *pool);
// 从slab内存池分配内存
void *ngx_slab_alloc(ngx_slab_pool_t *pool, size_t size);
// 从已锁定的slab内存池分配内存
void *ngx_slab_alloc_locked(ngx_slab_pool_t *pool, size_t size);
// 从slab内存池分配并初始化内存
void *ngx_slab_calloc(ngx_slab_pool_t *pool, size_t size);
// 从已锁定的slab内存池分配并初始化内存
void *ngx_slab_calloc_locked(ngx_slab_pool_t *pool, size_t size);
// 释放slab内存池中的内存
void ngx_slab_free(ngx_slab_pool_t *pool, void *p);
// 释放已锁定的slab内存池中的内存
void ngx_slab_free_locked(ngx_slab_pool_t *pool, void *p);


#endif /* _NGX_SLAB_H_INCLUDED_ */
