
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


/*
 * ngx_http_autoindex_module.c
 *
 * 该模块实现了Nginx的自动索引功能，用于生成目录列表。
 *
 * 支持的功能：
 * 1. 生成指定目录的文件和子目录列表
 * 2. 支持多种输出格式（HTML、JSON、XML等）
 * 3. 显示文件/目录名、大小、最后修改时间等信息
 * 4. 可配置是否使用本地时间
 * 5. 可配置是否显示精确的文件大小
 *
 * 支持的指令：
 * - autoindex on|off: 启用或禁用自动索引功能
 * - autoindex_format html|xml|json|jsonp: 设置输出格式
 * - autoindex_localtime on|off: 是否使用本地时间显示
 * - autoindex_exact_size on|off: 是否显示精确的文件大小
 *
 * 支持的变量：
 * - $autoindex: 当前是否启用了自动索引
 *
 * 使用注意点：
 * 1. 启用自动索引可能会暴露敏感文件信息，请谨慎使用
 * 2. 对于大型目录，生成索引可能会消耗较多资源，建议适当限制
 * 3. 确保Nginx进程对目标目录有足够的读取权限
 * 4. 考虑使用autoindex_format指令来选择最适合的输出格式
 * 5. 在生产环境中，建议仅在必要的位置启用自动索引功能
 */




#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#if 0

typedef struct {
    ngx_buf_t     *buf;
    size_t         size;
    ngx_pool_t    *pool;
    size_t         alloc_size;
    ngx_chain_t  **last_out;
} ngx_http_autoindex_ctx_t;

#endif


/**
 * @brief 自动索引条目结构体
 *
 * 用于存储目录中每个文件或子目录的信息
 */
typedef struct {
    ngx_str_t      name;         /**< 文件或目录名 */
    size_t         utf_len;      /**< UTF-8编码的名称长度 */
    size_t         escape;       /**< 需要转义的字符数 */
    size_t         escape_html;  /**< 需要HTML转义的字符数 */

    unsigned       dir:1;        /**< 是否为目录 */
    unsigned       file:1;       /**< 是否为文件 */

    time_t         mtime;        /**< 最后修改时间 */
    off_t          size;         /**< 文件大小 */
} ngx_http_autoindex_entry_t;

/**
 * @brief 自动索引模块的位置配置结构体
 *
 * 存储自动索引模块在特定位置的配置信息
 */
typedef struct {
    ngx_flag_t     enable;       /**< 是否启用自动索引 */
    ngx_uint_t     format;       /**< 输出格式 */
    ngx_flag_t     localtime;    /**< 是否使用本地时间 */
    ngx_flag_t     exact_size;   /**< 是否显示精确的文件大小 */
} ngx_http_autoindex_loc_conf_t;

/**
 * @brief 自动索引输出格式枚举
 */
#define NGX_HTTP_AUTOINDEX_HTML         0  /**< HTML格式 */
#define NGX_HTTP_AUTOINDEX_JSON         1  /**< JSON格式 */
#define NGX_HTTP_AUTOINDEX_JSONP        2  /**< JSONP格式 */
#define NGX_HTTP_AUTOINDEX_XML          3  /**< XML格式 */

/**
 * @brief 预分配的条目数量
 */
#define NGX_HTTP_AUTOINDEX_PREALLOCATE  50

/**
 * @brief 文件名显示的最大长度
 */
#define NGX_HTTP_AUTOINDEX_NAME_LEN     50


/**
 * @brief 生成HTML格式的自动索引页面
 * @param r HTTP请求
 * @param entries 目录条目数组
 * @return 包含HTML内容的ngx_buf_t结构体指针
 */
static ngx_buf_t *ngx_http_autoindex_html(ngx_http_request_t *r,
    ngx_array_t *entries);

/**
 * @brief 生成JSON格式的自动索引数据
 * @param r HTTP请求
 * @param entries 目录条目数组
 * @param callback JSONP回调函数名（可选）
 * @return 包含JSON数据的ngx_buf_t结构体指针
 */
static ngx_buf_t *ngx_http_autoindex_json(ngx_http_request_t *r,
    ngx_array_t *entries, ngx_str_t *callback);

/**
 * @brief 验证JSONP回调函数名
 * @param r HTTP请求
 * @param callback 回调函数名
 * @return NGX_OK表示验证通过，其他值表示失败
 */
static ngx_int_t ngx_http_autoindex_jsonp_callback(ngx_http_request_t *r,
    ngx_str_t *callback);

/**
 * @brief 生成XML格式的自动索引数据
 * @param r HTTP请求
 * @param entries 目录条目数组
 * @return 包含XML数据的ngx_buf_t结构体指针
 */
static ngx_buf_t *ngx_http_autoindex_xml(ngx_http_request_t *r,
    ngx_array_t *entries);

/**
 * @brief 比较两个目录条目的排序函数
 * @param one 第一个条目
 * @param two 第二个条目
 * @return 比较结果，用于qsort排序
 */
static int ngx_libc_cdecl ngx_http_autoindex_cmp_entries(const void *one,
    const void *two);

/**
 * @brief 处理自动索引过程中的错误
 * @param r HTTP请求
 * @param dir 目录句柄
 * @param name 出错的文件或目录名
 * @return NGX_OK或错误代码
 */
static ngx_int_t ngx_http_autoindex_error(ngx_http_request_t *r,
    ngx_dir_t *dir, ngx_str_t *name);

/**
 * @brief 自动索引模块初始化函数
 * @param cf 配置上下文
 * @return NGX_OK或错误代码
 */
static ngx_int_t ngx_http_autoindex_init(ngx_conf_t *cf);

/**
 * @brief 创建自动索引模块的location配置
 * @param cf 配置上下文
 * @return 新创建的配置结构体指针
 */
static void *ngx_http_autoindex_create_loc_conf(ngx_conf_t *cf);

/**
 * @brief 合并自动索引模块的location配置
 * @param cf 配置上下文
 * @param parent 父配置
 * @param child 子配置
 * @return NGX_CONF_OK或错误字符串
 */
static char *ngx_http_autoindex_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);


/**
 * @brief 定义自动索引格式的枚举数组
 *
 * 该数组定义了自动索引支持的不同输出格式。
 * 每个元素包含一个字符串表示的格式名称和对应的枚举值。
 */
static ngx_conf_enum_t  ngx_http_autoindex_format[] = {
    { ngx_string("html"), NGX_HTTP_AUTOINDEX_HTML },
    { ngx_string("json"), NGX_HTTP_AUTOINDEX_JSON },
    { ngx_string("jsonp"), NGX_HTTP_AUTOINDEX_JSONP },
    { ngx_string("xml"), NGX_HTTP_AUTOINDEX_XML },
    { ngx_null_string, 0 }
};


static ngx_command_t  ngx_http_autoindex_commands[] = {

    { ngx_string("autoindex"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_autoindex_loc_conf_t, enable),
      NULL },

    { ngx_string("autoindex_format"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_autoindex_loc_conf_t, format),
      &ngx_http_autoindex_format },

    { ngx_string("autoindex_localtime"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_autoindex_loc_conf_t, localtime),
      NULL },

    { ngx_string("autoindex_exact_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_autoindex_loc_conf_t, exact_size),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_autoindex_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_autoindex_init,               /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_autoindex_create_loc_conf,    /* create location configuration */
    ngx_http_autoindex_merge_loc_conf      /* merge location configuration */
};


ngx_module_t  ngx_http_autoindex_module = {
    NGX_MODULE_V1,
    &ngx_http_autoindex_module_ctx,        /* module context */
    ngx_http_autoindex_commands,           /* module directives */
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
 * ngx_http_autoindex_handler 函数的声明
 * 这个函数是自动索引模块的主要处理函数
 * 返回类型为 ngx_int_t，表示处理结果的状态码
 */
static ngx_int_t
ngx_http_autoindex_handler(ngx_http_request_t *r)
{
    u_char                         *last, *filename;
    size_t                          len, allocated, root;
    ngx_err_t                       err;
    ngx_buf_t                      *b;
    ngx_int_t                       rc;
    ngx_str_t                       path, callback;
    ngx_dir_t                       dir;
    ngx_uint_t                      level, format;
    ngx_pool_t                     *pool;
    ngx_chain_t                     out;
    ngx_array_t                     entries;
    ngx_http_autoindex_entry_t     *entry;
    ngx_http_autoindex_loc_conf_t  *alcf;

    if (r->uri.data[r->uri.len - 1] != '/') {
        return NGX_DECLINED;
    }

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_DECLINED;
    }

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_autoindex_module);

    if (!alcf->enable) {
        return NGX_DECLINED;
    }

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    last = ngx_http_map_uri_to_path(r, &path, &root,
                                    NGX_HTTP_AUTOINDEX_PREALLOCATE);
    if (last == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    allocated = path.len;
    path.len = last - path.data;
    if (path.len > 1) {
        path.len--;
    }
    path.data[path.len] = '\0';

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http autoindex: \"%s\"", path.data);

    format = alcf->format;

    if (format == NGX_HTTP_AUTOINDEX_JSONP) {
        if (ngx_http_autoindex_jsonp_callback(r, &callback) != NGX_OK) {
            return NGX_HTTP_BAD_REQUEST;
        }

        if (callback.len == 0) {
            format = NGX_HTTP_AUTOINDEX_JSON;
        }
    }

    if (ngx_open_dir(&path, &dir) == NGX_ERROR) {
        err = ngx_errno;

        if (err == NGX_ENOENT
            || err == NGX_ENOTDIR
            || err == NGX_ENAMETOOLONG)
        {
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_NOT_FOUND;

        } else if (err == NGX_EACCES) {
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_FORBIDDEN;

        } else {
            level = NGX_LOG_CRIT;
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_log_error(level, r->connection->log, err,
                      ngx_open_dir_n " \"%s\" failed", path.data);

        return rc;
    }

#if (NGX_SUPPRESS_WARN)

    /* MSVC thinks 'entries' may be used without having been initialized */
    ngx_memzero(&entries, sizeof(ngx_array_t));

#endif

    /* TODO: pool should be temporary pool */
    pool = r->pool;

    if (ngx_array_init(&entries, pool, 40, sizeof(ngx_http_autoindex_entry_t))
        != NGX_OK)
    {
        return ngx_http_autoindex_error(r, &dir, &path);
    }

    r->headers_out.status = NGX_HTTP_OK;

    switch (format) {

    case NGX_HTTP_AUTOINDEX_JSON:
        ngx_str_set(&r->headers_out.content_type, "application/json");
        break;

    case NGX_HTTP_AUTOINDEX_JSONP:
        ngx_str_set(&r->headers_out.content_type, "application/javascript");
        break;

    case NGX_HTTP_AUTOINDEX_XML:
        ngx_str_set(&r->headers_out.content_type, "text/xml");
        ngx_str_set(&r->headers_out.charset, "utf-8");
        break;

    default: /* NGX_HTTP_AUTOINDEX_HTML */
        ngx_str_set(&r->headers_out.content_type, "text/html");
        break;
    }

    r->headers_out.content_type_len = r->headers_out.content_type.len;
    r->headers_out.content_type_lowcase = NULL;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        if (ngx_close_dir(&dir) == NGX_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                          ngx_close_dir_n " \"%V\" failed", &path);
        }

        return rc;
    }

    filename = path.data;
    filename[path.len] = '/';

    for ( ;; ) {
        ngx_set_errno(0);

        if (ngx_read_dir(&dir) == NGX_ERROR) {
            err = ngx_errno;

            if (err != NGX_ENOMOREFILES) {
                ngx_log_error(NGX_LOG_CRIT, r->connection->log, err,
                              ngx_read_dir_n " \"%V\" failed", &path);
                return ngx_http_autoindex_error(r, &dir, &path);
            }

            break;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http autoindex file: \"%s\"", ngx_de_name(&dir));

        len = ngx_de_namelen(&dir);

        if (ngx_de_name(&dir)[0] == '.') {
            continue;
        }

        if (!dir.valid_info) {

            /* 1 byte for '/' and 1 byte for terminating '\0' */

            if (path.len + 1 + len + 1 > allocated) {
                allocated = path.len + 1 + len + 1
                                     + NGX_HTTP_AUTOINDEX_PREALLOCATE;

                filename = ngx_pnalloc(pool, allocated);
                if (filename == NULL) {
                    return ngx_http_autoindex_error(r, &dir, &path);
                }

                last = ngx_cpystrn(filename, path.data, path.len + 1);
                *last++ = '/';
            }

            ngx_cpystrn(last, ngx_de_name(&dir), len + 1);

            if (ngx_de_info(filename, &dir) == NGX_FILE_ERROR) {
                err = ngx_errno;

                if (err != NGX_ENOENT && err != NGX_ELOOP) {
                    ngx_log_error(NGX_LOG_CRIT, r->connection->log, err,
                                  ngx_de_info_n " \"%s\" failed", filename);

                    if (err == NGX_EACCES) {
                        continue;
                    }

                    return ngx_http_autoindex_error(r, &dir, &path);
                }

                if (ngx_de_link_info(filename, &dir) == NGX_FILE_ERROR) {
                    ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                                  ngx_de_link_info_n " \"%s\" failed",
                                  filename);
                    return ngx_http_autoindex_error(r, &dir, &path);
                }
            }
        }

        entry = ngx_array_push(&entries);
        if (entry == NULL) {
            return ngx_http_autoindex_error(r, &dir, &path);
        }

        entry->name.len = len;

        entry->name.data = ngx_pnalloc(pool, len + 1);
        if (entry->name.data == NULL) {
            return ngx_http_autoindex_error(r, &dir, &path);
        }

        ngx_cpystrn(entry->name.data, ngx_de_name(&dir), len + 1);

        entry->dir = ngx_de_is_dir(&dir);
        entry->file = ngx_de_is_file(&dir);
        entry->mtime = ngx_de_mtime(&dir);
        entry->size = ngx_de_size(&dir);
    }

    if (ngx_close_dir(&dir) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                      ngx_close_dir_n " \"%V\" failed", &path);
    }

    if (entries.nelts > 1) {
        ngx_qsort(entries.elts, (size_t) entries.nelts,
                  sizeof(ngx_http_autoindex_entry_t),
                  ngx_http_autoindex_cmp_entries);
    }

    switch (format) {

    case NGX_HTTP_AUTOINDEX_JSON:
        b = ngx_http_autoindex_json(r, &entries, NULL);
        break;

    case NGX_HTTP_AUTOINDEX_JSONP:
        b = ngx_http_autoindex_json(r, &entries, &callback);
        break;

    case NGX_HTTP_AUTOINDEX_XML:
        b = ngx_http_autoindex_xml(r, &entries);
        break;

    default: /* NGX_HTTP_AUTOINDEX_HTML */
        b = ngx_http_autoindex_html(r, &entries);
        break;
    }

    if (b == NULL) {
        return NGX_ERROR;
    }

    /* TODO: free temporary pool */

    if (r == r->main) {
        b->last_buf = 1;
    }

    b->last_in_chain = 1;

    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}


static ngx_buf_t *
ngx_http_autoindex_html(ngx_http_request_t *r, ngx_array_t *entries)
{
    u_char                         *last, scale;
    off_t                           length;
    size_t                          len, entry_len, char_len, escape_html;
    ngx_tm_t                        tm;
    ngx_buf_t                      *b;
    ngx_int_t                       size;
    ngx_uint_t                      i, utf8;
    ngx_time_t                     *tp;
    ngx_http_autoindex_entry_t     *entry;
    ngx_http_autoindex_loc_conf_t  *alcf;

    static u_char  title[] =
        "<html>" CRLF
        "<head><title>Index of "
    ;

    static u_char  header[] =
        "</title></head>" CRLF
        "<body>" CRLF
        "<h1>Index of "
    ;

    static u_char  tail[] =
        "</body>" CRLF
        "</html>" CRLF
    ;

    static char  *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    if (r->headers_out.charset.len == 5
        && ngx_strncasecmp(r->headers_out.charset.data, (u_char *) "utf-8", 5)
           == 0)
    {
        utf8 = 1;

    } else {
        utf8 = 0;
    }

    escape_html = ngx_escape_html(NULL, r->uri.data, r->uri.len);

    len = sizeof(title) - 1
          + r->uri.len + escape_html
          + sizeof(header) - 1
          + r->uri.len + escape_html
          + sizeof("</h1>") - 1
          + sizeof("<hr><pre><a href=\"../\">../</a>" CRLF) - 1
          + sizeof("</pre><hr>") - 1
          + sizeof(tail) - 1;

    entry = entries->elts;
    for (i = 0; i < entries->nelts; i++) {
        entry[i].escape = 2 * ngx_escape_uri(NULL, entry[i].name.data,
                                             entry[i].name.len,
                                             NGX_ESCAPE_URI_COMPONENT);

        entry[i].escape_html = ngx_escape_html(NULL, entry[i].name.data,
                                               entry[i].name.len);

        if (utf8) {
            entry[i].utf_len = ngx_utf8_length(entry[i].name.data,
                                               entry[i].name.len);
        } else {
            entry[i].utf_len = entry[i].name.len;
        }

        entry_len = sizeof("<a href=\"") - 1
                  + entry[i].name.len + entry[i].escape
                  + 1                                    /* 1 is for "/" */
                  + sizeof("\">") - 1
                  + entry[i].name.len - entry[i].utf_len
                  + entry[i].escape_html
                  + NGX_HTTP_AUTOINDEX_NAME_LEN + sizeof("&gt;") - 2
                  + sizeof("</a>") - 1
                  + sizeof(" 28-Sep-1970 12:00 ") - 1
                  + 20                                   /* the file size */
                  + 2;

        if (len > NGX_MAX_SIZE_T_VALUE - entry_len) {
            return NULL;
        }

        len += entry_len;
    }

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NULL;
    }

    b->last = ngx_cpymem(b->last, title, sizeof(title) - 1);

    if (escape_html) {
        b->last = (u_char *) ngx_escape_html(b->last, r->uri.data, r->uri.len);
        b->last = ngx_cpymem(b->last, header, sizeof(header) - 1);
        b->last = (u_char *) ngx_escape_html(b->last, r->uri.data, r->uri.len);

    } else {
        b->last = ngx_cpymem(b->last, r->uri.data, r->uri.len);
        b->last = ngx_cpymem(b->last, header, sizeof(header) - 1);
        b->last = ngx_cpymem(b->last, r->uri.data, r->uri.len);
    }

    b->last = ngx_cpymem(b->last, "</h1>", sizeof("</h1>") - 1);

    b->last = ngx_cpymem(b->last, "<hr><pre><a href=\"../\">../</a>" CRLF,
                         sizeof("<hr><pre><a href=\"../\">../</a>" CRLF) - 1);

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_autoindex_module);
    tp = ngx_timeofday();

    for (i = 0; i < entries->nelts; i++) {
        b->last = ngx_cpymem(b->last, "<a href=\"", sizeof("<a href=\"") - 1);

        if (entry[i].escape) {
            ngx_escape_uri(b->last, entry[i].name.data, entry[i].name.len,
                           NGX_ESCAPE_URI_COMPONENT);

            b->last += entry[i].name.len + entry[i].escape;

        } else {
            b->last = ngx_cpymem(b->last, entry[i].name.data,
                                 entry[i].name.len);
        }

        if (entry[i].dir) {
            *b->last++ = '/';
        }

        *b->last++ = '"';
        *b->last++ = '>';

        len = entry[i].utf_len;

        if (entry[i].name.len != len) {
            if (len > NGX_HTTP_AUTOINDEX_NAME_LEN) {
                char_len = NGX_HTTP_AUTOINDEX_NAME_LEN - 3 + 1;

            } else {
                char_len = NGX_HTTP_AUTOINDEX_NAME_LEN + 1;
            }

            last = b->last;
            b->last = ngx_utf8_cpystrn(b->last, entry[i].name.data,
                                       char_len, entry[i].name.len + 1);

            if (entry[i].escape_html) {
                b->last = (u_char *) ngx_escape_html(last, entry[i].name.data,
                                                     b->last - last);
            }

            last = b->last;

        } else {
            if (entry[i].escape_html) {
                if (len > NGX_HTTP_AUTOINDEX_NAME_LEN) {
                    char_len = NGX_HTTP_AUTOINDEX_NAME_LEN - 3;

                } else {
                    char_len = len;
                }

                b->last = (u_char *) ngx_escape_html(b->last,
                                                  entry[i].name.data, char_len);
                last = b->last;

            } else {
                b->last = ngx_cpystrn(b->last, entry[i].name.data,
                                      NGX_HTTP_AUTOINDEX_NAME_LEN + 1);
                last = b->last - 3;
            }
        }

        if (len > NGX_HTTP_AUTOINDEX_NAME_LEN) {
            b->last = ngx_cpymem(last, "..&gt;</a>", sizeof("..&gt;</a>") - 1);

        } else {
            if (entry[i].dir && NGX_HTTP_AUTOINDEX_NAME_LEN - len > 0) {
                *b->last++ = '/';
                len++;
            }

            b->last = ngx_cpymem(b->last, "</a>", sizeof("</a>") - 1);

            if (NGX_HTTP_AUTOINDEX_NAME_LEN - len > 0) {
                ngx_memset(b->last, ' ', NGX_HTTP_AUTOINDEX_NAME_LEN - len);
                b->last += NGX_HTTP_AUTOINDEX_NAME_LEN - len;
            }
        }

        *b->last++ = ' ';

        ngx_gmtime(entry[i].mtime + tp->gmtoff * 60 * alcf->localtime, &tm);

        b->last = ngx_sprintf(b->last, "%02d-%s-%d %02d:%02d ",
                              tm.ngx_tm_mday,
                              months[tm.ngx_tm_mon - 1],
                              tm.ngx_tm_year,
                              tm.ngx_tm_hour,
                              tm.ngx_tm_min);

        if (alcf->exact_size) {
            if (entry[i].dir) {
                b->last = ngx_cpymem(b->last,  "                  -",
                                     sizeof("                  -") - 1);
            } else {
                b->last = ngx_sprintf(b->last, "%19O", entry[i].size);
            }

        } else {
            if (entry[i].dir) {
                b->last = ngx_cpymem(b->last,  "      -",
                                     sizeof("      -") - 1);

            } else {
                length = entry[i].size;

                if (length > 1024 * 1024 * 1024 - 1) {
                    size = (ngx_int_t) (length / (1024 * 1024 * 1024));
                    if ((length % (1024 * 1024 * 1024))
                                                > (1024 * 1024 * 1024 / 2 - 1))
                    {
                        size++;
                    }
                    scale = 'G';

                } else if (length > 1024 * 1024 - 1) {
                    size = (ngx_int_t) (length / (1024 * 1024));
                    if ((length % (1024 * 1024)) > (1024 * 1024 / 2 - 1)) {
                        size++;
                    }
                    scale = 'M';

                } else if (length > 9999) {
                    size = (ngx_int_t) (length / 1024);
                    if (length % 1024 > 511) {
                        size++;
                    }
                    scale = 'K';

                } else {
                    size = (ngx_int_t) length;
                    scale = '\0';
                }

                if (scale) {
                    b->last = ngx_sprintf(b->last, "%6i%c", size, scale);

                } else {
                    b->last = ngx_sprintf(b->last, " %6i", size);
                }
            }
        }

        *b->last++ = CR;
        *b->last++ = LF;
    }

    b->last = ngx_cpymem(b->last, "</pre><hr>", sizeof("</pre><hr>") - 1);

    b->last = ngx_cpymem(b->last, tail, sizeof(tail) - 1);

    return b;
}


static ngx_buf_t *
ngx_http_autoindex_json(ngx_http_request_t *r, ngx_array_t *entries,
    ngx_str_t *callback)
{
    size_t                       len, entry_len;
    ngx_buf_t                   *b;
    ngx_uint_t                   i;
    ngx_http_autoindex_entry_t  *entry;

    len = sizeof("[" CRLF CRLF "]") - 1;

    if (callback) {
        len += sizeof("/* callback */" CRLF "();") - 1 + callback->len;
    }

    entry = entries->elts;

    for (i = 0; i < entries->nelts; i++) {
        entry[i].escape = ngx_escape_json(NULL, entry[i].name.data,
                                          entry[i].name.len);

        entry_len = sizeof("{  }," CRLF) - 1
                  + sizeof("\"name\":\"\"") - 1
                  + entry[i].name.len + entry[i].escape
                  + sizeof(", \"type\":\"directory\"") - 1
                  + sizeof(", \"mtime\":\"Wed, 31 Dec 1986 10:00:00 GMT\"") - 1;

        if (entry[i].file) {
            entry_len += sizeof(", \"size\":") - 1 + NGX_OFF_T_LEN;
        }

        if (len > NGX_MAX_SIZE_T_VALUE - entry_len) {
            return NULL;
        }

        len += entry_len;
    }

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NULL;
    }

    if (callback) {
        b->last = ngx_cpymem(b->last, "/* callback */" CRLF,
                             sizeof("/* callback */" CRLF) - 1);

        b->last = ngx_cpymem(b->last, callback->data, callback->len);

        *b->last++ = '(';
    }

    *b->last++ = '[';

    for (i = 0; i < entries->nelts; i++) {
        b->last = ngx_cpymem(b->last, CRLF "{ \"name\":\"",
                             sizeof(CRLF "{ \"name\":\"") - 1);

        if (entry[i].escape) {
            b->last = (u_char *) ngx_escape_json(b->last, entry[i].name.data,
                                                 entry[i].name.len);
        } else {
            b->last = ngx_cpymem(b->last, entry[i].name.data,
                                 entry[i].name.len);
        }

        b->last = ngx_cpymem(b->last, "\", \"type\":\"",
                             sizeof("\", \"type\":\"") - 1);

        if (entry[i].dir) {
            b->last = ngx_cpymem(b->last, "directory", sizeof("directory") - 1);

        } else if (entry[i].file) {
            b->last = ngx_cpymem(b->last, "file", sizeof("file") - 1);

        } else {
            b->last = ngx_cpymem(b->last, "other", sizeof("other") - 1);
        }

        b->last = ngx_cpymem(b->last, "\", \"mtime\":\"",
                             sizeof("\", \"mtime\":\"") - 1);

        b->last = ngx_http_time(b->last, entry[i].mtime);

        if (entry[i].file) {
            b->last = ngx_cpymem(b->last, "\", \"size\":",
                                 sizeof("\", \"size\":") - 1);
            b->last = ngx_sprintf(b->last, "%O", entry[i].size);

        } else {
            *b->last++ = '"';
        }

        b->last = ngx_cpymem(b->last, " },", sizeof(" },") - 1);
    }

    if (i > 0) {
        b->last--;  /* strip last comma */
    }

    b->last = ngx_cpymem(b->last, CRLF "]", sizeof(CRLF "]") - 1);

    if (callback) {
        *b->last++ = ')'; *b->last++ = ';';
    }

    return b;
}


static ngx_int_t
ngx_http_autoindex_jsonp_callback(ngx_http_request_t *r, ngx_str_t *callback)
{
    u_char      *p, c, ch;
    ngx_uint_t   i;

    if (ngx_http_arg(r, (u_char *) "callback", 8, callback) != NGX_OK) {
        callback->len = 0;
        return NGX_OK;
    }

    if (callback->len > 128) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "client sent too long callback name: \"%V\"", callback);
        return NGX_DECLINED;
    }

    p = callback->data;

    for (i = 0; i < callback->len; i++) {
        ch = p[i];

        c = (u_char) (ch | 0x20);
        if (c >= 'a' && c <= 'z') {
            continue;
        }

        if ((ch >= '0' && ch <= '9') || ch == '_' || ch == '.') {
            continue;
        }

        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "client sent invalid callback name: \"%V\"", callback);

        return NGX_DECLINED;
    }

    return NGX_OK;
}


static ngx_buf_t *
ngx_http_autoindex_xml(ngx_http_request_t *r, ngx_array_t *entries)
{
    size_t                          len, entry_len;
    ngx_tm_t                        tm;
    ngx_buf_t                      *b;
    ngx_str_t                       type;
    ngx_uint_t                      i;
    ngx_http_autoindex_entry_t     *entry;

    static u_char  head[] = "<?xml version=\"1.0\"?>" CRLF "<list>" CRLF;
    static u_char  tail[] = "</list>" CRLF;

    len = sizeof(head) - 1 + sizeof(tail) - 1;

    entry = entries->elts;

    for (i = 0; i < entries->nelts; i++) {
        entry[i].escape = ngx_escape_html(NULL, entry[i].name.data,
                                          entry[i].name.len);

        entry_len = sizeof("<directory></directory>" CRLF) - 1
                  + entry[i].name.len + entry[i].escape
                  + sizeof(" mtime=\"1986-12-31T10:00:00Z\"") - 1;

        if (entry[i].file) {
            entry_len += sizeof(" size=\"\"") - 1 + NGX_OFF_T_LEN;
        }

        if (len > NGX_MAX_SIZE_T_VALUE - entry_len) {
            return NULL;
        }

        len += entry_len;
    }

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NULL;
    }

    b->last = ngx_cpymem(b->last, head, sizeof(head) - 1);

    for (i = 0; i < entries->nelts; i++) {
        *b->last++ = '<';

        if (entry[i].dir) {
            ngx_str_set(&type, "directory");

        } else if (entry[i].file) {
            ngx_str_set(&type, "file");

        } else {
            ngx_str_set(&type, "other");
        }

        b->last = ngx_cpymem(b->last, type.data, type.len);

        b->last = ngx_cpymem(b->last, " mtime=\"", sizeof(" mtime=\"") - 1);

        ngx_gmtime(entry[i].mtime, &tm);

        b->last = ngx_sprintf(b->last, "%4d-%02d-%02dT%02d:%02d:%02dZ",
                              tm.ngx_tm_year, tm.ngx_tm_mon,
                              tm.ngx_tm_mday, tm.ngx_tm_hour,
                              tm.ngx_tm_min, tm.ngx_tm_sec);

        if (entry[i].file) {
            b->last = ngx_cpymem(b->last, "\" size=\"",
                                 sizeof("\" size=\"") - 1);
            b->last = ngx_sprintf(b->last, "%O", entry[i].size);
        }

        *b->last++ = '"'; *b->last++ = '>';

        if (entry[i].escape) {
            b->last = (u_char *) ngx_escape_html(b->last, entry[i].name.data,
                                                 entry[i].name.len);
        } else {
            b->last = ngx_cpymem(b->last, entry[i].name.data,
                                 entry[i].name.len);
        }

        *b->last++ = '<'; *b->last++ = '/';

        b->last = ngx_cpymem(b->last, type.data, type.len);

        *b->last++ = '>';

        *b->last++ = CR; *b->last++ = LF;
    }

    b->last = ngx_cpymem(b->last, tail, sizeof(tail) - 1);

    return b;
}


static int ngx_libc_cdecl
ngx_http_autoindex_cmp_entries(const void *one, const void *two)
{
    ngx_http_autoindex_entry_t *first = (ngx_http_autoindex_entry_t *) one;
    ngx_http_autoindex_entry_t *second = (ngx_http_autoindex_entry_t *) two;

    if (first->dir && !second->dir) {
        /* move the directories to the start */
        return -1;
    }

    if (!first->dir && second->dir) {
        /* move the directories to the start */
        return 1;
    }

    return (int) ngx_strcmp(first->name.data, second->name.data);
}


#if 0

static ngx_buf_t *
ngx_http_autoindex_alloc(ngx_http_autoindex_ctx_t *ctx, size_t size)
{
    ngx_chain_t  *cl;

    if (ctx->buf) {

        if ((size_t) (ctx->buf->end - ctx->buf->last) >= size) {
            return ctx->buf;
        }

        ctx->size += ctx->buf->last - ctx->buf->pos;
    }

    ctx->buf = ngx_create_temp_buf(ctx->pool, ctx->alloc_size);
    if (ctx->buf == NULL) {
        return NULL;
    }

    cl = ngx_alloc_chain_link(ctx->pool);
    if (cl == NULL) {
        return NULL;
    }

    cl->buf = ctx->buf;
    cl->next = NULL;

    *ctx->last_out = cl;
    ctx->last_out = &cl->next;

    return ctx->buf;
}

#endif


static ngx_int_t
ngx_http_autoindex_error(ngx_http_request_t *r, ngx_dir_t *dir, ngx_str_t *name)
{
    if (ngx_close_dir(dir) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                      ngx_close_dir_n " \"%V\" failed", name);
    }

    return r->header_sent ? NGX_ERROR : NGX_HTTP_INTERNAL_SERVER_ERROR;
}


static void *
ngx_http_autoindex_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_autoindex_loc_conf_t  *conf;

    conf = ngx_palloc(cf->pool, sizeof(ngx_http_autoindex_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->format = NGX_CONF_UNSET_UINT;
    conf->localtime = NGX_CONF_UNSET;
    conf->exact_size = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_autoindex_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_autoindex_loc_conf_t *prev = parent;
    ngx_http_autoindex_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_uint_value(conf->format, prev->format,
                              NGX_HTTP_AUTOINDEX_HTML);
    ngx_conf_merge_value(conf->localtime, prev->localtime, 0);
    ngx_conf_merge_value(conf->exact_size, prev->exact_size, 1);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_autoindex_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_autoindex_handler;

    return NGX_OK;
}
