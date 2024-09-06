
/*
 * Copyright (C) Ruslan Ermilov
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_RWLOCK_H_INCLUDED_
#define _NGX_RWLOCK_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 获取写锁
 *
 * 此函数用于获取读写锁的写锁。写锁是独占的，同一时间只能有一个线程持有写锁。
 * 如果当前有其他线程持有读锁或写锁，该函数将阻塞直到获取到写锁。
 *
 * @param lock 指向ngx_atomic_t类型的读写锁
 */
void ngx_rwlock_wlock(ngx_atomic_t *lock);

/**
 * @brief 获取读锁
 *
 * 此函数用于获取读写锁的读锁。读锁是共享的，多个线程可以同时持有读锁。
 * 如果当前有线程持有写锁，该函数将阻塞直到获取到读锁。
 *
 * @param lock 指向ngx_atomic_t类型的读写锁
 */
void ngx_rwlock_rlock(ngx_atomic_t *lock);

/**
 * @brief 释放读写锁
 *
 * 此函数用于释放之前获取的读锁或写锁。无论是读锁还是写锁，都使用此函数释放。
 *
 * @param lock 指向ngx_atomic_t类型的读写锁
 */
void ngx_rwlock_unlock(ngx_atomic_t *lock);

/**
 * @brief 将写锁降级为读锁
 *
 * 此函数用于将当前持有的写锁降级为读锁。这允许其他线程获取读锁，
 * 同时保证当前线程不会失去对共享资源的访问权。
 *
 * @param lock 指向ngx_atomic_t类型的读写锁
 */
void ngx_rwlock_downgrade(ngx_atomic_t *lock);


#endif /* _NGX_RWLOCK_H_INCLUDED_ */
