
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_list.c
 *
 * 本文件包含Nginx链表数据结构的实现。
 *
 * 主要功能:
 * 1. 创建新的链表 (ngx_list_create)
 * 2. 初始化已存在的链表 (ngx_list_init)
 * 3. 向链表中添加新元素 (ngx_list_push)
 *
 * 支持的操作:
 * - 动态分配新的链表节点
 * - 自动扩展链表容量
 * - 高效的内存管理（基于内存池）
 *
 * 使用注意:
 * - 确保在使用链表前正确初始化
 * - 注意内存池的生命周期，它决定了链表数据的有效期
 * - 添加元素时要检查返回值，确保操作成功
 * - 不支持直接删除单个元素，如需删除请考虑重建链表
 * - 遍历链表时需要考虑多个节点的情况
 * - 链表不保证元素顺序，如需排序功能需自行实现
 */


#include <ngx_config.h>
#include <ngx_core.h>


ngx_list_t *
ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    ngx_list_t  *list;

    list = ngx_palloc(pool, sizeof(ngx_list_t));
    if (list == NULL) {
        return NULL;
    }

    if (ngx_list_init(list, pool, n, size) != NGX_OK) {
        return NULL;
    }

    return list;
}


void *
ngx_list_push(ngx_list_t *l)
{
    void             *elt;
    ngx_list_part_t  *last;

    last = l->last;

    if (last->nelts == l->nalloc) {

        /* the last part is full, allocate a new list part */

        last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL) {
            return NULL;
        }

        last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL) {
            return NULL;
        }

        last->nelts = 0;
        last->next = NULL;

        l->last->next = last;
        l->last = last;
    }

    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}
