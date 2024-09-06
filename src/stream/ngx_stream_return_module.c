
/*
 * Copyright (C) Roman Arutyunyan
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_stream_return_module.c
 *
 * 该模块实现了Nginx流模块的返回功能。
 *
 * 支持的功能：
 * 1. 向客户端返回指定的文本内容
 * 2. 支持变量和复杂表达式作为返回内容
 * 3. 可以在会话建立时立即返回内容并关闭连接
 *
 * 支持的指令：
 * - return: 设置要返回的内容
 *   语法: return text;
 *   上下文: server
 *
 * 支持的变量：
 * 该模块不定义特定变量，但可以在return指令中使用任何有效的Nginx变量
 *
 * 使用注意点：
 * 1. return指令会导致会话立即终止，后续处理将不再执行
 * 2. 返回的内容支持变量插值，可以动态生成
 * 3. 大量使用复杂的返回内容可能会影响性能
 * 4. 返回的内容会直接发送给客户端，注意避免敏感信息泄露
 * 5. 如果返回内容较大，注意设置适当的缓冲区大小
 * 6. 可以结合其他模块（如access模块）使用，实现条件返回
 * 7. 在SSL/TLS连接中使用时，确保在握手完成后再返回内容
 * 8. 返回的内容不会自动添加换行符，如需换行请手动添加
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>


/**
 * @brief return 指令的服务器配置结构体
 */
typedef struct {
    ngx_stream_complex_value_t   text;  /* 存储要返回的文本内容 */
} ngx_stream_return_srv_conf_t;


/**
 * @brief return 模块的上下文结构体
 */
typedef struct {
    ngx_chain_t                 *out;  /* 存储输出数据的链表 */
} ngx_stream_return_ctx_t;


/**
 * @brief return 指令的主处理函数
 * @param s 流会话结构体
 */
static void ngx_stream_return_handler(ngx_stream_session_t *s);

/**
 * @brief return 指令的写事件处理函数
 * @param ev 事件结构体
 */
static void ngx_stream_return_write_handler(ngx_event_t *ev);

/**
 * @brief 创建 return 指令的服务器配置
 * @param cf 配置结构体
 * @return 创建的配置结构体指针
 */
static void *ngx_stream_return_create_srv_conf(ngx_conf_t *cf);

/**
 * @brief return 指令的解析函数
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return 解析结果
 */
static char *ngx_stream_return(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_command_t  ngx_stream_return_commands[] = {

    { ngx_string("return"),
      NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_stream_return,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_stream_module_t  ngx_stream_return_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_stream_return_create_srv_conf,     /* create server configuration */
    NULL                                   /* merge server configuration */
};


ngx_module_t  ngx_stream_return_module = {
    NGX_MODULE_V1,
    &ngx_stream_return_module_ctx,         /* module context */
    ngx_stream_return_commands,            /* module directives */
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


static void
ngx_stream_return_handler(ngx_stream_session_t *s)
{
    ngx_str_t                      text;
    ngx_buf_t                     *b;
    ngx_connection_t              *c;
    ngx_stream_return_ctx_t       *ctx;
    ngx_stream_return_srv_conf_t  *rscf;

    c = s->connection;

    c->log->action = "returning text";

    rscf = ngx_stream_get_module_srv_conf(s, ngx_stream_return_module);

    if (ngx_stream_complex_value(s, &rscf->text, &text) != NGX_OK) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0,
                   "stream return text: \"%V\"", &text);

    if (text.len == 0) {
        ngx_stream_finalize_session(s, NGX_STREAM_OK);
        return;
    }

    ctx = ngx_pcalloc(c->pool, sizeof(ngx_stream_return_ctx_t));
    if (ctx == NULL) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_stream_set_ctx(s, ctx, ngx_stream_return_module);

    b = ngx_calloc_buf(c->pool);
    if (b == NULL) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    b->memory = 1;
    b->pos = text.data;
    b->last = text.data + text.len;
    b->last_buf = 1;

    ctx->out = ngx_alloc_chain_link(c->pool);
    if (ctx->out == NULL) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ctx->out->buf = b;
    ctx->out->next = NULL;

    c->write->handler = ngx_stream_return_write_handler;

    ngx_stream_return_write_handler(c->write);
}


static void
ngx_stream_return_write_handler(ngx_event_t *ev)
{
    ngx_connection_t         *c;
    ngx_stream_session_t     *s;
    ngx_stream_return_ctx_t  *ctx;

    c = ev->data;
    s = c->data;

    if (ev->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "connection timed out");
        ngx_stream_finalize_session(s, NGX_STREAM_OK);
        return;
    }

    ctx = ngx_stream_get_module_ctx(s, ngx_stream_return_module);

    if (ngx_stream_top_filter(s, ctx->out, 1) == NGX_ERROR) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ctx->out = NULL;

    if (!c->buffered) {
        ngx_log_debug0(NGX_LOG_DEBUG_STREAM, c->log, 0,
                       "stream return done sending");
        ngx_stream_finalize_session(s, NGX_STREAM_OK);
        return;
    }

    if (ngx_handle_write_event(ev, 0) != NGX_OK) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_add_timer(ev, 5000);
}


static void *
ngx_stream_return_create_srv_conf(ngx_conf_t *cf)
{
    ngx_stream_return_srv_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_return_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}


static char *
ngx_stream_return(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_return_srv_conf_t *rscf = conf;

    ngx_str_t                           *value;
    ngx_stream_core_srv_conf_t          *cscf;
    ngx_stream_compile_complex_value_t   ccv;

    if (rscf->text.value.data) {
        return "is duplicate";
    }

    value = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_stream_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &rscf->text;

    if (ngx_stream_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    cscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_core_module);

    cscf->handler = ngx_stream_return_handler;

    return NGX_CONF_OK;
}
