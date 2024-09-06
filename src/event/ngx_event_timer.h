
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_TIMER_H_INCLUDED_
#define _NGX_EVENT_TIMER_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


/* 定义无限定时器的宏 */
#define NGX_TIMER_INFINITE  (ngx_msec_t) -1

/* 定义延迟定时器的阈值（毫秒） */
#define NGX_TIMER_LAZY_DELAY  300


/* 初始化事件定时器 */
ngx_int_t ngx_event_timer_init(ngx_log_t *log);

/* 查找最近的定时器事件，返回到期时间 */
ngx_msec_t ngx_event_find_timer(void);

/* 处理已到期的定时器事件 */
void ngx_event_expire_timers(void);

/* 检查是否还有未处理的定时器事件 */
ngx_int_t ngx_event_no_timers_left(void);


/* 全局定时器红黑树 */
extern ngx_rbtree_t  ngx_event_timer_rbtree;


/* 从定时器中删除事件 */
static ngx_inline void
ngx_event_del_timer(ngx_event_t *ev)
{
    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "event timer del: %d: %M",
                    ngx_event_ident(ev->data), ev->timer.key);

    /* 从红黑树中删除定时器节点 */
    ngx_rbtree_delete(&ngx_event_timer_rbtree, &ev->timer);

#if (NGX_DEBUG)
    /* 在调试模式下，清空定时器节点的指针 */
    ev->timer.left = NULL;
    ev->timer.right = NULL;
    ev->timer.parent = NULL;
#endif

    /* 标记定时器未设置 */
    ev->timer_set = 0;
}


/* 向定时器中添加事件 */
static ngx_inline void
ngx_event_add_timer(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t      key;
    ngx_msec_int_t  diff;

    /* 计算定时器的过期时间 */
    key = ngx_current_msec + timer;

    if (ev->timer_set) {
        /*
         * 如果定时器已经设置，且新旧过期时间的差值小于NGX_TIMER_LAZY_DELAY，
         * 则保留原定时器，以减少红黑树操作，提高性能
         */
        diff = (ngx_msec_int_t) (key - ev->timer.key);

        if (ngx_abs(diff) < NGX_TIMER_LAZY_DELAY) {
            ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                           "event timer: %d, old: %M, new: %M",
                            ngx_event_ident(ev->data), ev->timer.key, key);
            return;
        }

        /* 如果差值较大，则先删除原定时器 */
        ngx_del_timer(ev);
    }

    /* 设置新的过期时间 */
    ev->timer.key = key;

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "event timer add: %d: %M:%M",
                    ngx_event_ident(ev->data), timer, ev->timer.key);

    /* 将定时器节点插入红黑树 */
    ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);

    /* 标记定时器已设置 */
    ev->timer_set = 1;
}


#endif /* _NGX_EVENT_TIMER_H_INCLUDED_ */
