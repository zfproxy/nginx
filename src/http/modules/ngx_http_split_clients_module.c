
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_split_clients_module.c
 *
 * 该模块实现了基于百分比的客户端请求分流功能。
 *
 * 支持的功能:
 * - 根据指定的表达式值将客户端请求分流到不同的组
 * - 支持设置多个分流组及其对应的百分比
 * - 可用于A/B测试、灰度发布等场景
 *
 * 支持的指令:
 * - split_clients: 定义客户端分流规则
 *   语法: split_clients $variable { ... }
 *   上下文: http
 *
 * 支持的变量:
 * - 由split_clients指令定义的变量，用于存储分流结果
 *
 * 使用注意点:
 * 1. 确保分流表达式的值具有足够的随机性，以实现准确的百分比分配
 * 2. 所有分流组的百分比总和应该等于或小于100%
 * 3. 未被显式分配的剩余百分比将被分配到默认组
 * 4. 分流结果是确定性的，相同的输入值将始终得到相同的分流结果
 * 5. 在生产环境中更改分流规则时要谨慎，可能会影响用户体验的一致性
 * 6. 考虑使用其他Nginx变量（如$remote_addr, $http_user_agent等）作为分流依据
 * 7. 分流模块的执行顺序可能会影响其他模块的行为，需要仔细规划模块加载顺序
 * 8. 定期检查和分析分流结果，确保符合预期的分配比例
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief 定义分割客户端部分的结构体
 *
 * 这个结构体用于存储分割客户端配置中的每个部分的信息。
 * 它包含了百分比和对应的变量值。
 */
typedef struct {
    uint32_t                    percent;
    ngx_http_variable_value_t   value;
} ngx_http_split_clients_part_t;


/**
 * @brief 定义分割客户端上下文的结构体
 *
 * 这个结构体用于存储分割客户端模块的上下文信息。
 * 它包含了用于计算分割值的复杂表达式和各个分割部分的数组。
 */
typedef struct {
    ngx_http_complex_value_t    value;
    ngx_array_t                 parts;
} ngx_http_split_clients_ctx_t;


/**
 * @brief 解析split_clients配置块
 *
 * 该函数用于解析nginx配置文件中的split_clients配置块。
 * 它处理split_clients指令的参数，并设置相应的上下文。
 *
 * @param cf nginx配置结构体指针
 * @param cmd 当前正在处理的命令结构体指针
 * @param conf 模块配置结构体指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
static char *ngx_conf_split_clients_block(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/**
 * @brief 处理split_clients指令的函数
 *
 * 该函数用于处理nginx配置文件中的split_clients指令。
 * 它解析指令的参数，并设置相应的分割客户端配置。
 *
 * @param cf nginx配置结构体指针
 * @param dummy 未使用的参数，通常为NULL
 * @param conf 模块配置结构体指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
static char *ngx_http_split_clients(ngx_conf_t *cf, ngx_command_t *dummy,
    void *conf);

/**
 * @brief 定义split_clients模块的命令数组
 *
 * 这个数组包含了split_clients模块支持的Nginx配置指令。
 * 目前只包含一个"split_clients"指令。
 */
static ngx_command_t  ngx_http_split_clients_commands[] = {

    { ngx_string("split_clients"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE2,
      ngx_conf_split_clients_block,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


/**
 * @brief split_clients模块的上下文结构
 *
 * 这个静态变量定义了split_clients模块的上下文结构。
 * 它包含了模块在不同配置阶段的回调函数指针。
 * 在这个模块中，所有的回调函数都被设置为NULL，
 * 表示这个模块不需要在这些阶段执行特定的操作。
 */
static ngx_http_module_t  ngx_http_split_clients_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


/**
 * @brief split_clients模块的定义
 *
 * 这个结构体定义了split_clients模块的基本信息和行为。
 * 它包含了模块的版本、上下文、指令、类型以及各种生命周期回调函数。
 */
ngx_module_t  ngx_http_split_clients_module = {
    NGX_MODULE_V1,
    &ngx_http_split_clients_module_ctx,    /* module context */
    ngx_http_split_clients_commands,       /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
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
 * @brief split_clients变量处理函数
 *
 * 这个静态函数用于处理split_clients模块定义的变量。
 * 它根据配置的规则和请求的特定属性来确定变量的值。
 *
 * @param r 指向当前HTTP请求的指针
 * @param v 指向变量值结构的指针，用于存储计算得到的变量值
 * @param data 指向split_clients上下文的指针，包含了变量计算所需的配置信息
 * @return 返回NGX_OK表示成功处理，其他值表示处理过程中出现错误
 */
static ngx_int_t
ngx_http_split_clients_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_split_clients_ctx_t *ctx = (ngx_http_split_clients_ctx_t *) data;

    uint32_t                        hash;
    ngx_str_t                       val;
    ngx_uint_t                      i;
    ngx_http_split_clients_part_t  *part;

    *v = ngx_http_variable_null_value;

    if (ngx_http_complex_value(r, &ctx->value, &val) != NGX_OK) {
        return NGX_OK;
    }

    hash = ngx_murmur_hash2(val.data, val.len);

    part = ctx->parts.elts;

    for (i = 0; i < ctx->parts.nelts; i++) {

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http split: %uD %uD", hash, part[i].percent);

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
    ngx_http_variable_t                 *var;
    ngx_http_split_clients_ctx_t        *ctx;
    ngx_http_split_clients_part_t       *part;
    ngx_http_compile_complex_value_t     ccv;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_split_clients_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &ctx->value;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
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

    var = ngx_http_add_variable(cf, &name, NGX_HTTP_VAR_CHANGEABLE);
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }

    var->get_handler = ngx_http_split_clients_variable;
    var->data = (uintptr_t) ctx;

    if (ngx_array_init(&ctx->parts, cf->pool, 2,
                       sizeof(ngx_http_split_clients_part_t))
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    save = *cf;
    cf->ctx = ctx;
    cf->handler = ngx_http_split_clients;
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
ngx_http_split_clients(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    ngx_int_t                       n;
    ngx_str_t                      *value;
    ngx_http_split_clients_ctx_t   *ctx;
    ngx_http_split_clients_part_t  *part;

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
