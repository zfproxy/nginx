
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_linux_init.c
 *
 * 本文件包含 Nginx 在 Linux 系统上的初始化和特定功能实现。
 *
 * 主要功能：
 * 1. 获取 Linux 系统信息（内核类型和版本）
 * 2. 初始化 Linux 特定的 I/O 操作
 * 3. 设置 Nginx 在 Linux 上的特定配置
 *
 * 使用注意：
 * - 本模块依赖于 <ngx_config.h> 和 <ngx_core.h>
 * - 确保在编译时正确设置了 NGX_HAVE_SENDFILE 宏，以启用 sendfile 功能
 * - ngx_os_specific_init() 函数在 Nginx 启动时被调用，用于初始化 Linux 特定设置
 * - ngx_os_specific_status() 函数用于输出 Linux 系统状态信息
 */


#include <ngx_config.h>
#include <ngx_core.h>


// 存储Linux内核操作系统类型的缓冲区
u_char  ngx_linux_kern_ostype[50];

// 存储Linux内核操作系统版本的缓冲区
u_char  ngx_linux_kern_osrelease[50];


// 定义Linux系统特定的I/O操作结构体
static ngx_os_io_t ngx_linux_io = {
    ngx_unix_recv,              // 接收数据的函数
    ngx_readv_chain,            // 读取链式数据的函数
    ngx_udp_unix_recv,          // UDP接收数据的函数
    ngx_unix_send,              // 发送数据的函数
    ngx_udp_unix_send,          // UDP发送数据的函数
    ngx_udp_unix_sendmsg_chain, // UDP发送链式消息的函数
#if (NGX_HAVE_SENDFILE)
    ngx_linux_sendfile_chain,   // 使用sendfile发送文件的函数
    NGX_IO_SENDFILE             // 启用sendfile功能的标志
#else
    ngx_writev_chain,           // 不使用sendfile时的写入函数
    0                           // 不启用sendfile功能的标志
#endif
};


ngx_int_t
ngx_os_specific_init(ngx_log_t *log)
{
    struct utsname  u;

    if (uname(&u) == -1) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno, "uname() failed");
        return NGX_ERROR;
    }

    (void) ngx_cpystrn(ngx_linux_kern_ostype, (u_char *) u.sysname,
                       sizeof(ngx_linux_kern_ostype));

    (void) ngx_cpystrn(ngx_linux_kern_osrelease, (u_char *) u.release,
                       sizeof(ngx_linux_kern_osrelease));

    ngx_os_io = ngx_linux_io;

    return NGX_OK;
}


void
ngx_os_specific_status(ngx_log_t *log)
{
    ngx_log_error(NGX_LOG_NOTICE, log, 0, "OS: %s %s",
                  ngx_linux_kern_ostype, ngx_linux_kern_osrelease);
}
