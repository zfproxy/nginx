
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_flv_module.c
 *
 * 该模块提供了对FLV（Flash Video）文件的支持。
 * 它允许Nginx服务器以流媒体方式传输FLV文件，支持从指定时间点开始播放。
 *
 * 支持的功能:
 * 1. 处理FLV文件请求
 * 2. 支持从指定时间点开始播放（使用start参数）
 * 3. 添加FLV文件头
 * 4. 处理部分内容请求（支持Range头）
 *
 * 支持的指令:
 * - flv
 *   语法: flv;
 *   上下文: location
 *   描述: 启用FLV模块处理当前location的请求
 *
 * 支持的变量:
 * 本模块未定义特定变量，但可以使用Nginx核心提供的标准变量。
 *
 * 使用注意点:
 * 1. 确保正确配置MIME类型，通常为 application/x-flv
 * 2. 使用start参数时，值应为毫秒
 * 3. 建议配合ngx_http_mp4_module使用，以支持更多流媒体格式
 * 4. 注意处理大文件时的性能影响
 * 5. 考虑使用缓存来提高性能
 */



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief 处理 flv 指令的函数
 *
 * 这个函数用于处理 nginx 配置文件中的 flv 指令。
 * 当在 location 块中使用 flv 指令时，会调用此函数进行相应的设置。
 *
 * @param cf 指向 nginx 配置结构的指针
 * @param cmd 指向当前命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回 NGX_CONF_OK，失败时返回错误信息字符串
 */
static char *ngx_http_flv(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_command_t  ngx_http_flv_commands[] = {

    { ngx_string("flv"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_flv,
      0,
      0,
      NULL },

      ngx_null_command
};


/**
 * @brief FLV文件头
 *
 * 这是一个标准的FLV（Flash Video）文件头。
 * 它包含以下信息：
 * - "FLV": 文件类型标识符
 * - \x1: 版本号（1）
 * - \x5: 流信息标志（表示包含音频和视频）
 * - \0\0\0\x9: 数据偏移（9字节）
 * - \0\0\0\0: 前一个标签大小（初始为0）
 *
 * 这个头部会被添加到每个FLV响应的开始，以确保客户端能正确识别和处理FLV文件。
 */
static u_char  ngx_flv_header[] = "FLV\x1\x5\0\0\0\x9\0\0\0\0";


static ngx_http_module_t  ngx_http_flv_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    NULL,                          /* create location configuration */
    NULL                           /* merge location configuration */
};


ngx_module_t  ngx_http_flv_module = {
    NGX_MODULE_V1,
    &ngx_http_flv_module_ctx,      /* module context */
    ngx_http_flv_commands,         /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};


/**
 * @brief FLV请求处理函数
 *
 * 该函数用于处理FLV（Flash Video）文件的HTTP请求。
 * 它负责解析请求，定位FLV文件，并发送适当的响应。
 *
 * @param r 指向当前HTTP请求结构的指针
 * @return NGX_DECLINED 如果请求不应由此模块处理
 *         NGX_HTTP_NOT_ALLOWED 如果请求方法不被允许
 *         NGX_HTTP_INTERNAL_SERVER_ERROR 如果发生内部错误
 *         其他 由下游函数返回的状态码
 */
static ngx_int_t
ngx_http_flv_handler(ngx_http_request_t *r)
{
    u_char                    *last;
    off_t                      start, len;
    size_t                     root;
    ngx_int_t                  rc;
    ngx_uint_t                 level, i;
    ngx_str_t                  path, value;
    ngx_log_t                 *log;
    ngx_buf_t                 *b;
    ngx_chain_t                out[2];
    ngx_open_file_info_t       of;
    ngx_http_core_loc_conf_t  *clcf;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (r->uri.data[r->uri.len - 1] == '/') {
        return NGX_DECLINED;
    }

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    if (last == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    log = r->connection->log;

    path.len = last - path.data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "http flv filename: \"%V\"", &path);

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.read_ahead = clcf->read_ahead;
    of.directio = clcf->directio;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;

    if (ngx_http_set_disable_symlinks(r, clcf, &path, &of) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool)
        != NGX_OK)
    {
        switch (of.err) {

        case 0:
            return NGX_HTTP_INTERNAL_SERVER_ERROR;

        case NGX_ENOENT:
        case NGX_ENOTDIR:
        case NGX_ENAMETOOLONG:

            level = NGX_LOG_ERR;
            rc = NGX_HTTP_NOT_FOUND;
            break;

        case NGX_EACCES:
#if (NGX_HAVE_OPENAT)
        case NGX_EMLINK:
        case NGX_ELOOP:
#endif

            level = NGX_LOG_ERR;
            rc = NGX_HTTP_FORBIDDEN;
            break;

        default:

            level = NGX_LOG_CRIT;
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
            break;
        }

        if (rc != NGX_HTTP_NOT_FOUND || clcf->log_not_found) {
            ngx_log_error(level, log, of.err,
                          "%s \"%s\" failed", of.failed, path.data);
        }

        return rc;
    }

    if (!of.is_file) {
        return NGX_DECLINED;
    }

    r->root_tested = !r->error_page;

    start = 0;
    len = of.size;
    i = 1;

    if (r->args.len) {

        if (ngx_http_arg(r, (u_char *) "start", 5, &value) == NGX_OK) {

            start = ngx_atoof(value.data, value.len);

            if (start == NGX_ERROR || start >= len) {
                start = 0;
            }

            if (start) {
                len = sizeof(ngx_flv_header) - 1 + len - start;
                i = 0;
            }
        }
    }

    log->action = "sending flv to client";

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = len;
    r->headers_out.last_modified_time = of.mtime;

    if (ngx_http_set_etag(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_set_content_type(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (i == 0) {
        b = ngx_calloc_buf(r->pool);
        if (b == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b->pos = ngx_flv_header;
        b->last = ngx_flv_header + sizeof(ngx_flv_header) - 1;
        b->memory = 1;

        out[0].buf = b;
        out[0].next = &out[1];
    }


    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    if (b->file == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->allow_ranges = 1;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    b->file_pos = start;
    b->file_last = of.size;

    b->in_file = b->file_last ? 1 : 0;
    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;
    b->sync = (b->last_buf || b->in_file) ? 0 : 1;

    b->file->fd = of.fd;
    b->file->name = path;
    b->file->log = log;
    b->file->directio = of.is_directio;

    out[1].buf = b;
    out[1].next = NULL;

    return ngx_http_output_filter(r, &out[i]);
}


static char *
ngx_http_flv(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_flv_handler;

    return NGX_CONF_OK;
}
