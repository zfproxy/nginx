
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_setproctitle.c
 *
 * 该文件实现了Nginx在Unix系统上设置进程标题的功能。
 *
 * 支持的功能:
 * 1. 初始化进程标题设置环境
 * 2. 动态修改进程标题
 * 3. 在Linux和Solaris系统上通过修改argv[0]来设置进程标题
 * 4. 处理环境变量和命令行参数的内存布局
 * 5. 兼容不同操作系统的进程标题显示特性
 *
 * 使用注意点:
 * 1. 需要在程序启动早期调用ngx_init_setproctitle()进行初始化
 * 2. 设置进程标题可能会影响原有的环境变量，需谨慎处理
 * 3. 新的进程标题长度不应超过可用内存空间
 * 4. 在Solaris系统上，标准ps命令可能无法显示修改后的标题，需使用特定命令
 * 5. 进程标题的修改可能影响系统监控工具的行为，需要相应调整
 * 6. 在容器化环境中使用时，需要注意潜在的兼容性问题
 * 7. 修改进程标题可能会影响某些依赖于原始命令行的调试工具
 */


#include <ngx_config.h>
#include <ngx_core.h>


#if (NGX_SETPROCTITLE_USES_ENV)
/*
 * 在Linux和Solaris系统上修改进程标题的方法:
 * 1. 将argv[1]设置为NULL
 * 2. 将新标题复制到argv[0]指向的位置
 * 
 * 注意事项:
 * - argv[0]可能太小，无法容纳新标题
 * - Linux和Solaris将argv[]和environ[]连续存储
 * - 需确保内存连续性，然后为environ[]分配新内存并复制
 * - 之后可以使用从argv[0]开始的内存来存储新的进程标题
 *
 * Solaris特殊情况:
 * - 标准/bin/ps命令无法显示修改后的进程标题
 * - 需使用"/usr/ucb/ps -w"命令
 * - UCB ps在新标题长度小于原命令行长度时不显示新标题
 * - 为避免这种情况，我们在新标题后附加原始命令行(括号内)
 */

// 声明外部环境变量数组
extern char **environ;

// 定义静态变量，用于存储argv数组的末尾位置
static char *ngx_os_argv_last;

ngx_int_t
ngx_init_setproctitle(ngx_log_t *log)
{
    u_char      *p;
    size_t       size;
    ngx_uint_t   i;

    size = 0;

    for (i = 0; environ[i]; i++) {
        size += ngx_strlen(environ[i]) + 1;
    }

    p = ngx_alloc(size, log);
    if (p == NULL) {
        return NGX_ERROR;
    }

    ngx_os_argv_last = ngx_os_argv[0];

    for (i = 0; ngx_os_argv[i]; i++) {
        if (ngx_os_argv_last == ngx_os_argv[i]) {
            ngx_os_argv_last = ngx_os_argv[i] + ngx_strlen(ngx_os_argv[i]) + 1;
        }
    }

    for (i = 0; environ[i]; i++) {
        if (ngx_os_argv_last == environ[i]) {

            size = ngx_strlen(environ[i]) + 1;
            ngx_os_argv_last = environ[i] + size;

            ngx_cpystrn(p, (u_char *) environ[i], size);
            environ[i] = (char *) p;
            p += size;
        }
    }

    ngx_os_argv_last--;

    return NGX_OK;
}


void
ngx_setproctitle(char *title)
{
    u_char     *p;

#if (NGX_SOLARIS)

    ngx_int_t   i;
    size_t      size;

#endif

    ngx_os_argv[1] = NULL;

    p = ngx_cpystrn((u_char *) ngx_os_argv[0], (u_char *) "nginx: ",
                    ngx_os_argv_last - ngx_os_argv[0]);

    p = ngx_cpystrn(p, (u_char *) title, ngx_os_argv_last - (char *) p);

#if (NGX_SOLARIS)

    size = 0;

    for (i = 0; i < ngx_argc; i++) {
        size += ngx_strlen(ngx_argv[i]) + 1;
    }

    if (size > (size_t) ((char *) p - ngx_os_argv[0])) {

        /*
         * ngx_setproctitle() is too rare operation so we use
         * the non-optimized copies
         */

        p = ngx_cpystrn(p, (u_char *) " (", ngx_os_argv_last - (char *) p);

        for (i = 0; i < ngx_argc; i++) {
            p = ngx_cpystrn(p, (u_char *) ngx_argv[i],
                            ngx_os_argv_last - (char *) p);
            p = ngx_cpystrn(p, (u_char *) " ", ngx_os_argv_last - (char *) p);
        }

        if (*(p - 1) == ' ') {
            *(p - 1) = ')';
        }
    }

#endif

    if (ngx_os_argv_last - (char *) p) {
        ngx_memset(p, NGX_SETPROCTITLE_PAD, ngx_os_argv_last - (char *) p);
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                   "setproctitle: \"%s\"", ngx_os_argv[0]);
}

#endif /* NGX_SETPROCTITLE_USES_ENV */
