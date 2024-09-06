
/*
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_setaffinity.c
 *
 * 该文件实现了Nginx的CPU亲和性设置功能。
 *
 * 支持的功能:
 * 1. 使用cpuset_setaffinity()设置CPU亲和性 (FreeBSD系统)
 * 2. 使用sched_setaffinity()设置CPU亲和性 (Linux系统)
 *
 * 使用注意点:
 * 1. 需要操作系统支持CPU亲和性设置
 * 2. 正确配置NGX_HAVE_CPUSET_SETAFFINITY或NGX_HAVE_SCHED_SETAFFINITY宏
 * 3. 设置失败时会记录警告日志，但不会中断Nginx的运行
 * 4. CPU编号从0开始
 * 5. 设置亲和性可能会影响Nginx的性能，请根据实际情况谨慎使用
 */


#include <ngx_config.h>
#include <ngx_core.h>


#if (NGX_HAVE_CPUSET_SETAFFINITY)

void
ngx_setaffinity(ngx_cpuset_t *cpu_affinity, ngx_log_t *log)
{
    ngx_uint_t  i;

    for (i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, cpu_affinity)) {
            ngx_log_error(NGX_LOG_NOTICE, log, 0,
                          "cpuset_setaffinity(): using cpu #%ui", i);
        }
    }

    if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1,
                           sizeof(cpuset_t), cpu_affinity) == -1)
    {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      "cpuset_setaffinity() failed");
    }
}

#elif (NGX_HAVE_SCHED_SETAFFINITY)

void
ngx_setaffinity(ngx_cpuset_t *cpu_affinity, ngx_log_t *log)
{
    ngx_uint_t  i;

    for (i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, cpu_affinity)) {
            ngx_log_error(NGX_LOG_NOTICE, log, 0,
                          "sched_setaffinity(): using cpu #%ui", i);
        }
    }

    if (sched_setaffinity(0, sizeof(cpu_set_t), cpu_affinity) == -1) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      "sched_setaffinity() failed");
    }
}

#endif
