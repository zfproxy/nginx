
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/**
 * @file ngx_http_addition_filter_module.c
 * @brief Nginx HTTP内容添加过滤模块
 *
 * 本模块允许在响应体前后添加内容，用于实现页面装饰、广告插入等功能。
 *
 * 支持的功能：
 * 1. 在响应体前添加内容
 * 2. 在响应体后添加内容
 * 3. 基于MIME类型的内容添加控制
 * 4. 支持子请求获取添加内容
 *
 * 支持的指令：
 * - add_before_body: 在响应体前添加内容
 *   语法：add_before_body uri;
 *   上下文：http, server, location
 *
 * - add_after_body: 在响应体后添加内容
 *   语法：add_after_body uri;
 *   上下文：http, server, location
 *
 * - addition_types: 指定允许添加内容的MIME类型
 *   语法：addition_types mime-type ...;
 *   默认值：addition_types text/html;
 *   上下文：http, server, location
 *
 * 支持的变量：
 * 本模块不引入新的变量，但可以在添加的内容中使用Nginx支持的所有变量。
 *
 * 使用注意点：
 * 1. 确保添加的内容URI可访问，否则可能导致请求失败
 * 2. 合理使用addition_types指令，避免对不适合的内容类型进行添加
 * 3. 添加大量内容可能影响性能，需要权衡使用
 * 4. 注意添加内容可能影响原始响应的语义和结构
 * 5. 在使用子请求获取添加内容时，注意避免循环依赖
 */




#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief HTTP内容添加模块的配置结构
 *
 * 该结构用于存储HTTP内容添加模块的配置信息。
 */
typedef struct {
    ngx_str_t     before_body;  /**< 在响应体前添加的内容 */
    ngx_str_t     after_body;   /**< 在响应体后添加的内容 */

    ngx_hash_t    types;        /**< 存储允许添加内容的MIME类型的哈希表 */
    ngx_array_t  *types_keys;   /**< 存储允许添加内容的MIME类型的数组 */
} ngx_http_addition_conf_t;


/**
 * @brief HTTP内容添加模块的上下文结构
 *
 * 该结构用于存储HTTP内容添加模块的上下文信息。
 */
typedef struct {
    ngx_uint_t    before_body_sent;  /**< 标记是否已发送响应体前的内容 */
} ngx_http_addition_ctx_t;


/**
 * @brief 创建HTTP内容添加模块的配置结构
 *
 * 该函数用于创建和初始化ngx_http_addition_conf_t结构体，
 * 该结构体用于存储HTTP内容添加模块的配置信息。
 *
 * @param cf Nginx配置结构指针
 * @return 返回创建的配置结构指针，如果创建失败则返回NULL
 */
static void *ngx_http_addition_create_conf(ngx_conf_t *cf);
/**
 * @brief 合并HTTP内容添加模块的配置
 *
 * 该函数用于合并父配置和子配置中的HTTP内容添加模块配置信息。
 * 它将处理配置继承和覆盖的逻辑，确保最终的配置正确。
 *
 * @param cf Nginx配置结构指针
 * @param parent 父配置结构指针
 * @param child 子配置结构指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 */
static char *ngx_http_addition_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);
/**
 * @brief 初始化HTTP内容添加过滤器
 *
 * 该函数用于初始化HTTP内容添加过滤器。
 * 它在模块的postconfiguration阶段被调用，用于设置必要的回调函数和初始化过滤器。
 *
 * @param cf Nginx配置结构指针
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
static ngx_int_t ngx_http_addition_filter_init(ngx_conf_t *cf);


/**
 * @brief HTTP内容添加模块的指令数组
 *
 * 这个数组定义了HTTP内容添加模块支持的所有Nginx配置指令。
 * 每个指令都是一个ngx_command_t结构体，包含了指令的名称、类型、
 * 处理函数等信息。这些指令用于配置在HTTP响应中添加额外内容的行为。
 */
static ngx_command_t  ngx_http_addition_commands[] = {

    { ngx_string("add_before_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_addition_conf_t, before_body),
      NULL },

    { ngx_string("add_after_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_addition_conf_t, after_body),
      NULL },

    { ngx_string("addition_types"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_types_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_addition_conf_t, types_keys),
      &ngx_http_html_default_types[0] },

      ngx_null_command
};


/**
 * @brief HTTP内容添加过滤器模块的上下文结构
 *
 * 这个结构定义了HTTP内容添加过滤器模块的上下文。
 * 它包含了模块在不同配置阶段的回调函数指针，用于创建和合并配置。
 * 这些函数在模块初始化和请求处理过程中被Nginx核心调用。
 */
static ngx_http_module_t  ngx_http_addition_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_addition_filter_init,         /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_addition_create_conf,         /* create location configuration */
    ngx_http_addition_merge_conf           /* merge location configuration */
};


/**
 * @brief HTTP内容添加过滤器模块的主要结构
 *
 * 这个结构定义了HTTP内容添加过滤器模块的主要信息。
 * 它包含了模块的上下文、指令、类型以及各种生命周期回调函数。
 * 这个结构是Nginx识别和管理该模块的关键。
 */
ngx_module_t  ngx_http_addition_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_addition_filter_module_ctx,  /* module context */
    ngx_http_addition_commands,            /* module directives */
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
 * @brief 指向下一个HTTP头部过滤器的函数指针
 *
 * 这个静态变量存储了指向下一个要执行的HTTP头部过滤器函数的指针。
 * 在Nginx的过滤器链中，每个过滤器模块都会保存下一个过滤器的指针，
 * 以便在完成自己的处理后将控制权传递给下一个过滤器。
 * 这种设计允许多个过滤器模块以链式方式处理请求和响应。
 */
static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
/**
 * @brief 指向下一个HTTP主体过滤器的函数指针
 *
 * 这个静态变量存储了指向下一个要执行的HTTP主体过滤器函数的指针。
 * 在Nginx的过滤器链中，每个过滤器模块都会保存下一个过滤器的指针，
 * 以便在完成自己的处理后将控制权传递给下一个过滤器。
 * 这种设计允许多个过滤器模块以链式方式处理请求和响应的主体内容。
 */
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


/**
 * @brief HTTP内容添加过滤器的头部处理函数
 *
 * 这个函数负责处理HTTP响应的头部，决定是否需要应用内容添加过滤器。
 * 它检查响应状态、内容类型等条件，并设置必要的上下文信息。
 *
 * @param r 指向当前HTTP请求的指针
 * @return NGX_ERROR 如果出现错误
 *         ngx_http_next_header_filter的返回值 如果不需要应用过滤器或处理完成
 */
static ngx_int_t
ngx_http_addition_header_filter(ngx_http_request_t *r)
{
    ngx_http_addition_ctx_t   *ctx;
    ngx_http_addition_conf_t  *conf;

    if (r->headers_out.status != NGX_HTTP_OK || r != r->main) {
        return ngx_http_next_header_filter(r);
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_addition_filter_module);

    if (conf->before_body.len == 0 && conf->after_body.len == 0) {
        return ngx_http_next_header_filter(r);
    }

    if (ngx_http_test_content_type(r, &conf->types) == NULL) {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_addition_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, ctx, ngx_http_addition_filter_module);

    ngx_http_clear_content_length(r);
    ngx_http_clear_accept_ranges(r);
    ngx_http_weak_etag(r);

    r->preserve_body = 1;

    return ngx_http_next_header_filter(r);
}


/**
 * @brief HTTP内容添加过滤器的主体处理函数
 *
 * 这个函数负责处理HTTP响应的主体部分，根据配置添加额外的内容。
 * 它会在响应主体之前和之后添加指定的内容，并处理子请求。
 *
 * @param r 指向当前HTTP请求的指针
 * @param in 指向输入缓冲链的指针
 * @return NGX_ERROR 如果出现错误
 *         ngx_http_next_body_filter的返回值 处理完成后的结果
 */
static ngx_int_t
ngx_http_addition_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t                  rc;
    ngx_uint_t                 last;
    ngx_chain_t               *cl;
    ngx_http_request_t        *sr;
    ngx_http_addition_ctx_t   *ctx;
    ngx_http_addition_conf_t  *conf;

    if (in == NULL || r->header_only) {
        return ngx_http_next_body_filter(r, in);
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_addition_filter_module);

    if (ctx == NULL) {
        return ngx_http_next_body_filter(r, in);
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_addition_filter_module);

    if (!ctx->before_body_sent) {
        ctx->before_body_sent = 1;

        if (conf->before_body.len) {
            if (ngx_http_subrequest(r, &conf->before_body, NULL, &sr, NULL, 0)
                != NGX_OK)
            {
                return NGX_ERROR;
            }
        }
    }

    if (conf->after_body.len == 0) {
        ngx_http_set_ctx(r, NULL, ngx_http_addition_filter_module);
        return ngx_http_next_body_filter(r, in);
    }

    last = 0;

    for (cl = in; cl; cl = cl->next) {
        if (cl->buf->last_buf) {
            cl->buf->last_buf = 0;
            cl->buf->last_in_chain = 1;
            cl->buf->sync = 1;
            last = 1;
        }
    }

    rc = ngx_http_next_body_filter(r, in);

    if (rc == NGX_ERROR || !last || conf->after_body.len == 0) {
        return rc;
    }

    if (ngx_http_subrequest(r, &conf->after_body, NULL, &sr, NULL, 0)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, NULL, ngx_http_addition_filter_module);

    return ngx_http_send_special(r, NGX_HTTP_LAST);
}


static ngx_int_t
ngx_http_addition_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_addition_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_addition_body_filter;

    return NGX_OK;
}


static void *
ngx_http_addition_create_conf(ngx_conf_t *cf)
{
    ngx_http_addition_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_addition_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->before_body = { 0, NULL };
     *     conf->after_body = { 0, NULL };
     *     conf->types = { NULL };
     *     conf->types_keys = NULL;
     */

    return conf;
}


static char *
ngx_http_addition_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_addition_conf_t *prev = parent;
    ngx_http_addition_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->before_body, prev->before_body, "");
    ngx_conf_merge_str_value(conf->after_body, prev->after_body, "");

    if (ngx_http_merge_types(cf, &conf->types_keys, &conf->types,
                             &prev->types_keys, &prev->types,
                             ngx_http_html_default_types)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
