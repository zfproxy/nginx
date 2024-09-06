
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef _NGX_QUEUE_H_INCLUDED_
#define _NGX_QUEUE_H_INCLUDED_


// 定义队列结构体类型别名
typedef struct ngx_queue_s  ngx_queue_t;

// 队列结构体定义
struct ngx_queue_s {
    ngx_queue_t  *prev;  // 指向前一个节点
    ngx_queue_t  *next;  // 指向后一个节点
};


// 初始化队列
#define ngx_queue_init(q)                                                     \
    (q)->prev = q;                                                            \
    (q)->next = q


// 检查队列是否为空
#define ngx_queue_empty(h)                                                    \
    (h == (h)->prev)


// 在队列头部插入节点
#define ngx_queue_insert_head(h, x)                                           \
    (x)->next = (h)->next;                                                    \
    (x)->next->prev = x;                                                      \
    (x)->prev = h;                                                            \
    (h)->next = x


// 在节点后插入新节点（等同于在头部插入）
#define ngx_queue_insert_after   ngx_queue_insert_head


// 在队列尾部插入节点
#define ngx_queue_insert_tail(h, x)                                           \
    (x)->prev = (h)->prev;                                                    \
    (x)->prev->next = x;                                                      \
    (x)->next = h;                                                            \
    (h)->prev = x


// 在节点前插入新节点（等同于在尾部插入）
#define ngx_queue_insert_before   ngx_queue_insert_tail


// 获取队列头节点
#define ngx_queue_head(h)                                                     \
    (h)->next


// 获取队列尾节点
#define ngx_queue_last(h)                                                     \
    (h)->prev


// 获取队列哨兵节点
#define ngx_queue_sentinel(h)                                                 \
    (h)


// 获取下一个节点
#define ngx_queue_next(q)                                                     \
    (q)->next


// 获取前一个节点
#define ngx_queue_prev(q)                                                     \
    (q)->prev


#if (NGX_DEBUG)

// 调试模式下的节点移除（包含额外的空指针设置）
#define ngx_queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next;                                              \
    (x)->prev = NULL;                                                         \
    (x)->next = NULL

#else

// 非调试模式下的节点移除
#define ngx_queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next

#endif


// 分割队列
#define ngx_queue_split(h, q, n)                                              \
    (n)->prev = (h)->prev;                                                    \
    (n)->prev->next = n;                                                      \
    (n)->next = q;                                                            \
    (h)->prev = (q)->prev;                                                    \
    (h)->prev->next = h;                                                      \
    (q)->prev = n;


// 合并两个队列
#define ngx_queue_add(h, n)                                                   \
    (h)->prev->next = (n)->next;                                              \
    (n)->next->prev = (h)->prev;                                              \
    (h)->prev = (n)->prev;                                                    \
    (h)->prev->next = h;


// 获取包含队列节点的数据结构
#define ngx_queue_data(q, type, link)                                         \
    (type *) ((u_char *) q - offsetof(type, link))


// 获取队列中间节点
ngx_queue_t *ngx_queue_middle(ngx_queue_t *queue);

// 队列排序
void ngx_queue_sort(ngx_queue_t *queue,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *));


#endif /* _NGX_QUEUE_H_INCLUDED_ */
