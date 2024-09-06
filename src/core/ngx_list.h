
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_LIST_H_INCLUDED_
#define _NGX_LIST_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 链表节点结构体的前向声明
 */
typedef struct ngx_list_part_s  ngx_list_part_t;

/**
 * @brief 链表节点结构体定义
 */
struct ngx_list_part_s {
    void             *elts;    /**< 指向存储元素的内存块 */
    ngx_uint_t        nelts;   /**< 当前节点中元素的数量 */
    ngx_list_part_t  *next;    /**< 指向下一个节点 */
};

/**
 * @brief 链表结构体定义
 */
typedef struct {
    ngx_list_part_t  *last;    /**< 指向最后一个节点 */
    ngx_list_part_t   part;    /**< 第一个节点 */
    size_t            size;    /**< 每个元素的大小 */
    ngx_uint_t        nalloc;  /**< 每个节点可以容纳的元素数量 */
    ngx_pool_t       *pool;    /**< 内存池指针 */
} ngx_list_t;

/**
 * @brief 创建一个新的链表
 * @param pool 内存池
 * @param n 每个节点可以容纳的元素数量
 * @param size 每个元素的大小
 * @return 返回创建的链表指针，如果创建失败则返回NULL
 */
ngx_list_t *ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size);

/**
 * @brief 初始化一个已存在的链表
 * @param list 要初始化的链表指针
 * @param pool 内存池
 * @param n 每个节点可以容纳的元素数量
 * @param size 每个元素的大小
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
static ngx_inline ngx_int_t
ngx_list_init(ngx_list_t *list, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    list->part.elts = ngx_palloc(pool, n * size);
    if (list->part.elts == NULL) {
        return NGX_ERROR;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return NGX_OK;
}


/*
 *
 *  链表遍历示例:
 *
 *  part = &list.part;
 *  data = part->elts;
 *
 *  for (i = 0 ;; i++) {
 *
 *      if (i >= part->nelts) {
 *          if (part->next == NULL) {
 *              break;
 *          }
 *
 *          part = part->next;
 *          data = part->elts;
 *          i = 0;
 *      }
 *
 *      ...  data[i] ...
 *
 *  }
 */

/**
 * @brief 向链表中添加一个新元素
 * @param list 链表指针
 * @return 返回新添加元素的指针，如果添加失败则返回NULL
 */
void *ngx_list_push(ngx_list_t *list);


#endif /* _NGX_LIST_H_INCLUDED_ */
