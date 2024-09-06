
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_queue.c
 *
 * 该文件实现了Nginx的双向队列功能。
 *
 * 支持的功能:
 * 1. 队列初始化 (ngx_queue_init)
 * 2. 判断队列是否为空 (ngx_queue_empty) 
 * 3. 在队列头部插入元素 (ngx_queue_insert_head)
 * 4. 在队列尾部插入元素 (ngx_queue_insert_tail)
 * 5. 获取队列头部元素 (ngx_queue_head)
 * 6. 获取队列尾部元素 (ngx_queue_last)
 * 7. 获取队列中间元素 (ngx_queue_middle)
 * 8. 队列排序 (ngx_queue_sort)
 *
 * 使用注意点:
 * 1. 使用前必须先初始化队列
 * 2. 插入和删除操作需要注意保持队列的完整性
 * 3. 队列操作不是线程安全的，多线程环境下需要额外同步措施
 * 4. 排序操作会改变队列元素的顺序，但不会改变元素本身的内容
 * 5. 队列操作主要通过宏实现，使用时要注意可能的副作用
 * 6. 队列中的元素通常是更大结构体中的一个成员，使用时需要小心处理偏移量
 * 7. 遍历队列时要注意可能的无限循环，特别是在修改队列结构时
 * 8. 队列不负责内存管理，插入和删除元素时需要自行处理内存分配和释放
 * 9. 获取中间元素的操作对于偶数个元素时会返回下半部分的第一个元素
 * 10. 排序操作使用稳定的归并排序算法，需要提供比较函数
 */


#include <ngx_config.h>
#include <ngx_core.h>


static void ngx_queue_merge(ngx_queue_t *queue, ngx_queue_t *tail,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *));


/*
 * find the middle queue element if the queue has odd number of elements
 * or the first element of the queue's second part otherwise
 */

ngx_queue_t *
ngx_queue_middle(ngx_queue_t *queue)
{
    ngx_queue_t  *middle, *next;

    middle = ngx_queue_head(queue);

    if (middle == ngx_queue_last(queue)) {
        return middle;
    }

    next = ngx_queue_head(queue);

    for ( ;; ) {
        middle = ngx_queue_next(middle);

        next = ngx_queue_next(next);

        if (next == ngx_queue_last(queue)) {
            return middle;
        }

        next = ngx_queue_next(next);

        if (next == ngx_queue_last(queue)) {
            return middle;
        }
    }
}


/* the stable merge sort */

void
ngx_queue_sort(ngx_queue_t *queue,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *))
{
    ngx_queue_t  *q, tail;

    q = ngx_queue_head(queue);

    if (q == ngx_queue_last(queue)) {
        return;
    }

    q = ngx_queue_middle(queue);

    ngx_queue_split(queue, q, &tail);

    ngx_queue_sort(queue, cmp);
    ngx_queue_sort(&tail, cmp);

    ngx_queue_merge(queue, &tail, cmp);
}


static void
ngx_queue_merge(ngx_queue_t *queue, ngx_queue_t *tail,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *))
{
    ngx_queue_t  *q1, *q2;

    q1 = ngx_queue_head(queue);
    q2 = ngx_queue_head(tail);

    for ( ;; ) {
        if (q1 == ngx_queue_sentinel(queue)) {
            ngx_queue_add(queue, tail);
            break;
        }

        if (q2 == ngx_queue_sentinel(tail)) {
            break;
        }

        if (cmp(q1, q2) <= 0) {
            q1 = ngx_queue_next(q1);
            continue;
        }

        ngx_queue_remove(q2);
        ngx_queue_insert_before(q1, q2);

        q2 = ngx_queue_head(tail);
    }
}
