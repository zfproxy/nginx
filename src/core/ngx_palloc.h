
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PALLOC_H_INCLUDED_
#define _NGX_PALLOC_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * NGX_MAX_ALLOC_FROM_POOL应该是(ngx_pagesize - 1)，即在x86上为4095。
 * 在Windows NT上，它减少了内核中锁定页面的数量。
 */
#define NGX_MAX_ALLOC_FROM_POOL  (ngx_pagesize - 1)

/* 默认内存池大小为16KB */
#define NGX_DEFAULT_POOL_SIZE    (16 * 1024)

/* 内存池对齐大小为16字节 */
#define NGX_POOL_ALIGNMENT       16

/* 
 * 最小内存池大小
 * 确保至少能容纳内存池结构体和两个大块内存结构体，并按NGX_POOL_ALIGNMENT对齐
 */
#define NGX_MIN_POOL_SIZE                                                     \
    ngx_align((sizeof(ngx_pool_t) + 2 * sizeof(ngx_pool_large_t)),            \
              NGX_POOL_ALIGNMENT)


/* 定义清理函数的类型 */
typedef void (*ngx_pool_cleanup_pt)(void *data);

/* 清理结构体 */
typedef struct ngx_pool_cleanup_s  ngx_pool_cleanup_t;

struct ngx_pool_cleanup_s {
    ngx_pool_cleanup_pt   handler;  /* 清理函数 */
    void                 *data;     /* 清理数据 */
    ngx_pool_cleanup_t   *next;     /* 下一个清理结构体 */
};


/* 大块内存结构体 */
typedef struct ngx_pool_large_s  ngx_pool_large_t;

struct ngx_pool_large_s {
    ngx_pool_large_t     *next;    /* 下一个大块内存 */
    void                 *alloc;   /* 分配的内存 */
};


/* 内存池数据块结构体 */
typedef struct {
    u_char               *last;    /* 当前可用内存的起始位置 */
    u_char               *end;     /* 内存块结束位置 */
    ngx_pool_t           *next;    /* 下一个内存池 */
    ngx_uint_t            failed;  /* 失败次数 */
} ngx_pool_data_t;


/* 内存池主结构体 */
struct ngx_pool_s {
    ngx_pool_data_t       d;        /* 数据块 */
    size_t                max;      /* 最大允许分配的大小 */
    ngx_pool_t           *current;  /* 当前使用的内存池 */
    ngx_chain_t          *chain;    /* 缓冲区链 */
    ngx_pool_large_t     *large;    /* 大块内存链表 */
    ngx_pool_cleanup_t   *cleanup;  /* 清理函数链表 */
    ngx_log_t            *log;      /* 日志 */
};


/* 文件清理结构体 */
typedef struct {
    ngx_fd_t              fd;       /* 文件描述符 */
    u_char               *name;     /* 文件名 */
    ngx_log_t            *log;      /* 日志 */
} ngx_pool_cleanup_file_t;


/* 创建内存池 */
ngx_pool_t *ngx_create_pool(size_t size, ngx_log_t *log);
/* 销毁内存池 */
void ngx_destroy_pool(ngx_pool_t *pool);
/* 重置内存池 */
void ngx_reset_pool(ngx_pool_t *pool);

/* 从内存池分配内存 */
void *ngx_palloc(ngx_pool_t *pool, size_t size);
/* 从内存池分配内存，不进行内存对齐 */
void *ngx_pnalloc(ngx_pool_t *pool, size_t size);
/* 从内存池分配内存并初始化为0 */
void *ngx_pcalloc(ngx_pool_t *pool, size_t size);
/* 从内存池分配对齐的内存 */
void *ngx_pmemalign(ngx_pool_t *pool, size_t size, size_t alignment);
/* 尝试释放内存池中的大块内存 */
ngx_int_t ngx_pfree(ngx_pool_t *pool, void *p);


/* 添加清理回调 */
ngx_pool_cleanup_t *ngx_pool_cleanup_add(ngx_pool_t *p, size_t size);
/* 运行文件清理回调 */
void ngx_pool_run_cleanup_file(ngx_pool_t *p, ngx_fd_t fd);
/* 文件清理函数 */
void ngx_pool_cleanup_file(void *data);
/* 删除文件函数 */
void ngx_pool_delete_file(void *data);


#endif /* _NGX_PALLOC_H_INCLUDED_ */
