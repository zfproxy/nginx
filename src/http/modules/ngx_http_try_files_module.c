
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/**
 * @file ngx_http_try_files_module.c
 * @brief Nginx try_files 模块
 *
 * 该模块实现了 try_files 指令，允许按顺序检查多个文件是否存在，并返回第一个存在的文件。
 * 如果所有文件都不存在，则可以指定一个回退选项。
 *
 * 支持的功能：
 * 1. 按顺序检查多个文件或目录是否存在
 * 2. 支持变量替换
 * 3. 可以指定最后的回退选项（内部重定向或返回特定状态码）
 *
 * 支持的指令：
 * - try_files file ... uri;
 * - try_files file ... =code;
 *
 * 支持的变量：
 * - $uri：当前请求的 URI
 * - $document_root：当前请求的文档根目录
 * - 其他 Nginx 核心模块和其他模块提供的变量
 *
 * 使用注意点：
 * 1. try_files 指令必须在 server 或 location 上下文中使用
 * 2. 最后一个参数应该是回退选项，可以是内部重定向或状态码
 * 3. 检查文件存在性时会产生文件系统 I/O，过多的检查可能影响性能
 * 4. 与 index 指令结合使用时需要注意顺序和优先级
 * 5. 在使用变量时，要确保变量在运行时有正确的值
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief try_files 指令的配置结构
 *
 * 这个结构体用于存储 try_files 指令的配置信息。
 * 它包含了处理 try_files 指令所需的各种参数和标志。
 */
typedef struct {
    ngx_array_t           *lengths;
    ngx_array_t           *values;
    ngx_str_t              name;

    unsigned               code:10;
    unsigned               test_dir:1;
} ngx_http_try_file_t;


/**
 * @brief try_files 指令的位置配置结构
 *
 * 这个结构体用于存储 try_files 指令在特定位置的配置信息。
 * 它包含了处理 try_files 指令在当前位置所需的配置数据。
 */
typedef struct {
    ngx_http_try_file_t   *try_files;
} ngx_http_try_files_loc_conf_t;


/**
 * @brief try_files 指令的处理函数
 *
 * 这个函数是 try_files 指令的主要处理逻辑。
 * 它会按照配置的顺序尝试访问不同的文件或位置，
 * 直到找到一个可用的资源或达到最后一个选项。
 *
 * @param r 指向当前 HTTP 请求的指针
 * @return 返回处理结果的状态码
 */
static ngx_int_t ngx_http_try_files_handler(ngx_http_request_t *r);
/**
 * @brief 处理 try_files 指令的配置函数
 *
 * 这个函数负责解析和设置 try_files 指令的配置。
 * 它会处理指令的参数，设置相应的配置结构，并进行必要的初始化。
 *
 * @param cf Nginx 配置结构指针
 * @param cmd 当前正在处理的命令结构指针
 * @param conf 指向当前位置配置的指针
 * @return 成功时返回 NGX_CONF_OK，失败时返回错误信息字符串
 */
static char *ngx_http_try_files(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 创建 try_files 指令的位置配置
 *
 * 这个函数负责为 try_files 指令创建位置配置结构。
 * 它会分配内存并初始化 ngx_http_try_files_loc_conf_t 结构体。
 *
 * @param cf Nginx 配置结构指针
 * @return 成功时返回指向新创建的位置配置结构的指针，失败时返回 NULL
 */
static void *ngx_http_try_files_create_loc_conf(ngx_conf_t *cf);
/**
 * @brief 初始化 try_files 模块
 *
 * 这个函数负责 try_files 模块的初始化工作。
 * 它在 Nginx 配置阶段被调用，用于设置模块的初始状态和注册必要的处理程序。
 *
 * @param cf Nginx 配置结构指针
 * @return 成功时返回 NGX_OK，失败时返回错误码
 */
static ngx_int_t ngx_http_try_files_init(ngx_conf_t *cf);


/**
 * @brief try_files 模块的命令数组
 *
 * 这个静态数组定义了 try_files 模块支持的所有配置指令。
 * 每个元素都是一个 ngx_command_t 结构，描述了一个特定的配置指令。
 * 这个数组将被 Nginx 的配置解析器使用，以识别和处理 try_files 相关的配置。
 */
static ngx_command_t  ngx_http_try_files_commands[] = {

    { ngx_string("try_files"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_2MORE,
      ngx_http_try_files,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


/**
 * @brief try_files 模块的上下文结构
 *
 * 这个静态变量定义了 try_files 模块的上下文结构。
 * 它包含了模块在不同配置阶段的回调函数指针。
 * 这个结构体用于告诉 Nginx 核心如何处理该模块的配置和初始化。
 */
static ngx_http_module_t  ngx_http_try_files_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_try_files_init,               /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_try_files_create_loc_conf,    /* create location configuration */
    NULL                                   /* merge location configuration */
};


/**
 * @brief try_files 模块的定义
 *
 * 这个结构定义了 Nginx 的 try_files 模块。
 * 它包含了模块的基本信息、上下文、指令集等。
 * try_files 模块用于尝试访问多个文件路径，直到找到一个存在的文件或达到最后一个选项。
 */
ngx_module_t  ngx_http_try_files_module = {
    NGX_MODULE_V1,
    &ngx_http_try_files_module_ctx,        /* module context */
    ngx_http_try_files_commands,           /* module directives */
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
ngx_http_try_files_handler(ngx_http_request_t *r)
{
    size_t                          len, root, alias, reserve, allocated;
    u_char                         *p, *name;
    ngx_str_t                       path, args;
    ngx_uint_t                      test_dir;
    ngx_http_try_file_t            *tf;
    ngx_open_file_info_t            of;
    ngx_http_script_code_pt         code;
    ngx_http_script_engine_t        e;
    ngx_http_core_loc_conf_t       *clcf;
    ngx_http_script_len_code_pt     lcode;
    ngx_http_try_files_loc_conf_t  *tlcf;

    tlcf = ngx_http_get_module_loc_conf(r, ngx_http_try_files_module);

    if (tlcf->try_files == NULL) {
        return NGX_DECLINED;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "try files handler");

    allocated = 0;
    root = 0;
    name = NULL;
    /* suppress MSVC warning */
    path.data = NULL;

    tf = tlcf->try_files;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    alias = clcf->alias;

    for ( ;; ) {

        if (tf->lengths) {
            ngx_memzero(&e, sizeof(ngx_http_script_engine_t));

            e.ip = tf->lengths->elts;
            e.request = r;

            /* 1 is for terminating '\0' as in static names */
            len = 1;

            while (*(uintptr_t *) e.ip) {
                lcode = *(ngx_http_script_len_code_pt *) e.ip;
                len += lcode(&e);
            }

        } else {
            len = tf->name.len;
        }

        if (!alias) {
            reserve = len > r->uri.len ? len - r->uri.len : 0;

        } else if (alias == NGX_MAX_SIZE_T_VALUE) {
            reserve = len;

        } else {
            reserve = len > r->uri.len - alias ? len - (r->uri.len - alias) : 0;
        }

        if (reserve > allocated || !allocated) {

            /* 16 bytes are preallocation */
            allocated = reserve + 16;

            if (ngx_http_map_uri_to_path(r, &path, &root, allocated) == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            name = path.data + root;
        }

        if (tf->values == NULL) {

            /* tf->name.len includes the terminating '\0' */

            ngx_memcpy(name, tf->name.data, tf->name.len);

            path.len = (name + tf->name.len - 1) - path.data;

        } else {
            e.ip = tf->values->elts;
            e.pos = name;
            e.flushed = 1;

            while (*(uintptr_t *) e.ip) {
                code = *(ngx_http_script_code_pt *) e.ip;
                code((ngx_http_script_engine_t *) &e);
            }

            path.len = e.pos - path.data;

            *e.pos = '\0';

            if (alias && alias != NGX_MAX_SIZE_T_VALUE
                && ngx_strncmp(name, r->uri.data, alias) == 0)
            {
                ngx_memmove(name, name + alias, len - alias);
                path.len -= alias;
            }
        }

        test_dir = tf->test_dir;

        tf++;

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "trying to use %s: \"%s\" \"%s\"",
                       test_dir ? "dir" : "file", name, path.data);

        if (tf->lengths == NULL && tf->name.len == 0) {

            if (tf->code) {
                return tf->code;
            }

            path.len -= root;
            path.data += root;

            if (path.data[0] == '@') {
                (void) ngx_http_named_location(r, &path);

            } else {
                ngx_http_split_args(r, &path, &args);

                (void) ngx_http_internal_redirect(r, &path, &args);
            }

            ngx_http_finalize_request(r, NGX_DONE);
            return NGX_DONE;
        }

        ngx_memzero(&of, sizeof(ngx_open_file_info_t));

        of.read_ahead = clcf->read_ahead;
        of.directio = clcf->directio;
        of.valid = clcf->open_file_cache_valid;
        of.min_uses = clcf->open_file_cache_min_uses;
        of.test_only = 1;
        of.errors = clcf->open_file_cache_errors;
        of.events = clcf->open_file_cache_events;

        if (ngx_http_set_disable_symlinks(r, clcf, &path, &of) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool)
            != NGX_OK)
        {
            if (of.err == 0) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            if (of.err != NGX_ENOENT
                && of.err != NGX_ENOTDIR
                && of.err != NGX_ENAMETOOLONG)
            {
                ngx_log_error(NGX_LOG_CRIT, r->connection->log, of.err,
                              "%s \"%s\" failed", of.failed, path.data);
            }

            continue;
        }

        if (of.is_dir != test_dir) {
            continue;
        }

        path.len -= root;
        path.data += root;

        if (!alias) {
            r->uri = path;

        } else if (alias == NGX_MAX_SIZE_T_VALUE) {
            if (!test_dir) {
                r->uri = path;
                r->add_uri_to_alias = 1;
            }

        } else {
            name = r->uri.data;

            r->uri.len = alias + path.len;
            r->uri.data = ngx_pnalloc(r->pool, r->uri.len);
            if (r->uri.data == NULL) {
                r->uri.len = 0;
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            p = ngx_copy(r->uri.data, name, alias);
            ngx_memcpy(p, path.data, path.len);
        }

        ngx_http_set_exten(r);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "try file uri: \"%V\"", &r->uri);

        return NGX_DECLINED;
    }

    /* not reached */
}


static char *
ngx_http_try_files(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_try_files_loc_conf_t *tlcf = conf;

    ngx_str_t                  *value;
    ngx_int_t                   code;
    ngx_uint_t                  i, n;
    ngx_http_try_file_t        *tf;
    ngx_http_script_compile_t   sc;

    if (tlcf->try_files) {
        return "is duplicate";
    }

    tf = ngx_pcalloc(cf->pool, cf->args->nelts * sizeof(ngx_http_try_file_t));
    if (tf == NULL) {
        return NGX_CONF_ERROR;
    }

    tlcf->try_files = tf;

    value = cf->args->elts;

    for (i = 0; i < cf->args->nelts - 1; i++) {

        tf[i].name = value[i + 1];

        if (tf[i].name.len > 0
            && tf[i].name.data[tf[i].name.len - 1] == '/'
            && i + 2 < cf->args->nelts)
        {
            tf[i].test_dir = 1;
            tf[i].name.len--;
            tf[i].name.data[tf[i].name.len] = '\0';
        }

        n = ngx_http_script_variables_count(&tf[i].name);

        if (n) {
            ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

            sc.cf = cf;
            sc.source = &tf[i].name;
            sc.lengths = &tf[i].lengths;
            sc.values = &tf[i].values;
            sc.variables = n;
            sc.complete_lengths = 1;
            sc.complete_values = 1;

            if (ngx_http_script_compile(&sc) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else {
            /* add trailing '\0' to length */
            tf[i].name.len++;
        }
    }

    if (tf[i - 1].name.data[0] == '=') {

        code = ngx_atoi(tf[i - 1].name.data + 1, tf[i - 1].name.len - 2);

        if (code == NGX_ERROR || code > 999) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid code \"%*s\"",
                               tf[i - 1].name.len - 1, tf[i - 1].name.data);
            return NGX_CONF_ERROR;
        }

        tf[i].code = code;
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_try_files_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_try_files_loc_conf_t  *tlcf;

    tlcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_try_files_loc_conf_t));
    if (tlcf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     tlcf->try_files = NULL;
     */

    return tlcf;
}


static ngx_int_t
ngx_http_try_files_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_PRECONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_try_files_handler;

    return NGX_OK;
}
