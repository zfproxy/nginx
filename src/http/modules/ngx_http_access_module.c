
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


/**
 * @file ngx_http_access_module.c
 * @brief Nginx HTTP访问控制模块
 *
 * 本模块实现了基于IP地址的访问控制功能，用于限制或允许特定IP地址访问Web资源。
 *
 * 支持的功能：
 * 1. 基于IPv4地址的访问控制
 * 2. 基于IPv6地址的访问控制（如果系统支持）
 * 3. 基于Unix域套接字的访问控制（如果系统支持）
 * 4. 支持允许（allow）和拒绝（deny）规则
 * 5. 支持CIDR表示法的IP地址范围
 * 6. 规则按顺序匹配，首个匹配规则生效
 *
 * 支持的指令：
 * - allow: 允许指定的IP地址或地址范围访问
 *   语法：allow address | CIDR | unix: | all;
 *   上下文：http, server, location, limit_except
 *
 * - deny: 拒绝指定的IP地址或地址范围访问
 *   语法：deny address | CIDR | unix: | all;
 *   上下文：http, server, location, limit_except
 *
 * 支持的变量：
 * 本模块不引入新的变量，但使用了Nginx核心提供的$remote_addr变量。
 *
 * 使用注意点：
 * 1. 规则的顺序很重要，首个匹配的规则将被应用
 * 2. 使用"deny all"作为最后一条规则可以实现白名单机制
 * 3. 使用"allow all"作为最后一条规则可以实现黑名单机制
 * 4. 注意IPv4和IPv6规则的区别，确保正确配置
 * 5. 在使用反向代理时，可能需要结合realip模块使用以获取真实客户端IP
 * 6. 大规模部署时，考虑使用更高效的IP查找算法或外部认证系统
 */



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief 定义HTTP访问规则结构体
 *
 * 这个结构体用于存储单个IPv4访问规则的信息。
 */
typedef struct {
    in_addr_t         mask;      /**< IP地址掩码 */
    in_addr_t         addr;      /**< IP地址 */
    ngx_uint_t        deny;      /**< 是否拒绝访问，1表示拒绝，0表示允许 */
                                 /* 原注释：unsigned  deny:1; */
} ngx_http_access_rule_t;

#if (NGX_HAVE_INET6)

/**
 * @brief 定义IPv6访问规则结构体
 *
 * 这个结构体用于存储单个IPv6访问规则的信息。
 */
typedef struct {
    struct in6_addr   addr;
    struct in6_addr   mask;
    ngx_uint_t        deny;      /* unsigned  deny:1; */
} ngx_http_access_rule6_t;

#endif

#if (NGX_HAVE_UNIX_DOMAIN)

/**
 * @brief 定义Unix域套接字访问规则结构体
 *
 * 这个结构体用于存储Unix域套接字的访问规则信息。
 * 在支持Unix域套接字的系统上使用。
 */
typedef struct {
    ngx_uint_t        deny;      /* unsigned  deny:1; */
} ngx_http_access_rule_un_t;

#endif

/**
 * @brief 定义HTTP访问模块的位置配置结构体
 *
 * 这个结构体用于存储HTTP访问模块在特定位置（如server、location块）的配置信息。
 * 它包含了不同类型的访问规则数组，用于IPv4、IPv6和Unix域套接字（如果支持）。
 */
typedef struct {
    ngx_array_t      *rules;     /* array of ngx_http_access_rule_t */
#if (NGX_HAVE_INET6)
    ngx_array_t      *rules6;    /* array of ngx_http_access_rule6_t */
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
    ngx_array_t      *rules_un;  /* array of ngx_http_access_rule_un_t */
#endif
} ngx_http_access_loc_conf_t;


/**
 * @brief HTTP访问控制处理函数
 *
 * 这个函数是ngx_http_access_module模块的主要处理函数。
 * 它负责检查客户端的IP地址是否符合配置的访问规则，
 * 并决定是允许还是拒绝请求。
 *
 * @param r 指向当前HTTP请求的指针
 * @return NGX_OK 如果允许访问
 *         NGX_HTTP_FORBIDDEN 如果拒绝访问
 *         NGX_ERROR 如果处理过程中发生错误
 */
static ngx_int_t ngx_http_access_handler(ngx_http_request_t *r);
/**
 * @brief 检查IPv4地址是否符合访问规则
 *
 * 这个函数用于检查给定的IPv4地址是否符合配置的访问规则。
 * 它遍历所有的IPv4访问规则，并根据规则决定是否允许访问。
 *
 * @param r 指向当前HTTP请求的指针
 * @param alcf 指向ngx_http_access_loc_conf_t结构的指针，包含访问规则配置
 * @param addr 要检查的IPv4地址
 * @return NGX_OK 如果允许访问
 *         NGX_HTTP_FORBIDDEN 如果拒绝访问
 *         NGX_ERROR 如果处理过程中发生错误
 */
static ngx_int_t ngx_http_access_inet(ngx_http_request_t *r,
    ngx_http_access_loc_conf_t *alcf, in_addr_t addr);
#if (NGX_HAVE_INET6)
/**
 * @brief 检查IPv6地址是否符合访问规则
 *
 * 这个函数用于检查给定的IPv6地址是否符合配置的访问规则。
 * 它遍历所有的IPv6访问规则，并根据规则决定是否允许访问。
 *
 * @param r 指向当前HTTP请求的指针
 * @param alcf 指向ngx_http_access_loc_conf_t结构的指针，包含访问规则配置
 * @param p 指向IPv6地址的指针
 * @return NGX_OK 如果允许访问
 *         NGX_HTTP_FORBIDDEN 如果拒绝访问
 *         NGX_ERROR 如果处理过程中发生错误
 */
static ngx_int_t ngx_http_access_inet6(ngx_http_request_t *r,
    ngx_http_access_loc_conf_t *alcf, u_char *p);
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
/**
 * @brief 检查Unix域套接字连接是否符合访问规则
 *
 * 这个函数用于检查来自Unix域套接字的连接是否符合配置的访问规则。
 * 它主要用于处理本地socket连接的访问控制。
 *
 * @param r 指向当前HTTP请求的指针
 * @param alcf 指向ngx_http_access_loc_conf_t结构的指针，包含访问规则配置
 * @return NGX_OK 如果允许访问
 *         NGX_HTTP_FORBIDDEN 如果拒绝访问
 *         NGX_ERROR 如果处理过程中发生错误
 */
static ngx_int_t ngx_http_access_unix(ngx_http_request_t *r,
    ngx_http_access_loc_conf_t *alcf);
#endif
/**
 * @brief 处理访问规则匹配后的操作
 *
 * 这个函数在找到匹配的访问规则后被调用，用于执行相应的操作。
 * 根据规则是允许还是拒绝来决定最终的访问结果。
 *
 * @param r 指向当前HTTP请求的指针
 * @param deny 表示是否为拒绝规则的标志，0表示允许，非0表示拒绝
 * @return NGX_OK 如果允许访问
 *         NGX_HTTP_FORBIDDEN 如果拒绝访问
 *         NGX_ERROR 如果处理过程中发生错误
 */
static ngx_int_t ngx_http_access_found(ngx_http_request_t *r, ngx_uint_t deny);
/**
 * @brief 解析和设置访问规则
 *
 * 这个函数用于解析配置文件中的 allow 和 deny 指令，并设置相应的访问规则。
 * 它会处理 IP 地址、CIDR 网段或特殊值（如 all）等不同类型的规则。
 *
 * @param cf 指向 ngx_conf_t 结构的指针，包含配置信息
 * @param cmd 指向 ngx_command_t 结构的指针，表示当前正在处理的指令
 * @param conf 指向模块配置结构的指针
 * @return NGX_CONF_OK 如果解析成功
 *         NGX_CONF_ERROR 如果解析失败
 */
static char *ngx_http_access_rule(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/**
 * @brief 创建访问控制模块的位置配置
 *
 * 这个函数用于为每个位置块创建一个新的访问控制配置结构。
 * 它会分配内存并初始化配置结构中的成员变量。
 *
 * @param cf 指向ngx_conf_t结构的指针，包含当前配置上下文
 * @return 指向新创建的配置结构的指针，如果失败则返回NULL
 */
static void *ngx_http_access_create_loc_conf(ngx_conf_t *cf);
/**
 * @brief 合并访问控制模块的位置配置
 *
 * 这个函数用于合并父配置和子配置中的访问控制规则。
 * 它会将父配置中的规则复制到子配置中，确保子配置包含所有适用的访问规则。
 *
 * @param cf 指向ngx_conf_t结构的指针，包含当前配置上下文
 * @param parent 指向父配置结构的指针
 * @param child 指向子配置结构的指针
 * @return NGX_CONF_OK 如果合并成功，否则返回错误字符串
 */
static char *ngx_http_access_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
/**
 * @brief 初始化访问控制模块
 *
 * 这个函数在Nginx配置阶段完成后被调用，用于初始化访问控制模块。
 * 它可能会设置处理程序、初始化数据结构或执行其他必要的设置操作。
 *
 * @param cf 指向ngx_conf_t结构的指针，包含Nginx配置信息
 * @return NGX_OK 如果初始化成功
 *         NGX_ERROR 如果初始化失败
 */
static ngx_int_t ngx_http_access_init(ngx_conf_t *cf);


/**
 * @brief 定义访问控制模块的命令数组
 *
 * 这个数组包含了访问控制模块支持的Nginx配置指令。
 * 每个指令都有特定的处理函数和配置上下文。
 */
static ngx_command_t  ngx_http_access_commands[] = {

    { ngx_string("allow"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_access_rule,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("deny"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_access_rule,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};



/**
 * @brief 定义访问控制模块的上下文结构
 *
 * 这个结构体包含了访问控制模块的各种回调函数和配置处理函数。
 * 它定义了模块在不同阶段的行为，如创建配置、合并配置等。
 */
static ngx_http_module_t  ngx_http_access_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_access_init,                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_access_create_loc_conf,       /* create location configuration */
    ngx_http_access_merge_loc_conf         /* merge location configuration */
};


/**
 * @brief 定义访问控制模块的主结构
 *
 * 这个结构体是Nginx模块系统的标准结构，用于定义访问控制模块。
 * 它包含了模块的上下文、指令集、类型以及各种生命周期回调函数。
 */
ngx_module_t  ngx_http_access_module = {
    NGX_MODULE_V1,
    &ngx_http_access_module_ctx,           /* module context */
    ngx_http_access_commands,              /* module directives */
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
ngx_http_access_handler(ngx_http_request_t *r)
{
    struct sockaddr_in          *sin;
    ngx_http_access_loc_conf_t  *alcf;
#if (NGX_HAVE_INET6)
    u_char                      *p;
    in_addr_t                    addr;
    struct sockaddr_in6         *sin6;
#endif

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_access_module);

    switch (r->connection->sockaddr->sa_family) {

    case AF_INET:
        if (alcf->rules) {
            sin = (struct sockaddr_in *) r->connection->sockaddr;
            return ngx_http_access_inet(r, alcf, sin->sin_addr.s_addr);
        }
        break;

#if (NGX_HAVE_INET6)

    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) r->connection->sockaddr;
        p = sin6->sin6_addr.s6_addr;

        if (alcf->rules && IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
            addr = (in_addr_t) p[12] << 24;
            addr += p[13] << 16;
            addr += p[14] << 8;
            addr += p[15];
            return ngx_http_access_inet(r, alcf, htonl(addr));
        }

        if (alcf->rules6) {
            return ngx_http_access_inet6(r, alcf, p);
        }

        break;

#endif

#if (NGX_HAVE_UNIX_DOMAIN)

    case AF_UNIX:
        if (alcf->rules_un) {
            return ngx_http_access_unix(r, alcf);
        }

        break;

#endif
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_access_inet(ngx_http_request_t *r, ngx_http_access_loc_conf_t *alcf,
    in_addr_t addr)
{
    ngx_uint_t               i;
    ngx_http_access_rule_t  *rule;

    rule = alcf->rules->elts;
    for (i = 0; i < alcf->rules->nelts; i++) {

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "access: %08XD %08XD %08XD",
                       addr, rule[i].mask, rule[i].addr);

        if ((addr & rule[i].mask) == rule[i].addr) {
            return ngx_http_access_found(r, rule[i].deny);
        }
    }

    return NGX_DECLINED;
}


#if (NGX_HAVE_INET6)

static ngx_int_t
ngx_http_access_inet6(ngx_http_request_t *r, ngx_http_access_loc_conf_t *alcf,
    u_char *p)
{
    ngx_uint_t                n;
    ngx_uint_t                i;
    ngx_http_access_rule6_t  *rule6;

    rule6 = alcf->rules6->elts;
    for (i = 0; i < alcf->rules6->nelts; i++) {

#if (NGX_DEBUG)
        {
        size_t  cl, ml, al;
        u_char  ct[NGX_INET6_ADDRSTRLEN];
        u_char  mt[NGX_INET6_ADDRSTRLEN];
        u_char  at[NGX_INET6_ADDRSTRLEN];

        cl = ngx_inet6_ntop(p, ct, NGX_INET6_ADDRSTRLEN);
        ml = ngx_inet6_ntop(rule6[i].mask.s6_addr, mt, NGX_INET6_ADDRSTRLEN);
        al = ngx_inet6_ntop(rule6[i].addr.s6_addr, at, NGX_INET6_ADDRSTRLEN);

        ngx_log_debug6(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "access: %*s %*s %*s", cl, ct, ml, mt, al, at);
        }
#endif

        for (n = 0; n < 16; n++) {
            if ((p[n] & rule6[i].mask.s6_addr[n]) != rule6[i].addr.s6_addr[n]) {
                goto next;
            }
        }

        return ngx_http_access_found(r, rule6[i].deny);

    next:
        continue;
    }

    return NGX_DECLINED;
}

#endif


#if (NGX_HAVE_UNIX_DOMAIN)

static ngx_int_t
ngx_http_access_unix(ngx_http_request_t *r, ngx_http_access_loc_conf_t *alcf)
{
    ngx_uint_t                  i;
    ngx_http_access_rule_un_t  *rule_un;

    rule_un = alcf->rules_un->elts;
    for (i = 0; i < alcf->rules_un->nelts; i++) {

        /* TODO: check path */
        if (1) {
            return ngx_http_access_found(r, rule_un[i].deny);
        }
    }

    return NGX_DECLINED;
}

#endif


static ngx_int_t
ngx_http_access_found(ngx_http_request_t *r, ngx_uint_t deny)
{
    ngx_http_core_loc_conf_t  *clcf;

    if (deny) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->satisfy == NGX_HTTP_SATISFY_ALL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "access forbidden by rule");
        }

        return NGX_HTTP_FORBIDDEN;
    }

    return NGX_OK;
}


static char *
ngx_http_access_rule(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_access_loc_conf_t *alcf = conf;

    ngx_int_t                   rc;
    ngx_uint_t                  all;
    ngx_str_t                  *value;
    ngx_cidr_t                  cidr;
    ngx_http_access_rule_t     *rule;
#if (NGX_HAVE_INET6)
    ngx_http_access_rule6_t    *rule6;
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
    ngx_http_access_rule_un_t  *rule_un;
#endif

    all = 0;
    ngx_memzero(&cidr, sizeof(ngx_cidr_t));

    value = cf->args->elts;

    if (value[1].len == 3 && ngx_strcmp(value[1].data, "all") == 0) {
        all = 1;

#if (NGX_HAVE_UNIX_DOMAIN)
    } else if (value[1].len == 5 && ngx_strcmp(value[1].data, "unix:") == 0) {
        cidr.family = AF_UNIX;
#endif

    } else {
        rc = ngx_ptocidr(&value[1], &cidr);

        if (rc == NGX_ERROR) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                         "invalid parameter \"%V\"", &value[1]);
            return NGX_CONF_ERROR;
        }

        if (rc == NGX_DONE) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                         "low address bits of %V are meaningless", &value[1]);
        }
    }

    if (cidr.family == AF_INET || all) {

        if (alcf->rules == NULL) {
            alcf->rules = ngx_array_create(cf->pool, 4,
                                           sizeof(ngx_http_access_rule_t));
            if (alcf->rules == NULL) {
                return NGX_CONF_ERROR;
            }
        }

        rule = ngx_array_push(alcf->rules);
        if (rule == NULL) {
            return NGX_CONF_ERROR;
        }

        rule->mask = cidr.u.in.mask;
        rule->addr = cidr.u.in.addr;
        rule->deny = (value[0].data[0] == 'd') ? 1 : 0;
    }

#if (NGX_HAVE_INET6)
    if (cidr.family == AF_INET6 || all) {

        if (alcf->rules6 == NULL) {
            alcf->rules6 = ngx_array_create(cf->pool, 4,
                                            sizeof(ngx_http_access_rule6_t));
            if (alcf->rules6 == NULL) {
                return NGX_CONF_ERROR;
            }
        }

        rule6 = ngx_array_push(alcf->rules6);
        if (rule6 == NULL) {
            return NGX_CONF_ERROR;
        }

        rule6->mask = cidr.u.in6.mask;
        rule6->addr = cidr.u.in6.addr;
        rule6->deny = (value[0].data[0] == 'd') ? 1 : 0;
    }
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
    if (cidr.family == AF_UNIX || all) {

        if (alcf->rules_un == NULL) {
            alcf->rules_un = ngx_array_create(cf->pool, 1,
                                            sizeof(ngx_http_access_rule_un_t));
            if (alcf->rules_un == NULL) {
                return NGX_CONF_ERROR;
            }
        }

        rule_un = ngx_array_push(alcf->rules_un);
        if (rule_un == NULL) {
            return NGX_CONF_ERROR;
        }

        rule_un->deny = (value[0].data[0] == 'd') ? 1 : 0;
    }
#endif

    return NGX_CONF_OK;
}


static void *
ngx_http_access_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_access_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_access_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}


static char *
ngx_http_access_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_access_loc_conf_t  *prev = parent;
    ngx_http_access_loc_conf_t  *conf = child;

    if (conf->rules == NULL
#if (NGX_HAVE_INET6)
        && conf->rules6 == NULL
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
        && conf->rules_un == NULL
#endif
    ) {
        conf->rules = prev->rules;
#if (NGX_HAVE_INET6)
        conf->rules6 = prev->rules6;
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
        conf->rules_un = prev->rules_un;
#endif
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_access_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_access_handler;

    return NGX_OK;
}
