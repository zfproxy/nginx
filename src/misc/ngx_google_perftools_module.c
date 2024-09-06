
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_google_perftools_module.c
 *
 * 该模块集成了Google Perftools性能分析工具，用于Nginx服务器的性能分析和优化。
 *
 * 支持的功能:
 * - CPU性能分析
 * - 内存使用分析
 * - 线程分析
 * - 性能数据收集和输出
 *
 * 支持的指令:
 * - google_perftools_profiles: 设置性能分析数据的输出路径
 *
 * 相关变量:
 * 本模块未定义特定变量
 *
 * 使用注意点:
 * 1. 确保已正确安装Google Perftools库
 * 2. 性能分析可能会对服务器性能产生轻微影响，建议仅在测试环境中使用
 * 3. 定期检查和分析输出的性能数据文件
 * 4. 注意设置适当的文件权限以保护性能数据
 * 5. 在高负载环境下使用时要谨慎，可能会产生大量数据
 * 6. 与其他性能分析工具结合使用，以获得全面的性能评估
 * 7. 在解释性能数据时，考虑实际的业务场景和负载情况
 */


#include <ngx_config.h>
#include <ngx_core.h>

/*
 * declare Profiler interface here because
 * <google/profiler.h> is C++ header file
 */

int ProfilerStart(u_char* fname);
void ProfilerStop(void);
void ProfilerRegisterThread(void);


static void *ngx_google_perftools_create_conf(ngx_cycle_t *cycle);
static ngx_int_t ngx_google_perftools_worker(ngx_cycle_t *cycle);


typedef struct {
    ngx_str_t  profiles;
} ngx_google_perftools_conf_t;


static ngx_command_t  ngx_google_perftools_commands[] = {

    { ngx_string("google_perftools_profiles"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_google_perftools_conf_t, profiles),
      NULL },

      ngx_null_command
};


static ngx_core_module_t  ngx_google_perftools_module_ctx = {
    ngx_string("google_perftools"),
    ngx_google_perftools_create_conf,
    NULL
};


ngx_module_t  ngx_google_perftools_module = {
    NGX_MODULE_V1,
    &ngx_google_perftools_module_ctx,      /* module context */
    ngx_google_perftools_commands,         /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_google_perftools_worker,           /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_google_perftools_create_conf(ngx_cycle_t *cycle)
{
    ngx_google_perftools_conf_t  *gptcf;

    gptcf = ngx_pcalloc(cycle->pool, sizeof(ngx_google_perftools_conf_t));
    if (gptcf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc()
     *
     *     gptcf->profiles = { 0, NULL };
     */

    return gptcf;
}


static ngx_int_t
ngx_google_perftools_worker(ngx_cycle_t *cycle)
{
    u_char                       *profile;
    ngx_google_perftools_conf_t  *gptcf;

    gptcf = (ngx_google_perftools_conf_t *)
                ngx_get_conf(cycle->conf_ctx, ngx_google_perftools_module);

    if (gptcf->profiles.len == 0) {
        return NGX_OK;
    }

    profile = ngx_alloc(gptcf->profiles.len + NGX_INT_T_LEN + 2, cycle->log);
    if (profile == NULL) {
        return NGX_OK;
    }

    if (getenv("CPUPROFILE")) {
        /* disable inherited Profiler enabled in master process */
        ProfilerStop();
    }

    ngx_sprintf(profile, "%V.%d%Z", &gptcf->profiles, ngx_pid);

    if (ProfilerStart(profile)) {
        /* start ITIMER_PROF timer */
        ProfilerRegisterThread();

    } else {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_errno,
                      "ProfilerStart(%s) failed", profile);
    }

    ngx_free(profile);

    return NGX_OK;
}


/* ProfilerStop() is called on Profiler destruction */
