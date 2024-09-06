
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


/*
 * ngx_http_proxy_module.c
 *
 * 该模块实现了反向代理功能，允许Nginx将请求转发到后端服务器。
 *
 * 主要功能:
 * - 将客户端请求转发到上游服务器
 * - 支持HTTP、HTTPS、FastCGI、uwsgi、SCGI等协议
 * - 支持负载均衡
 * - 支持缓存上游响应
 * - 支持SSL/TLS连接到上游服务器
 * - 支持请求体缓冲
 * - 支持响应体缓冲
 * - 支持修改请求头和响应头
 * - 支持设置连接超时和读写超时
 *
 * 支持的指令:
 * - proxy_pass: 设置代理服务器的协议和地址
 * - proxy_redirect: 修改被代理服务器返回的响应头中的Location和Refresh
 * - proxy_set_header: 重新定义或添加字段到传递给代理服务器的请求头
 * - proxy_hide_header: 从代理服务器的响应中隐藏某些头
 * - proxy_pass_header: 允许将被屏蔽的头传递给客户端
 * - proxy_ignore_headers: 禁止处理来自代理服务器的某些响应头
 * - proxy_connect_timeout: 设置与代理服务器建立连接的超时时间
 * - proxy_read_timeout: 设置从代理服务器读取响应的超时时间
 * - proxy_send_timeout: 设置向代理服务器传送请求的超时时间
 * - proxy_buffer_size: 设置用于读取代理服务器响应的第一部分的缓冲区大小
 * - proxy_buffers: 设置用于读取代理服务器响应的缓冲区的数量和大小
 * - proxy_busy_buffers_size: 限制在响应过程中缓冲区的大小
 * - proxy_temp_file_write_size: 设置临时文件的大小
 * - proxy_next_upstream: 指定在何种情况下请求应该传递到下一个服务器
 * - proxy_ssl_protocols: 启用指定的SSL协议
 * - proxy_ssl_ciphers: 指定启用的密码
 *
 * 提供的变量:
 * - $proxy_host: 代理服务器的名称和端口
 * - $proxy_port: 代理服务器的端口
 * - $proxy_add_x_forwarded_for: 包含客户端的IP地址，附加到"X-Forwarded-For"头
 * - $proxy_internal_body_length: 发送给代理服务器的请求主体长度
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/* Cookie安全标志 */
#define  NGX_HTTP_PROXY_COOKIE_SECURE           0x0001  /* 启用Secure标志 */
#define  NGX_HTTP_PROXY_COOKIE_SECURE_ON        0x0002  /* 强制启用Secure标志 */
#define  NGX_HTTP_PROXY_COOKIE_SECURE_OFF       0x0004  /* 强制禁用Secure标志 */

/* Cookie HttpOnly标志 */
#define  NGX_HTTP_PROXY_COOKIE_HTTPONLY         0x0008  /* 启用HttpOnly标志 */
#define  NGX_HTTP_PROXY_COOKIE_HTTPONLY_ON      0x0010  /* 强制启用HttpOnly标志 */
#define  NGX_HTTP_PROXY_COOKIE_HTTPONLY_OFF     0x0020  /* 强制禁用HttpOnly标志 */

/* Cookie SameSite标志 */
#define  NGX_HTTP_PROXY_COOKIE_SAMESITE         0x0040  /* 启用SameSite标志 */
#define  NGX_HTTP_PROXY_COOKIE_SAMESITE_STRICT  0x0080  /* 设置SameSite为Strict */
#define  NGX_HTTP_PROXY_COOKIE_SAMESITE_LAX     0x0100  /* 设置SameSite为Lax */
#define  NGX_HTTP_PROXY_COOKIE_SAMESITE_NONE    0x0200  /* 设置SameSite为None */
#define  NGX_HTTP_PROXY_COOKIE_SAMESITE_OFF     0x0400  /* 禁用SameSite标志 */


/* 代理模块的主配置结构体 */
typedef struct {
    ngx_array_t                    caches;  /* 文件缓存数组，类型为 ngx_http_file_cache_t * */
} ngx_http_proxy_main_conf_t;


/* 前向声明代理重写结构体 */
typedef struct ngx_http_proxy_rewrite_s  ngx_http_proxy_rewrite_t;

/* 代理重写处理函数指针类型定义 */
typedef ngx_int_t (*ngx_http_proxy_rewrite_pt)(ngx_http_request_t *r,
    ngx_str_t *value, size_t prefix, size_t len,
    ngx_http_proxy_rewrite_t *pr);

/* 代理重写结构体定义 */
struct ngx_http_proxy_rewrite_s {
    ngx_http_proxy_rewrite_pt      handler;  /* 重写处理函数 */

    union {
        ngx_http_complex_value_t   complex;  /* 复杂值表达式 */
#if (NGX_PCRE)
        ngx_http_regex_t          *regex;    /* 正则表达式 */
#endif
    } pattern;  /* 匹配模式，可以是复杂值或正则表达式 */

    ngx_http_complex_value_t       replacement;  /* 替换值 */
};


/* 定义代理Cookie标志结构体 */
typedef struct {
    union {
        ngx_http_complex_value_t   complex;  /* 复杂值表达式 */
#if (NGX_PCRE)
        ngx_http_regex_t          *regex;    /* 正则表达式 */
#endif
    } cookie;  /* Cookie匹配模式 */

    ngx_array_t                    flags_values;  /* 标志值数组 */
    ngx_uint_t                     regex;         /* 是否使用正则表达式的标志 */
} ngx_http_proxy_cookie_flags_t;


/* 定义代理变量结构体 */
typedef struct {
    ngx_str_t                      key_start;    /* 键的起始部分 */
    ngx_str_t                      schema;       /* 协议模式 */
    ngx_str_t                      host_header;  /* 主机头 */
    ngx_str_t                      port;         /* 端口 */
    ngx_str_t                      uri;          /* URI */
} ngx_http_proxy_vars_t;


/* 定义代理头部结构体 */
typedef struct {
    ngx_array_t                   *flushes;  /* 刷新操作数组 */
    ngx_array_t                   *lengths;  /* 长度数组 */
    ngx_array_t                   *values;   /* 值数组 */
    ngx_hash_t                     hash;     /* 哈希表 */
} ngx_http_proxy_headers_t;


/* 定义代理位置配置结构体 */
typedef struct {
    ngx_http_upstream_conf_t       upstream;  /* 上游配置 */

    ngx_array_t                   *body_flushes;  /* 请求体刷新操作数组 */
    ngx_array_t                   *body_lengths;  /* 请求体长度数组 */
    ngx_array_t                   *body_values;   /* 请求体值数组 */
    ngx_str_t                      body_source;   /* 请求体源 */

    ngx_http_proxy_headers_t       headers;       /* 代理头部 */
#if (NGX_HTTP_CACHE)
    ngx_http_proxy_headers_t       headers_cache; /* 缓存头部 */
#endif
    ngx_array_t                   *headers_source;  /* 头部源数组 */

    ngx_array_t                   *proxy_lengths;   /* 代理长度数组 */
    ngx_array_t                   *proxy_values;    /* 代理值数组 */

    ngx_array_t                   *redirects;       /* 重定向数组 */
    ngx_array_t                   *cookie_domains;  /* Cookie域名数组 */
    ngx_array_t                   *cookie_paths;    /* Cookie路径数组 */
    ngx_array_t                   *cookie_flags;    /* Cookie标志数组 */

    ngx_http_complex_value_t      *method;          /* HTTP方法 */
    ngx_str_t                      location;        /* 位置 */
    ngx_str_t                      url;             /* URL */

#if (NGX_HTTP_CACHE)
    ngx_http_complex_value_t       cache_key;       /* 缓存键 */
#endif

    ngx_http_proxy_vars_t          vars;            /* 代理变量 */

    ngx_flag_t                     redirect;        /* 重定向标志 */

    ngx_uint_t                     http_version;    /* HTTP版本 */

    ngx_uint_t                     headers_hash_max_size;    /* 头部哈希最大大小 */
    ngx_uint_t                     headers_hash_bucket_size; /* 头部哈希桶大小 */

#if (NGX_HTTP_SSL)
    ngx_uint_t                     ssl;                      /* SSL标志 */
    ngx_uint_t                     ssl_protocols;            /* SSL协议 */
    ngx_str_t                      ssl_ciphers;              /* SSL加密套件 */
    ngx_uint_t                     ssl_verify_depth;         /* SSL验证深度 */
    ngx_str_t                      ssl_trusted_certificate;  /* SSL信任证书 */
    ngx_str_t                      ssl_crl;                  /* SSL证书吊销列表 */
    ngx_array_t                   *ssl_conf_commands;        /* SSL配置命令 */
#endif
} ngx_http_proxy_loc_conf_t;

/**
 * @brief 代理模块的上下文结构体
 */
typedef struct {
    ngx_http_status_t              status;              /* HTTP状态 */
    ngx_http_chunked_t             chunked;             /* 分块传输编码状态 */
    ngx_http_proxy_vars_t          vars;                /* 代理变量 */
    off_t                          internal_body_length; /* 内部请求体长度 */

    ngx_chain_t                   *free;                /* 空闲缓冲区链 */
    ngx_chain_t                   *busy;                /* 忙碌缓冲区链 */

    unsigned                       head:1;              /* 是否为HEAD请求 */
    unsigned                       internal_chunked:1;  /* 是否为内部分块传输 */
    unsigned                       header_sent:1;       /* 头部是否已发送 */
} ngx_http_proxy_ctx_t;


/**
 * @brief 评估代理请求
 * @param r 请求结构体
 * @param ctx 代理上下文
 * @param plcf 代理位置配置
 * @return ngx_int_t 评估结果
 */
static ngx_int_t ngx_http_proxy_eval(ngx_http_request_t *r,
    ngx_http_proxy_ctx_t *ctx, ngx_http_proxy_loc_conf_t *plcf);

#if (NGX_HTTP_CACHE)
/**
 * @brief 创建缓存键
 * @param r 请求结构体
 * @return ngx_int_t 创建结果
 */
static ngx_int_t ngx_http_proxy_create_key(ngx_http_request_t *r);
#endif

/**
 * @brief 创建代理请求
 * @param r 请求结构体
 * @return ngx_int_t 创建结果
 */
static ngx_int_t ngx_http_proxy_create_request(ngx_http_request_t *r);

/**
 * @brief 重新初始化代理请求
 * @param r 请求结构体
 * @return ngx_int_t 初始化结果
 */
static ngx_int_t ngx_http_proxy_reinit_request(ngx_http_request_t *r);

/**
 * @brief 代理请求体输出过滤器
 * @param data 数据指针
 * @param in 输入链
 * @return ngx_int_t 过滤结果
 */
static ngx_int_t ngx_http_proxy_body_output_filter(void *data, ngx_chain_t *in);

/**
 * @brief 处理代理响应状态行
 * @param r 请求结构体
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_process_status_line(ngx_http_request_t *r);

/**
 * @brief 处理代理响应头
 * @param r 请求结构体
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_process_header(ngx_http_request_t *r);

/**
 * @brief 初始化代理输入过滤器
 * @param data 数据指针
 * @return ngx_int_t 初始化结果
 */
static ngx_int_t ngx_http_proxy_input_filter_init(void *data);

/**
 * @brief 代理复制过滤器
 * @param p 事件管道
 * @param buf 缓冲区
 * @return ngx_int_t 过滤结果
 */
static ngx_int_t ngx_http_proxy_copy_filter(ngx_event_pipe_t *p,
    ngx_buf_t *buf);

/**
 * @brief 代理分块过滤器
 * @param p 事件管道
 * @param buf 缓冲区
 * @return ngx_int_t 过滤结果
 */
static ngx_int_t ngx_http_proxy_chunked_filter(ngx_event_pipe_t *p,
    ngx_buf_t *buf);

/**
 * @brief 非缓冲代理复制过滤器
 * @param data 数据指针
 * @param bytes 字节数
 * @return ngx_int_t 过滤结果
 */
static ngx_int_t ngx_http_proxy_non_buffered_copy_filter(void *data,
    ssize_t bytes);

/**
 * @brief 非缓冲代理分块过滤器
 * @param data 数据指针
 * @param bytes 字节数
 * @return ngx_int_t 过滤结果
 */
static ngx_int_t ngx_http_proxy_non_buffered_chunked_filter(void *data,
    ssize_t bytes);

/**
 * @brief 中止代理请求
 * @param r 请求结构体
 */
static void ngx_http_proxy_abort_request(ngx_http_request_t *r);

/**
 * @brief 完成代理请求
 * @param r 请求结构体
 * @param rc 返回码
 */
static void ngx_http_proxy_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc);

/**
 * @brief 处理代理主机变量
 * @param r 请求结构体
 * @param v 变量值结构体
 * @param data 附加数据
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_host_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

/**
 * @brief 处理代理端口变量
 * @param r 请求结构体
 * @param v 变量值结构体
 * @param data 附加数据
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_port_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

/**
 * @brief 添加X-Forwarded-For变量
 * @param r 请求结构体
 * @param v 变量值结构体
 * @param data 附加数据
 * @return ngx_int_t 处理结果
 */
static ngx_int_t
    ngx_http_proxy_add_x_forwarded_for_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

/**
 * @brief 处理内部请求体长度变量
 * @param r 请求结构体
 * @param v 变量值结构体
 * @param data 附加数据
 * @return ngx_int_t 处理结果
 */
static ngx_int_t
    ngx_http_proxy_internal_body_length_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

/**
 * @brief 处理内部分块传输变量
 * @param r 请求结构体
 * @param v 变量值结构体
 * @param data 附加数据
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_internal_chunked_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

/**
 * @brief 重写重定向URL
 * @param r 请求结构体
 * @param h 头部结构体
 * @param prefix 前缀长度
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_rewrite_redirect(ngx_http_request_t *r,
    ngx_table_elt_t *h, size_t prefix);

/**
 * @brief 重写Cookie
 * @param r 请求结构体
 * @param h 头部结构体
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_rewrite_cookie(ngx_http_request_t *r,
    ngx_table_elt_t *h);

/**
 * @brief 解析Cookie
 * @param value Cookie值
 * @param attrs 属性数组
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_parse_cookie(ngx_str_t *value,
    ngx_array_t *attrs);

/**
 * @brief 重写Cookie值
 * @param r 请求结构体
 * @param value Cookie值
 * @param rewrites 重写规则数组
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_rewrite_cookie_value(ngx_http_request_t *r,
    ngx_str_t *value, ngx_array_t *rewrites);

/**
 * @brief 重写Cookie标志
 * @param r 请求结构体
 * @param attrs 属性数组
 * @param flags 标志数组
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_rewrite_cookie_flags(ngx_http_request_t *r,
    ngx_array_t *attrs, ngx_array_t *flags);

/**
 * @brief 编辑Cookie标志
 * @param r 请求结构体
 * @param attrs 属性数组
 * @param flags 标志
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_edit_cookie_flags(ngx_http_request_t *r,
    ngx_array_t *attrs, ngx_uint_t flags);

/**
 * @brief 执行重写操作
 * @param r 请求结构体
 * @param value 待重写的值
 * @param prefix 前缀长度
 * @param len 长度
 * @param replacement 替换内容
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_rewrite(ngx_http_request_t *r,
    ngx_str_t *value, size_t prefix, size_t len, ngx_str_t *replacement);

/**
 * @brief 添加代理模块的变量
 * @param cf 配置结构体
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_add_variables(ngx_conf_t *cf);

/**
 * @brief 创建代理模块的主配置
 * @param cf 配置结构体
 * @return void* 创建的主配置
 */
static void *ngx_http_proxy_create_main_conf(ngx_conf_t *cf);

/**
 * @brief 创建代理模块的位置配置
 * @param cf 配置结构体
 * @return void* 创建的位置配置
 */
static void *ngx_http_proxy_create_loc_conf(ngx_conf_t *cf);

/**
 * @brief 合并代理模块的位置配置
 * @param cf 配置结构体
 * @param parent 父配置
 * @param child 子配置
 * @return char* 处理结果
 */
static char *ngx_http_proxy_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

/**
 * @brief 初始化代理模块的头部
 * @param cf 配置结构体
 * @param conf 代理位置配置
 * @param headers 头部结构体
 * @param default_headers 默认头部
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_init_headers(ngx_conf_t *cf,
    ngx_http_proxy_loc_conf_t *conf, ngx_http_proxy_headers_t *headers,
    ngx_keyval_t *default_headers);

/**
 * @brief 处理代理传递指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_pass(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 处理代理重定向指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_redirect(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 处理代理Cookie域名指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_cookie_domain(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 处理代理Cookie路径指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_cookie_path(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 处理代理Cookie标志指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_cookie_flags(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 处理代理存储指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_store(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

#if (NGX_HTTP_CACHE)
/**
 * @brief 处理代理缓存指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_cache(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 处理代理缓存键指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_cache_key(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif

#if (NGX_HTTP_SSL)
/**
 * @brief 处理代理SSL密码文件指令
 * @param cf 配置结构体
 * @param cmd 命令结构体
 * @param conf 配置指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_ssl_password_file(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
#endif
/**
 * @brief 检查代理低水位标记设置
 * @param cf 配置结构体
 * @param post 后处理函数指针
 * @param data 数据指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_lowat_check(ngx_conf_t *cf, void *post, void *data);

#if (NGX_HTTP_SSL)
/**
 * @brief 检查代理SSL配置命令
 * @param cf 配置结构体
 * @param post 后处理函数指针
 * @param data 数据指针
 * @return char* 处理结果
 */
static char *ngx_http_proxy_ssl_conf_command_check(ngx_conf_t *cf, void *post,
    void *data);
#endif

/**
 * @brief 处理代理重写正则表达式
 * @param cf 配置结构体
 * @param pr 代理重写结构体
 * @param regex 正则表达式字符串
 * @param caseless 是否忽略大小写
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_rewrite_regex(ngx_conf_t *cf,
    ngx_http_proxy_rewrite_t *pr, ngx_str_t *regex, ngx_uint_t caseless);

#if (NGX_HTTP_SSL)
/**
 * @brief 合并代理SSL配置
 * @param cf 配置结构体
 * @param conf 当前配置
 * @param prev 上一级配置
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_merge_ssl(ngx_conf_t *cf,
    ngx_http_proxy_loc_conf_t *conf, ngx_http_proxy_loc_conf_t *prev);

/**
 * @brief 设置代理SSL配置
 * @param cf 配置结构体
 * @param plcf 代理位置配置结构体
 * @return ngx_int_t 处理结果
 */
static ngx_int_t ngx_http_proxy_set_ssl(ngx_conf_t *cf,
    ngx_http_proxy_loc_conf_t *plcf);
#endif

/**
 * @brief 设置代理变量
 * @param u URL结构体
 * @param v 代理变量结构体
 */
static void ngx_http_proxy_set_vars(ngx_url_t *u, ngx_http_proxy_vars_t *v);


/**
 * @brief 代理低水位标记后处理函数
 */
static ngx_conf_post_t  ngx_http_proxy_lowat_post =
    { ngx_http_proxy_lowat_check };


/**
 * @brief 定义代理模块的next_upstream配置选项的位掩码数组
 *
 * 这个数组用于设置在何种情况下应该尝试下一个上游服务器。
 * 每个元素包含一个字符串和对应的位掩码值。
 */
static ngx_conf_bitmask_t  ngx_http_proxy_next_upstream_masks[] = {
    { ngx_string("error"), NGX_HTTP_UPSTREAM_FT_ERROR },
    { ngx_string("timeout"), NGX_HTTP_UPSTREAM_FT_TIMEOUT },
    { ngx_string("invalid_header"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER },
    { ngx_string("non_idempotent"), NGX_HTTP_UPSTREAM_FT_NON_IDEMPOTENT },
    { ngx_string("http_500"), NGX_HTTP_UPSTREAM_FT_HTTP_500 },
    { ngx_string("http_502"), NGX_HTTP_UPSTREAM_FT_HTTP_502 },
    { ngx_string("http_503"), NGX_HTTP_UPSTREAM_FT_HTTP_503 },
    { ngx_string("http_504"), NGX_HTTP_UPSTREAM_FT_HTTP_504 },
    { ngx_string("http_403"), NGX_HTTP_UPSTREAM_FT_HTTP_403 },
    { ngx_string("http_404"), NGX_HTTP_UPSTREAM_FT_HTTP_404 },
    { ngx_string("http_429"), NGX_HTTP_UPSTREAM_FT_HTTP_429 },
    { ngx_string("updating"), NGX_HTTP_UPSTREAM_FT_UPDATING },
    { ngx_string("off"), NGX_HTTP_UPSTREAM_FT_OFF },
    { ngx_null_string, 0 }
};


#if (NGX_HTTP_SSL)

/**
 * @brief 定义代理模块支持的SSL/TLS协议版本的位掩码数组
 *
 * 这个数组用于配置代理连接时允许使用的SSL/TLS协议版本。
 * 每个元素包含一个协议版本的字符串表示和对应的位掩码值。
 */
static ngx_conf_bitmask_t  ngx_http_proxy_ssl_protocols[] = {
    { ngx_string("SSLv2"), NGX_SSL_SSLv2 },
    { ngx_string("SSLv3"), NGX_SSL_SSLv3 },
    { ngx_string("TLSv1"), NGX_SSL_TLSv1 },
    { ngx_string("TLSv1.1"), NGX_SSL_TLSv1_1 },
    { ngx_string("TLSv1.2"), NGX_SSL_TLSv1_2 },
    { ngx_string("TLSv1.3"), NGX_SSL_TLSv1_3 },
    { ngx_null_string, 0 }
};

static ngx_conf_post_t  ngx_http_proxy_ssl_conf_command_post =
    { ngx_http_proxy_ssl_conf_command_check };

#endif


/**
 * @brief 定义代理模块支持的HTTP版本枚举数组
 *
 * 这个数组用于配置代理连接时使用的HTTP协议版本。
 * 每个元素包含一个HTTP版本的字符串表示和对应的枚举值。
 */
static ngx_conf_enum_t  ngx_http_proxy_http_version[] = {
    { ngx_string("1.0"), NGX_HTTP_VERSION_10 },
    { ngx_string("1.1"), NGX_HTTP_VERSION_11 },
    { ngx_null_string, 0 }
};


/**
 * @brief 定义HTTP代理模块
 *
 * 这是Nginx的HTTP代理模块的主要结构体。
 * 它包含了模块的配置、初始化和请求处理等相关函数。
 * 该模块负责将客户端请求转发到上游服务器，并将响应返回给客户端。
 */
ngx_module_t  ngx_http_proxy_module;


static ngx_command_t  ngx_http_proxy_commands[] = {

    { ngx_string("proxy_pass"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_proxy_redirect,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_cookie_domain"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_proxy_cookie_domain,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_cookie_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_proxy_cookie_path,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_cookie_flags"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1234,
      ngx_http_proxy_cookie_flags,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_store"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_store,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_store_access"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE123,
      ngx_conf_set_access_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.store_access),
      NULL },

    { ngx_string("proxy_buffering"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.buffering),
      NULL },

    { ngx_string("proxy_request_buffering"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.request_buffering),
      NULL },

    { ngx_string("proxy_ignore_client_abort"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ignore_client_abort),
      NULL },

    { ngx_string("proxy_bind"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_upstream_bind_set_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.local),
      NULL },

    { ngx_string("proxy_socket_keepalive"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.socket_keepalive),
      NULL },

    { ngx_string("proxy_connect_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.connect_timeout),
      NULL },

    { ngx_string("proxy_send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.send_timeout),
      NULL },

    { ngx_string("proxy_send_lowat"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.send_lowat),
      &ngx_http_proxy_lowat_post },

    { ngx_string("proxy_intercept_errors"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.intercept_errors),
      NULL },

    { ngx_string("proxy_set_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_keyval_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, headers_source),
      NULL },

    { ngx_string("proxy_headers_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, headers_hash_max_size),
      NULL },

    { ngx_string("proxy_headers_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, headers_hash_bucket_size),
      NULL },

    { ngx_string("proxy_set_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, body_source),
      NULL },

    { ngx_string("proxy_method"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, method),
      NULL },

    { ngx_string("proxy_pass_request_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.pass_request_headers),
      NULL },

    { ngx_string("proxy_pass_request_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.pass_request_body),
      NULL },

    { ngx_string("proxy_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.buffer_size),
      NULL },

    { ngx_string("proxy_read_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.read_timeout),
      NULL },

    { ngx_string("proxy_buffers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.bufs),
      NULL },

    { ngx_string("proxy_busy_buffers_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.busy_buffers_size_conf),
      NULL },

    { ngx_string("proxy_force_ranges"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.force_ranges),
      NULL },

    { ngx_string("proxy_limit_rate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.limit_rate),
      NULL },

#if (NGX_HTTP_CACHE)

    { ngx_string("proxy_cache"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_cache,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_cache_key"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_cache_key,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_cache_path"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_2MORE,
      ngx_http_file_cache_set_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_proxy_main_conf_t, caches),
      &ngx_http_proxy_module },

    { ngx_string("proxy_cache_bypass"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_set_predicate_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_bypass),
      NULL },

    { ngx_string("proxy_no_cache"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_set_predicate_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.no_cache),
      NULL },

    { ngx_string("proxy_cache_valid"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_file_cache_valid_set_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_valid),
      NULL },

    { ngx_string("proxy_cache_min_uses"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_min_uses),
      NULL },

    { ngx_string("proxy_cache_max_range_offset"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_off_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_max_range_offset),
      NULL },

    { ngx_string("proxy_cache_use_stale"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_use_stale),
      &ngx_http_proxy_next_upstream_masks },

    { ngx_string("proxy_cache_methods"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_methods),
      &ngx_http_upstream_cache_method_mask },

    { ngx_string("proxy_cache_lock"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_lock),
      NULL },

    { ngx_string("proxy_cache_lock_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_lock_timeout),
      NULL },

    { ngx_string("proxy_cache_lock_age"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_lock_age),
      NULL },

    { ngx_string("proxy_cache_revalidate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_revalidate),
      NULL },

    { ngx_string("proxy_cache_convert_head"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_convert_head),
      NULL },

    { ngx_string("proxy_cache_background_update"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_background_update),
      NULL },

#endif

    { ngx_string("proxy_temp_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1234,
      ngx_conf_set_path_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.temp_path),
      NULL },

    { ngx_string("proxy_max_temp_file_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.max_temp_file_size_conf),
      NULL },

    { ngx_string("proxy_temp_file_write_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.temp_file_write_size_conf),
      NULL },

    { ngx_string("proxy_next_upstream"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.next_upstream),
      &ngx_http_proxy_next_upstream_masks },

    { ngx_string("proxy_next_upstream_tries"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.next_upstream_tries),
      NULL },

    { ngx_string("proxy_next_upstream_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.next_upstream_timeout),
      NULL },

    { ngx_string("proxy_pass_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.pass_headers),
      NULL },

    { ngx_string("proxy_hide_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.hide_headers),
      NULL },

    { ngx_string("proxy_ignore_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ignore_headers),
      &ngx_http_upstream_ignore_headers_masks },

    { ngx_string("proxy_http_version"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, http_version),
      &ngx_http_proxy_http_version },

#if (NGX_HTTP_SSL)

    { ngx_string("proxy_ssl_session_reuse"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_session_reuse),
      NULL },

    { ngx_string("proxy_ssl_protocols"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_protocols),
      &ngx_http_proxy_ssl_protocols },

    { ngx_string("proxy_ssl_ciphers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_ciphers),
      NULL },

    { ngx_string("proxy_ssl_name"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_name),
      NULL },

    { ngx_string("proxy_ssl_server_name"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_server_name),
      NULL },

    { ngx_string("proxy_ssl_verify"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_verify),
      NULL },

    { ngx_string("proxy_ssl_verify_depth"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_verify_depth),
      NULL },

    { ngx_string("proxy_ssl_trusted_certificate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_trusted_certificate),
      NULL },

    { ngx_string("proxy_ssl_crl"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_crl),
      NULL },

    { ngx_string("proxy_ssl_certificate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_zero_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_certificate),
      NULL },

    { ngx_string("proxy_ssl_certificate_key"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_zero_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_certificate_key),
      NULL },

    { ngx_string("proxy_ssl_password_file"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_ssl_password_file,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_ssl_conf_command"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_keyval_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_conf_commands),
      &ngx_http_proxy_ssl_conf_command_post },

#endif

      ngx_null_command
};


static ngx_http_module_t  ngx_http_proxy_module_ctx = {
    ngx_http_proxy_add_variables,          /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_proxy_create_main_conf,       /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_proxy_create_loc_conf,        /* create location configuration */
    ngx_http_proxy_merge_loc_conf          /* merge location configuration */
};


ngx_module_t  ngx_http_proxy_module = {
    NGX_MODULE_V1,
    &ngx_http_proxy_module_ctx,            /* module context */
    ngx_http_proxy_commands,               /* module directives */
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


static char  ngx_http_proxy_version[] = " HTTP/1.0" CRLF;
static char  ngx_http_proxy_version_11[] = " HTTP/1.1" CRLF;


static ngx_keyval_t  ngx_http_proxy_headers[] = {
    { ngx_string("Host"), ngx_string("$proxy_host") },
    { ngx_string("Connection"), ngx_string("close") },
    { ngx_string("Content-Length"), ngx_string("$proxy_internal_body_length") },
    { ngx_string("Transfer-Encoding"), ngx_string("$proxy_internal_chunked") },
    { ngx_string("TE"), ngx_string("") },
    { ngx_string("Keep-Alive"), ngx_string("") },
    { ngx_string("Expect"), ngx_string("") },
    { ngx_string("Upgrade"), ngx_string("") },
    { ngx_null_string, ngx_null_string }
};


static ngx_str_t  ngx_http_proxy_hide_headers[] = {
    ngx_string("Date"),
    ngx_string("Server"),
    ngx_string("X-Pad"),
    ngx_string("X-Accel-Expires"),
    ngx_string("X-Accel-Redirect"),
    ngx_string("X-Accel-Limit-Rate"),
    ngx_string("X-Accel-Buffering"),
    ngx_string("X-Accel-Charset"),
    ngx_null_string
};


#if (NGX_HTTP_CACHE)

/**
 * @brief 定义代理缓存头部数组
 *
 * 这个数组包含了在处理缓存请求时需要设置或修改的HTTP头部。
 * 每个元素都是一个键值对，键是头部名称，值是对应的变量或固定值。
 */
static ngx_keyval_t  ngx_http_proxy_cache_headers[] = {
    { ngx_string("Host"), ngx_string("$proxy_host") },
    { ngx_string("Connection"), ngx_string("close") },
    { ngx_string("Content-Length"), ngx_string("$proxy_internal_body_length") },
    { ngx_string("Transfer-Encoding"), ngx_string("$proxy_internal_chunked") },
    { ngx_string("TE"), ngx_string("") },
    { ngx_string("Keep-Alive"), ngx_string("") },
    { ngx_string("Expect"), ngx_string("") },
    { ngx_string("Upgrade"), ngx_string("") },
    { ngx_string("If-Modified-Since"),
      ngx_string("$upstream_cache_last_modified") },
    { ngx_string("If-Unmodified-Since"), ngx_string("") },
    { ngx_string("If-None-Match"), ngx_string("$upstream_cache_etag") },
    { ngx_string("If-Match"), ngx_string("") },
    { ngx_string("Range"), ngx_string("") },
    { ngx_string("If-Range"), ngx_string("") },
    { ngx_null_string, ngx_null_string }
};

#endif


/* 定义代理模块的变量数组 */
static ngx_http_variable_t  ngx_http_proxy_vars[] = {

    { ngx_string("proxy_host"), NULL, ngx_http_proxy_host_variable, 0,
      NGX_HTTP_VAR_CHANGEABLE|NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("proxy_port"), NULL, ngx_http_proxy_port_variable, 0,
      NGX_HTTP_VAR_CHANGEABLE|NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("proxy_add_x_forwarded_for"), NULL,
      ngx_http_proxy_add_x_forwarded_for_variable, 0, NGX_HTTP_VAR_NOHASH, 0 },

#if 0
    { ngx_string("proxy_add_via"), NULL, NULL, 0, NGX_HTTP_VAR_NOHASH, 0 },
#endif

    { ngx_string("proxy_internal_body_length"), NULL,
      ngx_http_proxy_internal_body_length_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("proxy_internal_chunked"), NULL,
      ngx_http_proxy_internal_chunked_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

      ngx_http_null_variable
};


/* 定义代理模块的临时文件路径初始化结构 */
static ngx_path_init_t  ngx_http_proxy_temp_path = {
    ngx_string(NGX_HTTP_PROXY_TEMP_PATH), { 1, 2, 0 }
};


/* 定义代理模块的 cookie 标志掩码数组 */
static ngx_conf_bitmask_t  ngx_http_proxy_cookie_flags_masks[] = {

    { ngx_string("secure"),
      NGX_HTTP_PROXY_COOKIE_SECURE|NGX_HTTP_PROXY_COOKIE_SECURE_ON },

    { ngx_string("nosecure"),
      NGX_HTTP_PROXY_COOKIE_SECURE|NGX_HTTP_PROXY_COOKIE_SECURE_OFF },

    { ngx_string("httponly"),
      NGX_HTTP_PROXY_COOKIE_HTTPONLY|NGX_HTTP_PROXY_COOKIE_HTTPONLY_ON },

    { ngx_string("nohttponly"),
      NGX_HTTP_PROXY_COOKIE_HTTPONLY|NGX_HTTP_PROXY_COOKIE_HTTPONLY_OFF },

    { ngx_string("samesite=strict"),
      NGX_HTTP_PROXY_COOKIE_SAMESITE|NGX_HTTP_PROXY_COOKIE_SAMESITE_STRICT },

    { ngx_string("samesite=lax"),
      NGX_HTTP_PROXY_COOKIE_SAMESITE|NGX_HTTP_PROXY_COOKIE_SAMESITE_LAX },

    { ngx_string("samesite=none"),
      NGX_HTTP_PROXY_COOKIE_SAMESITE|NGX_HTTP_PROXY_COOKIE_SAMESITE_NONE },

    { ngx_string("nosamesite"),
      NGX_HTTP_PROXY_COOKIE_SAMESITE|NGX_HTTP_PROXY_COOKIE_SAMESITE_OFF },

    { ngx_null_string, 0 }
};


/**
 * @brief 代理模块的主处理函数
 * @param r 当前的HTTP请求
 * @return ngx_int_t 处理结果
 */
static ngx_int_t
ngx_http_proxy_handler(ngx_http_request_t *r)
{
    ngx_int_t                    rc;
    ngx_http_upstream_t         *u;
    ngx_http_proxy_ctx_t        *ctx;
    ngx_http_proxy_loc_conf_t   *plcf;
#if (NGX_HTTP_CACHE)
    ngx_http_proxy_main_conf_t  *pmcf;
#endif

    // 创建上游请求
    if (ngx_http_upstream_create(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 分配代理模块上下文
    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_proxy_ctx_t));
    if (ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 设置代理模块上下文
    ngx_http_set_ctx(r, ctx, ngx_http_proxy_module);

    // 获取代理模块的位置配置
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

    u = r->upstream;

    // 设置上游请求的schema和SSL配置
    if (plcf->proxy_lengths == NULL) {
        ctx->vars = plcf->vars;
        u->schema = plcf->vars.schema;
#if (NGX_HTTP_SSL)
        u->ssl = plcf->ssl;
#endif

    } else {
        // 如果代理URL是动态的，则需要评估
        if (ngx_http_proxy_eval(r, ctx, plcf) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    // 设置输出缓冲区的标签
    u->output.tag = (ngx_buf_tag_t) &ngx_http_proxy_module;

    // 设置上游配置
    u->conf = &plcf->upstream;

#if (NGX_HTTP_CACHE)
    // 设置缓存相关配置
    pmcf = ngx_http_get_module_main_conf(r, ngx_http_proxy_module);

    u->caches = &pmcf->caches;
    u->create_key = ngx_http_proxy_create_key;
#endif

    // 设置上游请求的各个处理函数
    u->create_request = ngx_http_proxy_create_request;
    u->reinit_request = ngx_http_proxy_reinit_request;
    u->process_header = ngx_http_proxy_process_status_line;
    u->abort_request = ngx_http_proxy_abort_request;
    u->finalize_request = ngx_http_proxy_finalize_request;
    r->state = 0;

    // 设置重定向处理函数
    if (plcf->redirects) {
        u->rewrite_redirect = ngx_http_proxy_rewrite_redirect;
    }

    // 设置Cookie重写函数
    if (plcf->cookie_domains || plcf->cookie_paths || plcf->cookie_flags) {
        u->rewrite_cookie = ngx_http_proxy_rewrite_cookie;
    }

    // 设置缓冲策略
    u->buffering = plcf->upstream.buffering;

    // 创建管道
    u->pipe = ngx_pcalloc(r->pool, sizeof(ngx_event_pipe_t));
    if (u->pipe == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 设置管道的输入过滤器
    u->pipe->input_filter = ngx_http_proxy_copy_filter;
    u->pipe->input_ctx = r;

    // 设置输入过滤器
    u->input_filter_init = ngx_http_proxy_input_filter_init;
    u->input_filter = ngx_http_proxy_non_buffered_copy_filter;
    u->input_filter_ctx = r;

    // 启用加速
    u->accel = 1;

    // 设置请求体不缓冲的条件
    if (!plcf->upstream.request_buffering
        && plcf->body_values == NULL && plcf->upstream.pass_request_body
        && (!r->headers_in.chunked
            || plcf->http_version == NGX_HTTP_VERSION_11))
    {
        r->request_body_no_buffering = 1;
    }

    // 读取客户端请求体并初始化上游
    rc = ngx_http_read_client_request_body(r, ngx_http_upstream_init);

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return NGX_DONE;
}


static ngx_int_t
ngx_http_proxy_eval(ngx_http_request_t *r, ngx_http_proxy_ctx_t *ctx,
    ngx_http_proxy_loc_conf_t *plcf)
{
    u_char               *p;
    size_t                add;
    u_short               port;
    ngx_str_t             proxy;
    ngx_url_t             url;
    ngx_http_upstream_t  *u;

    // 执行脚本，获取代理URL
    if (ngx_http_script_run(r, &proxy, plcf->proxy_lengths->elts, 0,
                            plcf->proxy_values->elts)
        == NULL)
    {
        return NGX_ERROR;
    }

    // 检查代理URL的协议
    if (proxy.len > 7
        && ngx_strncasecmp(proxy.data, (u_char *) "http://", 7) == 0)
    {
        add = 7;
        port = 80;

#if (NGX_HTTP_SSL)

    } else if (proxy.len > 8
               && ngx_strncasecmp(proxy.data, (u_char *) "https://", 8) == 0)
    {
        add = 8;
        port = 443;
        r->upstream->ssl = 1;

#endif

    } else {
        // 无效的URL前缀
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "invalid URL prefix in \"%V\"", &proxy);
        return NGX_ERROR;
    }

    u = r->upstream;

    // 设置上游的schema
    u->schema.len = add;
    u->schema.data = proxy.data;

    ngx_memzero(&url, sizeof(ngx_url_t));

    // 设置URL解析参数
    url.url.len = proxy.len - add;
    url.url.data = proxy.data + add;
    url.default_port = port;
    url.uri_part = 1;
    url.no_resolve = 1;

    // 解析URL
    if (ngx_parse_url(r->pool, &url) != NGX_OK) {
        if (url.err) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "%s in upstream \"%V\"", url.err, &url.url);
        }

        return NGX_ERROR;
    }

    // 处理URI
    if (url.uri.len) {
        if (url.uri.data[0] == '?') {
            p = ngx_pnalloc(r->pool, url.uri.len + 1);
            if (p == NULL) {
                return NGX_ERROR;
            }

            *p++ = '/';
            ngx_memcpy(p, url.uri.data, url.uri.len);

            url.uri.len++;
            url.uri.data = p - 1;
        }
    }

    // 设置变量
    ctx->vars.key_start = u->schema;

    ngx_http_proxy_set_vars(&url, &ctx->vars);

    // 分配并初始化resolved结构
    u->resolved = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_resolved_t));
    if (u->resolved == NULL) {
        return NGX_ERROR;
    }

    // 设置resolved的地址信息
    if (url.addrs) {
        u->resolved->sockaddr = url.addrs[0].sockaddr;
        u->resolved->socklen = url.addrs[0].socklen;
        u->resolved->name = url.addrs[0].name;
        u->resolved->naddrs = 1;
    }

    // 设置resolved的主机和端口信息
    u->resolved->host = url.host;
    u->resolved->port = (in_port_t) (url.no_port ? port : url.port);
    u->resolved->no_port = url.no_port;

    return NGX_OK;
}


#if (NGX_HTTP_CACHE)

static ngx_int_t
ngx_http_proxy_create_key(ngx_http_request_t *r)
{
    size_t                      len, loc_len;
    u_char                     *p;
    uintptr_t                   escape;
    ngx_str_t                  *key;
    ngx_http_upstream_t        *u;
    ngx_http_proxy_ctx_t       *ctx;
    ngx_http_proxy_loc_conf_t  *plcf;

    u = r->upstream;

    // 获取代理模块的位置配置
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

    // 获取代理模块的上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    // 为缓存键数组添加一个新元素
    key = ngx_array_push(&r->cache->keys);
    if (key == NULL) {
        return NGX_ERROR;
    }

    // 如果配置了自定义缓存键
    if (plcf->cache_key.value.data) {
        // 计算复杂值作为缓存键
        if (ngx_http_complex_value(r, &plcf->cache_key, key) != NGX_OK) {
            return NGX_ERROR;
        }

        return NGX_OK;
    }

    // 使用默认的键起始部分
    *key = ctx->vars.key_start;

    // 为缓存键数组添加另一个元素
    key = ngx_array_push(&r->cache->keys);
    if (key == NULL) {
        return NGX_ERROR;
    }

    // 如果配置了代理长度且URI不为空
    if (plcf->proxy_lengths && ctx->vars.uri.len) {
        *key = ctx->vars.uri;
        u->uri = ctx->vars.uri;
        return NGX_OK;
    } else if (ctx->vars.uri.len == 0 && r->valid_unparsed_uri) {
        // 如果URI为空但有有效的未解析URI
        *key = r->unparsed_uri;
        u->uri = r->unparsed_uri;
        return NGX_OK;
    }

    // 计算位置长度
    loc_len = (r->valid_location && ctx->vars.uri.len) ? plcf->location.len : 0;

    // 计算需要转义的字符数
    if (r->quoted_uri || r->internal) {
        escape = 2 * ngx_escape_uri(NULL, r->uri.data + loc_len,
                                    r->uri.len - loc_len, NGX_ESCAPE_URI);
    } else {
        escape = 0;
    }

    // 计算总长度
    len = ctx->vars.uri.len + r->uri.len - loc_len + escape
          + sizeof("?") - 1 + r->args.len;

    // 分配内存
    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    key->data = p;

    // 复制URI
    if (r->valid_location) {
        p = ngx_copy(p, ctx->vars.uri.data, ctx->vars.uri.len);
    }

    // 处理需要转义的URI部分
    if (escape) {
        ngx_escape_uri(p, r->uri.data + loc_len,
                       r->uri.len - loc_len, NGX_ESCAPE_URI);
        p += r->uri.len - loc_len + escape;
    } else {
        p = ngx_copy(p, r->uri.data + loc_len, r->uri.len - loc_len);
    }

    // 添加参数
    if (r->args.len > 0) {
        *p++ = '?';
        p = ngx_copy(p, r->args.data, r->args.len);
    }

    // 设置键长度和URI
    key->len = p - key->data;
    u->uri = *key;

    return NGX_OK;
}

#endif


/**
 * @brief 创建代理请求
 * @param r HTTP请求
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
static ngx_int_t
ngx_http_proxy_create_request(ngx_http_request_t *r)
{
    size_t                        len, uri_len, loc_len, body_len,
                                  key_len, val_len;
    uintptr_t                     escape;
    ngx_buf_t                    *b;
    ngx_str_t                     method;
    ngx_uint_t                    i, unparsed_uri;
    ngx_chain_t                  *cl, *body;
    ngx_list_part_t              *part;
    ngx_table_elt_t              *header;
    ngx_http_upstream_t          *u;
    ngx_http_proxy_ctx_t         *ctx;
    ngx_http_script_code_pt       code;
    ngx_http_proxy_headers_t     *headers;
    ngx_http_script_engine_t      e, le;
    ngx_http_proxy_loc_conf_t    *plcf;
    ngx_http_script_len_code_pt   lcode;

    u = r->upstream;

    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

#if (NGX_HTTP_CACHE)
    // 根据是否可缓存选择不同的头部配置
    headers = u->cacheable ? &plcf->headers_cache : &plcf->headers;
#else
    headers = &plcf->headers;
#endif

    // 确定请求方法
    if (u->method.len) {
        /* HEAD was changed to GET to cache response */
        method = u->method;
    } else if (plcf->method) {
        if (ngx_http_complex_value(r, plcf->method, &method) != NGX_OK) {
            return NGX_ERROR;
        }
    } else {
        method = r->method_name;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    // 检查是否为HEAD请求
    if (method.len == 4
        && ngx_strncasecmp(method.data, (u_char *) "HEAD", 4) == 0)
    {
        ctx->head = 1;
    }

    // 计算请求行长度
    len = method.len + 1 + sizeof(ngx_http_proxy_version) - 1
          + sizeof(CRLF) - 1;

    escape = 0;
    loc_len = 0;
    unparsed_uri = 0;

    // 确定URI长度
    if (plcf->proxy_lengths && ctx->vars.uri.len) {
        uri_len = ctx->vars.uri.len;
    } else if (ctx->vars.uri.len == 0 && r->valid_unparsed_uri) {
        unparsed_uri = 1;
        uri_len = r->unparsed_uri.len;
    } else {
        loc_len = (r->valid_location && ctx->vars.uri.len) ?
                      plcf->location.len : 0;

        if (r->quoted_uri || r->internal) {
            escape = 2 * ngx_escape_uri(NULL, r->uri.data + loc_len,
                                        r->uri.len - loc_len, NGX_ESCAPE_URI);
        }

        uri_len = ctx->vars.uri.len + r->uri.len - loc_len + escape
                  + sizeof("?") - 1 + r->args.len;
    }

    // 检查URI长度是否为0
    if (uri_len == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "zero length URI to proxy");
        return NGX_ERROR;
    }

    len += uri_len;

    ngx_memzero(&le, sizeof(ngx_http_script_engine_t));

    // 刷新不可缓存的变量
    ngx_http_script_flush_no_cacheable_variables(r, plcf->body_flushes);
    ngx_http_script_flush_no_cacheable_variables(r, headers->flushes);

    // 计算请求体长度
    if (plcf->body_lengths) {
        le.ip = plcf->body_lengths->elts;
        le.request = r;
        le.flushed = 1;
        body_len = 0;

        while (*(uintptr_t *) le.ip) {
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            body_len += lcode(&le);
        }

        ctx->internal_body_length = body_len;
        len += body_len;

    } else if (r->headers_in.chunked && r->reading_body) {
        ctx->internal_body_length = -1;
        ctx->internal_chunked = 1;

    } else {
        ctx->internal_body_length = r->headers_in.content_length_n;
    }

    // 计算头部长度
    le.ip = headers->lengths->elts;
    le.request = r;
    le.flushed = 1;

    while (*(uintptr_t *) le.ip) {
        lcode = *(ngx_http_script_len_code_pt *) le.ip;
        key_len = lcode(&le);

        for (val_len = 0; *(uintptr_t *) le.ip; val_len += lcode(&le)) {
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
        }
        le.ip += sizeof(uintptr_t);

        if (val_len == 0) {
            continue;
        }

        len += key_len + sizeof(": ") - 1 + val_len + sizeof(CRLF) - 1;
    }

    // 如果需要传递请求头，计算额外的头部长度
    if (plcf->upstream.pass_request_headers) {
        part = &r->headers_in.headers.part;
        header = part->elts;

        for (i = 0; /* void */; i++) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                header = part->elts;
                i = 0;
            }

            if (ngx_hash_find(&headers->hash, header[i].hash,
                              header[i].lowcase_key, header[i].key.len))
            {
                continue;
            }

            len += header[i].key.len + sizeof(": ") - 1
                + header[i].value.len + sizeof(CRLF) - 1;
        }
    }

    // 创建临时缓冲区
    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;

    // 构建请求行
    b->last = ngx_copy(b->last, method.data, method.len);
    *b->last++ = ' ';

    u->uri.data = b->last;

    // 构建URI
    if (plcf->proxy_lengths && ctx->vars.uri.len) {
        b->last = ngx_copy(b->last, ctx->vars.uri.data, ctx->vars.uri.len);
    } else if (unparsed_uri) {
        b->last = ngx_copy(b->last, r->unparsed_uri.data, r->unparsed_uri.len);
    } else {
        if (r->valid_location) {
            b->last = ngx_copy(b->last, ctx->vars.uri.data, ctx->vars.uri.len);
        }

        if (escape) {
            ngx_escape_uri(b->last, r->uri.data + loc_len,
                           r->uri.len - loc_len, NGX_ESCAPE_URI);
            b->last += r->uri.len - loc_len + escape;
        } else {
            b->last = ngx_copy(b->last, r->uri.data + loc_len,
                               r->uri.len - loc_len);
        }

        if (r->args.len > 0) {
            *b->last++ = '?';
            b->last = ngx_copy(b->last, r->args.data, r->args.len);
        }
    }

    u->uri.len = b->last - u->uri.data;

    // 添加HTTP版本
    if (plcf->http_version == NGX_HTTP_VERSION_11) {
        b->last = ngx_cpymem(b->last, ngx_http_proxy_version_11,
                             sizeof(ngx_http_proxy_version_11) - 1);
    } else {
        b->last = ngx_cpymem(b->last, ngx_http_proxy_version,
                             sizeof(ngx_http_proxy_version) - 1);
    }

    ngx_memzero(&e, sizeof(ngx_http_script_engine_t));

    e.ip = headers->values->elts;
    e.pos = b->last;
    e.request = r;
    e.flushed = 1;

    le.ip = headers->lengths->elts;

    // 构建请求头
    while (*(uintptr_t *) le.ip) {
        lcode = *(ngx_http_script_len_code_pt *) le.ip;
        (void) lcode(&le);

        for (val_len = 0; *(uintptr_t *) le.ip; val_len += lcode(&le)) {
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
        }
        le.ip += sizeof(uintptr_t);

        if (val_len == 0) {
            e.skip = 1;

            while (*(uintptr_t *) e.ip) {
                code = *(ngx_http_script_code_pt *) e.ip;
                code((ngx_http_script_engine_t *) &e);
            }
            e.ip += sizeof(uintptr_t);

            e.skip = 0;

            continue;
        }

        code = *(ngx_http_script_code_pt *) e.ip;
        code((ngx_http_script_engine_t *) &e);

        *e.pos++ = ':'; *e.pos++ = ' ';

        while (*(uintptr_t *) e.ip) {
            code = *(ngx_http_script_code_pt *) e.ip;
            code((ngx_http_script_engine_t *) &e);
        }
        e.ip += sizeof(uintptr_t);

        *e.pos++ = CR; *e.pos++ = LF;
    }

    b->last = e.pos;

    // 如果需要传递请求头，添加额外的头部
    if (plcf->upstream.pass_request_headers) {
        part = &r->headers_in.headers.part;
        header = part->elts;

        for (i = 0; /* void */; i++) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                header = part->elts;
                i = 0;
            }

            if (ngx_hash_find(&headers->hash, header[i].hash,
                              header[i].lowcase_key, header[i].key.len))
            {
                continue;
            }

            b->last = ngx_copy(b->last, header[i].key.data, header[i].key.len);

            *b->last++ = ':'; *b->last++ = ' ';

            b->last = ngx_copy(b->last, header[i].value.data,
                               header[i].value.len);

            *b->last++ = CR; *b->last++ = LF;

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy header: \"%V: %V\"",
                           &header[i].key, &header[i].value);
        }
    }

    // 添加头部结束标记
    *b->last++ = CR; *b->last++ = LF;

    // 处理请求体
    if (plcf->body_values) {
        e.ip = plcf->body_values->elts;
        e.pos = b->last;
        e.skip = 0;

        while (*(uintptr_t *) e.ip) {
            code = *(ngx_http_script_code_pt *) e.ip;
            code((ngx_http_script_engine_t *) &e);
        }

        b->last = e.pos;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http proxy header:%N\"%*s\"",
                   (size_t) (b->last - b->pos), b->pos);

    // 处理请求体缓冲
    if (r->request_body_no_buffering) {
        u->request_bufs = cl;

        if (ctx->internal_chunked) {
            u->output.output_filter = ngx_http_proxy_body_output_filter;
            u->output.filter_ctx = r;
        }
    } else if (plcf->body_values == NULL && plcf->upstream.pass_request_body) {
        body = u->request_bufs;
        u->request_bufs = cl;

        while (body) {
            b = ngx_alloc_buf(r->pool);
            if (b == NULL) {
                return NGX_ERROR;
            }

            ngx_memcpy(b, body->buf, sizeof(ngx_buf_t));

            cl->next = ngx_alloc_chain_link(r->pool);
            if (cl->next == NULL) {
                return NGX_ERROR;
            }

            cl = cl->next;
            cl->buf = b;

            body = body->next;
        }
    } else {
        u->request_bufs = cl;
    }

    b->flush = 1;
    cl->next = NULL;

    return NGX_OK;
}


/**
 * @brief 重新初始化代理请求
 * @param r HTTP请求
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
static ngx_int_t
ngx_http_proxy_reinit_request(ngx_http_request_t *r)
{
    ngx_http_proxy_ctx_t  *ctx;

    // 获取代理模块的上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        return NGX_OK;
    }

    // 重置状态相关的变量
    ctx->status.code = 0;
    ctx->status.count = 0;
    ctx->status.start = NULL;
    ctx->status.end = NULL;
    ctx->chunked.state = 0;

    // 重新设置上游处理函数
    r->upstream->process_header = ngx_http_proxy_process_status_line;
    r->upstream->pipe->input_filter = ngx_http_proxy_copy_filter;
    r->upstream->input_filter = ngx_http_proxy_non_buffered_copy_filter;
    r->state = 0;

    return NGX_OK;
}


/**
 * @brief 代理请求体输出过滤器
 * @param data 请求对象
 * @param in 输入链
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
static ngx_int_t
ngx_http_proxy_body_output_filter(void *data, ngx_chain_t *in)
{
    ngx_http_request_t  *r = data;

    off_t                  size;
    u_char                *chunk;
    ngx_int_t              rc;
    ngx_buf_t             *b;
    ngx_chain_t           *out, *cl, *tl, **ll, **fl;
    ngx_http_proxy_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "proxy output filter");

    // 获取代理模块的上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (in == NULL) {
        out = in;
        goto out;
    }

    out = NULL;
    ll = &out;

    // 处理首次发送的头部
    if (!ctx->header_sent) {
        /* first buffer contains headers, pass it unmodified */

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "proxy output header");

        ctx->header_sent = 1;

        tl = ngx_alloc_chain_link(r->pool);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        tl->buf = in->buf;
        *ll = tl;
        ll = &tl->next;

        in = in->next;

        if (in == NULL) {
            tl->next = NULL;
            goto out;
        }
    }

    // 计算请求体大小并构建输出链
    size = 0;
    cl = in;
    fl = ll;

    for ( ;; ) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "proxy output chunk: %O", ngx_buf_size(cl->buf));

        size += ngx_buf_size(cl->buf);

        if (cl->buf->flush
            || cl->buf->sync
            || ngx_buf_in_memory(cl->buf)
            || cl->buf->in_file)
        {
            tl = ngx_alloc_chain_link(r->pool);
            if (tl == NULL) {
                return NGX_ERROR;
            }

            tl->buf = cl->buf;
            *ll = tl;
            ll = &tl->next;
        }

        if (cl->next == NULL) {
            break;
        }

        cl = cl->next;
    }

    // 添加chunked编码的大小
    if (size) {
        tl = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        b = tl->buf;
        chunk = b->start;

        if (chunk == NULL) {
            /* the "0000000000000000" is 64-bit hexadecimal string */

            chunk = ngx_palloc(r->pool, sizeof("0000000000000000" CRLF) - 1);
            if (chunk == NULL) {
                return NGX_ERROR;
            }

            b->start = chunk;
            b->end = chunk + sizeof("0000000000000000" CRLF) - 1;
        }

        b->tag = (ngx_buf_tag_t) &ngx_http_proxy_body_output_filter;
        b->memory = 0;
        b->temporary = 1;
        b->pos = chunk;
        b->last = ngx_sprintf(chunk, "%xO" CRLF, size);

        tl->next = *fl;
        *fl = tl;
    }

    // 处理最后一个缓冲区
    if (cl->buf->last_buf) {
        tl = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        b = tl->buf;

        b->tag = (ngx_buf_tag_t) &ngx_http_proxy_body_output_filter;
        b->temporary = 0;
        b->memory = 1;
        b->last_buf = 1;
        b->pos = (u_char *) CRLF "0" CRLF CRLF;
        b->last = b->pos + 7;

        cl->buf->last_buf = 0;

        *ll = tl;

        if (size == 0) {
            b->pos += 2;
        }

    } else if (size > 0) {
        tl = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        b = tl->buf;

        b->tag = (ngx_buf_tag_t) &ngx_http_proxy_body_output_filter;
        b->temporary = 0;
        b->memory = 1;
        b->pos = (u_char *) CRLF;
        b->last = b->pos + 2;

        *ll = tl;

    } else {
        *ll = NULL;
    }

out:

    // 输出处理后的链
    rc = ngx_chain_writer(&r->upstream->writer, out);

    // 更新链的状态
    ngx_chain_update_chains(r->pool, &ctx->free, &ctx->busy, &out,
                            (ngx_buf_tag_t) &ngx_http_proxy_body_output_filter);

    return rc;
}


// 处理代理服务器返回的状态行
static ngx_int_t
ngx_http_proxy_process_status_line(ngx_http_request_t *r)
{
    size_t                 len;
    ngx_int_t              rc;
    ngx_http_upstream_t   *u;
    ngx_http_proxy_ctx_t  *ctx;

    // 获取代理模块的上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        return NGX_ERROR;
    }

    u = r->upstream;

    // 解析状态行
    rc = ngx_http_parse_status_line(r, &u->buffer, &ctx->status);

    if (rc == NGX_AGAIN) {
        return rc;
    }

    if (rc == NGX_ERROR) {

#if (NGX_HTTP_CACHE)

        // 如果是缓存请求，则认为是HTTP/0.9
        if (r->cache) {
            r->http_version = NGX_HTTP_VERSION_9;
            return NGX_OK;
        }

#endif

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent no valid HTTP/1.0 header");

#if 0
        if (u->accel) {
            return NGX_HTTP_UPSTREAM_INVALID_HEADER;
        }
#endif

        // 处理HTTP/0.9响应
        r->http_version = NGX_HTTP_VERSION_9;
        u->state->status = NGX_HTTP_OK;
        u->headers_in.connection_close = 1;

        return NGX_OK;
    }

    // 设置状态码
    if (u->state && u->state->status == 0) {
        u->state->status = ctx->status.code;
    }

    u->headers_in.status_n = ctx->status.code;

    // 复制状态行
    len = ctx->status.end - ctx->status.start;
    u->headers_in.status_line.len = len;

    u->headers_in.status_line.data = ngx_pnalloc(r->pool, len);
    if (u->headers_in.status_line.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(u->headers_in.status_line.data, ctx->status.start, len);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http proxy status %ui \"%V\"",
                   u->headers_in.status_n, &u->headers_in.status_line);

    // 如果是HTTP/1.0，设置connection_close
    if (ctx->status.http_version < NGX_HTTP_VERSION_11) {
        u->headers_in.connection_close = 1;
    }

    // 设置下一步处理函数为处理头部
    u->process_header = ngx_http_proxy_process_header;

    return ngx_http_proxy_process_header(r);
}


// 处理代理服务器返回的头部
static ngx_int_t
ngx_http_proxy_process_header(ngx_http_request_t *r)
{
    ngx_int_t                       rc;
    ngx_table_elt_t                *h;
    ngx_http_upstream_t            *u;
    ngx_http_proxy_ctx_t           *ctx;
    ngx_http_upstream_header_t     *hh;
    ngx_http_upstream_main_conf_t  *umcf;

    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

    for ( ;; ) {

        // 解析一行头部
        rc = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);

        if (rc == NGX_OK) {

            // 成功解析一行头部

            // 添加到头部列表
            h = ngx_list_push(&r->upstream->headers_in.headers);
            if (h == NULL) {
                return NGX_ERROR;
            }

            h->hash = r->header_hash;

            h->key.len = r->header_name_end - r->header_name_start;
            h->value.len = r->header_end - r->header_start;

            // 分配内存并复制头部名称和值
            h->key.data = ngx_pnalloc(r->pool,
                               h->key.len + 1 + h->value.len + 1 + h->key.len);
            if (h->key.data == NULL) {
                h->hash = 0;
                return NGX_ERROR;
            }

            h->value.data = h->key.data + h->key.len + 1;
            h->lowcase_key = h->key.data + h->key.len + 1 + h->value.len + 1;

            ngx_memcpy(h->key.data, r->header_name_start, h->key.len);
            h->key.data[h->key.len] = '\0';
            ngx_memcpy(h->value.data, r->header_start, h->value.len);
            h->value.data[h->value.len] = '\0';

            // 转换头部名称为小写
            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);

            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

            // 查找并处理特殊头部
            hh = ngx_hash_find(&umcf->headers_in_hash, h->hash,
                               h->lowcase_key, h->key.len);

            if (hh) {
                rc = hh->handler(r, h, hh->offset);

                if (rc != NGX_OK) {
                    return rc;
                }
            }

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy header: \"%V: %V\"",
                           &h->key, &h->value);

            continue;
        }

        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {

            // 头部解析完成

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy header done");

            // 如果没有Server和Date头部，添加空的头部

            if (r->upstream->headers_in.server == NULL) {
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }

                h->hash = ngx_hash(ngx_hash(ngx_hash(ngx_hash(
                                    ngx_hash('s', 'e'), 'r'), 'v'), 'e'), 'r');

                ngx_str_set(&h->key, "Server");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char *) "server";
                h->next = NULL;
            }

            if (r->upstream->headers_in.date == NULL) {
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }

                h->hash = ngx_hash(ngx_hash(ngx_hash('d', 'a'), 't'), 'e');

                ngx_str_set(&h->key, "Date");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char *) "date";
                h->next = NULL;
            }

            // 如果响应是分块的，清除content length

            u = r->upstream;

            if (u->headers_in.chunked) {
                u->headers_in.content_length_n = -1;
            }

            // 设置keepalive

            ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

            if (u->headers_in.status_n == NGX_HTTP_NO_CONTENT
                || u->headers_in.status_n == NGX_HTTP_NOT_MODIFIED
                || ctx->head
                || (!u->headers_in.chunked
                    && u->headers_in.content_length_n == 0))
            {
                u->keepalive = !u->headers_in.connection_close;
            }

            if (u->headers_in.status_n == NGX_HTTP_SWITCHING_PROTOCOLS) {
                u->keepalive = 0;

                if (r->headers_in.upgrade) {
                    u->upgrade = 1;
                }
            }

            return NGX_OK;
        }

        if (rc == NGX_AGAIN) {
            return NGX_AGAIN;
        }

        // 无效的头部

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent invalid header: \"%*s\\x%02xd...\"",
                      r->header_end - r->header_name_start,
                      r->header_name_start, *r->header_end);

        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }
}


static ngx_int_t
ngx_http_proxy_input_filter_init(void *data)
{
    ngx_http_request_t    *r = data;
    ngx_http_upstream_t   *u;
    ngx_http_proxy_ctx_t  *ctx;

    u = r->upstream;
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http proxy filter init s:%ui h:%d c:%d l:%O",
                   u->headers_in.status_n, ctx->head, u->headers_in.chunked,
                   u->headers_in.content_length_n);

    /* 根据RFC2616第4.4节 消息长度 */

    if (u->headers_in.status_n == NGX_HTTP_NO_CONTENT
        || u->headers_in.status_n == NGX_HTTP_NOT_MODIFIED
        || ctx->head)
    {
        /* 处理1xx, 204, 304响应和HEAD请求的回复 */
        /* 由于我们不发送Expect和Upgrade，所以没有1xx响应 */

        u->pipe->length = 0;
        u->length = 0;
        u->keepalive = !u->headers_in.connection_close;

    } else if (u->headers_in.chunked) {
        /* 处理分块传输编码 */

        u->pipe->input_filter = ngx_http_proxy_chunked_filter;
        u->pipe->length = 3; /* "0" LF LF */

        u->input_filter = ngx_http_proxy_non_buffered_chunked_filter;
        u->length = 1;

    } else if (u->headers_in.content_length_n == 0) {
        /* 处理空响应体：特殊情况，因为过滤器不会被调用 */

        u->pipe->length = 0;
        u->length = 0;
        u->keepalive = !u->headers_in.connection_close;

    } else {
        /* 处理有Content-Length或连接关闭的情况 */

        u->pipe->length = u->headers_in.content_length_n;
        u->length = u->headers_in.content_length_n;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_copy_filter(ngx_event_pipe_t *p, ngx_buf_t *buf)
{
    ngx_buf_t           *b;
    ngx_chain_t         *cl;
    ngx_http_request_t  *r;

    if (buf->pos == buf->last) {
        return NGX_OK;
    }

    if (p->upstream_done) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, p->log, 0,
                       "http proxy data after close");
        return NGX_OK;
    }

    if (p->length == 0) {
        /* 上游发送的数据超过Content-Length指定的长度 */
        ngx_log_error(NGX_LOG_WARN, p->log, 0,
                      "upstream sent more data than specified in "
                      "\"Content-Length\" header");

        r = p->input_ctx;
        r->upstream->keepalive = 0;
        p->upstream_done = 1;

        return NGX_OK;
    }

    /* 从内存池中获取一个新的缓冲区 */
    cl = ngx_chain_get_free_buf(p->pool, &p->free);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;

    /* 复制缓冲区属性 */
    ngx_memcpy(b, buf, sizeof(ngx_buf_t));
    b->shadow = buf;
    b->tag = p->tag;
    b->last_shadow = 1;
    b->recycled = 1;
    buf->shadow = b;

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0, "input buf #%d", b->num);

    /* 将新缓冲区添加到输入链中 */
    if (p->in) {
        *p->last_in = cl;
    } else {
        p->in = cl;
    }
    p->last_in = &cl->next;

    if (p->length == -1) {
        return NGX_OK;
    }

    if (b->last - b->pos > p->length) {
        /* 上游发送的数据超过Content-Length指定的长度 */
        ngx_log_error(NGX_LOG_WARN, p->log, 0,
                      "upstream sent more data than specified in "
                      "\"Content-Length\" header");

        b->last = b->pos + p->length;
        p->upstream_done = 1;

        return NGX_OK;
    }

    /* 更新剩余长度 */
    p->length -= b->last - b->pos;

    if (p->length == 0) {
        r = p->input_ctx;
        r->upstream->keepalive = !r->upstream->headers_in.connection_close;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_chunked_filter(ngx_event_pipe_t *p, ngx_buf_t *buf)
{
    ngx_int_t              rc;
    ngx_buf_t             *b, **prev;
    ngx_chain_t           *cl;
    ngx_http_request_t    *r;
    ngx_http_proxy_ctx_t  *ctx;

    // 如果缓冲区为空，直接返回
    if (buf->pos == buf->last) {
        return NGX_OK;
    }

    r = p->input_ctx;
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    // 检查上下文是否存在
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    // 如果上游已完成，记录日志并返回
    if (p->upstream_done) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, p->log, 0,
                       "http proxy data after close");
        return NGX_OK;
    }

    // 如果长度为0，说明上游发送了最终块之后的数据
    if (p->length == 0) {
        ngx_log_error(NGX_LOG_WARN, p->log, 0,
                      "upstream sent data after final chunk");

        r->upstream->keepalive = 0;
        p->upstream_done = 1;

        return NGX_OK;
    }

    b = NULL;
    prev = &buf->shadow;

    for ( ;; ) {
        // 解析分块编码
        rc = ngx_http_parse_chunked(r, buf, &ctx->chunked);

        if (rc == NGX_OK) {
            // 成功解析一个块

            // 获取一个新的缓冲区
            cl = ngx_chain_get_free_buf(p->pool, &p->free);
            if (cl == NULL) {
                return NGX_ERROR;
            }

            b = cl->buf;

            ngx_memzero(b, sizeof(ngx_buf_t));

            // 设置新缓冲区的属性
            b->pos = buf->pos;
            b->start = buf->start;
            b->end = buf->end;
            b->tag = p->tag;
            b->temporary = 1;
            b->recycled = 1;

            *prev = b;
            prev = &b->shadow;

            // 将新缓冲区添加到输入链中
            if (p->in) {
                *p->last_in = cl;
            } else {
                p->in = cl;
            }
            p->last_in = &cl->next;

            /* STUB */ b->num = buf->num;

            ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "input buf #%d %p", b->num, b->pos);

            // 处理块数据
            if (buf->last - buf->pos >= ctx->chunked.size) {
                buf->pos += (size_t) ctx->chunked.size;
                b->last = buf->pos;
                ctx->chunked.size = 0;

                continue;
            }

            ctx->chunked.size -= buf->last - buf->pos;
            buf->pos = buf->last;
            b->last = buf->last;

            continue;
        }

        if (rc == NGX_DONE) {
            // 成功解析整个响应

            p->length = 0;
            r->upstream->keepalive = !r->upstream->headers_in.connection_close;

            // 检查是否有多余数据
            if (buf->pos != buf->last) {
                ngx_log_error(NGX_LOG_WARN, p->log, 0,
                              "upstream sent data after final chunk");
                r->upstream->keepalive = 0;
            }

            break;
        }

        if (rc == NGX_AGAIN) {
            // 需要更多数据来完成解析
            p->length = ctx->chunked.length;
            break;
        }

        // 无效响应
        ngx_log_error(NGX_LOG_ERR, p->log, 0,
                      "upstream sent invalid chunked response");

        return NGX_ERROR;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, p->log, 0,
                   "http proxy chunked state %ui, length %O",
                   ctx->chunked.state, p->length);

    if (b) {
        // 设置缓冲区的shadow属性
        b->shadow = buf;
        b->last_shadow = 1;

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "input buf %p %z", b->pos, b->last - b->pos);

        return NGX_OK;
    }

    // 如果缓冲区中没有数据记录，将其添加到空闲链中
    if (ngx_event_pipe_add_free_buf(p, buf) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_non_buffered_copy_filter(void *data, ssize_t bytes)
{
    ngx_http_request_t   *r = data;

    ngx_buf_t            *b;
    ngx_chain_t          *cl, **ll;
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 检查是否接收到超过Content-Length指定的数据量
    if (u->length == 0) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent more data than specified in "
                      "\"Content-Length\" header");
        u->keepalive = 0;
        return NGX_OK;
    }

    // 找到输出缓冲链的末尾
    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next;
    }

    // 获取一个新的缓冲区
    cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    *ll = cl;

    // 设置新缓冲区的属性
    cl->buf->flush = 1;
    cl->buf->memory = 1;

    b = &u->buffer;

    cl->buf->pos = b->last;
    b->last += bytes;
    cl->buf->last = b->last;
    cl->buf->tag = u->output.tag;

    // 如果长度为-1，表示没有Content-Length头
    if (u->length == -1) {
        return NGX_OK;
    }

    // 检查接收到的数据是否超过Content-Length指定的长度
    if (bytes > u->length) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent more data than specified in "
                      "\"Content-Length\" header");

        cl->buf->last = cl->buf->pos + u->length;
        u->length = 0;

        return NGX_OK;
    }

    // 更新剩余长度
    u->length -= bytes;

    // 如果所有数据都已接收，设置keepalive标志
    if (u->length == 0) {
        u->keepalive = !u->headers_in.connection_close;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_non_buffered_chunked_filter(void *data, ssize_t bytes)
{
    ngx_http_request_t   *r = data;

    ngx_int_t              rc;
    ngx_buf_t             *b, *buf;
    ngx_chain_t           *cl, **ll;
    ngx_http_upstream_t   *u;
    ngx_http_proxy_ctx_t  *ctx;

    // 获取代理模块的上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        return NGX_ERROR;
    }

    // 获取上游结构
    u = r->upstream;
    buf = &u->buffer;

    // 更新缓冲区位置
    buf->pos = buf->last;
    buf->last += bytes;

    // 找到输出链的末尾
    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next;
    }

    for ( ;; ) {
        // 解析分块传输编码
        rc = ngx_http_parse_chunked(r, buf, &ctx->chunked);

        if (rc == NGX_OK) {
            // 成功解析一个块

            // 获取一个新的缓冲区
            cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs);
            if (cl == NULL) {
                return NGX_ERROR;
            }

            // 将新缓冲区添加到输出链中
            *ll = cl;
            ll = &cl->next;

            b = cl->buf;

            // 设置缓冲区属性
            b->flush = 1;
            b->memory = 1;

            b->pos = buf->pos;
            b->tag = u->output.tag;

            // 处理块数据
            if (buf->last - buf->pos >= ctx->chunked.size) {
                buf->pos += (size_t) ctx->chunked.size;
                b->last = buf->pos;
                ctx->chunked.size = 0;
            } else {
                ctx->chunked.size -= buf->last - buf->pos;
                buf->pos = buf->last;
                b->last = buf->last;
            }

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy out buf %p %z",
                           b->pos, b->last - b->pos);

            continue;
        }

        if (rc == NGX_DONE) {
            // 整个响应已成功解析完成

            u->keepalive = !u->headers_in.connection_close;
            u->length = 0;

            // 检查是否有多余数据
            if (buf->pos != buf->last) {
                ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                              "upstream sent data after final chunk");
                u->keepalive = 0;
            }

            break;
        }

        if (rc == NGX_AGAIN) {
            // 需要更多数据
            break;
        }

        // 无效响应
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent invalid chunked response");

        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_http_proxy_abort_request(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "abort http proxy request");

    return;
}


static void
ngx_http_proxy_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http proxy request");

    return;
}


static ngx_int_t
ngx_http_proxy_host_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_proxy_ctx_t  *ctx;

    // 获取代理模块的上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 设置变量值
    v->len = ctx->vars.host_header.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = ctx->vars.host_header.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_port_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_proxy_ctx_t  *ctx;

    // 获取代理模块的上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 设置变量值
    v->len = ctx->vars.port.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = ctx->vars.port.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_add_x_forwarded_for_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    size_t            len;
    u_char           *p;
    ngx_table_elt_t  *h, *xfwd;

    // 设置变量属性
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 获取X-Forwarded-For头
    xfwd = r->headers_in.x_forwarded_for;

    len = 0;

    // 计算所有X-Forwarded-For头的总长度
    for (h = xfwd; h; h = h->next) {
        len += h->value.len + sizeof(", ") - 1;
    }

    if (len == 0) {
        // 如果没有X-Forwarded-For头，直接使用客户端IP
        v->len = r->connection->addr_text.len;
        v->data = r->connection->addr_text.data;
        return NGX_OK;
    }

    // 加上客户端IP的长度
    len += r->connection->addr_text.len;

    // 分配内存
    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = len;
    v->data = p;

    // 复制所有X-Forwarded-For头的值
    for (h = xfwd; h; h = h->next) {
        p = ngx_copy(p, h->value.data, h->value.len);
        *p++ = ','; *p++ = ' ';
    }

    // 添加客户端IP
    ngx_memcpy(p, r->connection->addr_text.data, r->connection->addr_text.len);

    return NGX_OK;
}


/**
 * @brief 处理内部请求体长度变量
 * @param r HTTP请求
 * @param v 变量值
 * @param data 用户数据
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
static ngx_int_t
ngx_http_proxy_internal_body_length_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_proxy_ctx_t  *ctx;

    // 获取代理模块的上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    // 如果上下文不存在或内部请求体长度无效，则标记变量为未找到
    if (ctx == NULL || ctx->internal_body_length < 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 设置变量属性
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 分配内存用于存储长度字符串
    v->data = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);

    if (v->data == NULL) {
        return NGX_ERROR;
    }

    // 将长度转换为字符串
    v->len = ngx_sprintf(v->data, "%O", ctx->internal_body_length) - v->data;

    return NGX_OK;
}


/**
 * @brief 处理内部分块传输编码变量
 * @param r HTTP请求
 * @param v 变量值
 * @param data 用户数据
 * @return NGX_OK 成功
 */
static ngx_int_t
ngx_http_proxy_internal_chunked_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_proxy_ctx_t  *ctx;

    // 获取代理模块的上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    // 如果上下文不存在或未使用分块传输编码，则标记变量为未找到
    if (ctx == NULL || !ctx->internal_chunked) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 设置变量属性
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 设置变量值为 "chunked"
    v->data = (u_char *) "chunked";
    v->len = sizeof("chunked") - 1;

    return NGX_OK;
}


/**
 * @brief 重写重定向URL
 * @param r HTTP请求
 * @param h 头部
 * @param prefix 前缀长度
 * @return NGX_OK 成功，NGX_DECLINED 未处理，NGX_ERROR 失败
 */
static ngx_int_t
ngx_http_proxy_rewrite_redirect(ngx_http_request_t *r, ngx_table_elt_t *h,
    size_t prefix)
{
    size_t                      len;
    ngx_int_t                   rc;
    ngx_uint_t                  i;
    ngx_http_proxy_rewrite_t   *pr;
    ngx_http_proxy_loc_conf_t  *plcf;

    // 获取代理模块的位置配置
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

    pr = plcf->redirects->elts;

    if (pr == NULL) {
        return NGX_DECLINED;
    }

    // 计算需要重写的长度
    len = h->value.len - prefix;

    // 遍历所有重写规则
    for (i = 0; i < plcf->redirects->nelts; i++) {
        rc = pr[i].handler(r, &h->value, prefix, len, &pr[i]);

        if (rc != NGX_DECLINED) {
            return rc;
        }
    }

    return NGX_DECLINED;
}


/**
 * @brief 重写Cookie头部
 * @param r HTTP请求
 * @param h Cookie头部
 * @return NGX_OK 成功，NGX_DECLINED 未处理，NGX_ERROR 失败
 */
static ngx_int_t
ngx_http_proxy_rewrite_cookie(ngx_http_request_t *r, ngx_table_elt_t *h)
{
    u_char                     *p;
    size_t                      len;
    ngx_int_t                   rc, rv;
    ngx_str_t                  *key, *value;
    ngx_uint_t                  i;
    ngx_array_t                 attrs;
    ngx_keyval_t               *attr;
    ngx_http_proxy_loc_conf_t  *plcf;

    // 初始化属性数组
    if (ngx_array_init(&attrs, r->pool, 2, sizeof(ngx_keyval_t)) != NGX_OK) {
        return NGX_ERROR;
    }

    // 解析Cookie字符串
    if (ngx_http_proxy_parse_cookie(&h->value, &attrs) != NGX_OK) {
        return NGX_ERROR;
    }

    attr = attrs.elts;

    // 如果没有Cookie值，直接返回
    if (attr[0].value.data == NULL) {
        return NGX_DECLINED;
    }

    rv = NGX_DECLINED;

    // 获取代理模块的位置配置
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

    // 遍历所有Cookie属性
    for (i = 1; i < attrs.nelts; i++) {

        key = &attr[i].key;
        value = &attr[i].value;

        // 处理domain属性
        if (plcf->cookie_domains && key->len == 6
            && ngx_strncasecmp(key->data, (u_char *) "domain", 6) == 0
            && value->data)
        {
            rc = ngx_http_proxy_rewrite_cookie_value(r, value,
                                                     plcf->cookie_domains);
            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (rc != NGX_DECLINED) {
                rv = rc;
            }
        }

        // 处理path属性
        if (plcf->cookie_paths && key->len == 4
            && ngx_strncasecmp(key->data, (u_char *) "path", 4) == 0
            && value->data)
        {
            rc = ngx_http_proxy_rewrite_cookie_value(r, value,
                                                     plcf->cookie_paths);
            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (rc != NGX_DECLINED) {
                rv = rc;
            }
        }
    }

    // 处理Cookie标志
    if (plcf->cookie_flags) {
        rc = ngx_http_proxy_rewrite_cookie_flags(r, &attrs,
                                                 plcf->cookie_flags);
        if (rc == NGX_ERROR) {
            return NGX_ERROR;
        }

        if (rc != NGX_DECLINED) {
            rv = rc;
        }

        attr = attrs.elts;
    }

    if (rv != NGX_OK) {
        return rv;
    }

    // 计算新Cookie字符串的长度
    len = 0;

    for (i = 0; i < attrs.nelts; i++) {

        if (attr[i].key.data == NULL) {
            continue;
        }

        if (i > 0) {
            len += 2;
        }

        len += attr[i].key.len;

        if (attr[i].value.data) {
            len += 1 + attr[i].value.len;
        }
    }

    // 分配内存用于新Cookie字符串
    p = ngx_pnalloc(r->pool, len + 1);
    if (p == NULL) {
        return NGX_ERROR;
    }

    h->value.data = p;
    h->value.len = len;

    // 构建新的Cookie字符串
    for (i = 0; i < attrs.nelts; i++) {

        if (attr[i].key.data == NULL) {
            continue;
        }

        if (i > 0) {
            *p++ = ';';
            *p++ = ' ';
        }

        p = ngx_cpymem(p, attr[i].key.data, attr[i].key.len);

        if (attr[i].value.data) {
            *p++ = '=';
            p = ngx_cpymem(p, attr[i].value.data, attr[i].value.len);
        }
    }

    *p = '\0';

    return NGX_OK;
}


/**
 * @brief 解析Cookie字符串
 * @param value Cookie字符串
 * @param attrs 用于存储解析结果的数组
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
static ngx_int_t
ngx_http_proxy_parse_cookie(ngx_str_t *value, ngx_array_t *attrs)
{
    u_char        *start, *end, *p, *last;
    ngx_str_t      name, val;
    ngx_keyval_t  *attr;

    start = value->data;
    end = value->data + value->len;

    for ( ;; ) {

        // 查找分号，分割每个Cookie
        last = (u_char *) ngx_strchr(start, ';');

        if (last == NULL) {
            last = end;
        }

        // 跳过开头的空格
        while (start < last && *start == ' ') { start++; }

        // 查找等号，分割名称和值
        for (p = start; p < last && *p != '='; p++) { /* void */ }

        name.data = start;
        name.len = p - start;

        // 去除名称末尾的空格
        while (name.len && name.data[name.len - 1] == ' ') {
            name.len--;
        }

        if (p < last) {

            p++;

            // 跳过值开头的空格
            while (p < last && *p == ' ') { p++; }

            val.data = p;
            val.len = last - val.data;

            // 去除值末尾的空格
            while (val.len && val.data[val.len - 1] == ' ') {
                val.len--;
            }

        } else {
            ngx_str_null(&val);
        }

        // 将解析结果添加到属性数组
        attr = ngx_array_push(attrs);
        if (attr == NULL) {
            return NGX_ERROR;
        }

        attr->key = name;
        attr->value = val;

        if (last == end) {
            break;
        }

        start = last + 1;
    }

    return NGX_OK;
}


// 重写cookie值的函数
static ngx_int_t
ngx_http_proxy_rewrite_cookie_value(ngx_http_request_t *r, ngx_str_t *value,
    ngx_array_t *rewrites)
{
    ngx_int_t                  rc;
    ngx_uint_t                 i;
    ngx_http_proxy_rewrite_t  *pr;

    pr = rewrites->elts;

    // 遍历所有重写规则
    for (i = 0; i < rewrites->nelts; i++) {
        // 调用每个重写规则的处理函数
        rc = pr[i].handler(r, value, 0, value->len, &pr[i]);

        // 如果处理函数返回非NGX_DECLINED，则返回该结果
        if (rc != NGX_DECLINED) {
            return rc;
        }
    }

    // 如果所有规则都未匹配，返回NGX_DECLINED
    return NGX_DECLINED;
}


// 重写cookie标志的函数
static ngx_int_t
ngx_http_proxy_rewrite_cookie_flags(ngx_http_request_t *r, ngx_array_t *attrs,
    ngx_array_t *flags)
{
    ngx_str_t                       pattern, value;
#if (NGX_PCRE)
    ngx_int_t                       rc;
#endif
    ngx_uint_t                      i, m, f, nelts;
    ngx_keyval_t                   *attr;
    ngx_conf_bitmask_t             *mask;
    ngx_http_complex_value_t       *flags_values;
    ngx_http_proxy_cookie_flags_t  *pcf;

    attr = attrs->elts;
    pcf = flags->elts;

    // 遍历所有cookie标志规则
    for (i = 0; i < flags->nelts; i++) {

#if (NGX_PCRE)
        // 如果使用正则表达式匹配
        if (pcf[i].regex) {
            rc = ngx_http_regex_exec(r, pcf[i].cookie.regex, &attr[0].key);

            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (rc == NGX_OK) {
                break;
            }

            /* NGX_DECLINED */

            continue;
        }
#endif

        // 如果使用复杂值匹配
        if (ngx_http_complex_value(r, &pcf[i].cookie.complex, &pattern)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        // 比较cookie名称
        if (pattern.len == attr[0].key.len
            && ngx_strncasecmp(attr[0].key.data, pattern.data, pattern.len)
               == 0)
        {
            break;
        }
    }

    // 如果没有匹配的规则，返回NGX_DECLINED
    if (i == flags->nelts) {
        return NGX_DECLINED;
    }

    nelts = pcf[i].flags_values.nelts;
    flags_values = pcf[i].flags_values.elts;

    mask = ngx_http_proxy_cookie_flags_masks;
    f = 0;

    // 处理匹配规则的标志值
    for (i = 0; i < nelts; i++) {

        if (ngx_http_complex_value(r, &flags_values[i], &value) != NGX_OK) {
            return NGX_ERROR;
        }

        if (value.len == 0) {
            continue;
        }

        // 查找匹配的标志掩码
        for (m = 0; mask[m].name.len != 0; m++) {

            if (mask[m].name.len != value.len
                || ngx_strncasecmp(mask[m].name.data, value.data, value.len)
                   != 0)
            {
                continue;
            }

            f |= mask[m].mask;

            break;
        }

        // 如果没有找到匹配的标志，记录日志
        if (mask[m].name.len == 0) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "invalid proxy_cookie_flags flag \"%V\"", &value);
        }
    }

    // 如果没有设置任何标志，返回NGX_DECLINED
    if (f == 0) {
        return NGX_DECLINED;
    }

    // 编辑cookie标志
    return ngx_http_proxy_edit_cookie_flags(r, attrs, f);
}


// 编辑cookie标志的函数
static ngx_int_t
ngx_http_proxy_edit_cookie_flags(ngx_http_request_t *r, ngx_array_t *attrs,
    ngx_uint_t flags)
{
    ngx_str_t     *key, *value;
    ngx_uint_t     i;
    ngx_keyval_t  *attr;

    attr = attrs->elts;

    // 遍历所有属性
    for (i = 1; i < attrs->nelts; i++) {
        key = &attr[i].key;

        // 处理Secure标志
        if (key->len == 6
            && ngx_strncasecmp(key->data, (u_char *) "secure", 6) == 0)
        {
            if (flags & NGX_HTTP_PROXY_COOKIE_SECURE_ON) {
                flags &= ~NGX_HTTP_PROXY_COOKIE_SECURE_ON;

            } else if (flags & NGX_HTTP_PROXY_COOKIE_SECURE_OFF) {
                key->data = NULL;
            }

            continue;
        }

        // 处理HttpOnly标志
        if (key->len == 8
            && ngx_strncasecmp(key->data, (u_char *) "httponly", 8) == 0)
        {
            if (flags & NGX_HTTP_PROXY_COOKIE_HTTPONLY_ON) {
                flags &= ~NGX_HTTP_PROXY_COOKIE_HTTPONLY_ON;

            } else if (flags & NGX_HTTP_PROXY_COOKIE_HTTPONLY_OFF) {
                key->data = NULL;
            }

            continue;
        }

        // 处理SameSite标志
        if (key->len == 8
            && ngx_strncasecmp(key->data, (u_char *) "samesite", 8) == 0)
        {
            value = &attr[i].value;

            if (flags & NGX_HTTP_PROXY_COOKIE_SAMESITE_STRICT) {
                flags &= ~NGX_HTTP_PROXY_COOKIE_SAMESITE_STRICT;

                if (value->len != 6
                    || ngx_strncasecmp(value->data, (u_char *) "strict", 6)
                       != 0)
                {
                    ngx_str_set(key, "SameSite");
                    ngx_str_set(value, "Strict");
                }

            } else if (flags & NGX_HTTP_PROXY_COOKIE_SAMESITE_LAX) {
                flags &= ~NGX_HTTP_PROXY_COOKIE_SAMESITE_LAX;

                if (value->len != 3
                    || ngx_strncasecmp(value->data, (u_char *) "lax", 3) != 0)
                {
                    ngx_str_set(key, "SameSite");
                    ngx_str_set(value, "Lax");
                }

            } else if (flags & NGX_HTTP_PROXY_COOKIE_SAMESITE_NONE) {
                flags &= ~NGX_HTTP_PROXY_COOKIE_SAMESITE_NONE;

                if (value->len != 4
                    || ngx_strncasecmp(value->data, (u_char *) "none", 4) != 0)
                {
                    ngx_str_set(key, "SameSite");
                    ngx_str_set(value, "None");
                }

            } else if (flags & NGX_HTTP_PROXY_COOKIE_SAMESITE_OFF) {
                key->data = NULL;
            }

            continue;
        }
    }

    // 添加缺失的标志

    // 添加Secure标志
    if (flags & NGX_HTTP_PROXY_COOKIE_SECURE_ON) {
        attr = ngx_array_push(attrs);
        if (attr == NULL) {
            return NGX_ERROR;
        }

        ngx_str_set(&attr->key, "Secure");
        ngx_str_null(&attr->value);
    }

    // 添加HttpOnly标志
    if (flags & NGX_HTTP_PROXY_COOKIE_HTTPONLY_ON) {
        attr = ngx_array_push(attrs);
        if (attr == NULL) {
            return NGX_ERROR;
        }

        ngx_str_set(&attr->key, "HttpOnly");
        ngx_str_null(&attr->value);
    }

    // 添加SameSite标志
    if (flags & (NGX_HTTP_PROXY_COOKIE_SAMESITE_STRICT
                 |NGX_HTTP_PROXY_COOKIE_SAMESITE_LAX
                 |NGX_HTTP_PROXY_COOKIE_SAMESITE_NONE))
    {
        attr = ngx_array_push(attrs);
        if (attr == NULL) {
            return NGX_ERROR;
        }

        ngx_str_set(&attr->key, "SameSite");

        if (flags & NGX_HTTP_PROXY_COOKIE_SAMESITE_STRICT) {
            ngx_str_set(&attr->value, "Strict");

        } else if (flags & NGX_HTTP_PROXY_COOKIE_SAMESITE_LAX) {
            ngx_str_set(&attr->value, "Lax");

        } else {
            ngx_str_set(&attr->value, "None");
        }
    }

    return NGX_OK;
}


/**
 * @brief 处理复杂的代理重写规则
 * @param r HTTP请求
 * @param value 要重写的值
 * @param prefix 前缀长度
 * @param len 要匹配的长度
 * @param pr 代理重写规则
 * @return NGX_OK, NGX_ERROR, 或 NGX_DECLINED
 */
static ngx_int_t
ngx_http_proxy_rewrite_complex_handler(ngx_http_request_t *r, ngx_str_t *value,
    size_t prefix, size_t len, ngx_http_proxy_rewrite_t *pr)
{
    ngx_str_t  pattern, replacement;

    // 计算复杂模式的值
    if (ngx_http_complex_value(r, &pr->pattern.complex, &pattern) != NGX_OK) {
        return NGX_ERROR;
    }

    // 检查模式是否匹配
    if (pattern.len > len
        || ngx_rstrncmp(value->data + prefix, pattern.data, pattern.len) != 0)
    {
        return NGX_DECLINED;
    }

    // 计算复杂替换的值
    if (ngx_http_complex_value(r, &pr->replacement, &replacement) != NGX_OK) {
        return NGX_ERROR;
    }

    // 执行重写操作
    return ngx_http_proxy_rewrite(r, value, prefix, pattern.len, &replacement);
}


#if (NGX_PCRE)

/**
 * @brief 处理正则表达式的代理重写规则
 * @param r HTTP请求
 * @param value 要重写的值
 * @param prefix 前缀长度
 * @param len 要匹配的长度
 * @param pr 代理重写规则
 * @return NGX_OK, NGX_ERROR, 或 NGX_DECLINED
 */
static ngx_int_t
ngx_http_proxy_rewrite_regex_handler(ngx_http_request_t *r, ngx_str_t *value,
    size_t prefix, size_t len, ngx_http_proxy_rewrite_t *pr)
{
    ngx_str_t  pattern, replacement;

    // 设置要匹配的模式
    pattern.len = len;
    pattern.data = value->data + prefix;

    // 执行正则表达式匹配
    if (ngx_http_regex_exec(r, pr->pattern.regex, &pattern) != NGX_OK) {
        return NGX_DECLINED;
    }

    // 计算复杂替换的值
    if (ngx_http_complex_value(r, &pr->replacement, &replacement) != NGX_OK) {
        return NGX_ERROR;
    }

    // 执行重写操作
    return ngx_http_proxy_rewrite(r, value, prefix, len, &replacement);
}

#endif


/**
 * @brief 处理域名的代理重写规则
 * @param r HTTP请求
 * @param value 要重写的值
 * @param prefix 前缀长度
 * @param len 要匹配的长度
 * @param pr 代理重写规则
 * @return NGX_OK, NGX_ERROR, 或 NGX_DECLINED
 */
static ngx_int_t
ngx_http_proxy_rewrite_domain_handler(ngx_http_request_t *r, ngx_str_t *value,
    size_t prefix, size_t len, ngx_http_proxy_rewrite_t *pr)
{
    u_char     *p;
    ngx_str_t   pattern, replacement;

    // 计算复杂模式的值
    if (ngx_http_complex_value(r, &pr->pattern.complex, &pattern) != NGX_OK) {
        return NGX_ERROR;
    }

    p = value->data + prefix;

    // 处理以点开头的域名
    if (len && p[0] == '.') {
        p++;
        prefix++;
        len--;
    }

    // 检查域名是否匹配
    if (pattern.len != len || ngx_rstrncasecmp(pattern.data, p, len) != 0) {
        return NGX_DECLINED;
    }

    // 计算复杂替换的值
    if (ngx_http_complex_value(r, &pr->replacement, &replacement) != NGX_OK) {
        return NGX_ERROR;
    }

    // 执行重写操作
    return ngx_http_proxy_rewrite(r, value, prefix, len, &replacement);
}


/**
 * @brief 执行实际的重写操作
 * @param r HTTP请求
 * @param value 要重写的值
 * @param prefix 前缀长度
 * @param len 要替换的长度
 * @param replacement 替换的内容
 * @return NGX_OK 或 NGX_ERROR
 */
static ngx_int_t
ngx_http_proxy_rewrite(ngx_http_request_t *r, ngx_str_t *value, size_t prefix,
    size_t len, ngx_str_t *replacement)
{
    u_char  *p, *data;
    size_t   new_len;

    // 如果整个值都被替换，直接使用替换值
    if (len == value->len) {
        *value = *replacement;
        return NGX_OK;
    }

    new_len = replacement->len + value->len - len;

    // 如果替换后的长度大于原长度，需要重新分配内存
    if (replacement->len > len) {

        data = ngx_pnalloc(r->pool, new_len + 1);
        if (data == NULL) {
            return NGX_ERROR;
        }

        p = ngx_copy(data, value->data, prefix);
        p = ngx_copy(p, replacement->data, replacement->len);

        ngx_memcpy(p, value->data + prefix + len,
                   value->len - len - prefix + 1);

        value->data = data;

    } else {
        // 如果替换后的长度小于等于原长度，直接在原内存上操作
        p = ngx_copy(value->data + prefix, replacement->data, replacement->len);

        ngx_memmove(p, value->data + prefix + len,
                    value->len - len - prefix + 1);
    }

    value->len = new_len;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    // 遍历预定义的代理变量
    for (v = ngx_http_proxy_vars; v->name.len; v++) {
        // 添加变量到配置中
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        // 设置变量的获取处理函数和数据
        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


static void *
ngx_http_proxy_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_main_conf_t  *conf;

    // 分配内存并初始化为0
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proxy_main_conf_t));
    if (conf == NULL) {
        return NULL;
    }

#if (NGX_HTTP_CACHE)
    // 初始化缓存数组
    if (ngx_array_init(&conf->caches, cf->pool, 4,
                       sizeof(ngx_http_file_cache_t *))
        != NGX_OK)
    {
        return NULL;
    }
#endif

    return conf;
}


static void *
ngx_http_proxy_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_loc_conf_t  *conf;

    // 分配内存并初始化为0
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proxy_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * 以下成员由ngx_pcalloc()设置为0:
     *
     *     conf->upstream.bufs.num = 0;
     *     conf->upstream.ignore_headers = 0;
     *     conf->upstream.next_upstream = 0;
     *     conf->upstream.cache_zone = NULL;
     *     conf->upstream.cache_use_stale = 0;
     *     conf->upstream.cache_methods = 0;
     *     conf->upstream.temp_path = NULL;
     *     conf->upstream.hide_headers_hash = { NULL, 0 };
     *     conf->upstream.store_lengths = NULL;
     *     conf->upstream.store_values = NULL;
     *
     *     conf->location = NULL;
     *     conf->url = { 0, NULL };
     *     conf->headers.lengths = NULL;
     *     conf->headers.values = NULL;
     *     conf->headers.hash = { NULL, 0 };
     *     conf->headers_cache.lengths = NULL;
     *     conf->headers_cache.values = NULL;
     *     conf->headers_cache.hash = { NULL, 0 };
     *     conf->body_lengths = NULL;
     *     conf->body_values = NULL;
     *     conf->body_source = { 0, NULL };
     *     conf->redirects = NULL;
     *     conf->ssl = 0;
     *     conf->ssl_protocols = 0;
     *     conf->ssl_ciphers = { 0, NULL };
     *     conf->ssl_trusted_certificate = { 0, NULL };
     *     conf->ssl_crl = { 0, NULL };
     */

    // 设置各种配置项的初始值
    conf->upstream.store = NGX_CONF_UNSET;
    conf->upstream.store_access = NGX_CONF_UNSET_UINT;
    conf->upstream.next_upstream_tries = NGX_CONF_UNSET_UINT;
    conf->upstream.buffering = NGX_CONF_UNSET;
    conf->upstream.request_buffering = NGX_CONF_UNSET;
    conf->upstream.ignore_client_abort = NGX_CONF_UNSET;
    conf->upstream.force_ranges = NGX_CONF_UNSET;

    conf->upstream.local = NGX_CONF_UNSET_PTR;
    conf->upstream.socket_keepalive = NGX_CONF_UNSET;

    conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.next_upstream_timeout = NGX_CONF_UNSET_MSEC;

    conf->upstream.send_lowat = NGX_CONF_UNSET_SIZE;
    conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;
    conf->upstream.limit_rate = NGX_CONF_UNSET_PTR;

    conf->upstream.busy_buffers_size_conf = NGX_CONF_UNSET_SIZE;
    conf->upstream.max_temp_file_size_conf = NGX_CONF_UNSET_SIZE;
    conf->upstream.temp_file_write_size_conf = NGX_CONF_UNSET_SIZE;

    conf->upstream.pass_request_headers = NGX_CONF_UNSET;
    conf->upstream.pass_request_body = NGX_CONF_UNSET;

#if (NGX_HTTP_CACHE)
    // 设置缓存相关的配置项初始值
    conf->upstream.cache = NGX_CONF_UNSET;
    conf->upstream.cache_min_uses = NGX_CONF_UNSET_UINT;
    conf->upstream.cache_max_range_offset = NGX_CONF_UNSET;
    conf->upstream.cache_bypass = NGX_CONF_UNSET_PTR;
    conf->upstream.no_cache = NGX_CONF_UNSET_PTR;
    conf->upstream.cache_valid = NGX_CONF_UNSET_PTR;
    conf->upstream.cache_lock = NGX_CONF_UNSET;
    conf->upstream.cache_lock_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.cache_lock_age = NGX_CONF_UNSET_MSEC;
    conf->upstream.cache_revalidate = NGX_CONF_UNSET;
    conf->upstream.cache_convert_head = NGX_CONF_UNSET;
    conf->upstream.cache_background_update = NGX_CONF_UNSET;
#endif

    conf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
    conf->upstream.pass_headers = NGX_CONF_UNSET_PTR;

    conf->upstream.intercept_errors = NGX_CONF_UNSET;

#if (NGX_HTTP_SSL)
    // 设置SSL相关的配置项初始值
    conf->upstream.ssl_session_reuse = NGX_CONF_UNSET;
    conf->upstream.ssl_name = NGX_CONF_UNSET_PTR;
    conf->upstream.ssl_server_name = NGX_CONF_UNSET;
    conf->upstream.ssl_verify = NGX_CONF_UNSET;
    conf->upstream.ssl_certificate = NGX_CONF_UNSET_PTR;
    conf->upstream.ssl_certificate_key = NGX_CONF_UNSET_PTR;
    conf->upstream.ssl_passwords = NGX_CONF_UNSET_PTR;
    conf->ssl_verify_depth = NGX_CONF_UNSET_UINT;
    conf->ssl_conf_commands = NGX_CONF_UNSET_PTR;
#endif

    /* "proxy_cyclic_temp_file" is disabled */
    conf->upstream.cyclic_temp_file = 0;

    conf->upstream.change_buffering = 1;

    conf->headers_source = NGX_CONF_UNSET_PTR;

    conf->method = NGX_CONF_UNSET_PTR;

    conf->redirect = NGX_CONF_UNSET;

    conf->cookie_domains = NGX_CONF_UNSET_PTR;
    conf->cookie_paths = NGX_CONF_UNSET_PTR;
    conf->cookie_flags = NGX_CONF_UNSET_PTR;

    conf->http_version = NGX_CONF_UNSET_UINT;

    conf->headers_hash_max_size = NGX_CONF_UNSET_UINT;
    conf->headers_hash_bucket_size = NGX_CONF_UNSET_UINT;

    ngx_str_set(&conf->upstream.module, "proxy");

    return conf;
}


static char *
ngx_http_proxy_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_proxy_loc_conf_t *prev = parent;
    ngx_http_proxy_loc_conf_t *conf = child;

    u_char                     *p;
    size_t                      size;
    ngx_int_t                   rc;
    ngx_hash_init_t             hash;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_proxy_rewrite_t   *pr;
    ngx_http_script_compile_t   sc;

#if (NGX_HTTP_CACHE)
    // 如果启用了存储,则禁用缓存
    if (conf->upstream.store > 0) {
        conf->upstream.cache = 0;
    }

    // 如果启用了缓存,则禁用存储
    if (conf->upstream.cache > 0) {
        conf->upstream.store = 0;
    }

#endif

    // 合并存储相关配置
    if (conf->upstream.store == NGX_CONF_UNSET) {
        ngx_conf_merge_value(conf->upstream.store,
                              prev->upstream.store, 0);

        conf->upstream.store_lengths = prev->upstream.store_lengths;
        conf->upstream.store_values = prev->upstream.store_values;
    }

    ngx_conf_merge_uint_value(conf->upstream.store_access,
                              prev->upstream.store_access, 0600);

    ngx_conf_merge_uint_value(conf->upstream.next_upstream_tries,
                              prev->upstream.next_upstream_tries, 0);

    ngx_conf_merge_value(conf->upstream.buffering,
                              prev->upstream.buffering, 1);

    ngx_conf_merge_value(conf->upstream.request_buffering,
                              prev->upstream.request_buffering, 1);

    ngx_conf_merge_value(conf->upstream.ignore_client_abort,
                              prev->upstream.ignore_client_abort, 0);

    ngx_conf_merge_value(conf->upstream.force_ranges,
                              prev->upstream.force_ranges, 0);

    ngx_conf_merge_ptr_value(conf->upstream.local,
                              prev->upstream.local, NULL);

    ngx_conf_merge_value(conf->upstream.socket_keepalive,
                              prev->upstream.socket_keepalive, 0);

    ngx_conf_merge_msec_value(conf->upstream.connect_timeout,
                              prev->upstream.connect_timeout, 60000);

    ngx_conf_merge_msec_value(conf->upstream.send_timeout,
                              prev->upstream.send_timeout, 60000);

    ngx_conf_merge_msec_value(conf->upstream.read_timeout,
                              prev->upstream.read_timeout, 60000);

    ngx_conf_merge_msec_value(conf->upstream.next_upstream_timeout,
                              prev->upstream.next_upstream_timeout, 0);

    ngx_conf_merge_size_value(conf->upstream.send_lowat,
                              prev->upstream.send_lowat, 0);

    ngx_conf_merge_size_value(conf->upstream.buffer_size,
                              prev->upstream.buffer_size,
                              (size_t) ngx_pagesize);

    ngx_conf_merge_ptr_value(conf->upstream.limit_rate,
                              prev->upstream.limit_rate, NULL);

    ngx_conf_merge_bufs_value(conf->upstream.bufs, prev->upstream.bufs,
                              8, ngx_pagesize);

    if (conf->upstream.bufs.num < 2) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "there must be at least 2 \"proxy_buffers\"");
        return NGX_CONF_ERROR;
    }


    size = conf->upstream.buffer_size;
    if (size < conf->upstream.bufs.size) {
        size = conf->upstream.bufs.size;
    }


    ngx_conf_merge_size_value(conf->upstream.busy_buffers_size_conf,
                              prev->upstream.busy_buffers_size_conf,
                              NGX_CONF_UNSET_SIZE);

    if (conf->upstream.busy_buffers_size_conf == NGX_CONF_UNSET_SIZE) {
        conf->upstream.busy_buffers_size = 2 * size;
    } else {
        conf->upstream.busy_buffers_size =
                                         conf->upstream.busy_buffers_size_conf;
    }

    if (conf->upstream.busy_buffers_size < size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
             "\"proxy_busy_buffers_size\" must be equal to or greater than "
             "the maximum of the value of \"proxy_buffer_size\" and "
             "one of the \"proxy_buffers\"");

        return NGX_CONF_ERROR;
    }

    if (conf->upstream.busy_buffers_size
        > (conf->upstream.bufs.num - 1) * conf->upstream.bufs.size)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
             "\"proxy_busy_buffers_size\" must be less than "
             "the size of all \"proxy_buffers\" minus one buffer");

        return NGX_CONF_ERROR;
    }


    ngx_conf_merge_size_value(conf->upstream.temp_file_write_size_conf,
                              prev->upstream.temp_file_write_size_conf,
                              NGX_CONF_UNSET_SIZE);

    if (conf->upstream.temp_file_write_size_conf == NGX_CONF_UNSET_SIZE) {
        conf->upstream.temp_file_write_size = 2 * size;
    } else {
        conf->upstream.temp_file_write_size =
                                      conf->upstream.temp_file_write_size_conf;
    }

    if (conf->upstream.temp_file_write_size < size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
             "\"proxy_temp_file_write_size\" must be equal to or greater "
             "than the maximum of the value of \"proxy_buffer_size\" and "
             "one of the \"proxy_buffers\"");

        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_size_value(conf->upstream.max_temp_file_size_conf,
                              prev->upstream.max_temp_file_size_conf,
                              NGX_CONF_UNSET_SIZE);

    if (conf->upstream.max_temp_file_size_conf == NGX_CONF_UNSET_SIZE) {
        conf->upstream.max_temp_file_size = 1024 * 1024 * 1024;
    } else {
        conf->upstream.max_temp_file_size =
                                        conf->upstream.max_temp_file_size_conf;
    }

    if (conf->upstream.max_temp_file_size != 0
        && conf->upstream.max_temp_file_size < size)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
             "\"proxy_max_temp_file_size\" must be equal to zero to disable "
             "temporary files usage or must be equal to or greater than "
             "the maximum of the value of \"proxy_buffer_size\" and "
             "one of the \"proxy_buffers\"");

        return NGX_CONF_ERROR;
    }


    ngx_conf_merge_bitmask_value(conf->upstream.ignore_headers,
                              prev->upstream.ignore_headers,
                              NGX_CONF_BITMASK_SET);


    ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
                              prev->upstream.next_upstream,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_UPSTREAM_FT_ERROR
                               |NGX_HTTP_UPSTREAM_FT_TIMEOUT));

    if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF) {
        conf->upstream.next_upstream = NGX_CONF_BITMASK_SET
                                       |NGX_HTTP_UPSTREAM_FT_OFF;
    }

    if (ngx_conf_merge_path_value(cf, &conf->upstream.temp_path,
                              prev->upstream.temp_path,
                              &ngx_http_proxy_temp_path)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }


#if (NGX_HTTP_CACHE)

    if (conf->upstream.cache == NGX_CONF_UNSET) {
        ngx_conf_merge_value(conf->upstream.cache,
                              prev->upstream.cache, 0);

        conf->upstream.cache_zone = prev->upstream.cache_zone;
        conf->upstream.cache_value = prev->upstream.cache_value;
    }

    if (conf->upstream.cache_zone && conf->upstream.cache_zone->data == NULL) {
        ngx_shm_zone_t  *shm_zone;

        shm_zone = conf->upstream.cache_zone;

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"proxy_cache\" zone \"%V\" is unknown",
                           &shm_zone->shm.name);

        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_uint_value(conf->upstream.cache_min_uses,
                              prev->upstream.cache_min_uses, 1);

    ngx_conf_merge_off_value(conf->upstream.cache_max_range_offset,
                              prev->upstream.cache_max_range_offset,
                              NGX_MAX_OFF_T_VALUE);

    ngx_conf_merge_bitmask_value(conf->upstream.cache_use_stale,
                              prev->upstream.cache_use_stale,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_UPSTREAM_FT_OFF));

    if (conf->upstream.cache_use_stale & NGX_HTTP_UPSTREAM_FT_OFF) {
        conf->upstream.cache_use_stale = NGX_CONF_BITMASK_SET
                                         |NGX_HTTP_UPSTREAM_FT_OFF;
    }

    if (conf->upstream.cache_use_stale & NGX_HTTP_UPSTREAM_FT_ERROR) {
        conf->upstream.cache_use_stale |= NGX_HTTP_UPSTREAM_FT_NOLIVE;
    }

    if (conf->upstream.cache_methods == 0) {
        conf->upstream.cache_methods = prev->upstream.cache_methods;
    }

    conf->upstream.cache_methods |= NGX_HTTP_GET|NGX_HTTP_HEAD;

    ngx_conf_merge_ptr_value(conf->upstream.cache_bypass,
                             prev->upstream.cache_bypass, NULL);

    ngx_conf_merge_ptr_value(conf->upstream.no_cache,
                             prev->upstream.no_cache, NULL);

    ngx_conf_merge_ptr_value(conf->upstream.cache_valid,
                             prev->upstream.cache_valid, NULL);

    if (conf->cache_key.value.data == NULL) {
        conf->cache_key = prev->cache_key;
    }

    ngx_conf_merge_value(conf->upstream.cache_lock,
                              prev->upstream.cache_lock, 0);

    ngx_conf_merge_msec_value(conf->upstream.cache_lock_timeout,
                              prev->upstream.cache_lock_timeout, 5000);

    ngx_conf_merge_msec_value(conf->upstream.cache_lock_age,
                              prev->upstream.cache_lock_age, 5000);

    ngx_conf_merge_value(conf->upstream.cache_revalidate,
                              prev->upstream.cache_revalidate, 0);

    ngx_conf_merge_value(conf->upstream.cache_convert_head,
                              prev->upstream.cache_convert_head, 1);

    ngx_conf_merge_value(conf->upstream.cache_background_update,
                              prev->upstream.cache_background_update, 0);

#endif

    ngx_conf_merge_value(conf->upstream.pass_request_headers,
                              prev->upstream.pass_request_headers, 1);
    ngx_conf_merge_value(conf->upstream.pass_request_body,
                              prev->upstream.pass_request_body, 1);

    ngx_conf_merge_value(conf->upstream.intercept_errors,
                              prev->upstream.intercept_errors, 0);

#if (NGX_HTTP_SSL)

    if (ngx_http_proxy_merge_ssl(cf, conf, prev) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->upstream.ssl_session_reuse,
                              prev->upstream.ssl_session_reuse, 1);

    ngx_conf_merge_bitmask_value(conf->ssl_protocols, prev->ssl_protocols,
                                 (NGX_CONF_BITMASK_SET
                                  |NGX_SSL_TLSv1|NGX_SSL_TLSv1_1
                                  |NGX_SSL_TLSv1_2|NGX_SSL_TLSv1_3));

    ngx_conf_merge_str_value(conf->ssl_ciphers, prev->ssl_ciphers,
                             "DEFAULT");

    ngx_conf_merge_ptr_value(conf->upstream.ssl_name,
                              prev->upstream.ssl_name, NULL);
    ngx_conf_merge_value(conf->upstream.ssl_server_name,
                              prev->upstream.ssl_server_name, 0);
    ngx_conf_merge_value(conf->upstream.ssl_verify,
                              prev->upstream.ssl_verify, 0);
    ngx_conf_merge_uint_value(conf->ssl_verify_depth,
                              prev->ssl_verify_depth, 1);
    ngx_conf_merge_str_value(conf->ssl_trusted_certificate,
                              prev->ssl_trusted_certificate, "");
    ngx_conf_merge_str_value(conf->ssl_crl, prev->ssl_crl, "");

    ngx_conf_merge_ptr_value(conf->upstream.ssl_certificate,
                              prev->upstream.ssl_certificate, NULL);
    ngx_conf_merge_ptr_value(conf->upstream.ssl_certificate_key,
                              prev->upstream.ssl_certificate_key, NULL);
    ngx_conf_merge_ptr_value(conf->upstream.ssl_passwords,
                              prev->upstream.ssl_passwords, NULL);

    ngx_conf_merge_ptr_value(conf->ssl_conf_commands,
                              prev->ssl_conf_commands, NULL);

    if (conf->ssl && ngx_http_proxy_set_ssl(cf, conf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

#endif

    ngx_conf_merge_ptr_value(conf->method, prev->method, NULL);

    ngx_conf_merge_value(conf->redirect, prev->redirect, 1);

    if (conf->redirect) {

        if (conf->redirects == NULL) {
            conf->redirects = prev->redirects;
        }

        if (conf->redirects == NULL && conf->url.data) {

            conf->redirects = ngx_array_create(cf->pool, 1,
                                             sizeof(ngx_http_proxy_rewrite_t));
            if (conf->redirects == NULL) {
                return NGX_CONF_ERROR;
            }

            pr = ngx_array_push(conf->redirects);
            if (pr == NULL) {
                return NGX_CONF_ERROR;
            }

            ngx_memzero(&pr->pattern.complex,
                        sizeof(ngx_http_complex_value_t));

            ngx_memzero(&pr->replacement, sizeof(ngx_http_complex_value_t));

            pr->handler = ngx_http_proxy_rewrite_complex_handler;

            if (conf->vars.uri.len) {
                pr->pattern.complex.value = conf->url;
                pr->replacement.value = conf->location;

            } else {
                pr->pattern.complex.value.len = conf->url.len
                                                + sizeof("/") - 1;

                p = ngx_pnalloc(cf->pool, pr->pattern.complex.value.len);
                if (p == NULL) {
                    return NGX_CONF_ERROR;
                }

                pr->pattern.complex.value.data = p;

                p = ngx_cpymem(p, conf->url.data, conf->url.len);
                *p = '/';

                ngx_str_set(&pr->replacement.value, "/");
            }
        }
    }

    ngx_conf_merge_ptr_value(conf->cookie_domains, prev->cookie_domains, NULL);

    ngx_conf_merge_ptr_value(conf->cookie_paths, prev->cookie_paths, NULL);

    ngx_conf_merge_ptr_value(conf->cookie_flags, prev->cookie_flags, NULL);

    ngx_conf_merge_uint_value(conf->http_version, prev->http_version,
                              NGX_HTTP_VERSION_10);

    ngx_conf_merge_uint_value(conf->headers_hash_max_size,
                              prev->headers_hash_max_size, 512);

    ngx_conf_merge_uint_value(conf->headers_hash_bucket_size,
                              prev->headers_hash_bucket_size, 64);

    conf->headers_hash_bucket_size = ngx_align(conf->headers_hash_bucket_size,
                                               ngx_cacheline_size);

    hash.max_size = conf->headers_hash_max_size;
    hash.bucket_size = conf->headers_hash_bucket_size;
    hash.name = "proxy_headers_hash";

    if (ngx_http_upstream_hide_headers_hash(cf, &conf->upstream,
            &prev->upstream, ngx_http_proxy_hide_headers, &hash)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    if (clcf->noname
        && conf->upstream.upstream == NULL && conf->proxy_lengths == NULL)
    {
        conf->upstream.upstream = prev->upstream.upstream;
        conf->location = prev->location;
        conf->vars = prev->vars;

        conf->proxy_lengths = prev->proxy_lengths;
        conf->proxy_values = prev->proxy_values;

#if (NGX_HTTP_SSL)
        conf->ssl = prev->ssl;
#endif
    }

    if (clcf->lmt_excpt && clcf->handler == NULL
        && (conf->upstream.upstream || conf->proxy_lengths))
    {
        clcf->handler = ngx_http_proxy_handler;
    }

    if (conf->body_source.data == NULL) {
        conf->body_flushes = prev->body_flushes;
        conf->body_source = prev->body_source;
        conf->body_lengths = prev->body_lengths;
        conf->body_values = prev->body_values;
    }

    if (conf->body_source.data && conf->body_lengths == NULL) {

        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = &conf->body_source;
        sc.flushes = &conf->body_flushes;
        sc.lengths = &conf->body_lengths;
        sc.values = &conf->body_values;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    ngx_conf_merge_ptr_value(conf->headers_source, prev->headers_source, NULL);

    if (conf->headers_source == prev->headers_source) {
        conf->headers = prev->headers;
#if (NGX_HTTP_CACHE)
        conf->headers_cache = prev->headers_cache;
#endif
    }

    rc = ngx_http_proxy_init_headers(cf, conf, &conf->headers,
                                     ngx_http_proxy_headers);
    if (rc != NGX_OK) {
        return NGX_CONF_ERROR;
    }

#if (NGX_HTTP_CACHE)

    if (conf->upstream.cache) {
        rc = ngx_http_proxy_init_headers(cf, conf, &conf->headers_cache,
                                         ngx_http_proxy_cache_headers);
        if (rc != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

#endif

    /*
     * special handling to preserve conf->headers in the "http" section
     * to inherit it to all servers
     */

    if (prev->headers.hash.buckets == NULL
        && conf->headers_source == prev->headers_source)
    {
        prev->headers = conf->headers;
#if (NGX_HTTP_CACHE)
        prev->headers_cache = conf->headers_cache;
#endif
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_proxy_init_headers(ngx_conf_t *cf, ngx_http_proxy_loc_conf_t *conf,
    ngx_http_proxy_headers_t *headers, ngx_keyval_t *default_headers)
{
    u_char                       *p;
    size_t                        size;
    uintptr_t                    *code;
    ngx_uint_t                    i;
    ngx_array_t                   headers_names, headers_merged;
    ngx_keyval_t                 *src, *s, *h;
    ngx_hash_key_t               *hk;
    ngx_hash_init_t               hash;
    ngx_http_script_compile_t     sc;
    ngx_http_script_copy_code_t  *copy;

    // 如果哈希表已经初始化，直接返回
    if (headers->hash.buckets) {
        return NGX_OK;
    }

    // 初始化临时数组用于存储头部名称
    if (ngx_array_init(&headers_names, cf->temp_pool, 4, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 初始化临时数组用于存储合并后的头部
    if (ngx_array_init(&headers_merged, cf->temp_pool, 4, sizeof(ngx_keyval_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 创建用于存储头部长度的数组
    headers->lengths = ngx_array_create(cf->pool, 64, 1);
    if (headers->lengths == NULL) {
        return NGX_ERROR;
    }

    // 创建用于存储头部值的数组
    headers->values = ngx_array_create(cf->pool, 512, 1);
    if (headers->values == NULL) {
        return NGX_ERROR;
    }

    // 如果配置中有自定义头部，将其添加到合并数组中
    if (conf->headers_source) {
        src = conf->headers_source->elts;
        for (i = 0; i < conf->headers_source->nelts; i++) {
            s = ngx_array_push(&headers_merged);
            if (s == NULL) {
                return NGX_ERROR;
            }
            *s = src[i];
        }
    }

    // 添加默认头部，如果没有被自定义头部覆盖
    h = default_headers;
    while (h->key.len) {
        src = headers_merged.elts;
        for (i = 0; i < headers_merged.nelts; i++) {
            if (ngx_strcasecmp(h->key.data, src[i].key.data) == 0) {
                goto next;
            }
        }

        s = ngx_array_push(&headers_merged);
        if (s == NULL) {
            return NGX_ERROR;
        }
        *s = *h;

    next:
        h++;
    }

    // 处理合并后的头部
    src = headers_merged.elts;
    for (i = 0; i < headers_merged.nelts; i++) {
        // 添加头部名称到哈希表键数组
        hk = ngx_array_push(&headers_names);
        if (hk == NULL) {
            return NGX_ERROR;
        }

        hk->key = src[i].key;
        hk->key_hash = ngx_hash_key_lc(src[i].key.data, src[i].key.len);
        hk->value = (void *) 1;

        if (src[i].value.len == 0) {
            continue;
        }

        // 添加头部键的长度到lengths数组
        copy = ngx_array_push_n(headers->lengths,
                                sizeof(ngx_http_script_copy_code_t));
        if (copy == NULL) {
            return NGX_ERROR;
        }

        copy->code = (ngx_http_script_code_pt) (void *)
                                                 ngx_http_script_copy_len_code;
        copy->len = src[i].key.len;

        // 计算并分配存储头部键的空间
        size = (sizeof(ngx_http_script_copy_code_t)
                + src[i].key.len + sizeof(uintptr_t) - 1)
               & ~(sizeof(uintptr_t) - 1);

        copy = ngx_array_push_n(headers->values, size);
        if (copy == NULL) {
            return NGX_ERROR;
        }

        copy->code = ngx_http_script_copy_code;
        copy->len = src[i].key.len;

        // 复制头部键到分配的空间
        p = (u_char *) copy + sizeof(ngx_http_script_copy_code_t);
        ngx_memcpy(p, src[i].key.data, src[i].key.len);

        // 编译头部值
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = &src[i].value;
        sc.flushes = &headers->flushes;
        sc.lengths = &headers->lengths;
        sc.values = &headers->values;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_ERROR;
        }

        // 添加NULL标记到lengths和values数组
        code = ngx_array_push_n(headers->lengths, sizeof(uintptr_t));
        if (code == NULL) {
            return NGX_ERROR;
        }
        *code = (uintptr_t) NULL;

        code = ngx_array_push_n(headers->values, sizeof(uintptr_t));
        if (code == NULL) {
            return NGX_ERROR;
        }
        *code = (uintptr_t) NULL;
    }

    // 在lengths数组末尾添加NULL标记
    code = ngx_array_push_n(headers->lengths, sizeof(uintptr_t));
    if (code == NULL) {
        return NGX_ERROR;
    }
    *code = (uintptr_t) NULL;

    // 初始化哈希表
    hash.hash = &headers->hash;
    hash.key = ngx_hash_key_lc;
    hash.max_size = conf->headers_hash_max_size;
    hash.bucket_size = conf->headers_hash_bucket_size;
    hash.name = "proxy_headers_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    return ngx_hash_init(&hash, headers_names.elts, headers_names.nelts);
}


/**
 * @brief 处理proxy_pass指令的函数
 * @param cf 配置上下文
 * @param cmd 命令结构
 * @param conf 模块配置
 * @return char* 成功返回NGX_CONF_OK，失败返回错误信息
 */
static char *
ngx_http_proxy_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    size_t                      add;
    u_short                     port;
    ngx_str_t                  *value, *url;
    ngx_url_t                   u;
    ngx_uint_t                  n;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_script_compile_t   sc;

    // 检查是否重复配置
    if (plcf->upstream.upstream || plcf->proxy_lengths) {
        return "is duplicate";
    }

    // 获取核心模块的location配置
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    // 设置处理函数
    clcf->handler = ngx_http_proxy_handler;

    // 如果location以'/'结尾，设置自动重定向
    if (clcf->name.len && clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

    value = cf->args->elts;
    url = &value[1];

    // 计算URL中的变量数量
    n = ngx_http_script_variables_count(url);

    if (n) {
        // 如果URL包含变量，编译脚本
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = url;
        sc.lengths = &plcf->proxy_lengths;
        sc.values = &plcf->proxy_values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

#if (NGX_HTTP_SSL)
        plcf->ssl = 1;
#endif

        return NGX_CONF_OK;
    }

    // 解析URL前缀
    if (ngx_strncasecmp(url->data, (u_char *) "http://", 7) == 0) {
        add = 7;
        port = 80;
    } else if (ngx_strncasecmp(url->data, (u_char *) "https://", 8) == 0) {
#if (NGX_HTTP_SSL)
        plcf->ssl = 1;
        add = 8;
        port = 443;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "https protocol requires SSL support");
        return NGX_CONF_ERROR;
#endif
    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid URL prefix");
        return NGX_CONF_ERROR;
    }

    // 初始化URL解析结构
    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url.len = url->len - add;
    u.url.data = url->data + add;
    u.default_port = port;
    u.uri_part = 1;
    u.no_resolve = 1;

    // 添加上游服务器
    plcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
    if (plcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }

    // 设置schema和key_start
    plcf->vars.schema.len = add;
    plcf->vars.schema.data = url->data;
    plcf->vars.key_start = plcf->vars.schema;

    // 设置其他变量
    ngx_http_proxy_set_vars(&u, &plcf->vars);

    plcf->location = clcf->name;

    // 处理特殊location情况
    if (clcf->named
#if (NGX_PCRE)
        || clcf->regex
#endif
        || clcf->noname)
    {
        if (plcf->vars.uri.len) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"proxy_pass\" cannot have URI part in "
                               "location given by regular expression, "
                               "or inside named location, "
                               "or inside \"if\" statement, "
                               "or inside \"limit_except\" block");
            return NGX_CONF_ERROR;
        }

        plcf->location.len = 0;
    }

    // 保存完整URL
    plcf->url = *url;

    return NGX_CONF_OK;
}


static char *
ngx_http_proxy_redirect(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    u_char                            *p;
    ngx_str_t                         *value;
    ngx_http_proxy_rewrite_t          *pr;
    ngx_http_compile_complex_value_t   ccv;

    // 检查是否已经设置过重定向
    if (plcf->redirect == 0) {
        return "is duplicate";
    }

    plcf->redirect = 1;

    value = cf->args->elts;

    // 处理只有两个参数的情况
    if (cf->args->nelts == 2) {
        // 如果第二个参数是"off"，禁用重定向
        if (ngx_strcmp(value[1].data, "off") == 0) {

            if (plcf->redirects) {
                return "is duplicate";
            }

            plcf->redirect = 0;
            return NGX_CONF_OK;
        }

        // 如果第二个参数不是"default"，报错
        if (ngx_strcmp(value[1].data, "default") != 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid parameter \"%V\"", &value[1]);
            return NGX_CONF_ERROR;
        }
    }

    // 创建重定向数组（如果还没有创建）
    if (plcf->redirects == NULL) {
        plcf->redirects = ngx_array_create(cf->pool, 1,
                                           sizeof(ngx_http_proxy_rewrite_t));
        if (plcf->redirects == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    // 添加新的重定向规则
    pr = ngx_array_push(plcf->redirects);
    if (pr == NULL) {
        return NGX_CONF_ERROR;
    }

    // 处理默认重定向
    if (cf->args->nelts == 2
        && ngx_strcmp(value[1].data, "default") == 0)
    {
        // 检查是否与变量一起使用
        if (plcf->proxy_lengths) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"proxy_redirect default\" cannot be used "
                               "with \"proxy_pass\" directive with variables");
            return NGX_CONF_ERROR;
        }

        // 检查是否在proxy_pass之后
        if (plcf->url.data == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"proxy_redirect default\" should be placed "
                               "after the \"proxy_pass\" directive");
            return NGX_CONF_ERROR;
        }

        pr->handler = ngx_http_proxy_rewrite_complex_handler;

        ngx_memzero(&pr->pattern.complex, sizeof(ngx_http_complex_value_t));

        ngx_memzero(&pr->replacement, sizeof(ngx_http_complex_value_t));

        // 设置重定向模式和替换
        if (plcf->vars.uri.len) {
            pr->pattern.complex.value = plcf->url;
            pr->replacement.value = plcf->location;

        } else {
            pr->pattern.complex.value.len = plcf->url.len + sizeof("/") - 1;

            p = ngx_pnalloc(cf->pool, pr->pattern.complex.value.len);
            if (p == NULL) {
                return NGX_CONF_ERROR;
            }

            pr->pattern.complex.value.data = p;

            p = ngx_cpymem(p, plcf->url.data, plcf->url.len);
            *p = '/';

            ngx_str_set(&pr->replacement.value, "/");
        }

        return NGX_CONF_OK;
    }

    // 处理正则表达式重定向
    if (value[1].data[0] == '~') {
        value[1].len--;
        value[1].data++;

        if (value[1].data[0] == '*') {
            value[1].len--;
            value[1].data++;

            if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 1) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else {
            if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }

    } else {
        // 处理普通重定向
        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[1];
        ccv.complex_value = &pr->pattern.complex;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        pr->handler = ngx_http_proxy_rewrite_complex_handler;
    }

    // 编译替换值
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &pr->replacement;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_proxy_cookie_domain(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                         *value;
    ngx_http_proxy_rewrite_t          *pr;
    ngx_http_compile_complex_value_t   ccv;

    // 检查是否已经设置过cookie域
    if (plcf->cookie_domains == NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 处理只有两个参数的情况
    if (cf->args->nelts == 2) {

        // 如果第二个参数是"off"，禁用cookie域重写
        if (ngx_strcmp(value[1].data, "off") == 0) {

            if (plcf->cookie_domains != NGX_CONF_UNSET_PTR) {
                return "is duplicate";
            }

            plcf->cookie_domains = NULL;
            return NGX_CONF_OK;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }

    // 创建cookie域数组（如果还没有创建）
    if (plcf->cookie_domains == NGX_CONF_UNSET_PTR) {
        plcf->cookie_domains = ngx_array_create(cf->pool, 1,
                                     sizeof(ngx_http_proxy_rewrite_t));
        if (plcf->cookie_domains == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    // 添加新的cookie域重写规则
    pr = ngx_array_push(plcf->cookie_domains);
    if (pr == NULL) {
        return NGX_CONF_ERROR;
    }

    // 处理正则表达式cookie域重写
    if (value[1].data[0] == '~') {
        value[1].len--;
        value[1].data++;

        if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 1) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

    } else {
        // 处理普通cookie域重写

        // 如果域名以点开头，去掉点
        if (value[1].data[0] == '.') {
            value[1].len--;
            value[1].data++;
        }

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[1];
        ccv.complex_value = &pr->pattern.complex;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        pr->handler = ngx_http_proxy_rewrite_domain_handler;

        // 如果替换域名以点开头，去掉点
        if (value[2].data[0] == '.') {
            value[2].len--;
            value[2].data++;
        }
    }

    // 编译替换值
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &pr->replacement;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_proxy_cookie_path(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                         *value;
    ngx_http_proxy_rewrite_t          *pr;
    ngx_http_compile_complex_value_t   ccv;

    // 检查是否已经配置过cookie路径
    if (plcf->cookie_paths == NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 处理只有两个参数的情况
    if (cf->args->nelts == 2) {

        // 如果第二个参数是"off"
        if (ngx_strcmp(value[1].data, "off") == 0) {

            if (plcf->cookie_paths != NGX_CONF_UNSET_PTR) {
                return "is duplicate";
            }

            plcf->cookie_paths = NULL;
            return NGX_CONF_OK;
        }

        // 如果第二个参数不是"off"，则报错
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }

    // 初始化cookie_paths数组
    if (plcf->cookie_paths == NGX_CONF_UNSET_PTR) {
        plcf->cookie_paths = ngx_array_create(cf->pool, 1,
                                     sizeof(ngx_http_proxy_rewrite_t));
        if (plcf->cookie_paths == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    // 添加新的重写规则
    pr = ngx_array_push(plcf->cookie_paths);
    if (pr == NULL) {
        return NGX_CONF_ERROR;
    }

    // 处理正则表达式匹配
    if (value[1].data[0] == '~') {
        value[1].len--;
        value[1].data++;

        // 处理带*的正则表达式
        if (value[1].data[0] == '*') {
            value[1].len--;
            value[1].data++;

            if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 1) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else {
            if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }

    } else {
        // 处理普通字符串匹配

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[1];
        ccv.complex_value = &pr->pattern.complex;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        pr->handler = ngx_http_proxy_rewrite_complex_handler;
    }

    // 编译替换值
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &pr->replacement;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_proxy_cookie_flags(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                         *value;
    ngx_uint_t                         i;
    ngx_http_complex_value_t          *cv;
    ngx_http_proxy_cookie_flags_t     *pcf;
    ngx_http_compile_complex_value_t   ccv;
#if (NGX_PCRE)
    ngx_regex_compile_t                rc;
    u_char                             errstr[NGX_MAX_CONF_ERRSTR];
#endif

    // 检查是否已经配置过cookie标志
    if (plcf->cookie_flags == NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 处理只有两个参数的情况
    if (cf->args->nelts == 2) {

        // 如果第二个参数是"off"
        if (ngx_strcmp(value[1].data, "off") == 0) {

            if (plcf->cookie_flags != NGX_CONF_UNSET_PTR) {
                return "is duplicate";
            }

            plcf->cookie_flags = NULL;
            return NGX_CONF_OK;
        }

        // 如果第二个参数不是"off"，则报错
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }

    // 初始化cookie_flags数组
    if (plcf->cookie_flags == NGX_CONF_UNSET_PTR) {
        plcf->cookie_flags = ngx_array_create(cf->pool, 1,
                                        sizeof(ngx_http_proxy_cookie_flags_t));
        if (plcf->cookie_flags == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    // 添加新的cookie标志规则
    pcf = ngx_array_push(plcf->cookie_flags);
    if (pcf == NULL) {
        return NGX_CONF_ERROR;
    }

    pcf->regex = 0;

    // 处理正则表达式匹配
    if (value[1].data[0] == '~') {
        value[1].len--;
        value[1].data++;

#if (NGX_PCRE)
        ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

        rc.pattern = value[1];
        rc.err.len = NGX_MAX_CONF_ERRSTR;
        rc.err.data = errstr;
        rc.options = NGX_REGEX_CASELESS;

        pcf->cookie.regex = ngx_http_regex_compile(cf, &rc);
        if (pcf->cookie.regex == NULL) {
            return NGX_CONF_ERROR;
        }

        pcf->regex = 1;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "using regex \"%V\" requires PCRE library",
                           &value[1]);
        return NGX_CONF_ERROR;
#endif

    } else {
        // 处理普通字符串匹配

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[1];
        ccv.complex_value = &pcf->cookie.complex;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    // 初始化标志值数组
    if (ngx_array_init(&pcf->flags_values, cf->pool, cf->args->nelts - 2,
                       sizeof(ngx_http_complex_value_t))
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    // 编译所有标志值
    for (i = 2; i < cf->args->nelts; i++) {

        cv = ngx_array_push(&pcf->flags_values);
        if (cv == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[i];
        ccv.complex_value = cv;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_proxy_rewrite_regex(ngx_conf_t *cf, ngx_http_proxy_rewrite_t *pr,
    ngx_str_t *regex, ngx_uint_t caseless)
{
#if (NGX_PCRE)
    u_char               errstr[NGX_MAX_CONF_ERRSTR];
    ngx_regex_compile_t  rc;

    // 初始化正则表达式编译结构体
    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pattern = *regex;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

    // 如果需要忽略大小写，设置相应选项
    if (caseless) {
        rc.options = NGX_REGEX_CASELESS;
    }

    // 编译正则表达式
    pr->pattern.regex = ngx_http_regex_compile(cf, &rc);
    if (pr->pattern.regex == NULL) {
        return NGX_ERROR;
    }

    // 设置处理函数
    pr->handler = ngx_http_proxy_rewrite_regex_handler;

    return NGX_OK;

#else

    // 如果没有PCRE库支持，记录错误并返回
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "using regex \"%V\" requires PCRE library", regex);
    return NGX_ERROR;

#endif
}


static char *
ngx_http_proxy_store(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                  *value;
    ngx_http_script_compile_t   sc;

    // 检查是否重复配置
    if (plcf->upstream.store != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 如果配置为"off"，禁用存储
    if (ngx_strcmp(value[1].data, "off") == 0) {
        plcf->upstream.store = 0;
        return NGX_CONF_OK;
    }

#if (NGX_HTTP_CACHE)
    // 检查是否与缓存配置冲突
    if (plcf->upstream.cache > 0) {
        return "is incompatible with \"proxy_cache\"";
    }
#endif

    // 启用存储
    plcf->upstream.store = 1;

    // 如果配置为"on"，直接返回
    if (ngx_strcmp(value[1].data, "on") == 0) {
        return NGX_CONF_OK;
    }

    // 包含终止符'\0'到脚本中
    value[1].len++;

    // 初始化脚本编译结构体
    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

    sc.cf = cf;
    sc.source = &value[1];
    sc.lengths = &plcf->upstream.store_lengths;
    sc.values = &plcf->upstream.store_values;
    sc.variables = ngx_http_script_variables_count(&value[1]);
    sc.complete_lengths = 1;
    sc.complete_values = 1;

    // 编译脚本
    if (ngx_http_script_compile(&sc) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


#if (NGX_HTTP_CACHE)

static char *
ngx_http_proxy_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                         *value;
    ngx_http_complex_value_t           cv;
    ngx_http_compile_complex_value_t   ccv;

    value = cf->args->elts;

    // 检查是否重复配置
    if (plcf->upstream.cache != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    // 如果配置为"off"，禁用缓存
    if (ngx_strcmp(value[1].data, "off") == 0) {
        plcf->upstream.cache = 0;
        return NGX_CONF_OK;
    }

    // 检查是否与存储配置冲突
    if (plcf->upstream.store > 0) {
        return "is incompatible with \"proxy_store\"";
    }

    // 启用缓存
    plcf->upstream.cache = 1;

    // 初始化复杂值编译结构体
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &cv;

    // 编译复杂值
    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    // 如果是变量，保存复杂值
    if (cv.lengths != NULL) {

        plcf->upstream.cache_value = ngx_palloc(cf->pool,
                                             sizeof(ngx_http_complex_value_t));
        if (plcf->upstream.cache_value == NULL) {
            return NGX_CONF_ERROR;
        }

        *plcf->upstream.cache_value = cv;

        return NGX_CONF_OK;
    }

    // 如果是静态值，添加共享内存
    plcf->upstream.cache_zone = ngx_shared_memory_add(cf, &value[1], 0,
                                                      &ngx_http_proxy_module);
    if (plcf->upstream.cache_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_proxy_cache_key(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                         *value;
    ngx_http_compile_complex_value_t   ccv;

    value = cf->args->elts;

    // 检查是否已经设置了缓存键
    if (plcf->cache_key.value.data) {
        return "is duplicate";
    }

    // 初始化复杂值编译结构体
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &plcf->cache_key;

    // 编译复杂值
    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

#endif


#if (NGX_HTTP_SSL)

static char *
ngx_http_proxy_ssl_password_file(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t  *value;

    // 检查是否已经设置了SSL密码
    if (plcf->upstream.ssl_passwords != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 读取SSL密码文件
    plcf->upstream.ssl_passwords = ngx_ssl_read_password_file(cf, &value[1]);

    if (plcf->upstream.ssl_passwords == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

#endif


static char *
ngx_http_proxy_lowat_check(ngx_conf_t *cf, void *post, void *data)
{
#if (NGX_FREEBSD)
    ssize_t *np = data;

    // 检查发送低水位标记是否小于系统设置的TCP发送空间
    if ((u_long) *np >= ngx_freebsd_net_inet_tcp_sendspace) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"proxy_send_lowat\" must be less than %d "
                           "(sysctl net.inet.tcp.sendspace)",
                           ngx_freebsd_net_inet_tcp_sendspace);

        return NGX_CONF_ERROR;
    }

#elif !(NGX_HAVE_SO_SNDLOWAT)
    ssize_t *np = data;

    // 如果不支持SO_SNDLOWAT，记录警告并忽略设置
    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"proxy_send_lowat\" is not supported, ignored");

    *np = 0;

#endif

    return NGX_CONF_OK;
}


#if (NGX_HTTP_SSL)

static char *
ngx_http_proxy_ssl_conf_command_check(ngx_conf_t *cf, void *post, void *data)
{
#ifndef SSL_CONF_FLAG_FILE
    return "is not supported on this platform";
#else
    return NGX_CONF_OK;
#endif
}


static ngx_int_t
ngx_http_proxy_merge_ssl(ngx_conf_t *cf, ngx_http_proxy_loc_conf_t *conf,
    ngx_http_proxy_loc_conf_t *prev)
{
    ngx_uint_t  preserve;

    // 检查是否需要合并SSL配置
    if (conf->ssl_protocols == 0
        && conf->ssl_ciphers.data == NULL
        && conf->upstream.ssl_certificate == NGX_CONF_UNSET_PTR
        && conf->upstream.ssl_certificate_key == NGX_CONF_UNSET_PTR
        && conf->upstream.ssl_passwords == NGX_CONF_UNSET_PTR
        && conf->upstream.ssl_verify == NGX_CONF_UNSET
        && conf->ssl_verify_depth == NGX_CONF_UNSET_UINT
        && conf->ssl_trusted_certificate.data == NULL
        && conf->ssl_crl.data == NULL
        && conf->upstream.ssl_session_reuse == NGX_CONF_UNSET
        && conf->ssl_conf_commands == NGX_CONF_UNSET_PTR)
    {
        if (prev->upstream.ssl) {
            conf->upstream.ssl = prev->upstream.ssl;
            return NGX_OK;
        }

        preserve = 1;

    } else {
        preserve = 0;
    }

    // 分配SSL结构体内存
    conf->upstream.ssl = ngx_pcalloc(cf->pool, sizeof(ngx_ssl_t));
    if (conf->upstream.ssl == NULL) {
        return NGX_ERROR;
    }

    conf->upstream.ssl->log = cf->log;

    /*
     * 特殊处理以在"http"部分中保留conf->upstream.ssl
     * 以便继承到所有服务器
     */

    if (preserve) {
        prev->upstream.ssl = conf->upstream.ssl;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_set_ssl(ngx_conf_t *cf, ngx_http_proxy_loc_conf_t *plcf)
{
    ngx_pool_cleanup_t  *cln;

    // 如果SSL上下文已经存在，直接返回
    if (plcf->upstream.ssl->ctx) {
        return NGX_OK;
    }

    // 创建SSL上下文
    if (ngx_ssl_create(plcf->upstream.ssl, plcf->ssl_protocols, NULL)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 添加清理回调函数
    cln = ngx_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        ngx_ssl_cleanup_ctx(plcf->upstream.ssl);
        return NGX_ERROR;
    }

    cln->handler = ngx_ssl_cleanup_ctx;
    cln->data = plcf->upstream.ssl;

    // 设置SSL密码套件
    if (ngx_ssl_ciphers(cf, plcf->upstream.ssl, &plcf->ssl_ciphers, 0)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 处理SSL证书和密钥
    if (plcf->upstream.ssl_certificate
        && plcf->upstream.ssl_certificate->value.len)
    {
        // 检查是否设置了证书密钥
        if (plcf->upstream.ssl_certificate_key == NULL) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                          "no \"proxy_ssl_certificate_key\" is defined "
                          "for certificate \"%V\"",
                          &plcf->upstream.ssl_certificate->value);
            return NGX_ERROR;
        }

        // 处理动态SSL证书
        if (plcf->upstream.ssl_certificate->lengths
            || plcf->upstream.ssl_certificate_key->lengths)
        {
            plcf->upstream.ssl_passwords =
                  ngx_ssl_preserve_passwords(cf, plcf->upstream.ssl_passwords);
            if (plcf->upstream.ssl_passwords == NULL) {
                return NGX_ERROR;
            }

        } else {
            // 设置静态SSL证书
            if (ngx_ssl_certificate(cf, plcf->upstream.ssl,
                                    &plcf->upstream.ssl_certificate->value,
                                    &plcf->upstream.ssl_certificate_key->value,
                                    plcf->upstream.ssl_passwords)
                != NGX_OK)
            {
                return NGX_ERROR;
            }
        }
    }

    // 处理SSL验证
    if (plcf->upstream.ssl_verify) {
        if (plcf->ssl_trusted_certificate.len == 0) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "no proxy_ssl_trusted_certificate for proxy_ssl_verify");
            return NGX_ERROR;
        }

        // 设置受信任的证书
        if (ngx_ssl_trusted_certificate(cf, plcf->upstream.ssl,
                                        &plcf->ssl_trusted_certificate,
                                        plcf->ssl_verify_depth)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        // 设置证书吊销列表
        if (ngx_ssl_crl(cf, plcf->upstream.ssl, &plcf->ssl_crl) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    // 设置客户端会话缓存
    if (ngx_ssl_client_session_cache(cf, plcf->upstream.ssl,
                                     plcf->upstream.ssl_session_reuse)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 设置SSL配置命令
    if (ngx_ssl_conf_commands(cf, plcf->upstream.ssl, plcf->ssl_conf_commands)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}

#endif


static void
ngx_http_proxy_set_vars(ngx_url_t *u, ngx_http_proxy_vars_t *v)
{
    if (u->family != AF_UNIX) {
        // 处理非Unix域套接字

        if (u->no_port || u->port == u->default_port) {
            // 如果没有指定端口或使用默认端口

            v->host_header = u->host;

            if (u->default_port == 80) {
                ngx_str_set(&v->port, "80");
            } else {
                ngx_str_set(&v->port, "443");
            }

        } else {
            // 如果指定了非默认端口
            v->host_header.len = u->host.len + 1 + u->port_text.len;
            v->host_header.data = u->host.data;
            v->port = u->port_text;
        }

        v->key_start.len += v->host_header.len;

    } else {
        // 处理Unix域套接字
        ngx_str_set(&v->host_header, "localhost");
        ngx_str_null(&v->port);
        v->key_start.len += sizeof("unix:") - 1 + u->host.len + 1;
    }

    v->uri = u->uri;
}
