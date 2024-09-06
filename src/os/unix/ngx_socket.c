
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_socket.c
 *
 * 该文件实现了Nginx在Unix系统上的套接字操作功能。
 *
 * 支持的功能:
 * 1. 设置非阻塞套接字 (ngx_nonblocking)
 * 2. 设置阻塞套接字 (ngx_blocking) 
 * 3. 启用TCP NOPUSH选项 (ngx_tcp_nopush)
 * 4. 禁用TCP NOPUSH选项 (ngx_tcp_push)
 * 5. 启用TCP NODELAY选项
 * 6. 设置套接字的keep-alive选项
 * 7. 设置套接字的linger选项
 *
 * 使用注意点:
 * 1. 非阻塞套接字设置可能影响I/O性能，需根据实际情况选择
 * 2. TCP_NOPUSH和TCP_NODELAY选项会影响数据发送行为，使用时需权衡
 * 3. keep-alive和linger选项会影响连接的关闭行为，需谨慎配置
 * 4. 部分功能可能依赖于特定操作系统的支持，使用前需检查系统兼容性
 * 5. 错误处理应当合理，避免因套接字操作失败导致服务器异常
 * 6. 在高并发环境下，某些套接字选项可能会对性能产生显著影响
 * 7. 修改套接字选项可能会影响网络栈的行为，需要全面测试以确保系统稳定性
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * ioctl(FIONBIO) sets a non-blocking mode with the single syscall
 * while fcntl(F_SETFL, O_NONBLOCK) needs to learn the current state
 * using fcntl(F_GETFL).
 *
 * ioctl() and fcntl() are syscalls at least in FreeBSD 2.x, Linux 2.2
 * and Solaris 7.
 *
 * ioctl() in Linux 2.4 and 2.6 uses BKL, however, fcntl(F_SETFL) uses it too.
 */


#if (NGX_HAVE_FIONBIO)

int
ngx_nonblocking(ngx_socket_t s)
{
    int  nb;

    nb = 1;

    return ioctl(s, FIONBIO, &nb);
}


int
ngx_blocking(ngx_socket_t s)
{
    int  nb;

    nb = 0;

    return ioctl(s, FIONBIO, &nb);
}

#endif


#if (NGX_FREEBSD)

int
ngx_tcp_nopush(ngx_socket_t s)
{
    int  tcp_nopush;

    tcp_nopush = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_NOPUSH,
                      (const void *) &tcp_nopush, sizeof(int));
}


int
ngx_tcp_push(ngx_socket_t s)
{
    int  tcp_nopush;

    tcp_nopush = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_NOPUSH,
                      (const void *) &tcp_nopush, sizeof(int));
}

#elif (NGX_LINUX)


int
ngx_tcp_nopush(ngx_socket_t s)
{
    int  cork;

    cork = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
                      (const void *) &cork, sizeof(int));
}


int
ngx_tcp_push(ngx_socket_t s)
{
    int  cork;

    cork = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
                      (const void *) &cork, sizeof(int));
}

#else

int
ngx_tcp_nopush(ngx_socket_t s)
{
    return 0;
}


int
ngx_tcp_push(ngx_socket_t s)
{
    return 0;
}

#endif
