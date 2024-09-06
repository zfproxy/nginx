
/*
 * Copyright (C) Maxim Dounin
 * Copyright (C) Nginx, Inc.
 */
/*
 * ngx_http_auth_request_module.c
 *
 * 该模块实现了基于子请求的外部认证功能。
 *
 * 支持的功能:
 * 1. 发送子请求到指定URI进行认证
 * 2. 根据子请求的响应决定是否允许访问
 * 3. 支持将认证子请求的响应头传递给主请求
 * 4. 可配置认证失败时的错误响应
 *
 * 支持的指令:
 * - auth_request: 指定用于认证的URI
 *   语法: auth_request uri | off;
 *   默认值: auth_request off;
 *   上下文: http, server, location
 *
 * - auth_request_set: 设置变量为认证请求的响应头
 *   语法: auth_request_set $variable $upstream_http_name;
 *   上下文: http, server, location
 *
 * 支持的变量:
 * 该模块不直接提供变量，但可以通过auth_request_set指令设置变量。
 *
 * 使用注意点:
 * 1. 认证子请求会增加服务器负载，应谨慎使用
 * 2. 确保认证URI能够正确响应，避免认证失败导致无法访问
 * 3. 认证子请求的超时设置会影响主请求的响应时间
 * 4. 注意保护认证URI，防止未经授权的访问
 * 5. 可以结合其他模块（如access模块）实现更复杂的访问控制
 */



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief HTTP认证请求模块的配置结构
 *
 * 该结构用于存储HTTP认证请求模块的配置信息。
 */
typedef struct {
    ngx_str_t                 uri;    /**< 认证请求的URI */
    ngx_array_t              *vars;   /**< 存储认证变量的数组 */
} ngx_http_auth_request_conf_t;


/**
 * @brief HTTP认证请求模块的上下文结构
 *
 * 该结构用于存储HTTP认证请求模块的上下文信息。
 */
typedef struct {
    ngx_uint_t                done;       /**< 标记认证是否完成 */
    ngx_uint_t                status;     /**< 认证请求的状态码 */
    ngx_http_request_t       *subrequest; /**< 指向子请求的指针 */
} ngx_http_auth_request_ctx_t;


/**
 * @brief HTTP认证请求变量结构
 *
 * 该结构用于定义和处理HTTP认证请求中的变量。
 */
typedef struct {
    ngx_int_t                 index;        /**< 变量索引 */
    ngx_http_complex_value_t  value;        /**< 变量值 */
    ngx_http_set_variable_pt  set_handler;  /**< 变量设置处理函数 */
} ngx_http_auth_request_variable_t;


/**
 * @brief 处理认证请求的主要函数
 *
 * 该函数负责处理传入的HTTP请求，执行认证逻辑。
 *
 * @param r 指向当前HTTP请求的指针
 * @return NGX_DECLINED 如果认证通过，NGX_ERROR 如果出现错误，或其他适当的状态码
 */
static ngx_int_t ngx_http_auth_request_handler(ngx_http_request_t *r);

/**
 * @brief 认证子请求完成后的回调函数
 *
 * 处理认证子请求完成后的结果，设置相应的状态和变量。
 *
 * @param r 指向主HTTP请求的指针
 * @param data 用户定义的数据（通常是上下文）
 * @param rc 子请求的返回代码
 * @return NGX_OK 如果处理成功，否则返回错误代码
 */
static ngx_int_t ngx_http_auth_request_done(ngx_http_request_t *r,
    void *data, ngx_int_t rc);

/**
 * @brief 设置认证请求相关的变量
 *
 * 根据认证结果设置配置中定义的变量。
 *
 * @param r 指向当前HTTP请求的指针
 * @param arcf 指向认证请求配置的指针
 * @param ctx 指向认证请求上下文的指针
 * @return NGX_OK 如果设置成功，否则返回错误代码
 */
static ngx_int_t ngx_http_auth_request_set_variables(ngx_http_request_t *r,
    ngx_http_auth_request_conf_t *arcf, ngx_http_auth_request_ctx_t *ctx);

/**
 * @brief 处理认证请求中的变量
 *
 * 获取或设置认证请求中使用的变量的值。
 *
 * @param r 指向当前HTTP请求的指针
 * @param v 指向变量值的指针
 * @param data 变量相关的数据
 * @return NGX_OK 如果处理成功，否则返回错误代码
 */
static ngx_int_t ngx_http_auth_request_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

/**
 * @brief 创建认证请求模块的配置结构
 *
 * 为每个位置块创建一个新的配置结构。
 *
 * @param cf 指向Nginx配置的指针
 * @return 指向新创建的配置结构的指针，如果失败则返回NULL
 */
static void *ngx_http_auth_request_create_conf(ngx_conf_t *cf);

/**
 * @brief 合并认证请求模块的配置
 *
 * 合并父配置和子配置，确保所有必要的值都被正确设置。
 *
 * @param cf 指向Nginx配置的指针
 * @param parent 指向父配置的指针
 * @param child 指向子配置的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 */
static char *ngx_http_auth_request_merge_conf(ngx_conf_t *cf,
    void *parent, void *child);

/**
 * @brief 初始化认证请求模块
 *
 * 在配置阶段完成后进行模块的初始化工作。
 *
 * @param cf 指向Nginx配置的指针
 * @return NGX_OK 如果初始化成功，否则返回NGX_ERROR
 */
static ngx_int_t ngx_http_auth_request_init(ngx_conf_t *cf);

/**
 * @brief 处理auth_request指令
 *
 * 解析和设置auth_request指令的配置。
 *
 * @param cf 指向Nginx配置的指针
 * @param cmd 指向当前命令的指针
 * @param conf 指向模块配置的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 */
static char *ngx_http_auth_request(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 处理auth_request_set指令
 *
 * 解析和设置auth_request_set指令的配置。
 *
 * @param cf 指向Nginx配置的指针
 * @param cmd 指向当前命令的指针
 * @param conf 指向模块配置的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 */
static char *ngx_http_auth_request_set(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


static ngx_command_t  ngx_http_auth_request_commands[] = {

    { ngx_string("auth_request"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_auth_request,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("auth_request_set"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_http_auth_request_set,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_auth_request_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_auth_request_init,            /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_auth_request_create_conf,     /* create location configuration */
    ngx_http_auth_request_merge_conf       /* merge location configuration */
};


ngx_module_t  ngx_http_auth_request_module = {
    NGX_MODULE_V1,
    &ngx_http_auth_request_module_ctx,     /* module context */
    ngx_http_auth_request_commands,        /* module directives */
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
ngx_http_auth_request_handler(ngx_http_request_t *r)
{
    ngx_table_elt_t               *h, *ho, **ph;
    ngx_http_request_t            *sr;
    ngx_http_post_subrequest_t    *ps;
    ngx_http_auth_request_ctx_t   *ctx;
    ngx_http_auth_request_conf_t  *arcf;

    arcf = ngx_http_get_module_loc_conf(r, ngx_http_auth_request_module);

    if (arcf->uri.len == 0) {
        return NGX_DECLINED;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "auth request handler");

    ctx = ngx_http_get_module_ctx(r, ngx_http_auth_request_module);

    if (ctx != NULL) {
        if (!ctx->done) {
            return NGX_AGAIN;
        }

        /*
         * as soon as we are done - explicitly set variables to make
         * sure they will be available after internal redirects
         */

        if (ngx_http_auth_request_set_variables(r, arcf, ctx) != NGX_OK) {
            return NGX_ERROR;
        }

        /* return appropriate status */

        if (ctx->status == NGX_HTTP_FORBIDDEN) {
            return ctx->status;
        }

        if (ctx->status == NGX_HTTP_UNAUTHORIZED) {
            sr = ctx->subrequest;

            h = sr->headers_out.www_authenticate;

            if (!h && sr->upstream) {
                h = sr->upstream->headers_in.www_authenticate;
            }

            ph = &r->headers_out.www_authenticate;

            while (h) {
                ho = ngx_list_push(&r->headers_out.headers);
                if (ho == NULL) {
                    return NGX_ERROR;
                }

                *ho = *h;
                ho->next = NULL;

                *ph = ho;
                ph = &ho->next;

                h = h->next;
            }

            return ctx->status;
        }

        if (ctx->status >= NGX_HTTP_OK
            && ctx->status < NGX_HTTP_SPECIAL_RESPONSE)
        {
            return NGX_OK;
        }

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "auth request unexpected status: %ui", ctx->status);

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_auth_request_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ps = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (ps == NULL) {
        return NGX_ERROR;
    }

    ps->handler = ngx_http_auth_request_done;
    ps->data = ctx;

    if (ngx_http_subrequest(r, &arcf->uri, NULL, &sr, ps,
                            NGX_HTTP_SUBREQUEST_WAITED)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    /*
     * allocate fake request body to avoid attempts to read it and to make
     * sure real body file (if already read) won't be closed by upstream
     */

    sr->request_body = ngx_pcalloc(r->pool, sizeof(ngx_http_request_body_t));
    if (sr->request_body == NULL) {
        return NGX_ERROR;
    }

    sr->header_only = 1;

    ctx->subrequest = sr;

    ngx_http_set_ctx(r, ctx, ngx_http_auth_request_module);

    return NGX_AGAIN;
}


static ngx_int_t
ngx_http_auth_request_done(ngx_http_request_t *r, void *data, ngx_int_t rc)
{
    ngx_http_auth_request_ctx_t   *ctx = data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "auth request done s:%ui", r->headers_out.status);

    ctx->done = 1;
    ctx->status = r->headers_out.status;

    return rc;
}


static ngx_int_t
ngx_http_auth_request_set_variables(ngx_http_request_t *r,
    ngx_http_auth_request_conf_t *arcf, ngx_http_auth_request_ctx_t *ctx)
{
    ngx_str_t                          val;
    ngx_http_variable_t               *v;
    ngx_http_variable_value_t         *vv;
    ngx_http_auth_request_variable_t  *av, *last;
    ngx_http_core_main_conf_t         *cmcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "auth request set variables");

    if (arcf->vars == NULL) {
        return NGX_OK;
    }

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
    v = cmcf->variables.elts;

    av = arcf->vars->elts;
    last = av + arcf->vars->nelts;

    while (av < last) {
        /*
         * explicitly set new value to make sure it will be available after
         * internal redirects
         */

        vv = &r->variables[av->index];

        if (ngx_http_complex_value(ctx->subrequest, &av->value, &val)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        vv->valid = 1;
        vv->not_found = 0;
        vv->data = val.data;
        vv->len = val.len;

        if (av->set_handler) {
            /*
             * set_handler only available in cmcf->variables_keys, so we store
             * it explicitly
             */

            av->set_handler(r, vv, v[av->index].data);
        }

        av++;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_auth_request_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "auth request variable");

    v->not_found = 1;

    return NGX_OK;
}


static void *
ngx_http_auth_request_create_conf(ngx_conf_t *cf)
{
    ngx_http_auth_request_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_auth_request_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->uri = { 0, NULL };
     */

    conf->vars = NGX_CONF_UNSET_PTR;

    return conf;
}


static char *
ngx_http_auth_request_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_auth_request_conf_t *prev = parent;
    ngx_http_auth_request_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->uri, prev->uri, "");
    ngx_conf_merge_ptr_value(conf->vars, prev->vars, NULL);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_auth_request_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_auth_request_handler;

    return NGX_OK;
}


static char *
ngx_http_auth_request(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_auth_request_conf_t *arcf = conf;

    ngx_str_t        *value;

    if (arcf->uri.data != NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        arcf->uri.len = 0;
        arcf->uri.data = (u_char *) "";

        return NGX_CONF_OK;
    }

    arcf->uri = value[1];

    return NGX_CONF_OK;
}


static char *
ngx_http_auth_request_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_auth_request_conf_t *arcf = conf;

    ngx_str_t                         *value;
    ngx_http_variable_t               *v;
    ngx_http_auth_request_variable_t  *av;
    ngx_http_compile_complex_value_t   ccv;

    value = cf->args->elts;

    if (value[1].data[0] != '$') {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid variable name \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }

    value[1].len--;
    value[1].data++;

    if (arcf->vars == NGX_CONF_UNSET_PTR) {
        arcf->vars = ngx_array_create(cf->pool, 1,
                                      sizeof(ngx_http_auth_request_variable_t));
        if (arcf->vars == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    av = ngx_array_push(arcf->vars);
    if (av == NULL) {
        return NGX_CONF_ERROR;
    }

    v = ngx_http_add_variable(cf, &value[1], NGX_HTTP_VAR_CHANGEABLE);
    if (v == NULL) {
        return NGX_CONF_ERROR;
    }

    av->index = ngx_http_get_variable_index(cf, &value[1]);
    if (av->index == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    if (v->get_handler == NULL) {
        v->get_handler = ngx_http_auth_request_variable;
        v->data = (uintptr_t) av;
    }

    av->set_handler = v->set_handler;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &av->value;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
