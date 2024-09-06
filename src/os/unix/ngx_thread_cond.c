
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_thread_cond.c
 *
 * 该文件实现了Nginx在Unix系统上的条件变量操作功能。
 *
 * 支持的功能:
 * 1. 创建条件变量 (ngx_thread_cond_create)
 * 2. 销毁条件变量 (ngx_thread_cond_destroy)
 * 3. 发送条件变量信号 (ngx_thread_cond_signal)
 * 4. 广播条件变量信号 (ngx_thread_cond_broadcast)
 * 5. 等待条件变量 (ngx_thread_cond_wait)
 * 6. 带超时的条件变量等待 (ngx_thread_cond_timedwait)
 *
 * 使用注意点:
 * 1. 条件变量必须与互斥锁配合使用，以避免竞态条件
 * 2. 创建和销毁操作应成对出现，避免资源泄漏
 * 3. 在多线程环境中使用时需注意同步问题
 * 4. 错误处理应当合理，避免因条件变量操作失败导致程序异常
 * 5. 使用timedwait时需要正确设置超时时间，避免无限等待
 * 6. 在循环中使用wait操作时，需要重新检查条件，以防虚假唤醒
 * 7. 广播操作可能会导致性能问题，应谨慎使用
 */



#include <ngx_config.h>
#include <ngx_core.h>


ngx_int_t
ngx_thread_cond_create(ngx_thread_cond_t *cond, ngx_log_t *log)
{
    ngx_err_t  err;

    err = pthread_cond_init(cond, NULL);
    if (err == 0) {
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_EMERG, log, err, "pthread_cond_init() failed");
    return NGX_ERROR;
}


ngx_int_t
ngx_thread_cond_destroy(ngx_thread_cond_t *cond, ngx_log_t *log)
{
    ngx_err_t  err;

    err = pthread_cond_destroy(cond);
    if (err == 0) {
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_EMERG, log, err, "pthread_cond_destroy() failed");
    return NGX_ERROR;
}


ngx_int_t
ngx_thread_cond_signal(ngx_thread_cond_t *cond, ngx_log_t *log)
{
    ngx_err_t  err;

    err = pthread_cond_signal(cond);
    if (err == 0) {
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_EMERG, log, err, "pthread_cond_signal() failed");
    return NGX_ERROR;
}


ngx_int_t
ngx_thread_cond_wait(ngx_thread_cond_t *cond, ngx_thread_mutex_t *mtx,
    ngx_log_t *log)
{
    ngx_err_t  err;

    err = pthread_cond_wait(cond, mtx);

#if 0
    ngx_time_update();
#endif

    if (err == 0) {
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_ALERT, log, err, "pthread_cond_wait() failed");

    return NGX_ERROR;
}
