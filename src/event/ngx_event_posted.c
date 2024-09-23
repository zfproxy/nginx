
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_event_posted.c
 *
 * 该文件实现了Nginx的事件发布和处理机制，用于管理已发布的事件队列。
 *
 * 支持的功能:
 * 1. 事件发布到不同优先级的队列
 * 2. 处理已发布的事件
 * 3. 移动下一轮要处理的事件到当前队列
 *
 * 支持的指令:
 * - 无直接相关的配置指令
 *
 * 相关变量:
 * - ngx_posted_accept_events: 接受连接事件队列
 * - ngx_posted_next_events: 下一轮要处理的事件队列
 * - ngx_posted_events: 普通事件队列
 *
 * 使用注意点:
 * 1. 合理使用不同优先级的事件队列，避免高优先级事件被延迟处理
 * 2. 注意处理事件时可能产生的递归调用，防止栈溢出
 * 3. 在多工作进程模式下，需要注意事件队列的同步问题
 * 4. 定期检查事件队列的长度，避免队列过长导致性能下降
 * 5. 在处理事件时注意捕获和处理可能的异常，确保程序稳定性
 * 6. 合理设置日志级别，便于问题排查和性能优化
 * 7. 注意事件处理函数的执行时间，避免阻塞事件循环
 * 8. 在使用ngx_event_move_posted_next时，需要注意事件的ready和available状态设置
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


/* 接受连接事件队列 */
ngx_queue_t  ngx_posted_accept_events;

/* 下一轮要处理的事件队列 */
ngx_queue_t  ngx_posted_next_events;

/* 普通事件队列 */
ngx_queue_t  ngx_posted_events;


/*
 * ngx_event_process_posted - 处理已发布的事件
 *
 * 该函数用于处理指定队列中的已发布事件。
 * 它会遍历队列中的所有事件，并依次调用每个事件的处理函数。
 *
 * 参数:
 *   cycle: Nginx 核心周期结构体指针
 *   posted: 待处理的事件队列
 *
 * 处理流程:
 * 1. 循环检查队列是否为空
 * 2. 获取队列头部的事件
 * 3. 记录调试日志
 * 4. 从队列中删除该事件
 * 5. 调用事件的处理函数
 *
 * 注意事项:
 * - 确保事件处理函数不会长时间阻塞，以免影响其他事件的处理
 * - 处理函数可能会触发新的事件发布，需要注意避免无限递归
 * - 在多线程环境下使用时，需要考虑同步问题
 */
void
ngx_event_process_posted(ngx_cycle_t *cycle, ngx_queue_t *posted)
{
    ngx_queue_t  *q;
    ngx_event_t  *ev;

    // 循环处理队列中的所有事件
    while (!ngx_queue_empty(posted)) {

        // 获取队列头部的事件
        q = ngx_queue_head(posted);
        ev = ngx_queue_data(q, ngx_event_t, queue);

        // 记录调试日志
        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                      "posted event %p", ev);

        // 从队列中删除该事件
        ngx_delete_posted_event(ev);

        // 调用事件的处理函数
        ev->handler(ev);
    }
}


void
ngx_event_move_posted_next(ngx_cycle_t *cycle)
{
    ngx_queue_t  *q;
    ngx_event_t  *ev;

    for (q = ngx_queue_head(&ngx_posted_next_events);
         q != ngx_queue_sentinel(&ngx_posted_next_events);
         q = ngx_queue_next(q))
    {
        ev = ngx_queue_data(q, ngx_event_t, queue);

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                      "posted next event %p", ev);

        ev->ready = 1;
        ev->available = -1;
    }

    ngx_queue_add(&ngx_posted_events, &ngx_posted_next_events);
    ngx_queue_init(&ngx_posted_next_events);
}
