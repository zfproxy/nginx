
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/**
 * @file ngx_http_auth_basic_module.c
 * @brief Nginx HTTP基本认证模块
 *
 * 本模块实现了HTTP基本认证功能，用于保护Web资源免受未经授权的访问。
 *
 * 支持的功能：
 * 1. 基于用户名和密码的认证
 * 2. 可配置的认证域（realm）
 * 3. 外部用户文件支持
 * 4. 与其他Nginx模块的集成
 *
 * 支持的指令：
 * - auth_basic: 启用基本认证并设置认证域
 *   语法：auth_basic string | off;
 *   默认值：auth_basic off;
 *   上下文：http, server, location, limit_except
 *
 * - auth_basic_user_file: 指定用户文件路径
 *   语法：auth_basic_user_file file;
 *   默认值：-
 *   上下文：http, server, location, limit_except
 *
 * 支持的变量：
 * - $remote_user: 存储通过认证的用户名
 *
 * 使用注意点：
 * 1. 确保用户文件权限设置正确，只允许Nginx进程读取
 * 2. 定期更新用户文件以维护安全性
 * 3. 考虑使用HTTPS以加密认证过程
 * 4. 大型系统可能需要考虑更高级的认证方式，如OAuth或JWT
 * 5. 监控认证失败日志，防止暴力破解攻击
 */



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_crypt.h>


/**
 * @brief HTTP基本认证缓冲区大小
 *
 * 定义了HTTP基本认证模块使用的缓冲区大小。
 * 这个缓冲区用于存储和处理认证相关的数据，如用户名和密码。
 * 设置为2048字节，通常足够处理大多数认证场景。
 */
#define NGX_HTTP_AUTH_BUF_SIZE  2048


/**
 * @brief HTTP基本认证模块的位置配置结构
 *
 * 该结构用于存储HTTP基本认证模块在特定位置（如server、location块）的配置信息。
 * 包含了realm（认证域）和user_file（用户文件）的复杂值配置。
 */
typedef struct {
    ngx_http_complex_value_t  *realm;
    ngx_http_complex_value_t  *user_file;
} ngx_http_auth_basic_loc_conf_t;


/**
 * @brief HTTP基本认证处理函数
 *
 * 该函数是HTTP基本认证模块的主要处理函数。
 * 它负责验证客户端提供的认证信息，并决定是否允许访问。
 *
 * @param r 指向当前HTTP请求的指针
 * @return NGX_OK 如果认证成功
 *         NGX_HTTP_UNAUTHORIZED 如果认证失败
 *         NGX_ERROR 如果处理过程中发生错误
 */
static ngx_int_t ngx_http_auth_basic_handler(ngx_http_request_t *r);
/**
 * @brief HTTP基本认证加密处理函数
 *
 * 该函数负责处理HTTP基本认证中的加密部分。
 * 它验证用户提供的密码是否与存储的加密密码匹配。
 *
 * @param r 指向当前HTTP请求的指针
 * @param passwd 指向存储的加密密码的指针
 * @param realm 指向认证域的指针
 * @return NGX_OK 如果密码匹配
 *         NGX_HTTP_UNAUTHORIZED 如果密码不匹配
 *         NGX_ERROR 如果处理过程中发生错误
 */
static ngx_int_t ngx_http_auth_basic_crypt_handler(ngx_http_request_t *r,
    ngx_str_t *passwd, ngx_str_t *realm);
/**
 * @brief 设置HTTP基本认证的realm（认证域）
 *
 * 该函数负责为HTTP基本认证设置realm。realm是一个字符串，
 * 通常用于向用户指示他们正在访问的保护区域。
 *
 * @param r 指向当前HTTP请求的指针
 * @param realm 指向要设置的realm字符串的指针
 * @return NGX_OK 如果成功设置realm
 *         NGX_ERROR 如果设置过程中发生错误
 */
static ngx_int_t ngx_http_auth_basic_set_realm(ngx_http_request_t *r,
    ngx_str_t *realm);
/**
 * @brief 创建HTTP基本认证模块的位置配置
 *
 * 该函数负责为HTTP基本认证模块创建一个新的位置配置结构。
 * 它在每个location块或等效的配置块中被调用，用于初始化模块特定的配置。
 *
 * @param cf 指向nginx配置结构的指针
 * @return 指向新创建的ngx_http_auth_basic_loc_conf_t结构的指针，如果创建失败则返回NULL
 */
static void *ngx_http_auth_basic_create_loc_conf(ngx_conf_t *cf);
/**
 * @brief 合并HTTP基本认证模块的位置配置
 *
 * 该函数负责合并父配置和子配置中的HTTP基本认证模块配置信息。
 * 它处理配置继承和覆盖的逻辑，确保最终的配置正确。
 *
 * @param cf 指向nginx配置结构的指针
 * @param parent 指向父配置结构的指针
 * @param child 指向子配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 */
static char *ngx_http_auth_basic_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
/**
 * @brief 初始化HTTP基本认证模块
 *
 * 该函数负责初始化HTTP基本认证模块。它在Nginx配置阶段被调用，
 * 用于设置必要的处理程序和初始化模块所需的任何资源。
 *
 * @param cf 指向nginx配置结构的指针
 * @return NGX_OK 如果初始化成功
 *         NGX_ERROR 如果初始化过程中发生错误
 */
static ngx_int_t ngx_http_auth_basic_init(ngx_conf_t *cf);
/**
 * @brief 处理auth_basic_user_file指令
 *
 * 该函数负责处理nginx配置文件中的auth_basic_user_file指令。
 * 它设置用于HTTP基本认证的用户文件路径。
 *
 * @param cf 指向nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 */
static char *ngx_http_auth_basic_user_file(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


/**
 * @brief HTTP基本认证模块的命令数组
 *
 * 这个数组定义了HTTP基本认证模块支持的所有Nginx配置指令。
 * 每个指令都是一个ngx_command_t结构体，包含了指令的名称、类型、
 * 处理函数等信息。这些指令用于配置HTTP基本认证的行为。
 */
static ngx_command_t  ngx_http_auth_basic_commands[] = {

    { ngx_string("auth_basic"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_auth_basic_loc_conf_t, realm),
      NULL },

    { ngx_string("auth_basic_user_file"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_auth_basic_user_file,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_auth_basic_loc_conf_t, user_file),
      NULL },

      ngx_null_command
};


/**
 * @brief HTTP基本认证模块的上下文结构
 *
 * 这个结构定义了HTTP基本认证模块的上下文。
 * 它包含了模块在不同配置阶段的回调函数指针，用于创建和合并配置。
 * 这些函数在模块初始化和请求处理过程中被Nginx核心调用。
 */
static ngx_http_module_t  ngx_http_auth_basic_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_auth_basic_init,              /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_auth_basic_create_loc_conf,   /* create location configuration */
    ngx_http_auth_basic_merge_loc_conf     /* merge location configuration */
};


/**
 * @brief HTTP基本认证模块的主要结构体
 *
 * 这个结构体定义了HTTP基本认证模块的主要信息。
 * 它包含了模块的上下文、指令、类型以及各种生命周期回调函数。
 * 这个结构体是Nginx识别和管理该模块的关键。
 */
ngx_module_t  ngx_http_auth_basic_module = {
    NGX_MODULE_V1,
    &ngx_http_auth_basic_module_ctx,       /* module context */
    ngx_http_auth_basic_commands,          /* module directives */
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
ngx_http_auth_basic_handler(ngx_http_request_t *r)
{
    off_t                            offset;
    ssize_t                          n;
    ngx_fd_t                         fd;
    ngx_int_t                        rc;
    ngx_err_t                        err;
    ngx_str_t                        pwd, realm, user_file;
    ngx_uint_t                       i, level, login, left, passwd;
    ngx_file_t                       file;
    ngx_http_auth_basic_loc_conf_t  *alcf;
    u_char                           buf[NGX_HTTP_AUTH_BUF_SIZE];
    enum {
        sw_login,
        sw_passwd,
        sw_skip
    } state;

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_auth_basic_module);

    if (alcf->realm == NULL || alcf->user_file == NULL) {
        return NGX_DECLINED;
    }

    if (ngx_http_complex_value(r, alcf->realm, &realm) != NGX_OK) {
        return NGX_ERROR;
    }

    if (realm.len == 3 && ngx_strncmp(realm.data, "off", 3) == 0) {
        return NGX_DECLINED;
    }

    rc = ngx_http_auth_basic_user(r);

    if (rc == NGX_DECLINED) {

        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "no user/password was provided for basic authentication");

        return ngx_http_auth_basic_set_realm(r, &realm);
    }

    if (rc == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_complex_value(r, alcf->user_file, &user_file) != NGX_OK) {
        return NGX_ERROR;
    }

    fd = ngx_open_file(user_file.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);

    if (fd == NGX_INVALID_FILE) {
        err = ngx_errno;

        if (err == NGX_ENOENT) {
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_FORBIDDEN;

        } else {
            level = NGX_LOG_CRIT;
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_log_error(level, r->connection->log, err,
                      ngx_open_file_n " \"%s\" failed", user_file.data);

        return rc;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.fd = fd;
    file.name = user_file;
    file.log = r->connection->log;

    state = sw_login;
    passwd = 0;
    login = 0;
    left = 0;
    offset = 0;

    for ( ;; ) {
        i = left;

        n = ngx_read_file(&file, buf + left, NGX_HTTP_AUTH_BUF_SIZE - left,
                          offset);

        if (n == NGX_ERROR) {
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
            goto cleanup;
        }

        if (n == 0) {
            break;
        }

        for (i = left; i < left + n; i++) {
            switch (state) {

            case sw_login:
                if (login == 0) {

                    if (buf[i] == '#' || buf[i] == CR) {
                        state = sw_skip;
                        break;
                    }

                    if (buf[i] == LF) {
                        break;
                    }
                }

                if (buf[i] != r->headers_in.user.data[login]) {
                    state = sw_skip;
                    break;
                }

                if (login == r->headers_in.user.len) {
                    state = sw_passwd;
                    passwd = i + 1;
                }

                login++;

                break;

            case sw_passwd:
                if (buf[i] == LF || buf[i] == CR || buf[i] == ':') {
                    buf[i] = '\0';

                    pwd.len = i - passwd;
                    pwd.data = &buf[passwd];

                    rc = ngx_http_auth_basic_crypt_handler(r, &pwd, &realm);
                    goto cleanup;
                }

                break;

            case sw_skip:
                if (buf[i] == LF) {
                    state = sw_login;
                    login = 0;
                }

                break;
            }
        }

        if (state == sw_passwd) {
            left = left + n - passwd;
            ngx_memmove(buf, &buf[passwd], left);
            passwd = 0;

        } else {
            left = 0;
        }

        offset += n;
    }

    if (state == sw_passwd) {
        pwd.len = i - passwd;
        pwd.data = ngx_pnalloc(r->pool, pwd.len + 1);
        if (pwd.data == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_cpystrn(pwd.data, &buf[passwd], pwd.len + 1);

        rc = ngx_http_auth_basic_crypt_handler(r, &pwd, &realm);
        goto cleanup;
    }

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "user \"%V\" was not found in \"%s\"",
                  &r->headers_in.user, user_file.data);

    rc = ngx_http_auth_basic_set_realm(r, &realm);

cleanup:

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", user_file.data);
    }

    ngx_explicit_memzero(buf, NGX_HTTP_AUTH_BUF_SIZE);

    return rc;
}


static ngx_int_t
ngx_http_auth_basic_crypt_handler(ngx_http_request_t *r, ngx_str_t *passwd,
    ngx_str_t *realm)
{
    ngx_int_t   rc;
    u_char     *encrypted;

    rc = ngx_crypt(r->pool, r->headers_in.passwd.data, passwd->data,
                   &encrypted);

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "rc: %i user: \"%V\" salt: \"%s\"",
                   rc, &r->headers_in.user, passwd->data);

    if (rc != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_strcmp(encrypted, passwd->data) == 0) {
        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "encrypted: \"%s\"", encrypted);

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "user \"%V\": password mismatch",
                  &r->headers_in.user);

    return ngx_http_auth_basic_set_realm(r, realm);
}


static ngx_int_t
ngx_http_auth_basic_set_realm(ngx_http_request_t *r, ngx_str_t *realm)
{
    size_t   len;
    u_char  *basic, *p;

    r->headers_out.www_authenticate = ngx_list_push(&r->headers_out.headers);
    if (r->headers_out.www_authenticate == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    len = sizeof("Basic realm=\"\"") - 1 + realm->len;

    basic = ngx_pnalloc(r->pool, len);
    if (basic == NULL) {
        r->headers_out.www_authenticate->hash = 0;
        r->headers_out.www_authenticate = NULL;
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    p = ngx_cpymem(basic, "Basic realm=\"", sizeof("Basic realm=\"") - 1);
    p = ngx_cpymem(p, realm->data, realm->len);
    *p = '"';

    r->headers_out.www_authenticate->hash = 1;
    r->headers_out.www_authenticate->next = NULL;
    ngx_str_set(&r->headers_out.www_authenticate->key, "WWW-Authenticate");
    r->headers_out.www_authenticate->value.data = basic;
    r->headers_out.www_authenticate->value.len = len;

    return NGX_HTTP_UNAUTHORIZED;
}


static void *
ngx_http_auth_basic_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_auth_basic_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_auth_basic_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->realm = NGX_CONF_UNSET_PTR;
    conf->user_file = NGX_CONF_UNSET_PTR;

    return conf;
}


static char *
ngx_http_auth_basic_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_auth_basic_loc_conf_t  *prev = parent;
    ngx_http_auth_basic_loc_conf_t  *conf = child;

    ngx_conf_merge_ptr_value(conf->realm, prev->realm, NULL);
    ngx_conf_merge_ptr_value(conf->user_file, prev->user_file, NULL);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_auth_basic_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_auth_basic_handler;

    return NGX_OK;
}


static char *
ngx_http_auth_basic_user_file(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_auth_basic_loc_conf_t *alcf = conf;

    ngx_str_t                         *value;
    ngx_http_compile_complex_value_t   ccv;

    if (alcf->user_file != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    alcf->user_file = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
    if (alcf->user_file == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = alcf->user_file;
    ccv.zero = 1;
    ccv.conf_prefix = 1;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
