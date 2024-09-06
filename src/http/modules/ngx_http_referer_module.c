
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_referer_module.c
 *
 * 该模块实现了基于HTTP Referer头的访问控制功能。
 *
 * 支持的功能:
 * - 基于白名单验证HTTP请求的Referer头
 * - 支持精确匹配和正则表达式匹配
 * - 可配置是否允许缺少Referer头的请求
 * - 可配置是否允许被阻止的Referer
 * - 支持服务器名称匹配
 *
 * 支持的指令:
 * - valid_referers: 定义有效的引用者列表
 *   语法: valid_referers none | blocked | server_names | string ...;
 *   上下文: server, location
 *
 * 支持的变量:
 * - $invalid_referer: 如果referer头无效则为"1"，否则为空字符串
 *
 * 使用注意点:
 * 1. 合理配置valid_referers指令，避免错误地阻止合法请求
 * 2. 考虑到Referer头可能被伪造，不应该将其作为唯一的安全措施
 * 3. 使用正则表达式时要注意性能影响，过于复杂的正则可能会降低处理速度
 * 4. 启用server_names选项时，确保正确配置了server_name指令
 * 5. 对于不发送Referer头的请求（如HTTPS到HTTP的跳转），要考虑是否允许
 * 6. 定期检查和更新referer白名单，以适应网站结构的变化
 * 7. 在使用此模块时，建议同时启用日志记录，以便于分析和调试
 * 8. 注意referer检查可能会影响搜索引擎爬虫的访问，需要适当配置
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/*
 * 定义一个宏 NGX_HTTP_REFERER_NO_URI_PART
 * 该宏的值为 ((void *) 4)
 * 这个宏可能用于表示 Referer 头部中没有 URI 部分的特殊情况
 */
#define NGX_HTTP_REFERER_NO_URI_PART  ((void *) 4)

/*
 * NGX_HTTP_REFERER_NO_URI_PART 宏定义
 *
 * 该宏用于表示 Referer 头部中没有 URI 部分的特殊情况
 * 值为 ((void *) 4)，这是一个特殊的指针值
 * 在处理 Referer 时，如果遇到这个值，表示该 Referer 没有 URI 部分
 * 使用指针值作为标记可以节省内存，并且可以快速进行比较
 */
typedef struct {
    ngx_hash_combined_t      hash;  // 组合哈希表，用于快速查找引用者

#if (NGX_PCRE)
    ngx_array_t             *regex;  // 存储正则表达式的数组
    ngx_array_t             *server_name_regex;  // 存储服务器名称正则表达式的数组
#endif

    ngx_flag_t               no_referer;  // 标志位，表示是否允许没有 Referer 头的请求
    ngx_flag_t               blocked_referer;  // 标志位，表示是否允许被阻止的引用者
    ngx_flag_t               server_names;  // 标志位，表示是否检查服务器名称

    ngx_hash_keys_arrays_t  *keys;  // 用于构建哈希表的键值数组

    ngx_uint_t               referer_hash_max_size;  // 引用者哈希表的最大大小
    ngx_uint_t               referer_hash_bucket_size;  // 引用者哈希表的桶大小
} ngx_http_referer_conf_t;


/* 添加 referer 模块相关的变量 */
static ngx_int_t ngx_http_referer_add_variables(ngx_conf_t *cf);

/* 创建 referer 模块的配置结构 */
static void * ngx_http_referer_create_conf(ngx_conf_t *cf);

/* 合并 referer 模块的配置 */
static char * ngx_http_referer_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);

/* 处理 valid_referers 指令 */
static char *ngx_http_valid_referers(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/* 添加一个 referer 到哈希表中 */
static ngx_int_t ngx_http_add_referer(ngx_conf_t *cf,
    ngx_hash_keys_arrays_t *keys, ngx_str_t *value, ngx_str_t *uri);

/* 添加一个正则表达式 referer */
static ngx_int_t ngx_http_add_regex_referer(ngx_conf_t *cf,
    ngx_http_referer_conf_t *rlcf, ngx_str_t *name);

#if (NGX_PCRE)
/* 添加一个正则表达式服务器名称 */
static ngx_int_t ngx_http_add_regex_server_name(ngx_conf_t *cf,
    ngx_http_referer_conf_t *rlcf, ngx_http_regex_t *regex);
#endif

/* 比较两个 referer 通配符的函数 */
static int ngx_libc_cdecl ngx_http_cmp_referer_wildcards(const void *one,
    const void *two);


static ngx_command_t  ngx_http_referer_commands[] = {

    { ngx_string("valid_referers"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_valid_referers,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("referer_hash_max_size"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_referer_conf_t, referer_hash_max_size),
      NULL },

    { ngx_string("referer_hash_bucket_size"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_referer_conf_t, referer_hash_bucket_size),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_referer_module_ctx = {
    ngx_http_referer_add_variables,        /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_referer_create_conf,          /* create location configuration */
    ngx_http_referer_merge_conf            /* merge location configuration */
};


ngx_module_t  ngx_http_referer_module = {
    NGX_MODULE_V1,
    &ngx_http_referer_module_ctx,          /* module context */
    ngx_http_referer_commands,             /* module directives */
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


static ngx_str_t  ngx_http_invalid_referer_name = ngx_string("invalid_referer");


/**
 * @brief 处理HTTP Referer变量的函数
 *
 * 该函数用于处理和验证HTTP请求中的Referer头部信息。
 * 它检查Referer是否有效，并根据配置的规则进行验证。
 *
 * @param r HTTP请求结构体
 * @param v 变量值结构体
 * @param data 用户数据
 * @return ngx_int_t 返回处理结果
 */
static ngx_int_t
ngx_http_referer_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    u_char                    *p, *ref, *last;
    size_t                     len;
    ngx_str_t                 *uri;
    ngx_uint_t                 i, key;
    ngx_http_referer_conf_t   *rlcf;
    u_char                     buf[256];
#if (NGX_PCRE)
    ngx_int_t                  rc;
    ngx_str_t                  referer;
#endif

    rlcf = ngx_http_get_module_loc_conf(r, ngx_http_referer_module);

    if (rlcf->hash.hash.buckets == NULL
        && rlcf->hash.wc_head == NULL
        && rlcf->hash.wc_tail == NULL
#if (NGX_PCRE)
        && rlcf->regex == NULL
        && rlcf->server_name_regex == NULL
#endif
       )
    {
        goto valid;
    }

    if (r->headers_in.referer == NULL) {
        if (rlcf->no_referer) {
            goto valid;
        }

        goto invalid;
    }

    len = r->headers_in.referer->value.len;
    ref = r->headers_in.referer->value.data;

    if (len >= sizeof("http://i.ru") - 1) {
        last = ref + len;

        if (ngx_strncasecmp(ref, (u_char *) "http://", 7) == 0) {
            ref += 7;
            len -= 7;
            goto valid_scheme;

        } else if (ngx_strncasecmp(ref, (u_char *) "https://", 8) == 0) {
            ref += 8;
            len -= 8;
            goto valid_scheme;
        }
    }

    if (rlcf->blocked_referer) {
        goto valid;
    }

    goto invalid;

valid_scheme:

    i = 0;
    key = 0;

    for (p = ref; p < last; p++) {
        if (*p == '/' || *p == ':') {
            break;
        }

        if (i == 256) {
            goto invalid;
        }

        buf[i] = ngx_tolower(*p);
        key = ngx_hash(key, buf[i++]);
    }

    uri = ngx_hash_find_combined(&rlcf->hash, key, buf, p - ref);

    if (uri) {
        goto uri;
    }

#if (NGX_PCRE)

    if (rlcf->server_name_regex) {
        referer.len = p - ref;
        referer.data = buf;

        rc = ngx_regex_exec_array(rlcf->server_name_regex, &referer,
                                  r->connection->log);

        if (rc == NGX_OK) {
            goto valid;
        }

        if (rc == NGX_ERROR) {
            return rc;
        }

        /* NGX_DECLINED */
    }

    if (rlcf->regex) {
        referer.len = len;
        referer.data = ref;

        rc = ngx_regex_exec_array(rlcf->regex, &referer, r->connection->log);

        if (rc == NGX_OK) {
            goto valid;
        }

        if (rc == NGX_ERROR) {
            return rc;
        }

        /* NGX_DECLINED */
    }

#endif

invalid:

    *v = ngx_http_variable_true_value;

    return NGX_OK;

uri:

    for ( /* void */ ; p < last; p++) {
        if (*p == '/') {
            break;
        }
    }

    len = last - p;

    if (uri == NGX_HTTP_REFERER_NO_URI_PART) {
        goto valid;
    }

    if (len < uri->len || ngx_strncmp(uri->data, p, uri->len) != 0) {
        goto invalid;
    }

valid:

    *v = ngx_http_variable_null_value;

    return NGX_OK;
}


static ngx_int_t
ngx_http_referer_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var;

    var = ngx_http_add_variable(cf, &ngx_http_invalid_referer_name,
                                NGX_HTTP_VAR_CHANGEABLE);
    if (var == NULL) {
        return NGX_ERROR;
    }

    var->get_handler = ngx_http_referer_variable;

    return NGX_OK;
}


static void *
ngx_http_referer_create_conf(ngx_conf_t *cf)
{
    ngx_http_referer_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_referer_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->hash = { NULL };
     *     conf->server_names = 0;
     *     conf->keys = NULL;
     */

#if (NGX_PCRE)
    conf->regex = NGX_CONF_UNSET_PTR;
    conf->server_name_regex = NGX_CONF_UNSET_PTR;
#endif

    conf->no_referer = NGX_CONF_UNSET;
    conf->blocked_referer = NGX_CONF_UNSET;
    conf->referer_hash_max_size = NGX_CONF_UNSET_UINT;
    conf->referer_hash_bucket_size = NGX_CONF_UNSET_UINT;

    return conf;
}


static char *
ngx_http_referer_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_referer_conf_t *prev = parent;
    ngx_http_referer_conf_t *conf = child;

    ngx_uint_t                 n;
    ngx_hash_init_t            hash;
    ngx_http_server_name_t    *sn;
    ngx_http_core_srv_conf_t  *cscf;

    if (conf->keys == NULL) {
        conf->hash = prev->hash;

#if (NGX_PCRE)
        ngx_conf_merge_ptr_value(conf->regex, prev->regex, NULL);
        ngx_conf_merge_ptr_value(conf->server_name_regex,
                                 prev->server_name_regex, NULL);
#endif
        ngx_conf_merge_value(conf->no_referer, prev->no_referer, 0);
        ngx_conf_merge_value(conf->blocked_referer, prev->blocked_referer, 0);
        ngx_conf_merge_uint_value(conf->referer_hash_max_size,
                                  prev->referer_hash_max_size, 2048);
        ngx_conf_merge_uint_value(conf->referer_hash_bucket_size,
                                  prev->referer_hash_bucket_size, 64);

        return NGX_CONF_OK;
    }

    if (conf->server_names == 1) {
        cscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_core_module);

        sn = cscf->server_names.elts;
        for (n = 0; n < cscf->server_names.nelts; n++) {

#if (NGX_PCRE)
            if (sn[n].regex) {

                if (ngx_http_add_regex_server_name(cf, conf, sn[n].regex)
                    != NGX_OK)
                {
                    return NGX_CONF_ERROR;
                }

                continue;
            }
#endif

            if (ngx_http_add_referer(cf, conf->keys, &sn[n].name, NULL)
                != NGX_OK)
            {
                return NGX_CONF_ERROR;
            }
        }
    }

    if ((conf->no_referer == 1 || conf->blocked_referer == 1)
        && conf->keys->keys.nelts == 0
        && conf->keys->dns_wc_head.nelts == 0
        && conf->keys->dns_wc_tail.nelts == 0)
    {
        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "the \"none\" or \"blocked\" referers are specified "
                      "in the \"valid_referers\" directive "
                      "without any valid referer");
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_uint_value(conf->referer_hash_max_size,
                              prev->referer_hash_max_size, 2048);
    ngx_conf_merge_uint_value(conf->referer_hash_bucket_size,
                              prev->referer_hash_bucket_size, 64);
    conf->referer_hash_bucket_size = ngx_align(conf->referer_hash_bucket_size,
                                               ngx_cacheline_size);

    hash.key = ngx_hash_key_lc;
    hash.max_size = conf->referer_hash_max_size;
    hash.bucket_size = conf->referer_hash_bucket_size;
    hash.name = "referer_hash";
    hash.pool = cf->pool;

    if (conf->keys->keys.nelts) {
        hash.hash = &conf->hash.hash;
        hash.temp_pool = NULL;

        if (ngx_hash_init(&hash, conf->keys->keys.elts, conf->keys->keys.nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    if (conf->keys->dns_wc_head.nelts) {

        ngx_qsort(conf->keys->dns_wc_head.elts,
                  (size_t) conf->keys->dns_wc_head.nelts,
                  sizeof(ngx_hash_key_t),
                  ngx_http_cmp_referer_wildcards);

        hash.hash = NULL;
        hash.temp_pool = cf->temp_pool;

        if (ngx_hash_wildcard_init(&hash, conf->keys->dns_wc_head.elts,
                                   conf->keys->dns_wc_head.nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }

        conf->hash.wc_head = (ngx_hash_wildcard_t *) hash.hash;
    }

    if (conf->keys->dns_wc_tail.nelts) {

        ngx_qsort(conf->keys->dns_wc_tail.elts,
                  (size_t) conf->keys->dns_wc_tail.nelts,
                  sizeof(ngx_hash_key_t),
                  ngx_http_cmp_referer_wildcards);

        hash.hash = NULL;
        hash.temp_pool = cf->temp_pool;

        if (ngx_hash_wildcard_init(&hash, conf->keys->dns_wc_tail.elts,
                                   conf->keys->dns_wc_tail.nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }

        conf->hash.wc_tail = (ngx_hash_wildcard_t *) hash.hash;
    }

#if (NGX_PCRE)
    ngx_conf_merge_ptr_value(conf->regex, prev->regex, NULL);
    ngx_conf_merge_ptr_value(conf->server_name_regex, prev->server_name_regex,
                             NULL);
#endif

    if (conf->no_referer == NGX_CONF_UNSET) {
        conf->no_referer = 0;
    }

    if (conf->blocked_referer == NGX_CONF_UNSET) {
        conf->blocked_referer = 0;
    }

    conf->keys = NULL;

    return NGX_CONF_OK;
}


static char *
ngx_http_valid_referers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_referer_conf_t  *rlcf = conf;

    u_char      *p;
    ngx_str_t   *value, uri;
    ngx_uint_t   i;

    if (rlcf->keys == NULL) {
        rlcf->keys = ngx_pcalloc(cf->temp_pool, sizeof(ngx_hash_keys_arrays_t));
        if (rlcf->keys == NULL) {
            return NGX_CONF_ERROR;
        }

        rlcf->keys->pool = cf->pool;
        rlcf->keys->temp_pool = cf->pool;

        if (ngx_hash_keys_array_init(rlcf->keys, NGX_HASH_SMALL) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        if (value[i].len == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid referer \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (ngx_strcmp(value[i].data, "none") == 0) {
            rlcf->no_referer = 1;
            continue;
        }

        if (ngx_strcmp(value[i].data, "blocked") == 0) {
            rlcf->blocked_referer = 1;
            continue;
        }

        if (ngx_strcmp(value[i].data, "server_names") == 0) {
            rlcf->server_names = 1;
            continue;
        }

        if (value[i].data[0] == '~') {
            if (ngx_http_add_regex_referer(cf, rlcf, &value[i]) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_str_null(&uri);

        p = (u_char *) ngx_strchr(value[i].data, '/');

        if (p) {
            uri.len = (value[i].data + value[i].len) - p;
            uri.data = p;
            value[i].len = p - value[i].data;
        }

        if (ngx_http_add_referer(cf, rlcf->keys, &value[i], &uri) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_add_referer(ngx_conf_t *cf, ngx_hash_keys_arrays_t *keys,
    ngx_str_t *value, ngx_str_t *uri)
{
    ngx_int_t   rc;
    ngx_str_t  *u;

    if (uri == NULL || uri->len == 0) {
        u = NGX_HTTP_REFERER_NO_URI_PART;

    } else {
        u = ngx_palloc(cf->pool, sizeof(ngx_str_t));
        if (u == NULL) {
            return NGX_ERROR;
        }

        *u = *uri;
    }

    rc = ngx_hash_add_key(keys, value, u, NGX_HASH_WILDCARD_KEY);

    if (rc == NGX_OK) {
        return NGX_OK;
    }

    if (rc == NGX_DECLINED) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid hostname or wildcard \"%V\"", value);
    }

    if (rc == NGX_BUSY) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "conflicting parameter \"%V\"", value);
    }

    return NGX_ERROR;
}


static ngx_int_t
ngx_http_add_regex_referer(ngx_conf_t *cf, ngx_http_referer_conf_t *rlcf,
    ngx_str_t *name)
{
#if (NGX_PCRE)
    ngx_regex_elt_t      *re;
    ngx_regex_compile_t   rc;
    u_char                errstr[NGX_MAX_CONF_ERRSTR];

    if (name->len == 1) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "empty regex in \"%V\"", name);
        return NGX_ERROR;
    }

    if (rlcf->regex == NGX_CONF_UNSET_PTR) {
        rlcf->regex = ngx_array_create(cf->pool, 2, sizeof(ngx_regex_elt_t));
        if (rlcf->regex == NULL) {
            return NGX_ERROR;
        }
    }

    re = ngx_array_push(rlcf->regex);
    if (re == NULL) {
        return NGX_ERROR;
    }

    name->len--;
    name->data++;

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pattern = *name;
    rc.pool = cf->pool;
    rc.options = NGX_REGEX_CASELESS;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

    if (ngx_regex_compile(&rc) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &rc.err);
        return NGX_ERROR;
    }

    re->regex = rc.regex;
    re->name = name->data;

    return NGX_OK;

#else

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "using regex \"%V\" requires PCRE library",
                       name);

    return NGX_ERROR;

#endif
}


#if (NGX_PCRE)

static ngx_int_t
ngx_http_add_regex_server_name(ngx_conf_t *cf, ngx_http_referer_conf_t *rlcf,
    ngx_http_regex_t *regex)
{
    ngx_regex_elt_t  *re;

    if (rlcf->server_name_regex == NGX_CONF_UNSET_PTR) {
        rlcf->server_name_regex = ngx_array_create(cf->pool, 2,
                                                   sizeof(ngx_regex_elt_t));
        if (rlcf->server_name_regex == NULL) {
            return NGX_ERROR;
        }
    }

    re = ngx_array_push(rlcf->server_name_regex);
    if (re == NULL) {
        return NGX_ERROR;
    }

    re->regex = regex->regex;
    re->name = regex->name.data;

    return NGX_OK;
}

#endif


static int ngx_libc_cdecl
ngx_http_cmp_referer_wildcards(const void *one, const void *two)
{
    ngx_hash_key_t  *first, *second;

    first = (ngx_hash_key_t *) one;
    second = (ngx_hash_key_t *) two;

    return ngx_dns_strcmp(first->key.data, second->key.data);
}
