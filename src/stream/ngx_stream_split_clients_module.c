
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_stream_split_clients_module.c
 *
 * 该模块实现了Nginx流模块的客户端分流功能。
 *
 * 支持的功能：
 * 1. 基于变量值的哈希分流
 * 2. 支持百分比配置
 * 3. 动态分配客户端到不同的组
 * 4. 可用于A/B测试或灰度发布
 *
 * 支持的指令：
 * - split_clients: 定义客户端分流规则
 *   语法: split_clients $variable { ... }
 *   上下文: stream
 *
 * 支持的变量：
 * 该模块不定义特定变量，但可以使用任何有效的Nginx变量作为分流依据
 *
 * 使用注意点：
 * 1. 确保分流百分比总和不超过100%
 * 2. 选择合适的变量作为分流依据，如$remote_addr或$request_id
 * 3. 分流结果可以赋值给新变量，用于后续配置中的条件判断
 * 4. 注意分流规则的性能影响，特别是在高并发场景下
 * 5. 定期检查分流效果，确保符合预期
 * 6. 可以结合其他模块（如access模块）使用，实现更复杂的分流逻辑
 * 7. 在使用split_clients指令时，注意正确配置百分比和对应的值
 * 8. 分流结果是确定性的，相同的输入变量值会得到相同的分流结果
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>


// 定义 split_clients 指令中每个分组的结构体
typedef struct {
    uint32_t                      percent;  // 该分组的百分比
    ngx_stream_variable_value_t   value;    // 该分组对应的值
} ngx_stream_split_clients_part_t;


// 定义 split_clients 指令的上下文结构体
typedef struct {
    ngx_stream_complex_value_t    value;    // 用于计算哈希值的表达式
    ngx_array_t                   parts;    // 存储所有分组的数组
} ngx_stream_split_clients_ctx_t;


// 声明处理 split_clients 指令块的函数
static char *ngx_conf_split_clients_block(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

// 声明处理 split_clients 指令内容的函数
static char *ngx_stream_split_clients(ngx_conf_t *cf, ngx_command_t *dummy,
    void *conf);

static ngx_command_t  ngx_stream_split_clients_commands[] = {

    { ngx_string("split_clients"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE2,
      ngx_conf_split_clients_block,
      NGX_STREAM_MAIN_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_stream_module_t  ngx_stream_split_clients_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL                                   /* merge server configuration */
};


ngx_module_t  ngx_stream_split_clients_module = {
    NGX_MODULE_V1,
    &ngx_stream_split_clients_module_ctx,  /* module context */
    ngx_stream_split_clients_commands,     /* module directives */
    NGX_STREAM_MODULE,                     /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


/**
 * @brief 处理 split_clients 变量的函数
 * @param s 流会话结构体
 * @param v 变量值结构体
 * @param data 上下文数据
 * @return NGX_OK 表示成功，其他值表示失败
 */
static ngx_int_t
ngx_stream_split_clients_variable(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data)
{
    ngx_stream_split_clients_ctx_t *ctx =
                                       (ngx_stream_split_clients_ctx_t *) data;

    uint32_t                          hash;
    ngx_str_t                         val;
    ngx_uint_t                        i;
    ngx_stream_split_clients_part_t  *part;

    *v = ngx_stream_variable_null_value;

    if (ngx_stream_complex_value(s, &ctx->value, &val) != NGX_OK) {
        return NGX_OK;
    }

    hash = ngx_murmur_hash2(val.data, val.len);

    part = ctx->parts.elts;

    for (i = 0; i < ctx->parts.nelts; i++) {

        ngx_log_debug2(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                       "stream split: %uD %uD", hash, part[i].percent);

        if (hash < part[i].percent || part[i].percent == 0) {
            *v = part[i].value;
            return NGX_OK;
        }
    }

    return NGX_OK;
}


static char *
ngx_conf_split_clients_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                                *rv;
    uint32_t                             sum, last;
    ngx_str_t                           *value, name;
    ngx_uint_t                           i;
    ngx_conf_t                           save;
    ngx_stream_variable_t               *var;
    ngx_stream_split_clients_ctx_t      *ctx;
    ngx_stream_split_clients_part_t     *part;
    ngx_stream_compile_complex_value_t   ccv;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_stream_split_clients_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_stream_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &ctx->value;

    if (ngx_stream_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    name = value[2];

    if (name.data[0] != '$') {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid variable name \"%V\"", &name);
        return NGX_CONF_ERROR;
    }

    name.len--;
    name.data++;

    var = ngx_stream_add_variable(cf, &name, NGX_STREAM_VAR_CHANGEABLE);
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }

    var->get_handler = ngx_stream_split_clients_variable;
    var->data = (uintptr_t) ctx;

    if (ngx_array_init(&ctx->parts, cf->pool, 2,
                       sizeof(ngx_stream_split_clients_part_t))
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    save = *cf;
    cf->ctx = ctx;
    cf->handler = ngx_stream_split_clients;
    cf->handler_conf = conf;

    rv = ngx_conf_parse(cf, NULL);

    *cf = save;

    if (rv != NGX_CONF_OK) {
        return rv;
    }

    sum = 0;
    last = 0;
    part = ctx->parts.elts;

    for (i = 0; i < ctx->parts.nelts; i++) {
        sum = part[i].percent ? sum + part[i].percent : 10000;
        if (sum > 10000) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "percent total is greater than 100%%");
            return NGX_CONF_ERROR;
        }

        if (part[i].percent) {
            last += part[i].percent * (uint64_t) 0xffffffff / 10000;
            part[i].percent = last;
        }
    }

    return rv;
}


static char *
ngx_stream_split_clients(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    ngx_int_t                         n;
    ngx_str_t                        *value;
    ngx_stream_split_clients_ctx_t   *ctx;
    ngx_stream_split_clients_part_t  *part;

    ctx = cf->ctx;
    value = cf->args->elts;

    part = ngx_array_push(&ctx->parts);
    if (part == NULL) {
        return NGX_CONF_ERROR;
    }

    if (value[0].len == 1 && value[0].data[0] == '*') {
        part->percent = 0;

    } else {
        if (value[0].len == 0 || value[0].data[value[0].len - 1] != '%') {
            goto invalid;
        }

        n = ngx_atofp(value[0].data, value[0].len - 1, 2);
        if (n == NGX_ERROR || n == 0) {
            goto invalid;
        }

        part->percent = (uint32_t) n;
    }

    part->value.len = value[1].len;
    part->value.valid = 1;
    part->value.no_cacheable = 0;
    part->value.not_found = 0;
    part->value.data = value[1].data;

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid percent value \"%V\"", &value[0]);
    return NGX_CONF_ERROR;
}
