
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_charset_filter_module.c
 *
 * 该模块实现了HTTP响应的字符集转换功能。
 *
 * 支持的功能:
 * 1. 将响应内容从一种字符编码转换为另一种
 * 2. 自动检测源字符集
 * 3. 支持UTF-8和多种其他字符编码
 * 4. 可以对特定MIME类型进行字符集转换
 * 5. 支持HTML实体的转换
 *
 * 支持的指令:
 * - charset: 设置默认的字符集
 *   语法: charset charset | off;
 *   默认值: off
 *   上下文: http, server, location, location if
 *
 * - charset_map: 定义两个字符集之间的转换映射
 *   语法: charset_map charset1 charset2 { ... }
 *   上下文: http
 *
 * - override_charset: 是否覆盖已有的Content-Type头中的字符集
 *   语法: override_charset on | off;
 *   默认值: off
 *   上下文: http, server, location, location if
 *
 * - charset_types: 指定需要进行字符集转换的MIME类型
 *   语法: charset_types mime-type ...;
 *   默认值: charset_types text/html text/xml text/plain text/vnd.wap.wml application/javascript application/rss+xml;
 *   上下文: http, server, location
 *
 * - source_charset: 设置源字符集
 *   语法: source_charset charset;
 *   上下文: http, server, location, location if
 *
 * 支持的变量:
 * - $charset: 当前使用的字符集
 *
 * 使用注意点:
 * 1. 确保正确配置源字符集和目标字符集，避免数据丢失
 * 2. 字符集转换可能会增加服务器负载，建议只在必要时启用
 * 3. 对于大型响应，字符集转换可能会显著增加处理时间
 * 4. 某些特殊字符可能在转换过程中丢失或被替换
 * 5. 使用override_charset时要谨慎，以免覆盖客户端期望的字符集
 */



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


// 字符集转换已禁用的标志
#define NGX_HTTP_CHARSET_OFF    -2

// 没有指定字符集的标志
#define NGX_HTTP_NO_CHARSET     -3

// 字符集为变量的标志
#define NGX_HTTP_CHARSET_VAR    0x10000

/* 1字节长度加上最多3字节用于UCS-2的UTF-8编码 */
#define NGX_UTF_LEN             4

// HTML实体的最大长度（用于表示Unicode码点1114111）
#define NGX_HTML_ENTITY_LEN     (sizeof("&#1114111;") - 1)


// 定义字符集结构体
typedef struct {
    u_char                    **tables;    // 字符转换表数组
    ngx_str_t                   name;      // 字符集名称

    unsigned                    length:16; // 字符集编码的最大字节长度
    unsigned                    utf8:1;    // 是否为UTF-8编码
} ngx_http_charset_t;


// 定义字符集重编码结构体
typedef struct {
    ngx_int_t                   src;       // 源字符集索引
    ngx_int_t                   dst;       // 目标字符集索引
} ngx_http_charset_recode_t;


// 定义字符集转换表结构体
typedef struct {
    ngx_int_t                   src;       // 源字符集索引
    ngx_int_t                   dst;       // 目标字符集索引
    u_char                     *src2dst;   // 源到目标的转换表
    u_char                     *dst2src;   // 目标到源的转换表
} ngx_http_charset_tables_t;


typedef struct {
    ngx_array_t                 charsets;       /* ngx_http_charset_t */
    ngx_array_t                 tables;         /* ngx_http_charset_tables_t */
    ngx_array_t                 recodes;        /* ngx_http_charset_recode_t */
} ngx_http_charset_main_conf_t;


// 定义字符集位置配置结构体
typedef struct {
    ngx_int_t                   charset;         // 目标字符集
    ngx_int_t                   source_charset;  // 源字符集
    ngx_flag_t                  override_charset;// 是否覆盖字符集设置

    ngx_hash_t                  types;           // 支持字符集转换的MIME类型哈希表
    ngx_array_t                *types_keys;      // 支持字符集转换的MIME类型键数组
} ngx_http_charset_loc_conf_t;


// 定义字符集转换上下文结构体
typedef struct {
    u_char                     *table;           // 字符转换表
    ngx_int_t                   charset;         // 当前字符集
    ngx_str_t                   charset_name;    // 字符集名称

    ngx_chain_t                *busy;            // 正在使用的缓冲区链
    ngx_chain_t                *free_bufs;       // 空闲缓冲区链
    ngx_chain_t                *free_buffers;    // 空闲大缓冲区链

    size_t                      saved_len;       // 保存的未处理字符长度
    u_char                      saved[NGX_UTF_LEN]; // 保存的未处理字符

    unsigned                    length:16;       // 字符集编码的最大字节长度
    unsigned                    from_utf8:1;     // 是否从UTF-8转换
    unsigned                    to_utf8:1;       // 是否转换到UTF-8
} ngx_http_charset_ctx_t;


// 定义字符集配置上下文结构体
typedef struct {
    ngx_http_charset_tables_t  *table;           // 字符集转换表
    ngx_http_charset_t         *charset;         // 字符集信息
    ngx_uint_t                  characters;      // 字符数量
} ngx_http_charset_conf_ctx_t;


// 获取目标字符集
static ngx_int_t ngx_http_destination_charset(ngx_http_request_t *r,
    ngx_str_t *name);

// 获取主请求的字符集
static ngx_int_t ngx_http_main_request_charset(ngx_http_request_t *r,
    ngx_str_t *name);

// 获取源字符集
static ngx_int_t ngx_http_source_charset(ngx_http_request_t *r,
    ngx_str_t *name);

// 获取字符集
static ngx_int_t ngx_http_get_charset(ngx_http_request_t *r, ngx_str_t *name);

// 设置字符集
static ngx_inline void ngx_http_set_charset(ngx_http_request_t *r,
    ngx_str_t *charset);

// 创建或获取字符集上下文
static ngx_int_t ngx_http_charset_ctx(ngx_http_request_t *r,
    ngx_http_charset_t *charsets, ngx_int_t charset, ngx_int_t source_charset);

// 使用转换表进行字符重编码
static ngx_uint_t ngx_http_charset_recode(ngx_buf_t *b, u_char *table);

// 从UTF-8重编码到其他字符集
static ngx_chain_t *ngx_http_charset_recode_from_utf8(ngx_pool_t *pool,
    ngx_buf_t *buf, ngx_http_charset_ctx_t *ctx);

// 从其他字符集重编码到UTF-8
static ngx_chain_t *ngx_http_charset_recode_to_utf8(ngx_pool_t *pool,
    ngx_buf_t *buf, ngx_http_charset_ctx_t *ctx);

// 从内存池中获取缓冲区
static ngx_chain_t *ngx_http_charset_get_buf(ngx_pool_t *pool,
    ngx_http_charset_ctx_t *ctx);

// 从内存池中获取指定大小的缓冲区
static ngx_chain_t *ngx_http_charset_get_buffer(ngx_pool_t *pool,
    ngx_http_charset_ctx_t *ctx, size_t size);

// 处理字符集映射块配置
static char *ngx_http_charset_map_block(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

// 处理字符集映射配置
static char *ngx_http_charset_map(ngx_conf_t *cf, ngx_command_t *dummy,
    void *conf);

// 设置字符集配置槽
static char *ngx_http_set_charset_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

// 向字符集数组中添加新的字符集
static ngx_int_t ngx_http_add_charset(ngx_array_t *charsets, ngx_str_t *name);

// 创建主配置结构
static void *ngx_http_charset_create_main_conf(ngx_conf_t *cf);

// 创建位置配置结构
static void *ngx_http_charset_create_loc_conf(ngx_conf_t *cf);

// 合并位置配置
static char *ngx_http_charset_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

// 模块后配置处理
static ngx_int_t ngx_http_charset_postconfiguration(ngx_conf_t *cf);


/**
 * @brief 定义默认的字符集处理MIME类型数组
 *
 * 这个数组列出了默认情况下需要进行字符集处理的MIME类型。
 * 这些MIME类型通常包含文本内容，可能需要进行字符集转换。
 */
static ngx_str_t  ngx_http_charset_default_types[] = {
    ngx_string("text/html"),
    ngx_string("text/xml"),
    ngx_string("text/plain"),
    ngx_string("text/vnd.wap.wml"),
    ngx_string("application/javascript"),
    ngx_string("application/rss+xml"),
    ngx_null_string
};


static ngx_command_t  ngx_http_charset_filter_commands[] = {

    { ngx_string("charset"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF
                        |NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
      ngx_http_set_charset_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_charset_loc_conf_t, charset),
      NULL },

    { ngx_string("source_charset"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF
                        |NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
      ngx_http_set_charset_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_charset_loc_conf_t, source_charset),
      NULL },

    { ngx_string("override_charset"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF
                        |NGX_HTTP_LIF_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_charset_loc_conf_t, override_charset),
      NULL },

    { ngx_string("charset_types"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_types_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_charset_loc_conf_t, types_keys),
      &ngx_http_charset_default_types[0] },

    { ngx_string("charset_map"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE2,
      ngx_http_charset_map_block,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_charset_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_charset_postconfiguration,    /* postconfiguration */

    ngx_http_charset_create_main_conf,     /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_charset_create_loc_conf,      /* create location configuration */
    ngx_http_charset_merge_loc_conf        /* merge location configuration */
};


ngx_module_t  ngx_http_charset_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_charset_filter_module_ctx,   /* module context */
    ngx_http_charset_filter_commands,      /* module directives */
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


/* 指向下一个HTTP头部过滤器的函数指针 */
static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;

/* 指向下一个HTTP主体过滤器的函数指针 */
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


/**
 * @brief HTTP字符集头部过滤器函数
 *
 * 该函数用于处理HTTP响应的字符集转换。
 * 它检查请求和响应的字符集，并在必要时进行转换。
 *
 * @param r 指向当前HTTP请求的指针
 * @return NGX_OK 成功时返回
 *         NGX_ERROR 发生错误时返回
 *         NGX_DECLINED 不需要处理时返回
 */
static ngx_int_t
ngx_http_charset_header_filter(ngx_http_request_t *r)
{
    ngx_int_t                      charset, source_charset;
    ngx_str_t                      dst, src;
    ngx_http_charset_t            *charsets;
    ngx_http_charset_main_conf_t  *mcf;

    if (r == r->main) {
        charset = ngx_http_destination_charset(r, &dst);

    } else {
        charset = ngx_http_main_request_charset(r, &dst);
    }

    if (charset == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (charset == NGX_DECLINED) {
        return ngx_http_next_header_filter(r);
    }

    /* charset: charset index or NGX_HTTP_NO_CHARSET */

    source_charset = ngx_http_source_charset(r, &src);

    if (source_charset == NGX_ERROR) {
        return NGX_ERROR;
    }

    /*
     * source_charset: charset index, NGX_HTTP_NO_CHARSET,
     *                 or NGX_HTTP_CHARSET_OFF
     */

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "charset: \"%V\" > \"%V\"", &src, &dst);

    if (source_charset == NGX_HTTP_CHARSET_OFF) {
        ngx_http_set_charset(r, &dst);

        return ngx_http_next_header_filter(r);
    }

    if (charset == NGX_HTTP_NO_CHARSET
        || source_charset == NGX_HTTP_NO_CHARSET)
    {
        if (source_charset != charset
            || ngx_strncasecmp(dst.data, src.data, dst.len) != 0)
        {
            goto no_charset_map;
        }

        ngx_http_set_charset(r, &dst);

        return ngx_http_next_header_filter(r);
    }

    if (source_charset == charset) {
        r->headers_out.content_type.len = r->headers_out.content_type_len;

        ngx_http_set_charset(r, &dst);

        return ngx_http_next_header_filter(r);
    }

    /* source_charset != charset */

    if (r->headers_out.content_encoding
        && r->headers_out.content_encoding->value.len)
    {
        return ngx_http_next_header_filter(r);
    }

    mcf = ngx_http_get_module_main_conf(r, ngx_http_charset_filter_module);
    charsets = mcf->charsets.elts;

    if (charsets[source_charset].tables == NULL
        || charsets[source_charset].tables[charset] == NULL)
    {
        goto no_charset_map;
    }

    r->headers_out.content_type.len = r->headers_out.content_type_len;

    ngx_http_set_charset(r, &dst);

    return ngx_http_charset_ctx(r, charsets, charset, source_charset);

no_charset_map:

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "no \"charset_map\" between the charsets \"%V\" and \"%V\"",
                  &src, &dst);

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_destination_charset(ngx_http_request_t *r, ngx_str_t *name)
{
    ngx_int_t                      charset;
    ngx_http_charset_t            *charsets;
    ngx_http_variable_value_t     *vv;
    ngx_http_charset_loc_conf_t   *mlcf;
    ngx_http_charset_main_conf_t  *mcf;

    if (r->headers_out.content_type.len == 0) {
        return NGX_DECLINED;
    }

    if (r->headers_out.override_charset
        && r->headers_out.override_charset->len)
    {
        *name = *r->headers_out.override_charset;

        charset = ngx_http_get_charset(r, name);

        if (charset != NGX_HTTP_NO_CHARSET) {
            return charset;
        }

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "unknown charset \"%V\" to override", name);

        return NGX_DECLINED;
    }

    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_charset_filter_module);
    charset = mlcf->charset;

    if (charset == NGX_HTTP_CHARSET_OFF) {
        return NGX_DECLINED;
    }

    if (r->headers_out.charset.len) {
        if (mlcf->override_charset == 0) {
            return NGX_DECLINED;
        }

    } else {
        if (ngx_http_test_content_type(r, &mlcf->types) == NULL) {
            return NGX_DECLINED;
        }
    }

    if (charset < NGX_HTTP_CHARSET_VAR) {
        mcf = ngx_http_get_module_main_conf(r, ngx_http_charset_filter_module);
        charsets = mcf->charsets.elts;
        *name = charsets[charset].name;
        return charset;
    }

    vv = ngx_http_get_indexed_variable(r, charset - NGX_HTTP_CHARSET_VAR);

    if (vv == NULL || vv->not_found) {
        return NGX_ERROR;
    }

    name->len = vv->len;
    name->data = vv->data;

    return ngx_http_get_charset(r, name);
}


static ngx_int_t
ngx_http_main_request_charset(ngx_http_request_t *r, ngx_str_t *src)
{
    ngx_int_t                charset;
    ngx_str_t               *main_charset;
    ngx_http_charset_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r->main, ngx_http_charset_filter_module);

    if (ctx) {
        *src = ctx->charset_name;
        return ctx->charset;
    }

    main_charset = &r->main->headers_out.charset;

    if (main_charset->len == 0) {
        return NGX_DECLINED;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_charset_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r->main, ctx, ngx_http_charset_filter_module);

    charset = ngx_http_get_charset(r, main_charset);

    ctx->charset = charset;
    ctx->charset_name = *main_charset;
    *src = *main_charset;

    return charset;
}


static ngx_int_t
ngx_http_source_charset(ngx_http_request_t *r, ngx_str_t *name)
{
    ngx_int_t                      charset;
    ngx_http_charset_t            *charsets;
    ngx_http_variable_value_t     *vv;
    ngx_http_charset_loc_conf_t   *lcf;
    ngx_http_charset_main_conf_t  *mcf;

    if (r->headers_out.charset.len) {
        *name = r->headers_out.charset;
        return ngx_http_get_charset(r, name);
    }

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_charset_filter_module);

    charset = lcf->source_charset;

    if (charset == NGX_HTTP_CHARSET_OFF) {
        name->len = 0;
        return charset;
    }

    if (charset < NGX_HTTP_CHARSET_VAR) {
        mcf = ngx_http_get_module_main_conf(r, ngx_http_charset_filter_module);
        charsets = mcf->charsets.elts;
        *name = charsets[charset].name;
        return charset;
    }

    vv = ngx_http_get_indexed_variable(r, charset - NGX_HTTP_CHARSET_VAR);

    if (vv == NULL || vv->not_found) {
        return NGX_ERROR;
    }

    name->len = vv->len;
    name->data = vv->data;

    return ngx_http_get_charset(r, name);
}


static ngx_int_t
ngx_http_get_charset(ngx_http_request_t *r, ngx_str_t *name)
{
    ngx_uint_t                     i, n;
    ngx_http_charset_t            *charset;
    ngx_http_charset_main_conf_t  *mcf;

    mcf = ngx_http_get_module_main_conf(r, ngx_http_charset_filter_module);

    charset = mcf->charsets.elts;
    n = mcf->charsets.nelts;

    for (i = 0; i < n; i++) {
        if (charset[i].name.len != name->len) {
            continue;
        }

        if (ngx_strncasecmp(charset[i].name.data, name->data, name->len) == 0) {
            return i;
        }
    }

    return NGX_HTTP_NO_CHARSET;
}


static ngx_inline void
ngx_http_set_charset(ngx_http_request_t *r, ngx_str_t *charset)
{
    if (r != r->main) {
        return;
    }

    if (r->headers_out.status == NGX_HTTP_MOVED_PERMANENTLY
        || r->headers_out.status == NGX_HTTP_MOVED_TEMPORARILY)
    {
        /*
         * do not set charset for the redirect because NN 4.x
         * use this charset instead of the next page charset
         */

        r->headers_out.charset.len = 0;
        return;
    }

    r->headers_out.charset = *charset;
}


static ngx_int_t
ngx_http_charset_ctx(ngx_http_request_t *r, ngx_http_charset_t *charsets,
    ngx_int_t charset, ngx_int_t source_charset)
{
    ngx_http_charset_ctx_t  *ctx;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_charset_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, ctx, ngx_http_charset_filter_module);

    ctx->table = charsets[source_charset].tables[charset];
    ctx->charset = charset;
    ctx->charset_name = charsets[charset].name;
    ctx->length = charsets[charset].length;
    ctx->from_utf8 = charsets[source_charset].utf8;
    ctx->to_utf8 = charsets[charset].utf8;

    r->filter_need_in_memory = 1;

    if ((ctx->to_utf8 || ctx->from_utf8) && r == r->main) {
        ngx_http_clear_content_length(r);

    } else {
        r->filter_need_temporary = 1;
    }

    return ngx_http_next_header_filter(r);
}


/*
 * 字符集转换主体过滤器函数
 * 
 * 该函数负责对HTTP响应主体进行字符集转换处理。
 * 它会根据上下文中设置的转换表对输入的缓冲链进行重编码。
 *
 * @param r 指向当前HTTP请求的指针
 * @param in 输入的缓冲链
 * @return NGX_OK 处理成功
 *         NGX_ERROR 处理失败
 */
static ngx_int_t
ngx_http_charset_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t                rc;        /* 用于存储函数返回值 */
    ngx_buf_t               *b;         /* 指向当前处理的缓冲区 */
    ngx_chain_t             *cl, *out, **ll;  /* cl: 输入链表, out: 输出链表, ll: 输出链表的尾指针 */
    ngx_http_charset_ctx_t  *ctx;       /* 字符集转换上下文 */

    /* 获取字符集转换上下文 */
    ctx = ngx_http_get_module_ctx(r, ngx_http_charset_filter_module);

    /* 如果上下文不存在或转换表为空,直接传递给下一个过滤器 */
    if (ctx == NULL || ctx->table == NULL) {
        return ngx_http_next_body_filter(r, in);
    }

    /* 处理UTF-8转换或有未处理完的数据 */
    if ((ctx->to_utf8 || ctx->from_utf8) || ctx->busy) {

        out = NULL;
        ll = &out;

        /* 遍历输入缓冲链 */
        for (cl = in; cl; cl = cl->next) {
            b = cl->buf;

            /* 处理空缓冲区 */
            if (ngx_buf_size(b) == 0) {

                /* 分配新的链节点 */
                *ll = ngx_alloc_chain_link(r->pool);
                if (*ll == NULL) {
                    return NGX_ERROR;
                }

                /* 设置链节点 */
                (*ll)->buf = b;
                (*ll)->next = NULL;

                ll = &(*ll)->next;

                continue;
            }

            /* 根据转换方向调用相应的重编码函数 */
            if (ctx->to_utf8) {
                *ll = ngx_http_charset_recode_to_utf8(r->pool, b, ctx);

            } else {
                *ll = ngx_http_charset_recode_from_utf8(r->pool, b, ctx);
            }

            /* 检查重编码是否成功 */
            if (*ll == NULL) {
                return NGX_ERROR;
            }

            /* 移动到链表末尾 */
            while (*ll) {
                ll = &(*ll)->next;
            }
        }

        /* 调用下一个主体过滤器 */
        rc = ngx_http_next_body_filter(r, out);

        /* 处理输出链表 */
        if (out) {
            if (ctx->busy == NULL) {
                ctx->busy = out;

            } else {
                /* 将输出链表添加到busy链表末尾 */
                for (cl = ctx->busy; cl->next; cl = cl->next) { /* void */ }
                cl->next = out;
            }
        }

        /* 清理已处理完的缓冲区 */
        while (ctx->busy) {

            cl = ctx->busy;
            b = cl->buf;

            /* 如果缓冲区还有数据,停止清理 */
            if (ngx_buf_size(b) != 0) {
                break;
            }

            ctx->busy = cl->next;

            /* 跳过非本模块创建的缓冲区 */
            if (b->tag != (ngx_buf_tag_t) &ngx_http_charset_filter_module) {
                continue;
            }

            /* 处理影子缓冲区 */
            if (b->shadow) {
                b->shadow->pos = b->shadow->last;
            }

            /* 回收缓冲区 */
            if (b->pos) {
                cl->next = ctx->free_buffers;
                ctx->free_buffers = cl;
                continue;
            }

            cl->next = ctx->free_bufs;
            ctx->free_bufs = cl;
        }

        return rc;
    }

    /* 非UTF-8转换,直接对每个缓冲区进行重编码 */
    for (cl = in; cl; cl = cl->next) {
        (void) ngx_http_charset_recode(cl->buf, ctx->table);
    }

    /* 调用下一个主体过滤器 */
    return ngx_http_next_body_filter(r, in);
}


static ngx_uint_t
ngx_http_charset_recode(ngx_buf_t *b, u_char *table)
{
    u_char  *p, *last;

    last = b->last;

    for (p = b->pos; p < last; p++) {

        if (*p != table[*p]) {
            goto recode;
        }
    }

    return 0;

recode:

    do {
        if (*p != table[*p]) {
            *p = table[*p];
        }

        p++;

    } while (p < last);

    b->in_file = 0;

    return 1;
}


static ngx_chain_t *
ngx_http_charset_recode_from_utf8(ngx_pool_t *pool, ngx_buf_t *buf,
    ngx_http_charset_ctx_t *ctx)
{
    size_t        len, size;
    u_char        c, *p, *src, *dst, *saved, **table;
    uint32_t      n;
    ngx_buf_t    *b;
    ngx_uint_t    i;
    ngx_chain_t  *out, *cl, **ll;

    src = buf->pos;

    if (ctx->saved_len == 0) {

        for ( /* void */ ; src < buf->last; src++) {

            if (*src < 0x80) {
                continue;
            }

            len = src - buf->pos;

            if (len > 512) {
                out = ngx_http_charset_get_buf(pool, ctx);
                if (out == NULL) {
                    return NULL;
                }

                b = out->buf;

                b->temporary = buf->temporary;
                b->memory = buf->memory;
                b->mmap = buf->mmap;
                b->flush = buf->flush;

                b->pos = buf->pos;
                b->last = src;

                out->buf = b;
                out->next = NULL;

                size = buf->last - src;

                saved = src;
                n = ngx_utf8_decode(&saved, size);

                if (n == 0xfffffffe) {
                    /* incomplete UTF-8 symbol */

                    ngx_memcpy(ctx->saved, src, size);
                    ctx->saved_len = size;

                    b->shadow = buf;

                    return out;
                }

            } else {
                out = NULL;
                size = len + buf->last - src;
                src = buf->pos;
            }

            if (size < NGX_HTML_ENTITY_LEN) {
                size += NGX_HTML_ENTITY_LEN;
            }

            cl = ngx_http_charset_get_buffer(pool, ctx, size);
            if (cl == NULL) {
                return NULL;
            }

            if (out) {
                out->next = cl;

            } else {
                out = cl;
            }

            b = cl->buf;
            dst = b->pos;

            goto recode;
        }

        out = ngx_alloc_chain_link(pool);
        if (out == NULL) {
            return NULL;
        }

        out->buf = buf;
        out->next = NULL;

        return out;
    }

    /* process incomplete UTF sequence from previous buffer */

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pool->log, 0,
                   "http charset utf saved: %z", ctx->saved_len);

    p = src;

    for (i = ctx->saved_len; i < NGX_UTF_LEN; i++) {
        ctx->saved[i] = *p++;

        if (p == buf->last) {
            break;
        }
    }

    saved = ctx->saved;
    n = ngx_utf8_decode(&saved, i);

    c = '\0';

    if (n < 0x10000) {
        table = (u_char **) ctx->table;
        p = table[n >> 8];

        if (p) {
            c = p[n & 0xff];
        }

    } else if (n == 0xfffffffe) {

        /* incomplete UTF-8 symbol */

        if (i < NGX_UTF_LEN) {
            out = ngx_http_charset_get_buf(pool, ctx);
            if (out == NULL) {
                return NULL;
            }

            b = out->buf;

            b->pos = buf->pos;
            b->last = buf->last;
            b->sync = 1;
            b->shadow = buf;

            ngx_memcpy(&ctx->saved[ctx->saved_len], src, i);
            ctx->saved_len += i;

            return out;
        }
    }

    size = buf->last - buf->pos;

    if (size < NGX_HTML_ENTITY_LEN) {
        size += NGX_HTML_ENTITY_LEN;
    }

    cl = ngx_http_charset_get_buffer(pool, ctx, size);
    if (cl == NULL) {
        return NULL;
    }

    out = cl;

    b = cl->buf;
    dst = b->pos;

    if (c) {
        *dst++ = c;

    } else if (n == 0xfffffffe) {
        *dst++ = '?';

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pool->log, 0,
                       "http charset invalid utf 0");

        saved = &ctx->saved[NGX_UTF_LEN];

    } else if (n > 0x10ffff) {
        *dst++ = '?';

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pool->log, 0,
                       "http charset invalid utf 1");

    } else {
        dst = ngx_sprintf(dst, "&#%uD;", n);
    }

    src += (saved - ctx->saved) - ctx->saved_len;
    ctx->saved_len = 0;

recode:

    ll = &cl->next;

    table = (u_char **) ctx->table;

    while (src < buf->last) {

        if ((size_t) (b->end - dst) < NGX_HTML_ENTITY_LEN) {
            b->last = dst;

            size = buf->last - src + NGX_HTML_ENTITY_LEN;

            cl = ngx_http_charset_get_buffer(pool, ctx, size);
            if (cl == NULL) {
                return NULL;
            }

            *ll = cl;
            ll = &cl->next;

            b = cl->buf;
            dst = b->pos;
        }

        if (*src < 0x80) {
            *dst++ = *src++;
            continue;
        }

        len = buf->last - src;

        n = ngx_utf8_decode(&src, len);

        if (n < 0x10000) {

            p = table[n >> 8];

            if (p) {
                c = p[n & 0xff];

                if (c) {
                    *dst++ = c;
                    continue;
                }
            }

            dst = ngx_sprintf(dst, "&#%uD;", n);

            continue;
        }

        if (n == 0xfffffffe) {
            /* incomplete UTF-8 symbol */

            ngx_memcpy(ctx->saved, src, len);
            ctx->saved_len = len;

            if (b->pos == dst) {
                b->sync = 1;
                b->temporary = 0;
            }

            break;
        }

        if (n > 0x10ffff) {
            *dst++ = '?';

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pool->log, 0,
                           "http charset invalid utf 2");

            continue;
        }

        /* n > 0xffff */

        dst = ngx_sprintf(dst, "&#%uD;", n);
    }

    b->last = dst;

    b->last_buf = buf->last_buf;
    b->last_in_chain = buf->last_in_chain;
    b->flush = buf->flush;

    b->shadow = buf;

    return out;
}


static ngx_chain_t *
ngx_http_charset_recode_to_utf8(ngx_pool_t *pool, ngx_buf_t *buf,
    ngx_http_charset_ctx_t *ctx)
{
    size_t        len, size;
    u_char       *p, *src, *dst, *table;
    ngx_buf_t    *b;
    ngx_chain_t  *out, *cl, **ll;

    table = ctx->table;

    for (src = buf->pos; src < buf->last; src++) {
        if (table[*src * NGX_UTF_LEN] == '\1') {
            continue;
        }

        goto recode;
    }

    out = ngx_alloc_chain_link(pool);
    if (out == NULL) {
        return NULL;
    }

    out->buf = buf;
    out->next = NULL;

    return out;

recode:

    /*
     * we assume that there are about half of characters to be recoded,
     * so we preallocate "size / 2 + size / 2 * ctx->length"
     */

    len = src - buf->pos;

    if (len > 512) {
        out = ngx_http_charset_get_buf(pool, ctx);
        if (out == NULL) {
            return NULL;
        }

        b = out->buf;

        b->temporary = buf->temporary;
        b->memory = buf->memory;
        b->mmap = buf->mmap;
        b->flush = buf->flush;

        b->pos = buf->pos;
        b->last = src;

        out->buf = b;
        out->next = NULL;

        size = buf->last - src;
        size = size / 2 + size / 2 * ctx->length;

    } else {
        out = NULL;

        size = buf->last - src;
        size = len + size / 2 + size / 2 * ctx->length;

        src = buf->pos;
    }

    cl = ngx_http_charset_get_buffer(pool, ctx, size);
    if (cl == NULL) {
        return NULL;
    }

    if (out) {
        out->next = cl;

    } else {
        out = cl;
    }

    ll = &cl->next;

    b = cl->buf;
    dst = b->pos;

    while (src < buf->last) {

        p = &table[*src++ * NGX_UTF_LEN];
        len = *p++;

        if ((size_t) (b->end - dst) < len) {
            b->last = dst;

            size = buf->last - src;
            size = len + size / 2 + size / 2 * ctx->length;

            cl = ngx_http_charset_get_buffer(pool, ctx, size);
            if (cl == NULL) {
                return NULL;
            }

            *ll = cl;
            ll = &cl->next;

            b = cl->buf;
            dst = b->pos;
        }

        while (len) {
            *dst++ = *p++;
            len--;
        }
    }

    b->last = dst;

    b->last_buf = buf->last_buf;
    b->last_in_chain = buf->last_in_chain;
    b->flush = buf->flush;

    b->shadow = buf;

    return out;
}


static ngx_chain_t *
ngx_http_charset_get_buf(ngx_pool_t *pool, ngx_http_charset_ctx_t *ctx)
{
    ngx_chain_t  *cl;

    cl = ctx->free_bufs;

    if (cl) {
        ctx->free_bufs = cl->next;

        cl->buf->shadow = NULL;
        cl->next = NULL;

        return cl;
    }

    cl = ngx_alloc_chain_link(pool);
    if (cl == NULL) {
        return NULL;
    }

    cl->buf = ngx_calloc_buf(pool);
    if (cl->buf == NULL) {
        return NULL;
    }

    cl->next = NULL;

    cl->buf->tag = (ngx_buf_tag_t) &ngx_http_charset_filter_module;

    return cl;
}


static ngx_chain_t *
ngx_http_charset_get_buffer(ngx_pool_t *pool, ngx_http_charset_ctx_t *ctx,
    size_t size)
{
    ngx_buf_t    *b;
    ngx_chain_t  *cl, **ll;

    for (ll = &ctx->free_buffers, cl = ctx->free_buffers;
         cl;
         ll = &cl->next, cl = cl->next)
    {
        b = cl->buf;

        if ((size_t) (b->end - b->start) >= size) {
            *ll = cl->next;
            cl->next = NULL;

            b->pos = b->start;
            b->temporary = 1;
            b->shadow = NULL;

            return cl;
        }
    }

    cl = ngx_alloc_chain_link(pool);
    if (cl == NULL) {
        return NULL;
    }

    cl->buf = ngx_create_temp_buf(pool, size);
    if (cl->buf == NULL) {
        return NULL;
    }

    cl->next = NULL;

    cl->buf->temporary = 1;
    cl->buf->tag = (ngx_buf_tag_t) &ngx_http_charset_filter_module;

    return cl;
}


static char *
ngx_http_charset_map_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_charset_main_conf_t  *mcf = conf;

    char                         *rv;
    u_char                       *p, *dst2src, **pp;
    ngx_int_t                     src, dst;
    ngx_uint_t                    i, n;
    ngx_str_t                    *value;
    ngx_conf_t                    pvcf;
    ngx_http_charset_t           *charset;
    ngx_http_charset_tables_t    *table;
    ngx_http_charset_conf_ctx_t   ctx;

    value = cf->args->elts;

    src = ngx_http_add_charset(&mcf->charsets, &value[1]);
    if (src == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    dst = ngx_http_add_charset(&mcf->charsets, &value[2]);
    if (dst == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    if (src == dst) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"charset_map\" between the same charsets "
                           "\"%V\" and \"%V\"", &value[1], &value[2]);
        return NGX_CONF_ERROR;
    }

    table = mcf->tables.elts;
    for (i = 0; i < mcf->tables.nelts; i++) {
        if ((src == table->src && dst == table->dst)
             || (src == table->dst && dst == table->src))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "duplicate \"charset_map\" between "
                               "\"%V\" and \"%V\"", &value[1], &value[2]);
            return NGX_CONF_ERROR;
        }
    }

    table = ngx_array_push(&mcf->tables);
    if (table == NULL) {
        return NGX_CONF_ERROR;
    }

    table->src = src;
    table->dst = dst;

    if (ngx_strcasecmp(value[2].data, (u_char *) "utf-8") == 0) {
        table->src2dst = ngx_pcalloc(cf->pool, 256 * NGX_UTF_LEN);
        if (table->src2dst == NULL) {
            return NGX_CONF_ERROR;
        }

        table->dst2src = ngx_pcalloc(cf->pool, 256 * sizeof(void *));
        if (table->dst2src == NULL) {
            return NGX_CONF_ERROR;
        }

        dst2src = ngx_pcalloc(cf->pool, 256);
        if (dst2src == NULL) {
            return NGX_CONF_ERROR;
        }

        pp = (u_char **) &table->dst2src[0];
        pp[0] = dst2src;

        for (i = 0; i < 128; i++) {
            p = &table->src2dst[i * NGX_UTF_LEN];
            p[0] = '\1';
            p[1] = (u_char) i;
            dst2src[i] = (u_char) i;
        }

        for (/* void */; i < 256; i++) {
            p = &table->src2dst[i * NGX_UTF_LEN];
            p[0] = '\1';
            p[1] = '?';
        }

    } else {
        table->src2dst = ngx_palloc(cf->pool, 256);
        if (table->src2dst == NULL) {
            return NGX_CONF_ERROR;
        }

        table->dst2src = ngx_palloc(cf->pool, 256);
        if (table->dst2src == NULL) {
            return NGX_CONF_ERROR;
        }

        for (i = 0; i < 128; i++) {
            table->src2dst[i] = (u_char) i;
            table->dst2src[i] = (u_char) i;
        }

        for (/* void */; i < 256; i++) {
            table->src2dst[i] = '?';
            table->dst2src[i] = '?';
        }
    }

    charset = mcf->charsets.elts;

    ctx.table = table;
    ctx.charset = &charset[dst];
    ctx.characters = 0;

    pvcf = *cf;
    cf->ctx = &ctx;
    cf->handler = ngx_http_charset_map;
    cf->handler_conf = conf;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pvcf;

    if (ctx.characters) {
        n = ctx.charset->length;
        ctx.charset->length /= ctx.characters;

        if (((n * 10) / ctx.characters) % 10 > 4) {
            ctx.charset->length++;
        }
    }

    return rv;
}


static char *
ngx_http_charset_map(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    u_char                       *p, *dst2src, **pp;
    uint32_t                      n;
    ngx_int_t                     src, dst;
    ngx_str_t                    *value;
    ngx_uint_t                    i;
    ngx_http_charset_tables_t    *table;
    ngx_http_charset_conf_ctx_t  *ctx;

    if (cf->args->nelts != 2) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameters number");
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    src = ngx_hextoi(value[0].data, value[0].len);
    if (src == NGX_ERROR || src > 255) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid value \"%V\"", &value[0]);
        return NGX_CONF_ERROR;
    }

    ctx = cf->ctx;
    table = ctx->table;

    if (ctx->charset->utf8) {
        p = &table->src2dst[src * NGX_UTF_LEN];

        *p++ = (u_char) (value[1].len / 2);

        for (i = 0; i < value[1].len; i += 2) {
            dst = ngx_hextoi(&value[1].data[i], 2);
            if (dst == NGX_ERROR || dst > 255) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid value \"%V\"", &value[1]);
                return NGX_CONF_ERROR;
            }

            *p++ = (u_char) dst;
        }

        i /= 2;

        ctx->charset->length += i;
        ctx->characters++;

        p = &table->src2dst[src * NGX_UTF_LEN] + 1;

        n = ngx_utf8_decode(&p, i);

        if (n > 0xffff) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid value \"%V\"", &value[1]);
            return NGX_CONF_ERROR;
        }

        pp = (u_char **) &table->dst2src[0];

        dst2src = pp[n >> 8];

        if (dst2src == NULL) {
            dst2src = ngx_pcalloc(cf->pool, 256);
            if (dst2src == NULL) {
                return NGX_CONF_ERROR;
            }

            pp[n >> 8] = dst2src;
        }

        dst2src[n & 0xff] = (u_char) src;

    } else {
        dst = ngx_hextoi(value[1].data, value[1].len);
        if (dst == NGX_ERROR || dst > 255) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid value \"%V\"", &value[1]);
            return NGX_CONF_ERROR;
        }

        table->src2dst[src] = (u_char) dst;
        table->dst2src[dst] = (u_char) src;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_set_charset_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_int_t                     *cp;
    ngx_str_t                     *value, var;
    ngx_http_charset_main_conf_t  *mcf;

    cp = (ngx_int_t *) (p + cmd->offset);

    if (*cp != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (cmd->offset == offsetof(ngx_http_charset_loc_conf_t, charset)
        && ngx_strcmp(value[1].data, "off") == 0)
    {
        *cp = NGX_HTTP_CHARSET_OFF;
        return NGX_CONF_OK;
    }


    if (value[1].data[0] == '$') {
        var.len = value[1].len - 1;
        var.data = value[1].data + 1;

        *cp = ngx_http_get_variable_index(cf, &var);

        if (*cp == NGX_ERROR) {
            return NGX_CONF_ERROR;
        }

        *cp += NGX_HTTP_CHARSET_VAR;

        return NGX_CONF_OK;
    }

    mcf = ngx_http_conf_get_module_main_conf(cf,
                                             ngx_http_charset_filter_module);

    *cp = ngx_http_add_charset(&mcf->charsets, &value[1]);
    if (*cp == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_add_charset(ngx_array_t *charsets, ngx_str_t *name)
{
    ngx_uint_t           i;
    ngx_http_charset_t  *c;

    c = charsets->elts;
    for (i = 0; i < charsets->nelts; i++) {
        if (name->len != c[i].name.len) {
            continue;
        }

        if (ngx_strcasecmp(name->data, c[i].name.data) == 0) {
            break;
        }
    }

    if (i < charsets->nelts) {
        return i;
    }

    c = ngx_array_push(charsets);
    if (c == NULL) {
        return NGX_ERROR;
    }

    c->tables = NULL;
    c->name = *name;
    c->length = 0;

    if (ngx_strcasecmp(name->data, (u_char *) "utf-8") == 0) {
        c->utf8 = 1;

    } else {
        c->utf8 = 0;
    }

    return i;
}


static void *
ngx_http_charset_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_charset_main_conf_t  *mcf;

    mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_charset_main_conf_t));
    if (mcf == NULL) {
        return NULL;
    }

    if (ngx_array_init(&mcf->charsets, cf->pool, 2, sizeof(ngx_http_charset_t))
        != NGX_OK)
    {
        return NULL;
    }

    if (ngx_array_init(&mcf->tables, cf->pool, 1,
                       sizeof(ngx_http_charset_tables_t))
        != NGX_OK)
    {
        return NULL;
    }

    if (ngx_array_init(&mcf->recodes, cf->pool, 2,
                       sizeof(ngx_http_charset_recode_t))
        != NGX_OK)
    {
        return NULL;
    }

    return mcf;
}


static void *
ngx_http_charset_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_charset_loc_conf_t  *lcf;

    lcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_charset_loc_conf_t));
    if (lcf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     lcf->types = { NULL };
     *     lcf->types_keys = NULL;
     */

    lcf->charset = NGX_CONF_UNSET;
    lcf->source_charset = NGX_CONF_UNSET;
    lcf->override_charset = NGX_CONF_UNSET;

    return lcf;
}


static char *
ngx_http_charset_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_charset_loc_conf_t *prev = parent;
    ngx_http_charset_loc_conf_t *conf = child;

    ngx_uint_t                     i;
    ngx_http_charset_recode_t     *recode;
    ngx_http_charset_main_conf_t  *mcf;

    if (ngx_http_merge_types(cf, &conf->types_keys, &conf->types,
                             &prev->types_keys, &prev->types,
                             ngx_http_charset_default_types)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->override_charset, prev->override_charset, 0);
    ngx_conf_merge_value(conf->charset, prev->charset, NGX_HTTP_CHARSET_OFF);
    ngx_conf_merge_value(conf->source_charset, prev->source_charset,
                         NGX_HTTP_CHARSET_OFF);

    if (conf->charset == NGX_HTTP_CHARSET_OFF
        || conf->source_charset == NGX_HTTP_CHARSET_OFF
        || conf->charset == conf->source_charset)
    {
        return NGX_CONF_OK;
    }

    if (conf->source_charset >= NGX_HTTP_CHARSET_VAR
        || conf->charset >= NGX_HTTP_CHARSET_VAR)
    {
        return NGX_CONF_OK;
    }

    mcf = ngx_http_conf_get_module_main_conf(cf,
                                             ngx_http_charset_filter_module);
    recode = mcf->recodes.elts;
    for (i = 0; i < mcf->recodes.nelts; i++) {
        if (conf->source_charset == recode[i].src
            && conf->charset == recode[i].dst)
        {
            return NGX_CONF_OK;
        }
    }

    recode = ngx_array_push(&mcf->recodes);
    if (recode == NULL) {
        return NGX_CONF_ERROR;
    }

    recode->src = conf->source_charset;
    recode->dst = conf->charset;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_charset_postconfiguration(ngx_conf_t *cf)
{
    u_char                       **src, **dst;
    ngx_int_t                      c;
    ngx_uint_t                     i, t;
    ngx_http_charset_t            *charset;
    ngx_http_charset_recode_t     *recode;
    ngx_http_charset_tables_t     *tables;
    ngx_http_charset_main_conf_t  *mcf;

    mcf = ngx_http_conf_get_module_main_conf(cf,
                                             ngx_http_charset_filter_module);

    recode = mcf->recodes.elts;
    tables = mcf->tables.elts;
    charset = mcf->charsets.elts;

    for (i = 0; i < mcf->recodes.nelts; i++) {

        c = recode[i].src;

        for (t = 0; t < mcf->tables.nelts; t++) {

            if (c == tables[t].src && recode[i].dst == tables[t].dst) {
                goto next;
            }

            if (c == tables[t].dst && recode[i].dst == tables[t].src) {
                goto next;
            }
        }

        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                   "no \"charset_map\" between the charsets \"%V\" and \"%V\"",
                   &charset[c].name, &charset[recode[i].dst].name);
        return NGX_ERROR;

    next:
        continue;
    }


    for (t = 0; t < mcf->tables.nelts; t++) {

        src = charset[tables[t].src].tables;

        if (src == NULL) {
            src = ngx_pcalloc(cf->pool, sizeof(u_char *) * mcf->charsets.nelts);
            if (src == NULL) {
                return NGX_ERROR;
            }

            charset[tables[t].src].tables = src;
        }

        dst = charset[tables[t].dst].tables;

        if (dst == NULL) {
            dst = ngx_pcalloc(cf->pool, sizeof(u_char *) * mcf->charsets.nelts);
            if (dst == NULL) {
                return NGX_ERROR;
            }

            charset[tables[t].dst].tables = dst;
        }

        src[tables[t].dst] = tables[t].src2dst;
        dst[tables[t].src] = tables[t].dst2src;
    }

    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_charset_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_charset_body_filter;

    return NGX_OK;
}
