
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_stream_limit_conn_module.c
 *
 * 该模块实现了基于共享内存的连接数限制功能。
 *
 * 支持的功能:
 * 1. 限制特定key的并发连接数
 * 2. 可配置多个限制规则
 * 3. 支持日志记录被拒绝的连接
 * 4. 支持dry run模式
 *
 * 指令:
 * - limit_conn_zone: 设置共享内存区域和key
 * - limit_conn: 设置连接数限制
 * - limit_conn_log_level: 设置日志级别
 * - limit_conn_dry_run: 启用dry run模式
 *
 * 变量:
 * $limit_conn_status: 连接限制状态
 *
 * 使用注意点:
 * 1. 需要先使用limit_conn_zone指令定义共享内存区域
 * 2. limit_conn指令可在stream、server块中使用
 * 3. 当达到限制时，新连接将被拒绝
 * 4. dry run模式下不会实际拒绝连接，仅用于测试
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>


// 定义连接限制状态的常量
#define NGX_STREAM_LIMIT_CONN_PASSED            1  // 连接通过
#define NGX_STREAM_LIMIT_CONN_REJECTED          2  // 连接被拒绝
#define NGX_STREAM_LIMIT_CONN_REJECTED_DRY_RUN  3  // 连接在dry run模式下被拒绝

// 定义红黑树节点结构体
typedef struct {
    u_char                          color;  // 节点颜色
    u_char                          len;    // 键长度
    u_short                         conn;   // 当前连接数
    u_char                          data[1];  // 键数据
} ngx_stream_limit_conn_node_t;

// 定义清理函数的参数结构体
typedef struct {
    ngx_shm_zone_t                 *shm_zone;  // 共享内存区域
    ngx_rbtree_node_t              *node;      // 红黑树节点
} ngx_stream_limit_conn_cleanup_t;

// 定义共享内存上下文结构体
typedef struct {
    ngx_rbtree_t                    rbtree;    // 红黑树
    ngx_rbtree_node_t               sentinel;  // 哨兵节点
} ngx_stream_limit_conn_shctx_t;

// 定义模块上下文结构体
typedef struct {
    ngx_stream_limit_conn_shctx_t  *sh;      // 共享内存上下文
    ngx_slab_pool_t                *shpool;  // 共享内存池
    ngx_stream_complex_value_t      key;     // 键的复杂表达式
} ngx_stream_limit_conn_ctx_t;

// 定义单个连接限制规则结构体
typedef struct {
    ngx_shm_zone_t                 *shm_zone;  // 共享内存区域
    ngx_uint_t                      conn;      // 最大连接数
} ngx_stream_limit_conn_limit_t;

// 定义模块配置结构体
typedef struct {
    ngx_array_t                     limits;     // 连接限制规则数组
    ngx_uint_t                      log_level;  // 日志级别
    ngx_flag_t                      dry_run;    // 是否为dry run模式
} ngx_stream_limit_conn_conf_t;


// 在红黑树中查找指定键的节点
static ngx_rbtree_node_t *ngx_stream_limit_conn_lookup(ngx_rbtree_t *rbtree,
    ngx_str_t *key, uint32_t hash);

// 清理连接限制相关的资源
static void ngx_stream_limit_conn_cleanup(void *data);

// 清理所有连接限制相关的资源
static ngx_inline void ngx_stream_limit_conn_cleanup_all(ngx_pool_t *pool);

// 处理连接限制状态变量
static ngx_int_t ngx_stream_limit_conn_status_variable(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data);

// 创建模块配置结构
static void *ngx_stream_limit_conn_create_conf(ngx_conf_t *cf);

// 合并模块配置
static char *ngx_stream_limit_conn_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);

// 处理limit_conn_zone指令
static char *ngx_stream_limit_conn_zone(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

// 处理limit_conn指令
static char *ngx_stream_limit_conn(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

// 添加模块相关的变量
static ngx_int_t ngx_stream_limit_conn_add_variables(ngx_conf_t *cf);

// 模块初始化
static ngx_int_t ngx_stream_limit_conn_init(ngx_conf_t *cf);


static ngx_conf_enum_t  ngx_stream_limit_conn_log_levels[] = {
    { ngx_string("info"), NGX_LOG_INFO },
    { ngx_string("notice"), NGX_LOG_NOTICE },
    { ngx_string("warn"), NGX_LOG_WARN },
    { ngx_string("error"), NGX_LOG_ERR },
    { ngx_null_string, 0 }
};


static ngx_command_t  ngx_stream_limit_conn_commands[] = {

    // 定义共享内存区域用于存储连接限制信息
    { ngx_string("limit_conn_zone"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE2,
      ngx_stream_limit_conn_zone,
      0,
      0,
      NULL },

    // 设置特定共享内存区域的连接数限制
    { ngx_string("limit_conn"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE2,
      ngx_stream_limit_conn,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    // 设置超出连接限制时的日志级别
    { ngx_string("limit_conn_log_level"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_limit_conn_conf_t, log_level),
      &ngx_stream_limit_conn_log_levels },

    // 设置是否启用连接限制的试运行模式
    { ngx_string("limit_conn_dry_run"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_stream_limit_conn_conf_t, dry_run),
      NULL },

    // 命令数组结束标志
    ngx_null_command
};


static ngx_stream_module_t  ngx_stream_limit_conn_module_ctx = {
    ngx_stream_limit_conn_add_variables,   /* preconfiguration */
    ngx_stream_limit_conn_init,            /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_stream_limit_conn_create_conf,     /* create server configuration */
    ngx_stream_limit_conn_merge_conf       /* merge server configuration */
};


ngx_module_t  ngx_stream_limit_conn_module = {
    NGX_MODULE_V1,
    &ngx_stream_limit_conn_module_ctx,     /* module context */
    ngx_stream_limit_conn_commands,        /* module directives */
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


// 定义连接限制模块的变量数组
static ngx_stream_variable_t  ngx_stream_limit_conn_vars[] = {

    // 定义 limit_conn_status 变量
    { ngx_string("limit_conn_status"), NULL,
      ngx_stream_limit_conn_status_variable, 0, NGX_STREAM_VAR_NOCACHEABLE, 0 },

    // 数组结束标志
    ngx_stream_null_variable
};


// 定义连接限制状态的字符串数组
static ngx_str_t  ngx_stream_limit_conn_status[] = {
    ngx_string("PASSED"),           // 通过
    ngx_string("REJECTED"),         // 拒绝
    ngx_string("REJECTED_DRY_RUN")  // 试运行模式下拒绝
};



static ngx_int_t
ngx_stream_limit_conn_handler(ngx_stream_session_t *s)
{
    size_t                            n;
    uint32_t                          hash;
    ngx_str_t                         key;
    ngx_uint_t                        i;
    ngx_rbtree_node_t                *node;
    ngx_pool_cleanup_t               *cln;
    ngx_stream_limit_conn_ctx_t      *ctx;
    ngx_stream_limit_conn_node_t     *lc;
    ngx_stream_limit_conn_conf_t     *lccf;
    ngx_stream_limit_conn_limit_t    *limits;
    ngx_stream_limit_conn_cleanup_t  *lccln;

    // 获取连接限制模块的服务器配置
    lccf = ngx_stream_get_module_srv_conf(s, ngx_stream_limit_conn_module);
    limits = lccf->limits.elts;

    // 遍历所有限制规则
    for (i = 0; i < lccf->limits.nelts; i++) {
        ctx = limits[i].shm_zone->data;

        // 获取限制键值
        if (ngx_stream_complex_value(s, &ctx->key, &key) != NGX_OK) {
            return NGX_ERROR;
        }

        if (key.len == 0) {
            continue;
        }

        // 检查键值长度
        if (key.len > 255) {
            ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                          "the value of the \"%V\" key "
                          "is more than 255 bytes: \"%V\"",
                          &ctx->key.value, &key);
            continue;
        }

        // 初始化连接状态为通过
        s->limit_conn_status = NGX_STREAM_LIMIT_CONN_PASSED;

        // 计算键值的哈希
        hash = ngx_crc32_short(key.data, key.len);

        // 锁定共享内存
        ngx_shmtx_lock(&ctx->shpool->mutex);

        // 在红黑树中查找节点
        node = ngx_stream_limit_conn_lookup(&ctx->sh->rbtree, &key, hash);

        if (node == NULL) {
            // 如果节点不存在，创建新节点

            // 计算所需内存大小
            n = offsetof(ngx_rbtree_node_t, color)
                + offsetof(ngx_stream_limit_conn_node_t, data)
                + key.len;

            // 分配共享内存
            node = ngx_slab_alloc_locked(ctx->shpool, n);

            if (node == NULL) {
                // 内存分配失败，清理并返回错误
                ngx_shmtx_unlock(&ctx->shpool->mutex);
                ngx_stream_limit_conn_cleanup_all(s->connection->pool);

                if (lccf->dry_run) {
                    s->limit_conn_status =
                                        NGX_STREAM_LIMIT_CONN_REJECTED_DRY_RUN;
                    return NGX_DECLINED;
                }

                s->limit_conn_status = NGX_STREAM_LIMIT_CONN_REJECTED;

                return NGX_STREAM_SERVICE_UNAVAILABLE;
            }

            // 初始化新节点
            lc = (ngx_stream_limit_conn_node_t *) &node->color;

            node->key = hash;
            lc->len = (u_char) key.len;
            lc->conn = 1;
            ngx_memcpy(lc->data, key.data, key.len);

            // 将新节点插入红黑树
            ngx_rbtree_insert(&ctx->sh->rbtree, node);

        } else {
            // 如果节点已存在

            lc = (ngx_stream_limit_conn_node_t *) &node->color;

            // 检查是否超过连接限制
            if ((ngx_uint_t) lc->conn >= limits[i].conn) {
                // 超过限制，记录日志并返回错误
                ngx_shmtx_unlock(&ctx->shpool->mutex);

                ngx_log_error(lccf->log_level, s->connection->log, 0,
                              "limiting connections%s by zone \"%V\"",
                              lccf->dry_run ? ", dry run," : "",
                              &limits[i].shm_zone->shm.name);

                ngx_stream_limit_conn_cleanup_all(s->connection->pool);

                if (lccf->dry_run) {
                    s->limit_conn_status =
                                        NGX_STREAM_LIMIT_CONN_REJECTED_DRY_RUN;
                    return NGX_DECLINED;
                }

                s->limit_conn_status = NGX_STREAM_LIMIT_CONN_REJECTED;

                return NGX_STREAM_SERVICE_UNAVAILABLE;
            }

            // 增加连接计数
            lc->conn++;
        }

        // 记录调试日志
        ngx_log_debug2(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                       "limit conn: %08Xi %d", node->key, lc->conn);

        // 解锁共享内存
        ngx_shmtx_unlock(&ctx->shpool->mutex);

        // 添加清理回调
        cln = ngx_pool_cleanup_add(s->connection->pool,
                                   sizeof(ngx_stream_limit_conn_cleanup_t));
        if (cln == NULL) {
            return NGX_ERROR;
        }

        cln->handler = ngx_stream_limit_conn_cleanup;
        lccln = cln->data;

        lccln->shm_zone = limits[i].shm_zone;
        lccln->node = node;
    }

    // 所有限制检查通过，返回 NGX_DECLINED
    return NGX_DECLINED;
}


static void
ngx_stream_limit_conn_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t             **p;
    ngx_stream_limit_conn_node_t   *lcn, *lcnt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            lcn = (ngx_stream_limit_conn_node_t *) &node->color;
            lcnt = (ngx_stream_limit_conn_node_t *) &temp->color;

            p = (ngx_memn2cmp(lcn->data, lcnt->data, lcn->len, lcnt->len) < 0)
                ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static ngx_rbtree_node_t *
ngx_stream_limit_conn_lookup(ngx_rbtree_t *rbtree, ngx_str_t *key,
    uint32_t hash)
{
    ngx_int_t                      rc;
    ngx_rbtree_node_t             *node, *sentinel;
    ngx_stream_limit_conn_node_t  *lcn;

    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        lcn = (ngx_stream_limit_conn_node_t *) &node->color;

        rc = ngx_memn2cmp(key->data, lcn->data, key->len, (size_t) lcn->len);

        if (rc == 0) {
            return node;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    return NULL;
}


static void
ngx_stream_limit_conn_cleanup(void *data)
{
    ngx_stream_limit_conn_cleanup_t  *lccln = data;

    ngx_rbtree_node_t             *node;
    ngx_stream_limit_conn_ctx_t   *ctx;
    ngx_stream_limit_conn_node_t  *lc;

    ctx = lccln->shm_zone->data;
    node = lccln->node;
    lc = (ngx_stream_limit_conn_node_t *) &node->color;

    ngx_shmtx_lock(&ctx->shpool->mutex);

    ngx_log_debug2(NGX_LOG_DEBUG_STREAM, lccln->shm_zone->shm.log, 0,
                   "limit conn cleanup: %08Xi %d", node->key, lc->conn);

    lc->conn--;

    if (lc->conn == 0) {
        ngx_rbtree_delete(&ctx->sh->rbtree, node);
        ngx_slab_free_locked(ctx->shpool, node);
    }

    ngx_shmtx_unlock(&ctx->shpool->mutex);
}


static ngx_inline void
ngx_stream_limit_conn_cleanup_all(ngx_pool_t *pool)
{
    ngx_pool_cleanup_t  *cln;

    cln = pool->cleanup;

    while (cln && cln->handler == ngx_stream_limit_conn_cleanup) {
        ngx_stream_limit_conn_cleanup(cln->data);
        cln = cln->next;
    }

    pool->cleanup = cln;
}


static ngx_int_t
ngx_stream_limit_conn_init_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_stream_limit_conn_ctx_t  *octx = data;

    size_t                        len;
    ngx_stream_limit_conn_ctx_t  *ctx;

    ctx = shm_zone->data;

    if (octx) {
        if (ctx->key.value.len != octx->key.value.len
            || ngx_strncmp(ctx->key.value.data, octx->key.value.data,
                           ctx->key.value.len)
               != 0)
        {
            ngx_log_error(NGX_LOG_EMERG, shm_zone->shm.log, 0,
                          "limit_conn_zone \"%V\" uses the \"%V\" key "
                          "while previously it used the \"%V\" key",
                          &shm_zone->shm.name, &ctx->key.value,
                          &octx->key.value);
            return NGX_ERROR;
        }

        ctx->sh = octx->sh;
        ctx->shpool = octx->shpool;

        return NGX_OK;
    }

    ctx->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (shm_zone->shm.exists) {
        ctx->sh = ctx->shpool->data;

        return NGX_OK;
    }

    ctx->sh = ngx_slab_alloc(ctx->shpool,
                             sizeof(ngx_stream_limit_conn_shctx_t));
    if (ctx->sh == NULL) {
        return NGX_ERROR;
    }

    ctx->shpool->data = ctx->sh;

    ngx_rbtree_init(&ctx->sh->rbtree, &ctx->sh->sentinel,
                    ngx_stream_limit_conn_rbtree_insert_value);

    len = sizeof(" in limit_conn_zone \"\"") + shm_zone->shm.name.len;

    ctx->shpool->log_ctx = ngx_slab_alloc(ctx->shpool, len);
    if (ctx->shpool->log_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_sprintf(ctx->shpool->log_ctx, " in limit_conn_zone \"%V\"%Z",
                &shm_zone->shm.name);

    return NGX_OK;
}


static ngx_int_t
ngx_stream_limit_conn_status_variable(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data)
{
    if (s->limit_conn_status == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->len = ngx_stream_limit_conn_status[s->limit_conn_status - 1].len;
    v->data = ngx_stream_limit_conn_status[s->limit_conn_status - 1].data;

    return NGX_OK;
}


static void *
ngx_stream_limit_conn_create_conf(ngx_conf_t *cf)
{
    ngx_stream_limit_conn_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_stream_limit_conn_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->limits.elts = NULL;
     */

    conf->log_level = NGX_CONF_UNSET_UINT;
    conf->dry_run = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_stream_limit_conn_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_stream_limit_conn_conf_t *prev = parent;
    ngx_stream_limit_conn_conf_t *conf = child;

    if (conf->limits.elts == NULL) {
        conf->limits = prev->limits;
    }

    ngx_conf_merge_uint_value(conf->log_level, prev->log_level, NGX_LOG_ERR);

    ngx_conf_merge_value(conf->dry_run, prev->dry_run, 0);

    return NGX_CONF_OK;
}


static char *
ngx_stream_limit_conn_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    u_char                              *p;
    ssize_t                              size;
    ngx_str_t                           *value, name, s;
    ngx_uint_t                           i;
    ngx_shm_zone_t                      *shm_zone;
    ngx_stream_limit_conn_ctx_t         *ctx;
    ngx_stream_compile_complex_value_t   ccv;

    value = cf->args->elts;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_stream_limit_conn_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(&ccv, sizeof(ngx_stream_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &ctx->key;

    if (ngx_stream_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    size = 0;
    name.len = 0;

    for (i = 2; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "zone=", 5) == 0) {

            name.data = value[i].data + 5;

            p = (u_char *) ngx_strchr(name.data, ':');

            if (p == NULL) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid zone size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            name.len = p - name.data;

            s.data = p + 1;
            s.len = value[i].data + value[i].len - s.data;

            size = ngx_parse_size(&s);

            if (size == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid zone size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            if (size < (ssize_t) (8 * ngx_pagesize)) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "zone \"%V\" is too small", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    if (name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"zone\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    shm_zone = ngx_shared_memory_add(cf, &name, size,
                                     &ngx_stream_limit_conn_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (shm_zone->data) {
        ctx = shm_zone->data;

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "%V \"%V\" is already bound to key \"%V\"",
                           &cmd->name, &name, &ctx->key.value);
        return NGX_CONF_ERROR;
    }

    shm_zone->init = ngx_stream_limit_conn_init_zone;
    shm_zone->data = ctx;

    return NGX_CONF_OK;
}


static char *
ngx_stream_limit_conn(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_shm_zone_t                 *shm_zone;
    ngx_stream_limit_conn_conf_t   *lccf = conf;
    ngx_stream_limit_conn_limit_t  *limit, *limits;

    ngx_str_t   *value;
    ngx_int_t    n;
    ngx_uint_t   i;

    value = cf->args->elts;

    shm_zone = ngx_shared_memory_add(cf, &value[1], 0,
                                     &ngx_stream_limit_conn_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    limits = lccf->limits.elts;

    if (limits == NULL) {
        if (ngx_array_init(&lccf->limits, cf->pool, 1,
                           sizeof(ngx_stream_limit_conn_limit_t))
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    for (i = 0; i < lccf->limits.nelts; i++) {
        if (shm_zone == limits[i].shm_zone) {
            return "is duplicate";
        }
    }

    n = ngx_atoi(value[2].data, value[2].len);
    if (n <= 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid number of connections \"%V\"", &value[2]);
        return NGX_CONF_ERROR;
    }

    if (n > 65535) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "connection limit must be less 65536");
        return NGX_CONF_ERROR;
    }

    limit = ngx_array_push(&lccf->limits);
    if (limit == NULL) {
        return NGX_CONF_ERROR;
    }

    limit->conn = n;
    limit->shm_zone = shm_zone;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_stream_limit_conn_add_variables(ngx_conf_t *cf)
{
    ngx_stream_variable_t  *var, *v;

    for (v = ngx_stream_limit_conn_vars; v->name.len; v++) {
        var = ngx_stream_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_stream_limit_conn_init(ngx_conf_t *cf)
{
    ngx_stream_handler_pt        *h;
    ngx_stream_core_main_conf_t  *cmcf;

    cmcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_STREAM_PREACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_stream_limit_conn_handler;

    return NGX_OK;
}
