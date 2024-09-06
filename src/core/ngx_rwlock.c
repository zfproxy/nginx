
/*
 * Copyright (C) Ruslan Ermilov
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_rwlock.c
 *
 * 该文件实现了Nginx的读写锁功能。
 *
 * 支持的功能:
 * 1. 写锁获取 (ngx_rwlock_wlock)
 * 2. 读锁获取 (ngx_rwlock_rlock)
 * 3. 写锁释放 (ngx_rwlock_unlock)
 * 4. 读锁释放 (ngx_rwlock_unlock)
 * 5. 自旋等待机制
 * 6. 多CPU优化
 *
 * 使用注意点:
 * 1. 确保正确配对使用锁的获取和释放函数
 * 2. 避免在持有写锁时再次获取读锁，可能导致死锁
 * 3. 尽量减少锁的持有时间，以提高并发性能
 * 4. 在多CPU系统上，适当调整自旋等待的次数可能会提高性能
 * 5. 使用原子操作确保锁操作的正确性和性能
 * 6. 注意处理锁获取失败的情况，可能需要重试或采取其他措施
 * 7. 在调试时，可以考虑添加锁状态的日志记录
 * 8. 在高并发场景下，需要注意读写锁可能带来的性能开销
 * 9. 考虑使用条件变量来优化长时间等待的情况
 * 10. 定期检查和优化锁的使用模式，以适应系统的实际需求
 */


#include <ngx_config.h>
#include <ngx_core.h>


#if (NGX_HAVE_ATOMIC_OPS)


#define NGX_RWLOCK_SPIN   2048
#define NGX_RWLOCK_WLOCK  ((ngx_atomic_uint_t) -1)


void
ngx_rwlock_wlock(ngx_atomic_t *lock)
{
    ngx_uint_t  i, n;

    for ( ;; ) {

        if (*lock == 0 && ngx_atomic_cmp_set(lock, 0, NGX_RWLOCK_WLOCK)) {
            return;
        }

        if (ngx_ncpu > 1) {

            for (n = 1; n < NGX_RWLOCK_SPIN; n <<= 1) {

                for (i = 0; i < n; i++) {
                    ngx_cpu_pause();
                }

                if (*lock == 0
                    && ngx_atomic_cmp_set(lock, 0, NGX_RWLOCK_WLOCK))
                {
                    return;
                }
            }
        }

        ngx_sched_yield();
    }
}


void
ngx_rwlock_rlock(ngx_atomic_t *lock)
{
    ngx_uint_t         i, n;
    ngx_atomic_uint_t  readers;

    for ( ;; ) {
        readers = *lock;

        if (readers != NGX_RWLOCK_WLOCK
            && ngx_atomic_cmp_set(lock, readers, readers + 1))
        {
            return;
        }

        if (ngx_ncpu > 1) {

            for (n = 1; n < NGX_RWLOCK_SPIN; n <<= 1) {

                for (i = 0; i < n; i++) {
                    ngx_cpu_pause();
                }

                readers = *lock;

                if (readers != NGX_RWLOCK_WLOCK
                    && ngx_atomic_cmp_set(lock, readers, readers + 1))
                {
                    return;
                }
            }
        }

        ngx_sched_yield();
    }
}


void
ngx_rwlock_unlock(ngx_atomic_t *lock)
{
    if (*lock == NGX_RWLOCK_WLOCK) {
        (void) ngx_atomic_cmp_set(lock, NGX_RWLOCK_WLOCK, 0);
    } else {
        (void) ngx_atomic_fetch_add(lock, -1);
    }
}


void
ngx_rwlock_downgrade(ngx_atomic_t *lock)
{
    if (*lock == NGX_RWLOCK_WLOCK) {
        *lock = 1;
    }
}


#else

#if (NGX_HTTP_UPSTREAM_ZONE || NGX_STREAM_UPSTREAM_ZONE)

#error ngx_atomic_cmp_set() is not defined!

#endif

#endif
