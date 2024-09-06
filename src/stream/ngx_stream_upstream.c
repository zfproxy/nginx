
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_stream_upstream.c
 *
 * 该模块实现了Nginx流模块的上游（upstream）功能。
 *
 * 支持的功能：
 * 1. 定义上游服务器组
 * 2. 负载均衡
 * 3. 健康检查
 * 4. 连接池管理
 * 5. 故障转移
 *
 * 支持的指令：
 * - upstream: 定义一个上游服务器组
 * - server: 在upstream块内定义具体的上游服务器
 *
 * 支持的变量：
 * - $upstream_addr: 上游服务器的IP地址和端口
 * - $upstream_bytes_sent: 发送到上游服务器的字节数
 * - $upstream_bytes_received: 从上游服务器接收的字节数
 * - $upstream_connect_time: 与上游服务器建立连接所花费的时间
 * - $upstream_first_byte_time: 接收上游服务器第一个字节的时间
 * - $upstream_session_time: 与上游服务器的会话时间
 *
 * 使用注意点：
 * 1. 确保在stream块内正确配置upstream
 * 2. 合理设置负载均衡策略和服务器权重
 * 3. 适当配置健康检查参数以保证服务可用性
 * 4. 注意连接池的大小设置，以平衡资源利用和性能
 * 5. 合理使用故障转移机制，避免过度依赖单一服务器
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>


// 添加上游相关变量到配置中
static ngx_int_t ngx_stream_upstream_add_variables(ngx_conf_t *cf);

// 处理上游地址变量
static ngx_int_t ngx_stream_upstream_addr_variable(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data);

// 处理上游响应时间变量
static ngx_int_t ngx_stream_upstream_response_time_variable(
    ngx_stream_session_t *s, ngx_stream_variable_value_t *v, uintptr_t data);

// 处理上游字节数变量
static ngx_int_t ngx_stream_upstream_bytes_variable(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data);

// 处理upstream指令
static char *ngx_stream_upstream(ngx_conf_t *cf, ngx_command_t *cmd,
    void *dummy);

// 处理server指令
static char *ngx_stream_upstream_server(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

// 创建上游主配置
static void *ngx_stream_upstream_create_main_conf(ngx_conf_t *cf);

// 初始化上游主配置
static char *ngx_stream_upstream_init_main_conf(ngx_conf_t *cf, void *conf);

// 定义 ngx_stream_upstream 模块的指令数组
static ngx_command_t  ngx_stream_upstream_commands[] = {

    // upstream 指令：用于定义一组上游服务器
    { ngx_string("upstream"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE1,  // 指令可在 stream 主配置中使用，是一个块指令，需要一个参数
      ngx_stream_upstream,  // 处理该指令的函数
      0,
      0,
      NULL },

    // server 指令：用于在 upstream 块中定义具体的上游服务器
    { ngx_string("server"),
      NGX_STREAM_UPS_CONF|NGX_CONF_1MORE,  // 指令可在 upstream 配置中使用，需要至少一个参数
      ngx_stream_upstream_server,  // 处理该指令的函数
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    // 指令数组结束标志
    ngx_null_command
};


static ngx_stream_module_t  ngx_stream_upstream_module_ctx = {
    ngx_stream_upstream_add_variables,     /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_stream_upstream_create_main_conf,  /* create main configuration */
    ngx_stream_upstream_init_main_conf,    /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL                                   /* merge server configuration */
};


ngx_module_t  ngx_stream_upstream_module = {
    NGX_MODULE_V1,
    &ngx_stream_upstream_module_ctx,       /* module context */
    ngx_stream_upstream_commands,          /* module directives */
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


// 定义上游变量数组
static ngx_stream_variable_t  ngx_stream_upstream_vars[] = {

    // 上游服务器地址变量
    { ngx_string("upstream_addr"), NULL,
      ngx_stream_upstream_addr_variable, 0,
      NGX_STREAM_VAR_NOCACHEABLE, 0 },

    // 发送到上游服务器的字节数变量
    { ngx_string("upstream_bytes_sent"), NULL,
      ngx_stream_upstream_bytes_variable, 0,
      NGX_STREAM_VAR_NOCACHEABLE, 0 },

    // 与上游服务器建立连接所需时间变量
    { ngx_string("upstream_connect_time"), NULL,
      ngx_stream_upstream_response_time_variable, 2,
      NGX_STREAM_VAR_NOCACHEABLE, 0 },

    // 接收上游服务器第一个字节的时间变量
    { ngx_string("upstream_first_byte_time"), NULL,
      ngx_stream_upstream_response_time_variable, 1,
      NGX_STREAM_VAR_NOCACHEABLE, 0 },

    // 与上游服务器的会话时间变量
    { ngx_string("upstream_session_time"), NULL,
      ngx_stream_upstream_response_time_variable, 0,
      NGX_STREAM_VAR_NOCACHEABLE, 0 },

    // 从上游服务器接收的字节数变量
    { ngx_string("upstream_bytes_received"), NULL,
      ngx_stream_upstream_bytes_variable, 1,
      NGX_STREAM_VAR_NOCACHEABLE, 0 },

    // 数组结束标志
    ngx_stream_null_variable
};


// 添加上游变量到配置中
static ngx_int_t
ngx_stream_upstream_add_variables(ngx_conf_t *cf)
{
    ngx_stream_variable_t  *var, *v;

    // 遍历上游变量数组
    for (v = ngx_stream_upstream_vars; v->name.len; v++) {
        // 添加变量到配置中
        var = ngx_stream_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        // 设置变量的获取处理函数和数据
        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


// 处理上游地址变量
static ngx_int_t
ngx_stream_upstream_addr_variable(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data)
{
    u_char                       *p;
    size_t                        len;
    ngx_uint_t                    i;
    ngx_stream_upstream_state_t  *state;

    // 设置变量属性
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 检查上游状态是否存在
    if (s->upstream_states == NULL || s->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 计算所需内存长度
    len = 0;
    state = s->upstream_states->elts;

    for (i = 0; i < s->upstream_states->nelts; i++) {
        if (state[i].peer) {
            len += state[i].peer->len;
        }

        len += 2; // 为分隔符预留空间
    }

    // 分配内存
    p = ngx_pnalloc(s->connection->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    // 构建地址字符串
    i = 0;

    for ( ;; ) {
        if (state[i].peer) {
            p = ngx_cpymem(p, state[i].peer->data, state[i].peer->len);
        }

        if (++i == s->upstream_states->nelts) {
            break;
        }

        *p++ = ',';
        *p++ = ' ';
    }

    v->len = p - v->data;

    return NGX_OK;
}


// 处理上游字节数变量
static ngx_int_t
ngx_stream_upstream_bytes_variable(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data)
{
    u_char                       *p;
    size_t                        len;
    ngx_uint_t                    i;
    ngx_stream_upstream_state_t  *state;

    // 设置变量属性
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 检查上游状态是否存在
    if (s->upstream_states == NULL || s->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 计算所需内存长度
    len = s->upstream_states->nelts * (NGX_OFF_T_LEN + 2);

    // 分配内存
    p = ngx_pnalloc(s->connection->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    // 构建字节数字符串
    i = 0;
    state = s->upstream_states->elts;

    for ( ;; ) {

        if (data == 1) {
            // 接收的字节数
            p = ngx_sprintf(p, "%O", state[i].bytes_received);

        } else {
            // 发送的字节数
            p = ngx_sprintf(p, "%O", state[i].bytes_sent);
        }

        if (++i == s->upstream_states->nelts) {
            break;
        }

        *p++ = ',';
        *p++ = ' ';
    }

    v->len = p - v->data;

    return NGX_OK;
}


static ngx_int_t
ngx_stream_upstream_response_time_variable(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data)
{
    u_char                       *p;
    size_t                        len;
    ngx_uint_t                    i;
    ngx_msec_int_t                ms;
    ngx_stream_upstream_state_t  *state;

    // 设置变量属性
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 检查上游状态是否存在
    if (s->upstream_states == NULL || s->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 计算所需内存长度
    len = s->upstream_states->nelts * (NGX_TIME_T_LEN + 4 + 2);

    // 分配内存
    p = ngx_pnalloc(s->connection->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    i = 0;
    state = s->upstream_states->elts;

    // 遍历所有上游状态
    for ( ;; ) {

        // 根据data参数选择不同的时间类型
        if (data == 1) {
            ms = state[i].first_byte_time;
        } else if (data == 2) {
            ms = state[i].connect_time;
        } else {
            ms = state[i].response_time;
        }

        // 格式化时间字符串
        if (ms != -1) {
            ms = ngx_max(ms, 0);
            p = ngx_sprintf(p, "%T.%03M", (time_t) ms / 1000, ms % 1000);
        } else {
            *p++ = '-';
        }

        // 检查是否为最后一个元素
        if (++i == s->upstream_states->nelts) {
            break;
        }

        // 添加分隔符
        *p++ = ',';
        *p++ = ' ';
    }

    // 设置变量长度
    v->len = p - v->data;

    return NGX_OK;
}


static char *
ngx_stream_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    char                            *rv;
    void                            *mconf;
    ngx_str_t                       *value;
    ngx_url_t                        u;
    ngx_uint_t                       m;
    ngx_conf_t                       pcf;
    ngx_stream_module_t             *module;
    ngx_stream_conf_ctx_t           *ctx, *stream_ctx;
    ngx_stream_upstream_srv_conf_t  *uscf;

    // 初始化 ngx_url_t 结构体
    ngx_memzero(&u, sizeof(ngx_url_t));

    // 获取配置参数
    value = cf->args->elts;
    u.host = value[1];
    u.no_resolve = 1;
    u.no_port = 1;

    // 添加上游配置
    uscf = ngx_stream_upstream_add(cf, &u, NGX_STREAM_UPSTREAM_CREATE
                                           |NGX_STREAM_UPSTREAM_WEIGHT
                                           |NGX_STREAM_UPSTREAM_MAX_CONNS
                                           |NGX_STREAM_UPSTREAM_MAX_FAILS
                                           |NGX_STREAM_UPSTREAM_FAIL_TIMEOUT
                                           |NGX_STREAM_UPSTREAM_DOWN
                                           |NGX_STREAM_UPSTREAM_BACKUP);
    if (uscf == NULL) {
        return NGX_CONF_ERROR;
    }

    // 创建配置上下文
    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_stream_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    stream_ctx = cf->ctx;
    ctx->main_conf = stream_ctx->main_conf;

    // 为上游服务器配置分配内存

    ctx->srv_conf = ngx_pcalloc(cf->pool,
                                sizeof(void *) * ngx_stream_max_module);
    if (ctx->srv_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    ctx->srv_conf[ngx_stream_upstream_module.ctx_index] = uscf;

    uscf->srv_conf = ctx->srv_conf;

    // 创建各模块的服务器配置
    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NGX_STREAM_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;

        if (module->create_srv_conf) {
            mconf = module->create_srv_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->srv_conf[cf->cycle->modules[m]->ctx_index] = mconf;
        }
    }

    // 创建上游服务器数组
    uscf->servers = ngx_array_create(cf->pool, 4,
                                     sizeof(ngx_stream_upstream_server_t));
    if (uscf->servers == NULL) {
        return NGX_CONF_ERROR;
    }

    // 解析 upstream{} 块内的配置

    pcf = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NGX_STREAM_UPS_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NGX_CONF_OK) {
        return rv;
    }

    // 检查是否有服务器配置
    if (uscf->servers->nelts == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no servers are inside upstream");
        return NGX_CONF_ERROR;
    }

    return rv;
}


static char *
ngx_stream_upstream_server(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_upstream_srv_conf_t  *uscf = conf;

    time_t                         fail_timeout;
    ngx_str_t                     *value, s;
    ngx_url_t                      u;
    ngx_int_t                      weight, max_conns, max_fails;
    ngx_uint_t                     i;
    ngx_stream_upstream_server_t  *us;

    // 添加新的上游服务器配置
    us = ngx_array_push(uscf->servers);
    if (us == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(us, sizeof(ngx_stream_upstream_server_t));

    value = cf->args->elts;

    // 设置默认值
    weight = 1;
    max_conns = 0;
    max_fails = 1;
    fail_timeout = 10;

    // 解析服务器配置参数
    for (i = 2; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "weight=", 7) == 0) {

            if (!(uscf->flags & NGX_STREAM_UPSTREAM_WEIGHT)) {
                goto not_supported;
            }

            weight = ngx_atoi(&value[i].data[7], value[i].len - 7);

            if (weight == NGX_ERROR || weight == 0) {
                goto invalid;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "max_conns=", 10) == 0) {

            if (!(uscf->flags & NGX_STREAM_UPSTREAM_MAX_CONNS)) {
                goto not_supported;
            }

            max_conns = ngx_atoi(&value[i].data[10], value[i].len - 10);

            if (max_conns == NGX_ERROR) {
                goto invalid;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "max_fails=", 10) == 0) {

            if (!(uscf->flags & NGX_STREAM_UPSTREAM_MAX_FAILS)) {
                goto not_supported;
            }

            max_fails = ngx_atoi(&value[i].data[10], value[i].len - 10);

            if (max_fails == NGX_ERROR) {
                goto invalid;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "fail_timeout=", 13) == 0) {

            if (!(uscf->flags & NGX_STREAM_UPSTREAM_FAIL_TIMEOUT)) {
                goto not_supported;
            }

            s.len = value[i].len - 13;
            s.data = &value[i].data[13];

            fail_timeout = ngx_parse_time(&s, 1);

            if (fail_timeout == (time_t) NGX_ERROR) {
                goto invalid;
            }

            continue;
        }

        if (ngx_strcmp(value[i].data, "backup") == 0) {

            if (!(uscf->flags & NGX_STREAM_UPSTREAM_BACKUP)) {
                goto not_supported;
            }

            us->backup = 1;

            continue;
        }

        if (ngx_strcmp(value[i].data, "down") == 0) {

            if (!(uscf->flags & NGX_STREAM_UPSTREAM_DOWN)) {
                goto not_supported;
            }

            us->down = 1;

            continue;
        }

        goto invalid;
    }

    // 解析服务器地址
    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url = value[1];

    if (ngx_parse_url(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "%s in upstream \"%V\"", u.err, &u.url);
        }

        return NGX_CONF_ERROR;
    }

    if (u.no_port) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no port in upstream \"%V\"", &u.url);
        return NGX_CONF_ERROR;
    }

    // 设置服务器配置
    us->name = u.url;
    us->addrs = u.addrs;
    us->naddrs = u.naddrs;
    us->weight = weight;
    us->max_conns = max_conns;
    us->max_fails = max_fails;
    us->fail_timeout = fail_timeout;

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);

    return NGX_CONF_ERROR;

not_supported:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "balancing method does not support parameter \"%V\"",
                       &value[i]);

    return NGX_CONF_ERROR;
}


// 添加上游服务器配置
ngx_stream_upstream_srv_conf_t *
ngx_stream_upstream_add(ngx_conf_t *cf, ngx_url_t *u, ngx_uint_t flags)
{
    ngx_uint_t                        i;
    ngx_stream_upstream_server_t     *us;
    ngx_stream_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_stream_upstream_main_conf_t  *umcf;

    // 如果不是创建新的上游，则解析URL
    if (!(flags & NGX_STREAM_UPSTREAM_CREATE)) {

        if (ngx_parse_url(cf->pool, u) != NGX_OK) {
            if (u->err) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "%s in upstream \"%V\"", u->err, &u->url);
            }

            return NULL;
        }
    }

    // 获取主配置
    umcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_upstream_module);

    uscfp = umcf->upstreams.elts;

    // 遍历已存在的上游配置
    for (i = 0; i < umcf->upstreams.nelts; i++) {

        // 检查主机名是否匹配
        if (uscfp[i]->host.len != u->host.len
            || ngx_strncasecmp(uscfp[i]->host.data, u->host.data, u->host.len)
               != 0)
        {
            continue;
        }

        // 检查是否重复创建
        if ((flags & NGX_STREAM_UPSTREAM_CREATE)
             && (uscfp[i]->flags & NGX_STREAM_UPSTREAM_CREATE))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "duplicate upstream \"%V\"", &u->host);
            return NULL;
        }

        // 检查端口冲突
        if ((uscfp[i]->flags & NGX_STREAM_UPSTREAM_CREATE) && !u->no_port) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "upstream \"%V\" may not have port %d",
                               &u->host, u->port);
            return NULL;
        }

        if ((flags & NGX_STREAM_UPSTREAM_CREATE) && !uscfp[i]->no_port) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                          "upstream \"%V\" may not have port %d in %s:%ui",
                          &u->host, uscfp[i]->port,
                          uscfp[i]->file_name, uscfp[i]->line);
            return NULL;
        }

        // 检查端口是否匹配
        if (uscfp[i]->port != u->port) {
            continue;
        }

        // 如果是创建新的上游，更新标志
        if (flags & NGX_STREAM_UPSTREAM_CREATE) {
            uscfp[i]->flags = flags;
        }

        return uscfp[i];
    }

    // 创建新的上游服务器配置
    uscf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_upstream_srv_conf_t));
    if (uscf == NULL) {
        return NULL;
    }

    // 初始化新的上游服务器配置
    uscf->flags = flags;
    uscf->host = u->host;
    uscf->file_name = cf->conf_file->file.name.data;
    uscf->line = cf->conf_file->line;
    uscf->port = u->port;
    uscf->no_port = u->no_port;

    // 如果只有一个地址且有端口或是Unix域套接字，创建服务器数组
    if (u->naddrs == 1 && (u->port || u->family == AF_UNIX)) {
        uscf->servers = ngx_array_create(cf->pool, 1,
                                         sizeof(ngx_stream_upstream_server_t));
        if (uscf->servers == NULL) {
            return NULL;
        }

        us = ngx_array_push(uscf->servers);
        if (us == NULL) {
            return NULL;
        }

        ngx_memzero(us, sizeof(ngx_stream_upstream_server_t));

        us->addrs = u->addrs;
        us->naddrs = 1;
    }

    // 将新的上游服务器配置添加到主配置中
    uscfp = ngx_array_push(&umcf->upstreams);
    if (uscfp == NULL) {
        return NULL;
    }

    *uscfp = uscf;

    return uscf;
}


// 创建上游主配置
static void *
ngx_stream_upstream_create_main_conf(ngx_conf_t *cf)
{
    ngx_stream_upstream_main_conf_t  *umcf;

    // 分配内存
    umcf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_upstream_main_conf_t));
    if (umcf == NULL) {
        return NULL;
    }

    // 初始化上游数组
    if (ngx_array_init(&umcf->upstreams, cf->pool, 4,
                       sizeof(ngx_stream_upstream_srv_conf_t *))
        != NGX_OK)
    {
        return NULL;
    }

    return umcf;
}


// 初始化上游主配置
static char *
ngx_stream_upstream_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_stream_upstream_main_conf_t *umcf = conf;

    ngx_uint_t                        i;
    ngx_stream_upstream_init_pt       init;
    ngx_stream_upstream_srv_conf_t  **uscfp;

    uscfp = umcf->upstreams.elts;

    // 遍历所有上游配置
    for (i = 0; i < umcf->upstreams.nelts; i++) {

        // 获取初始化函数，如果没有设置则使用默认的轮询初始化函数
        init = uscfp[i]->peer.init_upstream
                                         ? uscfp[i]->peer.init_upstream
                                         : ngx_stream_upstream_init_round_robin;

        // 调用初始化函数
        if (init(cf, uscfp[i]) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}
