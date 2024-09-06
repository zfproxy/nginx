
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_event_timer.c
 *
 * 该文件实现了Nginx的事件定时器功能，用于管理和处理定时事件。
 *
 * 支持的功能:
 * 1. 初始化定时器红黑树
 * 2. 添加定时器事件
 * 3. 删除定时器事件
 * 4. 查找最近的定时器事件
 * 5. 处理到期的定时器事件
 *
 * 支持的指令:
 * - worker_connections (影响最大定时器事件数)
 * - timer_resolution (影响定时器精度)
 *
 * 相关变量:
 * - ngx_event_timer_rbtree: 定时器红黑树
 * - ngx_event_timer_sentinel: 红黑树哨兵节点
 * - ngx_current_msec: 当前时间戳(毫秒)
 *
 * 使用注意点:
 * 1. 定时器事件应及时删除，避免内存泄漏
 * 2. 高并发场景下需注意定时器数量对性能的影响
 * 3. 定时器精度受系统时钟分辨率影响
 * 4. 避免设置过多的短期定时器，可能影响系统性能
 * 5. 定时器回调函数应尽量简短，避免阻塞事件循环
 * 6. 注意处理定时器溢出的情况
 * 7. 合理设置timer_resolution，平衡精度和性能
 * 8. 在多工作进程模式下，每个进程有独立的定时器树
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


ngx_rbtree_t              ngx_event_timer_rbtree;
static ngx_rbtree_node_t  ngx_event_timer_sentinel;

/*
 * the event timer rbtree may contain the duplicate keys, however,
 * it should not be a problem, because we use the rbtree to find
 * a minimum timer value only
 */

ngx_int_t
ngx_event_timer_init(ngx_log_t *log)
{
    ngx_rbtree_init(&ngx_event_timer_rbtree, &ngx_event_timer_sentinel,
                    ngx_rbtree_insert_timer_value);

    return NGX_OK;
}


ngx_msec_t
ngx_event_find_timer(void)
{
    ngx_msec_int_t      timer;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    if (ngx_event_timer_rbtree.root == &ngx_event_timer_sentinel) {
        return NGX_TIMER_INFINITE;
    }

    root = ngx_event_timer_rbtree.root;
    sentinel = ngx_event_timer_rbtree.sentinel;

    node = ngx_rbtree_min(root, sentinel);

    timer = (ngx_msec_int_t) (node->key - ngx_current_msec);

    return (ngx_msec_t) (timer > 0 ? timer : 0);
}


void
ngx_event_expire_timers(void)
{
    ngx_event_t        *ev;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    sentinel = ngx_event_timer_rbtree.sentinel;

    for ( ;; ) {
        root = ngx_event_timer_rbtree.root;

        if (root == sentinel) {
            return;
        }

        node = ngx_rbtree_min(root, sentinel);

        /* node->key > ngx_current_msec */

        if ((ngx_msec_int_t) (node->key - ngx_current_msec) > 0) {
            return;
        }

        ev = ngx_rbtree_data(node, ngx_event_t, timer);

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                       "event timer del: %d: %M",
                       ngx_event_ident(ev->data), ev->timer.key);

        ngx_rbtree_delete(&ngx_event_timer_rbtree, &ev->timer);

#if (NGX_DEBUG)
        ev->timer.left = NULL;
        ev->timer.right = NULL;
        ev->timer.parent = NULL;
#endif

        ev->timer_set = 0;

        ev->timedout = 1;

        ev->handler(ev);
    }
}


ngx_int_t
ngx_event_no_timers_left(void)
{
    ngx_event_t        *ev;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    sentinel = ngx_event_timer_rbtree.sentinel;
    root = ngx_event_timer_rbtree.root;

    if (root == sentinel) {
        return NGX_OK;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&ngx_event_timer_rbtree, node))
    {
        ev = ngx_rbtree_data(node, ngx_event_t, timer);

        if (!ev->cancelable) {
            return NGX_AGAIN;
        }
    }

    /* only cancelable timers left */

    return NGX_OK;
}
