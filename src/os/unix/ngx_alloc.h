
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ALLOC_H_INCLUDED_
#define _NGX_ALLOC_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 分配指定大小的内存
 * @param size 要分配的内存大小
 * @param log 日志对象
 * @return 分配的内存指针，失败时返回NULL
 */
void *ngx_alloc(size_t size, ngx_log_t *log);

/**
 * @brief 分配指定大小的内存并初始化为0
 * @param size 要分配的内存大小
 * @param log 日志对象
 * @return 分配的内存指针，失败时返回NULL
 */
void *ngx_calloc(size_t size, ngx_log_t *log);

/**
 * @brief 释放内存的宏定义，直接使用标准库的free函数
 */
#define ngx_free          free


/*
 * Linux has memalign() or posix_memalign()
 * Solaris has memalign()
 * FreeBSD 7.0 has posix_memalign(), besides, early version's malloc()
 * aligns allocations bigger than page size at the page boundary
 */

#if (NGX_HAVE_POSIX_MEMALIGN || NGX_HAVE_MEMALIGN)

/**
 * @brief 分配对齐的内存
 * @param alignment 对齐字节数
 * @param size 要分配的内存大小
 * @param log 日志对象
 * @return 分配的内存指针，失败时返回NULL
 */
void *ngx_memalign(size_t alignment, size_t size, ngx_log_t *log);

#else

/**
 * @brief 当不支持内存对齐分配时，使用普通的ngx_alloc替代
 */
#define ngx_memalign(alignment, size, log)  ngx_alloc(size, log)

#endif


/**
 * @brief 系统页面大小
 */
extern ngx_uint_t  ngx_pagesize;

/**
 * @brief 系统页面大小的位移值（用于快速计算）
 */
extern ngx_uint_t  ngx_pagesize_shift;

/**
 * @brief CPU缓存行大小
 */
extern ngx_uint_t  ngx_cacheline_size;


#endif /* _NGX_ALLOC_H_INCLUDED_ */
