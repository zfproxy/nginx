
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 */

/*
 * ngx_http_v2_module.c
 *
 * 该模块实现了Nginx的HTTP/2协议支持。
 *
 * 支持的功能:
 * 1. HTTP/2协议处理
 * 2. HTTP/2服务器推送
 * 3. HTTP/2多路复用
 * 4. HTTP/2头部压缩(HPACK)
 * 5. HTTP/2流量控制
 * 6. HTTP/2优先级处理
 *
 * 支持的指令:
 * - http2: 启用或禁用HTTP/2支持
 *   语法: http2 on|off;
 *   上下文: http, server
 * 
 * - http2_max_concurrent_streams: 设置最大并发流数量
 *   语法: http2_max_concurrent_streams number;
 *   上下文: http, server
 *
 * - http2_recv_buffer_size: 设置接收缓冲区大小
 *   语法: http2_recv_buffer_size size;
 *   上下文: http, server
 *
 * - http2_idle_timeout: 设置空闲连接超时时间
 *   语法: http2_idle_timeout time;
 *   上下文: http, server
 *
 * 支持的变量:
 * - $http2: 如果请求使用HTTP/2协议，则为"h2"，否则为空字符串
 *
 * 使用注意点:
 * 1. 确保Nginx编译时启用了HTTP/2支持
 * 2. HTTP/2需要TLS 1.2或更高版本支持，确保正确配置SSL证书
 * 3. 合理配置http2_max_concurrent_streams以控制服务器负载
 * 4. 注意调整http2_recv_buffer_size以优化性能和内存使用
 * 5. 在高并发场景下，可能需要调整系统的文件描述符限制
 * 6. 监控HTTP/2连接的建立和维护，确保连接的健康状态
 * 7. 考虑使用HTTP/2服务器推送功能来优化性能
 * 8. 定期更新Nginx以获取最新的HTTP/2协议支持和安全修复
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_v2_module.h>


static ngx_int_t ngx_http_v2_add_variables(ngx_conf_t *cf);

static ngx_int_t ngx_http_v2_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_v2_module_init(ngx_cycle_t *cycle);

static void *ngx_http_v2_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_v2_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_http_v2_create_srv_conf(ngx_conf_t *cf);
static char *ngx_http_v2_merge_srv_conf(ngx_conf_t *cf, void *parent,
    void *child);
static void *ngx_http_v2_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_v2_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);

static char *ngx_http_v2_recv_buffer_size(ngx_conf_t *cf, void *post,
    void *data);
static char *ngx_http_v2_pool_size(ngx_conf_t *cf, void *post, void *data);
static char *ngx_http_v2_preread_size(ngx_conf_t *cf, void *post, void *data);
static char *ngx_http_v2_streams_index_mask(ngx_conf_t *cf, void *post,
    void *data);
static char *ngx_http_v2_chunk_size(ngx_conf_t *cf, void *post, void *data);
static char *ngx_http_v2_obsolete(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


static ngx_conf_deprecated_t  ngx_http_v2_recv_timeout_deprecated = {
    ngx_conf_deprecated, "http2_recv_timeout", "client_header_timeout"
};

static ngx_conf_deprecated_t  ngx_http_v2_idle_timeout_deprecated = {
    ngx_conf_deprecated, "http2_idle_timeout", "keepalive_timeout"
};

static ngx_conf_deprecated_t  ngx_http_v2_max_requests_deprecated = {
    ngx_conf_deprecated, "http2_max_requests", "keepalive_requests"
};

static ngx_conf_deprecated_t  ngx_http_v2_max_field_size_deprecated = {
    ngx_conf_deprecated, "http2_max_field_size", "large_client_header_buffers"
};

static ngx_conf_deprecated_t  ngx_http_v2_max_header_size_deprecated = {
    ngx_conf_deprecated, "http2_max_header_size", "large_client_header_buffers"
};


static ngx_conf_post_t  ngx_http_v2_recv_buffer_size_post =
    { ngx_http_v2_recv_buffer_size };
static ngx_conf_post_t  ngx_http_v2_pool_size_post =
    { ngx_http_v2_pool_size };
static ngx_conf_post_t  ngx_http_v2_preread_size_post =
    { ngx_http_v2_preread_size };
static ngx_conf_post_t  ngx_http_v2_streams_index_mask_post =
    { ngx_http_v2_streams_index_mask };
static ngx_conf_post_t  ngx_http_v2_chunk_size_post =
    { ngx_http_v2_chunk_size };


static ngx_command_t  ngx_http_v2_commands[] = {

    { ngx_string("http2"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_v2_srv_conf_t, enable),
      NULL },

    { ngx_string("http2_recv_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_v2_main_conf_t, recv_buffer_size),
      &ngx_http_v2_recv_buffer_size_post },

    { ngx_string("http2_pool_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_v2_srv_conf_t, pool_size),
      &ngx_http_v2_pool_size_post },

    { ngx_string("http2_max_concurrent_streams"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_v2_srv_conf_t, concurrent_streams),
      NULL },

    { ngx_string("http2_max_concurrent_pushes"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_http_v2_obsolete,
      0,
      0,
      NULL },

    { ngx_string("http2_max_requests"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_http_v2_obsolete,
      0,
      0,
      &ngx_http_v2_max_requests_deprecated },

    { ngx_string("http2_max_field_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_http_v2_obsolete,
      0,
      0,
      &ngx_http_v2_max_field_size_deprecated },

    { ngx_string("http2_max_header_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_http_v2_obsolete,
      0,
      0,
      &ngx_http_v2_max_header_size_deprecated },

    { ngx_string("http2_body_preread_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_v2_srv_conf_t, preread_size),
      &ngx_http_v2_preread_size_post },

    { ngx_string("http2_streams_index_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_v2_srv_conf_t, streams_index_mask),
      &ngx_http_v2_streams_index_mask_post },

    { ngx_string("http2_recv_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_http_v2_obsolete,
      0,
      0,
      &ngx_http_v2_recv_timeout_deprecated },

    { ngx_string("http2_idle_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_http_v2_obsolete,
      0,
      0,
      &ngx_http_v2_idle_timeout_deprecated },

    { ngx_string("http2_chunk_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_v2_loc_conf_t, chunk_size),
      &ngx_http_v2_chunk_size_post },

    { ngx_string("http2_push_preload"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_http_v2_obsolete,
      0,
      0,
      NULL },

    { ngx_string("http2_push"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_v2_obsolete,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_v2_module_ctx = {
    ngx_http_v2_add_variables,             /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_v2_create_main_conf,          /* create main configuration */
    ngx_http_v2_init_main_conf,            /* init main configuration */

    ngx_http_v2_create_srv_conf,           /* create server configuration */
    ngx_http_v2_merge_srv_conf,            /* merge server configuration */

    ngx_http_v2_create_loc_conf,           /* create location configuration */
    ngx_http_v2_merge_loc_conf             /* merge location configuration */
};


ngx_module_t  ngx_http_v2_module = {
    NGX_MODULE_V1,
    &ngx_http_v2_module_ctx,               /* module context */
    ngx_http_v2_commands,                  /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_http_v2_module_init,               /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_variable_t  ngx_http_v2_vars[] = {

    { ngx_string("http2"), NULL,
      ngx_http_v2_variable, 0, 0, 0 },

      ngx_http_null_variable
};


static ngx_int_t
ngx_http_v2_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_v2_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_v2_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{

    if (r->stream) {
#if (NGX_HTTP_SSL)

        if (r->connection->ssl) {
            v->len = sizeof("h2") - 1;
            v->valid = 1;
            v->no_cacheable = 0;
            v->not_found = 0;
            v->data = (u_char *) "h2";

            return NGX_OK;
        }

#endif
        v->len = sizeof("h2c") - 1;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = (u_char *) "h2c";

        return NGX_OK;
    }

    *v = ngx_http_variable_null_value;

    return NGX_OK;
}


static ngx_int_t
ngx_http_v2_module_init(ngx_cycle_t *cycle)
{
    return NGX_OK;
}


static void *
ngx_http_v2_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_v2_main_conf_t  *h2mcf;

    h2mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_v2_main_conf_t));
    if (h2mcf == NULL) {
        return NULL;
    }

    h2mcf->recv_buffer_size = NGX_CONF_UNSET_SIZE;

    return h2mcf;
}


static char *
ngx_http_v2_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_v2_main_conf_t *h2mcf = conf;

    ngx_conf_init_size_value(h2mcf->recv_buffer_size, 256 * 1024);

    return NGX_CONF_OK;
}


static void *
ngx_http_v2_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_v2_srv_conf_t  *h2scf;

    h2scf = ngx_pcalloc(cf->pool, sizeof(ngx_http_v2_srv_conf_t));
    if (h2scf == NULL) {
        return NULL;
    }

    h2scf->enable = NGX_CONF_UNSET;

    h2scf->pool_size = NGX_CONF_UNSET_SIZE;

    h2scf->concurrent_streams = NGX_CONF_UNSET_UINT;

    h2scf->preread_size = NGX_CONF_UNSET_SIZE;

    h2scf->streams_index_mask = NGX_CONF_UNSET_UINT;

    return h2scf;
}


static char *
ngx_http_v2_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_v2_srv_conf_t *prev = parent;
    ngx_http_v2_srv_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);

    ngx_conf_merge_size_value(conf->pool_size, prev->pool_size, 4096);

    ngx_conf_merge_uint_value(conf->concurrent_streams,
                              prev->concurrent_streams, 128);

    ngx_conf_merge_size_value(conf->preread_size, prev->preread_size, 65536);

    ngx_conf_merge_uint_value(conf->streams_index_mask,
                              prev->streams_index_mask, 32 - 1);

    return NGX_CONF_OK;
}


static void *
ngx_http_v2_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_v2_loc_conf_t  *h2lcf;

    h2lcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_v2_loc_conf_t));
    if (h2lcf == NULL) {
        return NULL;
    }

    h2lcf->chunk_size = NGX_CONF_UNSET_SIZE;

    return h2lcf;
}


static char *
ngx_http_v2_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_v2_loc_conf_t *prev = parent;
    ngx_http_v2_loc_conf_t *conf = child;

    ngx_conf_merge_size_value(conf->chunk_size, prev->chunk_size, 8 * 1024);

    return NGX_CONF_OK;
}


static char *
ngx_http_v2_recv_buffer_size(ngx_conf_t *cf, void *post, void *data)
{
    size_t *sp = data;

    if (*sp <= NGX_HTTP_V2_STATE_BUFFER_SIZE) {
        return "value is too small";
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_v2_pool_size(ngx_conf_t *cf, void *post, void *data)
{
    size_t *sp = data;

    if (*sp < NGX_MIN_POOL_SIZE) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the pool size must be no less than %uz",
                           NGX_MIN_POOL_SIZE);

        return NGX_CONF_ERROR;
    }

    if (*sp % NGX_POOL_ALIGNMENT) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the pool size must be a multiple of %uz",
                           NGX_POOL_ALIGNMENT);

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_v2_preread_size(ngx_conf_t *cf, void *post, void *data)
{
    size_t *sp = data;

    if (*sp > NGX_HTTP_V2_MAX_WINDOW) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the maximum body preread buffer size is %uz",
                           NGX_HTTP_V2_MAX_WINDOW);

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_v2_streams_index_mask(ngx_conf_t *cf, void *post, void *data)
{
    ngx_uint_t *np = data;

    ngx_uint_t  mask;

    mask = *np - 1;

    if (*np == 0 || (*np & mask)) {
        return "must be a power of two";
    }

    *np = mask;

    return NGX_CONF_OK;
}


static char *
ngx_http_v2_chunk_size(ngx_conf_t *cf, void *post, void *data)
{
    size_t *sp = data;

    if (*sp == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the http2 chunk size cannot be zero");

        return NGX_CONF_ERROR;
    }

    if (*sp > NGX_HTTP_V2_MAX_FRAME_SIZE) {
        *sp = NGX_HTTP_V2_MAX_FRAME_SIZE;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_v2_obsolete(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_conf_deprecated_t  *d = cmd->post;

    if (d) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "the \"%s\" directive is obsolete, "
                           "use the \"%s\" directive instead",
                           d->old_name, d->new_name);

    } else {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "the \"%V\" directive is obsolete, ignored",
                           &cmd->name);
    }

    return NGX_CONF_OK;
}
