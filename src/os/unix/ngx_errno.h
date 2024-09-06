
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ERRNO_H_INCLUDED_
#define _NGX_ERRNO_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


// 定义错误码类型为int
typedef int               ngx_err_t;

// 定义Nginx使用的错误码,对应系统errno值
#define NGX_EPERM         EPERM           // 操作不允许
#define NGX_ENOENT        ENOENT          // 文件或目录不存在
#define NGX_ENOPATH       ENOENT          // 路径不存在
#define NGX_ESRCH         ESRCH           // 没有这个进程
#define NGX_EINTR         EINTR           // 被中断的系统调用
#define NGX_ECHILD        ECHILD          // 没有子进程
#define NGX_ENOMEM        ENOMEM          // 内存不足
#define NGX_EACCES        EACCES          // 权限不足
#define NGX_EBUSY         EBUSY           // 设备或资源忙
#define NGX_EEXIST        EEXIST          // 文件已存在
#define NGX_EEXIST_FILE   EEXIST          // 文件已存在
#define NGX_EXDEV         EXDEV           // 跨设备链接
#define NGX_ENOTDIR       ENOTDIR         // 不是目录
#define NGX_EISDIR        EISDIR          // 是目录
#define NGX_EINVAL        EINVAL          // 无效参数
#define NGX_ENFILE        ENFILE          // 系统打开文件数达到上限
#define NGX_EMFILE        EMFILE          // 进程打开文件数达到上限
#define NGX_ENOSPC        ENOSPC          // 设备空间不足
#define NGX_EPIPE         EPIPE           // 管道破裂
#define NGX_EINPROGRESS   EINPROGRESS     // 操作正在进行中
#define NGX_ENOPROTOOPT   ENOPROTOOPT     // 协议不可用
#define NGX_EOPNOTSUPP    EOPNOTSUPP      // 不支持的操作
#define NGX_EADDRINUSE    EADDRINUSE      // 地址已被使用
#define NGX_ECONNABORTED  ECONNABORTED    // 连接中止
#define NGX_ECONNRESET    ECONNRESET      // 连接被重置
#define NGX_ENOTCONN      ENOTCONN        // 传输终点未连接
#define NGX_ETIMEDOUT     ETIMEDOUT       // 操作超时
#define NGX_ECONNREFUSED  ECONNREFUSED    // 连接被拒绝
#define NGX_ENAMETOOLONG  ENAMETOOLONG    // 文件名过长
#define NGX_ENETDOWN      ENETDOWN        // 网络已关闭
#define NGX_ENETUNREACH   ENETUNREACH     // 网络不可达
#define NGX_EHOSTDOWN     EHOSTDOWN       // 主机已关闭
#define NGX_EHOSTUNREACH  EHOSTUNREACH    // 主机不可达
#define NGX_ENOSYS        ENOSYS          // 功能未实现
#define NGX_ECANCELED     ECANCELED       // 操作已取消
#define NGX_EILSEQ        EILSEQ          // 非法字节序列
#define NGX_ENOMOREFILES  0               // 没有更多文件
#define NGX_ELOOP         ELOOP           // 符号链接层数过多
#define NGX_EBADF         EBADF           // 错误的文件描述符
#define NGX_EMSGSIZE      EMSGSIZE        // 消息过长

#if (NGX_HAVE_OPENAT)
#define NGX_EMLINK        EMLINK          // 链接过多
#endif

#if (__hpux__)
#define NGX_EAGAIN        EWOULDBLOCK     // HP-UX系统上,EAGAIN使用EWOULDBLOCK
#else
#define NGX_EAGAIN        EAGAIN          // 资源暂时不可用
#endif

// 定义错误码相关的宏
#define ngx_errno                  errno                  // 获取错误码
#define ngx_socket_errno           errno                  // 获取socket错误码
#define ngx_set_errno(err)         errno = err            // 设置错误码
#define ngx_set_socket_errno(err)  errno = err            // 设置socket错误码

// 错误信息相关函数声明
u_char *ngx_strerror(ngx_err_t err, u_char *errstr, size_t size);  // 获取错误描述
ngx_int_t ngx_strerror_init(void);  // 初始化错误信息


#endif /* _NGX_ERRNO_H_INCLUDED_ */
