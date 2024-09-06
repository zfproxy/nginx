
/*
 * Copyright (C) Roman Arutyunyan
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_mirror_module.c
 *
 * 该模块实现了HTTP请求镜像功能，允许将原始请求复制并发送到一个或多个镜像目标。
 *
 * 支持的功能：
 * 1. 将请求镜像到多个目标
 * 2. 可选择是否镜像请求体
 * 3. 异步处理镜像请求，不影响原始请求的响应
 *
 * 支持的指令：
 * - mirror：指定镜像目标URI
 *   语法：mirror uri | off;
 *   默认值：off
 *   上下文：location, if in location
 *
 * - mirror_request_body：控制是否镜像请求体
 *   语法：mirror_request_body on | off;
 *   默认值：on
 *   上下文：http, server, location
 *
 * 支持的变量：
 * 该模块未引入新的变量
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief 镜像模块的位置配置结构
 */
typedef struct {
    ngx_array_t  *mirror;      /* 镜像目标数组 */
    ngx_flag_t    request_body; /* 是否镜像请求体的标志 */
} ngx_http_mirror_loc_conf_t;


/**
 * @brief 镜像模块的请求上下文结构
 */
typedef struct {
    ngx_int_t     status;      /* 镜像请求的状态码 */
} ngx_http_mirror_ctx_t;


/**
 * @brief 处理镜像请求的主函数
 * @param r HTTP请求结构体
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_mirror_handler(ngx_http_request_t *r);

/**
 * @brief 处理镜像请求体的函数
 * @param r HTTP请求结构体
 */
static void ngx_http_mirror_body_handler(ngx_http_request_t *r);

/**
 * @brief 内部镜像请求处理函数
 * @param r HTTP请求结构体
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_mirror_handler_internal(ngx_http_request_t *r);

/**
 * @brief 创建镜像模块的位置配置
 * @param cf 配置结构体
 * @return void* 创建的位置配置
 */
static void *ngx_http_mirror_create_loc_conf(ngx_conf_t *cf);

/**
 * @brief 合并镜像模块的位置配置
 * @param cf 配置结构体
 * @param parent 父配置
 * @param child 子配置
 * @return char* 合并结果
 */
static char *ngx_http_mirror_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);

/**
 * @brief 处理mirror指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_mirror(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

/**
 * @brief 初始化镜像模块
 * @param cf 配置结构体
 * @return ngx_int_t 初始化结果
 */
static ngx_int_t ngx_http_mirror_init(ngx_conf_t *cf);


/**
 * @brief 定义镜像模块的命令数组
 *
 * 这个数组包含了镜像模块支持的所有配置指令。
 * 每个指令都有其特定的处理函数和配置。
 */
static ngx_command_t  ngx_http_mirror_commands[] = {

    { ngx_string("mirror"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_mirror,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("mirror_request_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_mirror_loc_conf_t, request_body),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_mirror_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_mirror_init,                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_mirror_create_loc_conf,       /* create location configuration */
    ngx_http_mirror_merge_loc_conf         /* merge location configuration */
};


ngx_module_t  ngx_http_mirror_module = {
    NGX_MODULE_V1,
    &ngx_http_mirror_module_ctx,           /* module context */
    ngx_http_mirror_commands,              /* module directives */
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


static ngx_int_t
ngx_http_mirror_handler(ngx_http_request_t *r)
{
    ngx_int_t                    rc;
    ngx_http_mirror_ctx_t       *ctx;
    ngx_http_mirror_loc_conf_t  *mlcf;

    if (r != r->main) {
        return NGX_DECLINED;
    }

    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_mirror_module);

    if (mlcf->mirror == NULL) {
        return NGX_DECLINED;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "mirror handler");

    if (mlcf->request_body) {
        ctx = ngx_http_get_module_ctx(r, ngx_http_mirror_module);

        if (ctx) {
            return ctx->status;
        }

        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_mirror_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }

        ctx->status = NGX_DONE;

        ngx_http_set_ctx(r, ctx, ngx_http_mirror_module);

        rc = ngx_http_read_client_request_body(r, ngx_http_mirror_body_handler);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        ngx_http_finalize_request(r, NGX_DONE);
        return NGX_DONE;
    }

    return ngx_http_mirror_handler_internal(r);
}


static void
ngx_http_mirror_body_handler(ngx_http_request_t *r)
{
    ngx_http_mirror_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_mirror_module);

    ctx->status = ngx_http_mirror_handler_internal(r);

    r->preserve_body = 1;

    r->write_event_handler = ngx_http_core_run_phases;
    ngx_http_core_run_phases(r);
}


static ngx_int_t
ngx_http_mirror_handler_internal(ngx_http_request_t *r)
{
    ngx_str_t                   *name;
    ngx_uint_t                   i;
    ngx_http_request_t          *sr;
    ngx_http_mirror_loc_conf_t  *mlcf;

    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_mirror_module);

    name = mlcf->mirror->elts;

    for (i = 0; i < mlcf->mirror->nelts; i++) {
        if (ngx_http_subrequest(r, &name[i], &r->args, &sr, NULL,
                                NGX_HTTP_SUBREQUEST_BACKGROUND)
            != NGX_OK)
        {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        sr->header_only = 1;
        sr->method = r->method;
        sr->method_name = r->method_name;
    }

    return NGX_DECLINED;
}


static void *
ngx_http_mirror_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_mirror_loc_conf_t  *mlcf;

    mlcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_mirror_loc_conf_t));
    if (mlcf == NULL) {
        return NULL;
    }

    mlcf->mirror = NGX_CONF_UNSET_PTR;
    mlcf->request_body = NGX_CONF_UNSET;

    return mlcf;
}


static char *
ngx_http_mirror_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_mirror_loc_conf_t *prev = parent;
    ngx_http_mirror_loc_conf_t *conf = child;

    ngx_conf_merge_ptr_value(conf->mirror, prev->mirror, NULL);
    ngx_conf_merge_value(conf->request_body, prev->request_body, 1);

    return NGX_CONF_OK;
}


static char *
ngx_http_mirror(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_mirror_loc_conf_t *mlcf = conf;

    ngx_str_t  *value, *s;

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        if (mlcf->mirror != NGX_CONF_UNSET_PTR) {
            return "is duplicate";
        }

        mlcf->mirror = NULL;
        return NGX_CONF_OK;
    }

    if (mlcf->mirror == NULL) {
        return "is duplicate";
    }

    if (mlcf->mirror == NGX_CONF_UNSET_PTR) {
        mlcf->mirror = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
        if (mlcf->mirror == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    s = ngx_array_push(mlcf->mirror);
    if (s == NULL) {
        return NGX_CONF_ERROR;
    }

    *s = value[1];

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_mirror_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_PRECONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_mirror_handler;

    return NGX_OK;
}
