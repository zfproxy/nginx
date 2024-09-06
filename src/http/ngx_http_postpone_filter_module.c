
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_postpone_filter_module.c
 *
 * 该模块实现了Nginx的HTTP延迟过滤器功能，用于处理子请求和主请求的输出顺序。
 *
 * 支持的功能:
 * 1. 管理子请求和主请求的输出顺序
 * 2. 缓存子请求的输出，直到主请求准备好发送
 * 3. 确保响应体按正确顺序发送给客户端
 * 4. 处理嵌套子请求的情况
 * 5. 优化内存使用，避免不必要的数据复制
 *
 * 支持的指令:
 * 该模块不直接提供可配置的指令，而是作为Nginx核心功能的一部分自动运行。
 *
 * 相关变量:
 * 本模块不直接提供可在配置中使用的变量。
 *
 * 使用注意点:
 * 1. 延迟过滤器会影响响应的发送顺序，可能会增加响应时间
 * 2. 在处理大量并发请求时，需要注意内存使用情况
 * 3. 复杂的子请求嵌套可能会增加处理复杂度和资源消耗
 * 4. 在调试性能问题时，可能需要关注此模块的行为
 * 5. 该模块与其他过滤器模块的交互可能会影响整体性能
 * 6. 在使用subrequest功能时，需要考虑延迟过滤器的影响
 * 7. 对于需要实时响应的应用，可能需要评估使用延迟过滤器的影响
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/*
 * 函数: ngx_http_postpone_filter_add
 * 功能: 向延迟过滤器添加数据
 * 参数:
 *   r: HTTP请求对象
 *   in: 输入的数据链
 * 返回值: ngx_int_t类型，表示操作的结果
 */
static ngx_int_t ngx_http_postpone_filter_add(ngx_http_request_t *r,
    ngx_chain_t *in);
/*
 * 函数: ngx_http_postpone_filter_in_memory
 * 功能: 处理内存中的延迟过滤
 * 参数:
 *   r: HTTP请求对象
 *   in: 输入的数据链
 * 返回值: ngx_int_t类型，表示操作的结果
 * 说明: 该函数用于处理子请求在内存中的情况下的延迟过滤
 */
static ngx_int_t ngx_http_postpone_filter_in_memory(ngx_http_request_t *r,
    ngx_chain_t *in);
/*
 * 函数: ngx_http_postpone_filter_init
 * 功能: 初始化延迟过滤器模块
 * 参数:
 *   cf: nginx配置结构体指针
 * 返回值: ngx_int_t类型，表示初始化的结果
 * 说明: 该函数在nginx配置阶段被调用，用于设置延迟过滤器的初始化工作
 */
static ngx_int_t ngx_http_postpone_filter_init(ngx_conf_t *cf);


/*
 * 定义延迟过滤器模块的上下文结构
 * 该结构包含了模块的各种回调函数指针
 * 大多数字段为NULL，表示不需要相应的处理
 * 只设置了postconfiguration字段，指向模块的初始化函数
 */
static ngx_http_module_t  ngx_http_postpone_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_postpone_filter_init,         /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


/*
 * 定义延迟过滤器模块的主结构
 * 该结构包含了模块的基本信息和配置
 * ngx_module_t 是 Nginx 模块的标准结构
 */
ngx_module_t  ngx_http_postpone_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_postpone_filter_module_ctx,  /* module context */
    NULL,                                  /* module directives */
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


/*
 * 定义下一个输出体过滤器的函数指针
 * 这个指针用于存储过滤器链中的下一个过滤器函数
 * 在当前过滤器处理完后，会调用这个函数继续处理
 */
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


/*
 * 延迟过滤器的主要处理函数
 * @param r: HTTP请求结构体指针
 * @param in: 输入的数据链
 * @return: 处理结果的状态码
 */
static ngx_int_t
ngx_http_postpone_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_connection_t              *c;
    ngx_http_postponed_request_t  *pr;

    c = r->connection;

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http postpone filter \"%V?%V\" %p", &r->uri, &r->args, in);

    if (r->subrequest_in_memory) {
        return ngx_http_postpone_filter_in_memory(r, in);
    }

    if (r != c->data) {

        if (in) {
            if (ngx_http_postpone_filter_add(r, in) != NGX_OK) {
                return NGX_ERROR;
            }

            return NGX_OK;
        }

#if 0
        /* TODO: SSI may pass NULL */
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "http postpone filter NULL inactive request");
#endif

        return NGX_OK;
    }

    if (r->postponed == NULL) {

        if (in || c->buffered) {
            return ngx_http_next_body_filter(r->main, in);
        }

        return NGX_OK;
    }

    if (in) {
        if (ngx_http_postpone_filter_add(r, in) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    do {
        pr = r->postponed;

        if (pr->request) {

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http postpone filter wake \"%V?%V\"",
                           &pr->request->uri, &pr->request->args);

            r->postponed = pr->next;

            c->data = pr->request;

            return ngx_http_post_request(pr->request, NULL);
        }

        if (pr->out == NULL) {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "http postpone filter NULL output");

        } else {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http postpone filter output \"%V?%V\"",
                           &r->uri, &r->args);

            if (ngx_http_next_body_filter(r->main, pr->out) == NGX_ERROR) {
                return NGX_ERROR;
            }
        }

        r->postponed = pr->next;

    } while (r->postponed);

    return NGX_OK;
}


static ngx_int_t
ngx_http_postpone_filter_add(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_postponed_request_t  *pr, **ppr;

    if (r->postponed) {
        for (pr = r->postponed; pr->next; pr = pr->next) { /* void */ }

        if (pr->request == NULL) {
            goto found;
        }

        ppr = &pr->next;

    } else {
        ppr = &r->postponed;
    }

    pr = ngx_palloc(r->pool, sizeof(ngx_http_postponed_request_t));
    if (pr == NULL) {
        return NGX_ERROR;
    }

    *ppr = pr;

    pr->request = NULL;
    pr->out = NULL;
    pr->next = NULL;

found:

    if (ngx_chain_add_copy(r->pool, &pr->out, in) == NGX_OK) {
        return NGX_OK;
    }

    return NGX_ERROR;
}


static ngx_int_t
ngx_http_postpone_filter_in_memory(ngx_http_request_t *r, ngx_chain_t *in)
{
    size_t                     len;
    ngx_buf_t                 *b;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http postpone filter in memory");

    if (r->out == NULL) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (r->headers_out.content_length_n != -1) {
            len = r->headers_out.content_length_n;

            if (len > clcf->subrequest_output_buffer_size) {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "too big subrequest response: %uz", len);
                return NGX_ERROR;
            }

        } else {
            len = clcf->subrequest_output_buffer_size;
        }

        b = ngx_create_temp_buf(r->pool, len);
        if (b == NULL) {
            return NGX_ERROR;
        }

        b->last_buf = 1;

        r->out = ngx_alloc_chain_link(r->pool);
        if (r->out == NULL) {
            return NGX_ERROR;
        }

        r->out->buf = b;
        r->out->next = NULL;
    }

    b = r->out->buf;

    for ( /* void */ ; in; in = in->next) {

        if (ngx_buf_special(in->buf)) {
            continue;
        }

        len = in->buf->last - in->buf->pos;

        if (len > (size_t) (b->end - b->last)) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0,
                          "too big subrequest response");
            return NGX_ERROR;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http postpone filter in memory %uz bytes", len);

        b->last = ngx_cpymem(b->last, in->buf->pos, len);
        in->buf->pos = in->buf->last;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_postpone_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_postpone_filter;

    return NGX_OK;
}
