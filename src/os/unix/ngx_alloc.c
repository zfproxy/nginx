
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_alloc.c - Nginx内存分配模块
 *
 * 本文件实现了Nginx的内存分配相关功能，包括:
 * - 基本内存分配 (ngx_alloc)
 * - 清零内存分配 (ngx_calloc) 
 * - 内存对齐分配 (ngx_memalign)
 * - 页面大小和缓存行大小的初始化
 *
 * 使用注意:
 * 1. 所有分配函数都需要传入ngx_log_t参数用于错误日志
 * 2. 分配失败时会记录错误日志,但不会中止程序运行
 * 3. 使用ngx_free释放通过这些函数分配的内存
 * 4. 大块内存分配请考虑使用ngx_palloc等内存池函数
 */


#include <ngx_config.h>
#include <ngx_core.h>


// 系统页面大小
ngx_uint_t  ngx_pagesize;

// 系统页面大小的位移值(用于快速计算)
ngx_uint_t  ngx_pagesize_shift;

// CPU缓存行大小
ngx_uint_t  ngx_cacheline_size;


void *
ngx_alloc(size_t size, ngx_log_t *log)
{
    void  *p;

    p = malloc(size);
    if (p == NULL) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "malloc(%uz) failed", size);
    }

    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, log, 0, "malloc: %p:%uz", p, size);

    return p;
}


void *
ngx_calloc(size_t size, ngx_log_t *log)
{
    void  *p;

    p = ngx_alloc(size, log);

    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}


#if (NGX_HAVE_POSIX_MEMALIGN)

void *
ngx_memalign(size_t alignment, size_t size, ngx_log_t *log)
{
    void  *p;
    int    err;

    err = posix_memalign(&p, alignment, size);

    if (err) {
        ngx_log_error(NGX_LOG_EMERG, log, err,
                      "posix_memalign(%uz, %uz) failed", alignment, size);
        p = NULL;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_ALLOC, log, 0,
                   "posix_memalign: %p:%uz @%uz", p, size, alignment);

    return p;
}

#elif (NGX_HAVE_MEMALIGN)

void *
ngx_memalign(size_t alignment, size_t size, ngx_log_t *log)
{
    void  *p;

    p = memalign(alignment, size);
    if (p == NULL) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "memalign(%uz, %uz) failed", alignment, size);
    }

    ngx_log_debug3(NGX_LOG_DEBUG_ALLOC, log, 0,
                   "memalign: %p:%uz @%uz", p, size, alignment);

    return p;
}

#endif
