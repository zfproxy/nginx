
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SHMTX_H_INCLUDED_
#define _NGX_SHMTX_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 共享内存互斥锁的共享部分结构体
 */
typedef struct {
    ngx_atomic_t   lock;  /**< 原子锁 */
#if (NGX_HAVE_POSIX_SEM)
    ngx_atomic_t   wait;  /**< 等待计数器 */
#endif
} ngx_shmtx_sh_t;


/**
 * @brief 共享内存互斥锁结构体
 */
typedef struct {
#if (NGX_HAVE_ATOMIC_OPS)
    ngx_atomic_t  *lock;  /**< 指向原子锁的指针 */
#if (NGX_HAVE_POSIX_SEM)
    ngx_atomic_t  *wait;  /**< 指向等待计数器的指针 */
    ngx_uint_t     semaphore;  /**< 信号量标志 */
    sem_t          sem;   /**< POSIX信号量 */
#endif
#else
    ngx_fd_t       fd;    /**< 文件描述符 */
    u_char        *name;  /**< 锁文件名 */
#endif
    ngx_uint_t     spin;  /**< 自旋锁次数 */
} ngx_shmtx_t;


/**
 * @brief 创建共享内存互斥锁
 * @param mtx 互斥锁结构体指针
 * @param addr 共享内存地址
 * @param name 锁名称
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_shmtx_create(ngx_shmtx_t *mtx, ngx_shmtx_sh_t *addr,
    u_char *name);

/**
 * @brief 销毁共享内存互斥锁
 * @param mtx 互斥锁结构体指针
 */
void ngx_shmtx_destroy(ngx_shmtx_t *mtx);

/**
 * @brief 尝试获取共享内存互斥锁
 * @param mtx 互斥锁结构体指针
 * @return 成功获取锁返回1，否则返回0
 */
ngx_uint_t ngx_shmtx_trylock(ngx_shmtx_t *mtx);

/**
 * @brief 获取共享内存互斥锁
 * @param mtx 互斥锁结构体指针
 */
void ngx_shmtx_lock(ngx_shmtx_t *mtx);

/**
 * @brief 释放共享内存互斥锁
 * @param mtx 互斥锁结构体指针
 */
void ngx_shmtx_unlock(ngx_shmtx_t *mtx);

/**
 * @brief 强制解锁共享内存互斥锁
 * @param mtx 互斥锁结构体指针
 * @param pid 进程ID
 * @return 成功解锁返回1，否则返回0
 */
ngx_uint_t ngx_shmtx_force_unlock(ngx_shmtx_t *mtx, ngx_pid_t pid);


#endif /* _NGX_SHMTX_H_INCLUDED_ */
