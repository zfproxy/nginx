
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_thread_id.c
 *
 * 本文件实现了获取线程ID的功能，支持多个操作系统平台。
 *
 * 支持的功能：
 * 1. 获取当前线程的唯一标识符（线程ID）
 *
 * 支持的平台：
 * - Linux
 * - FreeBSD (版本 >= 900031)
 * - MacOSX (Darwin)
 *
 * 使用注意点：
 * 1. 不同平台下，获取线程ID的方法有所不同，请根据目标平台选择适当的实现。
 * 2. 在Linux平台下，使用系统调用SYS_gettid获取线程ID。
 * 3. 在FreeBSD平台下，使用pthread_getthreadid_np()函数获取线程ID。
 * 4. 在MacOSX平台下，使用pthread_threadid_np()函数获取线程ID。
 * 5. 返回的线程ID类型为ngx_tid_t，在不同平台下可能有不同的具体类型。
 * 6. 本文件依赖于ngx_config.h、ngx_core.h和ngx_thread_pool.h头文件。
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_thread_pool.h>


#if (NGX_LINUX)

/*
 * Linux thread id is a pid of thread created by clone(2),
 * glibc does not provide a wrapper for gettid().
 */

ngx_tid_t
ngx_thread_tid(void)
{
    return syscall(SYS_gettid);
}

#elif (NGX_FREEBSD) && (__FreeBSD_version >= 900031)

#include <pthread_np.h>

ngx_tid_t
ngx_thread_tid(void)
{
    return pthread_getthreadid_np();
}

#elif (NGX_DARWIN)

/*
 * MacOSX thread has two thread ids:
 *
 * 1) MacOSX 10.6 (Snow Leoprad) has pthread_threadid_np() returning
 *    an uint64_t value, which is obtained using the __thread_selfid()
 *    syscall.  It is a number above 300,000.
 */

ngx_tid_t
ngx_thread_tid(void)
{
    uint64_t  tid;

    (void) pthread_threadid_np(NULL, &tid);
    return tid;
}

/*
 * 2) Kernel thread mach_port_t returned by pthread_mach_thread_np().
 *    It is a number in range 100-100,000.
 *
 * return pthread_mach_thread_np(pthread_self());
 */

#else

ngx_tid_t
ngx_thread_tid(void)
{
    return (uint64_t) (uintptr_t) pthread_self();
}

#endif
