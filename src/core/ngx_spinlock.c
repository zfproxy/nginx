
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/**
 * @file ngx_spinlock.c
 * @brief Nginx自旋锁实现
 *
 * 本文件实现了Nginx的自旋锁功能，用于多线程环境下的同步控制。
 *
 * 主要功能:
 * - 实现自旋锁的获取和释放
 * - 支持多CPU环境下的自适应自旋
 * - 提供原子操作支持
 * 
 * 使用注意:
 * 1. 自旋锁适用于短期持有的锁，避免长时间占用造成其他线程忙等
 * 2. 在单CPU系统上自旋锁可能会降低性能，应谨慎使用
 * 3. 确保正确释放锁，防止死锁
 * 4. 自旋次数应根据实际情况调整，以平衡等待时间和CPU占用
 * 5. 在不支持原子操作的平台上，需要提供替代实现
 * 6. 使用自旋锁时要注意避免优先级反转问题
 * 7. 在中断上下文中使用时需格外小心，确保不会导致系统死锁
 */


#include <ngx_config.h>
#include <ngx_core.h>


void
ngx_spinlock(ngx_atomic_t *lock, ngx_atomic_int_t value, ngx_uint_t spin)
{

#if (NGX_HAVE_ATOMIC_OPS)

    ngx_uint_t  i, n;

    for ( ;; ) {

        if (*lock == 0 && ngx_atomic_cmp_set(lock, 0, value)) {
            return;
        }

        if (ngx_ncpu > 1) {

            for (n = 1; n < spin; n <<= 1) {

                for (i = 0; i < n; i++) {
                    ngx_cpu_pause();
                }

                if (*lock == 0 && ngx_atomic_cmp_set(lock, 0, value)) {
                    return;
                }
            }
        }

        ngx_sched_yield();
    }

#else

#if (NGX_THREADS)

#error ngx_spinlock() or ngx_atomic_cmp_set() are not defined !

#endif

#endif

}
