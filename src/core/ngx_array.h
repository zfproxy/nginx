
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ARRAY_H_INCLUDED_
#define _NGX_ARRAY_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 定义动态数组结构体
 *
 * 这个结构体用于表示一个动态数组，可以根据需要动态增长。
 * 它包含了数组的基本信息，如元素指针、元素数量、元素大小等。
 */
typedef struct {
    void        *elts;    /* 指向数组元素的指针 */
    ngx_uint_t   nelts;   /* 当前数组中的元素数量 */
    size_t       size;    /* 每个元素的大小（字节） */
    ngx_uint_t   nalloc;  /* 当前分配的元素数量（容量） */
    ngx_pool_t  *pool;    /* 用于内存分配的内存池指针 */
} ngx_array_t;


/**
 * @brief 创建一个新的动态数组
 *
 * 该函数用于创建并初始化一个新的动态数组。
 *
 * @param p 内存池指针，用于分配数组内存
 * @param n 初始分配的元素数量
 * @param size 每个元素的大小（字节）
 * @return 返回新创建的动态数组指针，如果创建失败则返回NULL
 */
ngx_array_t *ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size);

/**
 * @brief 销毁动态数组
 *
 * 该函数用于销毁一个动态数组，释放其占用的内存资源。
 *
 * @param a 要销毁的动态数组指针
 */
void ngx_array_destroy(ngx_array_t *a);

/**
 * @brief 向动态数组中添加一个新元素
 *
 * 该函数用于在动态数组的末尾添加一个新元素。如果数组已满，它会自动扩展数组的容量。
 *
 * @param a 指向要操作的动态数组的指针
 * @return 返回新添加元素的指针。如果内存分配失败，则返回NULL
 */
void *ngx_array_push(ngx_array_t *a);

/**
 * @brief 向动态数组中添加多个新元素
 *
 * 该函数用于在动态数组的末尾添加指定数量的新元素。如果数组容量不足，它会自动扩展数组的容量。
 *
 * @param a 指向要操作的动态数组的指针
 * @param n 要添加的新元素数量
 * @return 返回新添加元素块的起始指针。如果内存分配失败，则返回NULL
 */
void *ngx_array_push_n(ngx_array_t *a, ngx_uint_t n);


/**
 * @brief 初始化动态数组
 *
 * 该内联函数用于初始化一个已存在的动态数组结构。
 *
 * @param array 指向要初始化的动态数组的指针
 * @param pool 用于内存分配的内存池指针
 * @param n 初始分配的元素数量
 * @param size 每个元素的大小（字节）
 * @return 返回NGX_OK表示初始化成功，NGX_ERROR表示失败
 */
static ngx_inline ngx_int_t
ngx_array_init(ngx_array_t *array, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;  /* 初始化元素数量为0 */
    array->size = size;  /* 设置每个元素的大小 */
    array->nalloc = n;  /* 设置初始分配的元素数量 */
    array->pool = pool;  /* 设置内存池 */

    /* 分配内存用于存储数组元素 */
    array->elts = ngx_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NGX_ERROR;  /* 内存分配失败，返回错误 */
    }

    return NGX_OK;  /* 初始化成功 */
}

#endif /* _NGX_ARRAY_H_INCLUDED_ */
