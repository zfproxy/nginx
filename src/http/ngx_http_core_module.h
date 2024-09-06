
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_CORE_H_INCLUDED_
#define _NGX_HTTP_CORE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#if (NGX_THREADS)
#include <ngx_thread_pool.h>
#elif (NGX_COMPAT)
typedef struct ngx_thread_pool_s  ngx_thread_pool_t;
#endif


/* 定义用于配置代理请求的gzip压缩行为的标志 */

/* 禁用对代理请求的gzip压缩 */
#define NGX_HTTP_GZIP_PROXIED_OFF       0x0002

/* 对已过期的代理响应启用gzip压缩 */
#define NGX_HTTP_GZIP_PROXIED_EXPIRED   0x0004

/* 对标记为不可缓存的代理响应启用gzip压缩 */
#define NGX_HTTP_GZIP_PROXIED_NO_CACHE  0x0008

/* 对标记为不可存储的代理响应启用gzip压缩 */
#define NGX_HTTP_GZIP_PROXIED_NO_STORE  0x0010

/* 对私有（非共享）代理响应启用gzip压缩 */
#define NGX_HTTP_GZIP_PROXIED_PRIVATE   0x0020

/* 对没有Last-Modified头的代理响应启用gzip压缩 */
#define NGX_HTTP_GZIP_PROXIED_NO_LM     0x0040

/* 对没有ETag头的代理响应启用gzip压缩 */
#define NGX_HTTP_GZIP_PROXIED_NO_ETAG   0x0080

/* 对经过身份验证的请求的代理响应启用gzip压缩 */
#define NGX_HTTP_GZIP_PROXIED_AUTH      0x0100

/* 对所有代理请求启用gzip压缩 */
#define NGX_HTTP_GZIP_PROXIED_ANY       0x0200


/*
 * 定义 NGX_HTTP_AIO_OFF 常量
 *
 * 此常量用于配置 Nginx 的异步 I/O (AIO) 功能
 * 当设置为此值 (0) 时，表示关闭异步 I/O 功能
 * 在此模式下，Nginx 将使用传统的同步 I/O 操作
 */
#define NGX_HTTP_AIO_OFF                0
/*
 * 定义 NGX_HTTP_AIO_ON 常量
 *
 * 此常量用于配置 Nginx 的异步 I/O (AIO) 功能
 * 当设置为此值 (1) 时，表示启用异步 I/O 功能
 * 在此模式下，Nginx 将使用异步 I/O 操作来提高性能和并发处理能力
 */
#define NGX_HTTP_AIO_ON                 1
/*
 * 定义 NGX_HTTP_AIO_THREADS 常量
 *
 * 此常量用于配置 Nginx 的异步 I/O (AIO) 功能
 * 当设置为此值 (2) 时，表示启用基于线程的异步 I/O 功能
 * 在此模式下，Nginx 将使用专门的线程池来处理异步 I/O 操作，
 * 这可以在某些情况下提供更好的性能和资源利用率
 */
#define NGX_HTTP_AIO_THREADS            2


/*
 * 定义 NGX_HTTP_SATISFY_ALL 常量
 *
 * 此常量用于配置 Nginx 的 satisfy 指令
 * 当设置为此值 (0) 时，表示所有访问控制模块的检查都必须通过
 * 才能允许访问。这提供了更严格的访问控制策略。
 */
#define NGX_HTTP_SATISFY_ALL            0
/*
 * 定义 NGX_HTTP_SATISFY_ANY 常量
 *
 * 此常量用于配置 Nginx 的 satisfy 指令
 * 当设置为此值 (1) 时，表示只要有一个访问控制模块的检查通过
 * 就允许访问。这提供了更灵活的访问控制策略。
 */
#define NGX_HTTP_SATISFY_ANY            1


/*
 * 定义 NGX_HTTP_LINGERING_OFF 常量
 *
 * 此常量用于配置 Nginx 的 lingering_close 指令
 * 当设置为此值 (0) 时，表示禁用 lingering close 功能
 * lingering close 是一种在关闭连接前等待并读取客户端可能发送的额外数据的机制
 * 禁用此功能可能会导致某些客户端出现问题，但可以提高服务器的响应速度
 */
#define NGX_HTTP_LINGERING_OFF          0
/*
 * 定义 NGX_HTTP_LINGERING_ON 常量
 *
 * 此常量用于配置 Nginx 的 lingering_close 指令
 * 当设置为此值 (1) 时，表示启用 lingering close 功能
 * lingering close 是一种在关闭连接前等待并读取客户端可能发送的额外数据的机制
 * 启用此功能可以提高与某些客户端的兼容性，但可能会略微增加服务器的资源消耗
 */
#define NGX_HTTP_LINGERING_ON           1
/*
 * 定义 NGX_HTTP_LINGERING_ALWAYS 常量
 *
 * 此常量用于配置 Nginx 的 lingering_close 指令
 * 当设置为此值 (2) 时，表示始终启用 lingering close 功能
 * 在这种模式下，Nginx 将始终等待并读取客户端可能发送的额外数据，
 * 无论连接的状态如何。这提供了最高级别的兼容性，
 * 但可能会增加服务器资源的消耗。
 */
#define NGX_HTTP_LINGERING_ALWAYS       2


/*
 * 定义 NGX_HTTP_IMS_OFF 常量
 *
 * 此常量用于配置 Nginx 的 If-Modified-Since (IMS) 处理行为
 * 当设置为此值 (0) 时，表示禁用 If-Modified-Since 头部的处理
 * 在这种模式下，Nginx 将忽略客户端发送的 If-Modified-Since 头部，
 * 始终返回完整的响应内容，而不考虑资源的最后修改时间
 */
#define NGX_HTTP_IMS_OFF                0
/*
 * 定义 NGX_HTTP_IMS_EXACT 常量
 *
 * 此常量用于配置 Nginx 的 If-Modified-Since (IMS) 处理行为
 * 当设置为此值 (1) 时，表示启用精确的 If-Modified-Since 头部处理
 * 在这种模式下，Nginx 将严格比较资源的最后修改时间与 If-Modified-Since 头部中的时间
 * 只有当两个时间完全相同时，才会返回 304 Not Modified 响应
 * 这提供了最精确的缓存验证，但可能会增加服务器的负载
 */
#define NGX_HTTP_IMS_EXACT              1
/*
 * 定义 NGX_HTTP_IMS_BEFORE 常量
 *
 * 此常量用于配置 Nginx 的 If-Modified-Since (IMS) 处理行为
 * 当设置为此值 (2) 时，表示启用宽松的 If-Modified-Since 头部处理
 * 在这种模式下，Nginx 将比较资源的最后修改时间是否在 If-Modified-Since 头部中的时间之后
 * 如果资源在指定时间之后被修改，则返回完整的响应内容
 * 这种模式提供了一种折中的缓存验证方法，既考虑了时间差异，又不像 EXACT 模式那样严格
 */
#define NGX_HTTP_IMS_BEFORE             2


/*
 * 定义 NGX_HTTP_KEEPALIVE_DISABLE_NONE 常量
 *
 * 此常量用于配置 Nginx 的 keepalive_disable 指令
 * 当设置为此值时，表示不禁用任何浏览器的 HTTP keepalive 功能
 * 0x0002 是一个位掩码，用于在位操作中表示此选项
 */
#define NGX_HTTP_KEEPALIVE_DISABLE_NONE    0x0002

/*
 * 定义 NGX_HTTP_KEEPALIVE_DISABLE_MSIE6 常量
 *
 * 此常量用于配置 Nginx 的 keepalive_disable 指令
 * 当设置为此值时，表示禁用 Internet Explorer 6 浏览器的 HTTP keepalive 功能
 * 0x0004 是一个位掩码，用于在位操作中表示此选项
 * 这是因为 IE6 在处理 keepalive 连接时存在一些已知问题
 */
#define NGX_HTTP_KEEPALIVE_DISABLE_MSIE6   0x0004

/*
 * 定义 NGX_HTTP_KEEPALIVE_DISABLE_SAFARI 常量
 *
 * 此常量用于配置 Nginx 的 keepalive_disable 指令
 * 当设置为此值时，表示禁用 Safari 浏览器的 HTTP keepalive 功能
 * 0x0008 是一个位掩码，用于在位操作中表示此选项
 * 这可能是为了解决某些版本的 Safari 浏览器在处理 keepalive 连接时的特定问题
 */
#define NGX_HTTP_KEEPALIVE_DISABLE_SAFARI  0x0008


/*
 * 定义 NGX_HTTP_SERVER_TOKENS_OFF 常量
 * 
 * 此常量用于配置 Nginx 的 server_tokens 指令
 * 当设置为此值时，Nginx 将不会在响应头和错误页面中
 * 显示服务器版本信息，从而提高安全性
 */
#define NGX_HTTP_SERVER_TOKENS_OFF      0
/*
 * 定义 NGX_HTTP_SERVER_TOKENS_ON 常量
 *
 * 此常量用于配置 Nginx 的 server_tokens 指令
 * 当设置为此值时，Nginx 将在响应头和错误页面中
 * 显示服务器版本信息，这是默认行为
 * 但可能会泄露服务器信息，从安全角度考虑可能不推荐
 */
#define NGX_HTTP_SERVER_TOKENS_ON       1
/*
 * 定义 NGX_HTTP_SERVER_TOKENS_BUILD 常量
 *
 * 此常量用于配置 Nginx 的 server_tokens 指令
 * 当设置为此值时，Nginx 将在响应头和错误页面中
 * 显示服务器版本信息以及编译信息
 * 这提供了最详细的服务器信息，但从安全角度考虑可能不推荐
 */
#define NGX_HTTP_SERVER_TOKENS_BUILD    2


/*
 * 定义 ngx_http_location_tree_node_t 类型
 *
 * 这是一个类型定义，将 struct ngx_http_location_tree_node_s 定义为 ngx_http_location_tree_node_t
 * 这个结构体用于表示 Nginx HTTP 模块中的位置树节点
 * 位置树是 Nginx 用于快速匹配 URI 的数据结构
 * 通过这种方式定义类型可以简化代码，使其更易读和维护
 *
 * 注意：这个结构体的具体实现可能在其他文件中定义
 * 这里只是声明了类型，使得其他部分的代码可以使用这个类型
 * 位置树在 Nginx 的路由匹配中扮演着重要角色，能够高效地处理复杂的 URI 匹配规则
 */
typedef struct ngx_http_location_tree_node_s  ngx_http_location_tree_node_t;
/*
 * 定义 ngx_http_core_loc_conf_t 类型
 *
 * 这是一个类型定义，将 struct ngx_http_core_loc_conf_s 定义为 ngx_http_core_loc_conf_t
 * 这个结构体用于表示 Nginx HTTP 模块中的位置配置
 * 位置配置包含了特定 location 块的各种设置和指令
 * 通过这种方式定义类型可以简化代码，使其更易读和维护
 */
typedef struct ngx_http_core_loc_conf_s  ngx_http_core_loc_conf_t;


/*
 * HTTP监听选项结构体
 *
 * 此结构体定义了HTTP服务器监听端口的各种选项和配置。
 * 它包含了诸如套接字地址、SSL配置、HTTP/2支持等信息。
 * 这些选项用于配置Nginx如何监听和接受incoming HTTP连接。
 */
typedef struct {
    struct sockaddr           *sockaddr;    /* 套接字地址结构指针 */
    socklen_t                  socklen;     /* 套接字地址结构长度 */
    ngx_str_t                  addr_text;   /* 地址的文本表示 */

    unsigned                   set:1;             /* 是否已设置 */
    unsigned                   default_server:1;  /* 是否为默认服务器 */
    unsigned                   bind:1;            /* 是否绑定地址 */
    unsigned                   wildcard:1;        /* 是否使用通配符地址 */
    unsigned                   ssl:1;             /* 是否启用SSL */
    unsigned                   http2:1;           /* 是否支持HTTP/2 */
    unsigned                   quic:1;            /* 是否支持QUIC */
#if (NGX_HAVE_INET6)
    unsigned                   ipv6only:1;        /* 是否仅支持IPv6 */
#endif
    unsigned                   deferred_accept:1; /* 是否启用延迟接受 */
    unsigned                   reuseport:1;       /* 是否启用端口重用 */
    unsigned                   so_keepalive:2;    /* TCP keepalive选项 */
    unsigned                   proxy_protocol:1;  /* 是否启用代理协议 */

    int                        backlog;     /* 连接队列长度 */
    int                        rcvbuf;      /* 接收缓冲区大小 */
    int                        sndbuf;      /* 发送缓冲区大小 */
    int                        type;        /* 套接字类型 */
#if (NGX_HAVE_SETFIB)
    int                        setfib;      /* 设置FIB（转发信息库） */
#endif
#if (NGX_HAVE_TCP_FASTOPEN)
    int                        fastopen;    /* TCP快速打开选项 */
#endif
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int                        tcp_keepidle;   /* TCP keepalive空闲时间 */
    int                        tcp_keepintvl;  /* TCP keepalive间隔 */
    int                        tcp_keepcnt;    /* TCP keepalive探测次数 */
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    char                      *accept_filter;  /* 接受过滤器名称 */
#endif
} ngx_http_listen_opt_t;


/*
 * HTTP请求处理阶段枚举
 *
 * 此枚举定义了HTTP请求处理的各个阶段。
 * Nginx按照这些阶段的顺序处理每个HTTP请求，
 * 允许模块在特定阶段注册处理函数。
 */
typedef enum {
    NGX_HTTP_POST_READ_PHASE = 0,    /* 读取请求头之后的处理阶段 */

    NGX_HTTP_SERVER_REWRITE_PHASE,   /* 服务器级别的URL重写阶段 */

    NGX_HTTP_FIND_CONFIG_PHASE,      /* 查找请求对应的location配置阶段 */
    NGX_HTTP_REWRITE_PHASE,          /* location级别的URL重写阶段 */
    NGX_HTTP_POST_REWRITE_PHASE,     /* 重写完成后的处理阶段 */

    NGX_HTTP_PREACCESS_PHASE,        /* 访问权限检查前的预处理阶段 */

    NGX_HTTP_ACCESS_PHASE,           /* 访问权限检查阶段 */
    NGX_HTTP_POST_ACCESS_PHASE,      /* 访问权限检查后的处理阶段 */

    NGX_HTTP_PRECONTENT_PHASE,       /* 生成响应内容前的预处理阶段 */

    NGX_HTTP_CONTENT_PHASE,          /* 生成响应内容的阶段 */

    NGX_HTTP_LOG_PHASE               /* 日志记录阶段 */
} ngx_http_phases;

/*
 * HTTP阶段处理器结构体前向声明
 *
 * 此前向声明定义了ngx_http_phase_handler_t类型，
 * 它是ngx_http_phase_handler_s结构体的别名。
 * 这种声明方式允许在结构体完整定义之前使用该类型，
 * 有助于解决循环依赖问题，并提高代码的模块化程度。
 */
typedef struct ngx_http_phase_handler_s  ngx_http_phase_handler_t;

/*
 * HTTP阶段处理器函数指针类型定义
 *
 * @param r HTTP请求结构体指针
 * @param ph HTTP阶段处理器结构体指针
 * @return ngx_int_t 返回处理结果的状态码
 *
 * 此函数指针类型用于定义HTTP请求处理各个阶段的处理器函数。
 * 每个阶段处理器接收当前的HTTP请求和阶段处理器结构体作为参数，
 * 执行特定的处理逻辑，并返回处理结果的状态码。
 * 这允许模块开发者为不同的处理阶段实现自定义的处理逻辑。
 */
typedef ngx_int_t (*ngx_http_phase_handler_pt)(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);

/*
 * HTTP阶段处理器结构体
 *
 * 此结构体定义了HTTP请求处理过程中单个阶段的处理器。
 * 它包含了检查器函数、处理器函数和下一个处理器的索引，
 * 用于控制请求在不同阶段之间的流转和处理。
 */
struct ngx_http_phase_handler_s {
    ngx_http_phase_handler_pt  checker;
    ngx_http_handler_pt        handler;
    ngx_uint_t                 next;
};


/*
 * HTTP阶段引擎结构体
 *
 * 此结构体用于管理HTTP请求处理的各个阶段。
 * 它包含了处理器数组和重写索引，用于控制请求处理的流程。
 */
typedef struct {
    ngx_http_phase_handler_t  *handlers;
    ngx_uint_t                 server_rewrite_index;
    ngx_uint_t                 location_rewrite_index;
} ngx_http_phase_engine_t;


/*
 * HTTP阶段结构体
 *
 * 此结构体用于定义HTTP请求处理的单个阶段。
 * 它包含一个handlers数组，用于存储该阶段中所有的处理器。
 * 每个HTTP模块可以在不同的处理阶段注册自己的处理器，
 * 以实现特定的功能或修改请求/响应。
 */
typedef struct {
    ngx_array_t                handlers;
} ngx_http_phase_t;


/*
 * HTTP核心主配置结构体
 *
 * 此结构体用于存储和管理HTTP模块的全局配置信息。
 * 它包含了服务器列表、处理阶段引擎、哈希表、变量等重要的HTTP配置数据。
 * 这个结构体在Nginx的HTTP模块初始化和请求处理过程中起着核心作用。
 */
typedef struct {
    ngx_array_t                servers;         /* 存储ngx_http_core_srv_conf_t类型的服务器配置 */

    ngx_http_phase_engine_t    phase_engine;    /* HTTP请求处理阶段引擎 */

    ngx_hash_t                 headers_in_hash; /* 请求头哈希表 */

    ngx_hash_t                 variables_hash;  /* 变量哈希表 */

    ngx_array_t                variables;         /* 存储ngx_http_variable_t类型的变量数组 */
    ngx_array_t                prefix_variables;  /* 存储ngx_http_variable_t类型的前缀变量数组 */
    ngx_uint_t                 ncaptures;         /* 正则表达式捕获组数量 */

    ngx_uint_t                 server_names_hash_max_size;     /* 服务器名称哈希表最大大小 */
    ngx_uint_t                 server_names_hash_bucket_size;  /* 服务器名称哈希表桶大小 */

    ngx_uint_t                 variables_hash_max_size;        /* 变量哈希表最大大小 */
    ngx_uint_t                 variables_hash_bucket_size;     /* 变量哈希表桶大小 */

    ngx_hash_keys_arrays_t    *variables_keys;  /* 变量键数组 */

    ngx_array_t               *ports;           /* 监听端口数组 */

    ngx_http_phase_t           phases[NGX_HTTP_LOG_PHASE + 1]; /* HTTP请求处理阶段数组 */
} ngx_http_core_main_conf_t;


/*
 * HTTP服务器配置结构体
 *
 * 此结构体用于存储和管理单个HTTP服务器的配置信息。
 * 它包含了服务器名称、上下文、连接池大小、请求处理参数等重要配置。
 * 在Nginx处理HTTP请求时，这个结构体提供了服务器级别的配置信息。
 */
typedef struct {
    /* 存储ngx_http_server_name_t类型的数组，用于"server_name"指令 */
    ngx_array_t                 server_names;

    /* 服务器上下文 */
    ngx_http_conf_ctx_t        *ctx;

    /* 配置文件名 */
    u_char                     *file_name;
    /* 配置在文件中的行号 */
    ngx_uint_t                  line;

    /* 服务器名称 */
    ngx_str_t                   server_name;

    /* 连接池大小 */
    size_t                      connection_pool_size;
    /* 请求池大小 */
    size_t                      request_pool_size;
    /* 客户端请求头缓冲区大小 */
    size_t                      client_header_buffer_size;

    /* 大型客户端请求头缓冲区 */
    ngx_bufs_t                  large_client_header_buffers;

    /* 客户端请求头超时时间 */
    ngx_msec_t                  client_header_timeout;

    /* 是否忽略无效的请求头 */
    ngx_flag_t                  ignore_invalid_headers;
    /* 是否合并URL中的多个斜杠 */
    ngx_flag_t                  merge_slashes;
    /* 是否允许请求头中使用下划线 */
    ngx_flag_t                  underscores_in_headers;

    /* 是否已配置监听指令 */
    unsigned                    listen:1;
#if (NGX_PCRE)
    /* 是否启用正则表达式捕获 */
    unsigned                    captures:1;
#endif

    /* 命名location配置数组 */
    ngx_http_core_loc_conf_t  **named_locations;
} ngx_http_core_srv_conf_t;


/* list of structures to find core_srv_conf quickly at run time */


/*
 * HTTP服务器名称结构体
 *
 * 此结构体用于存储和管理HTTP服务器的名称配置。
 * 它包含了服务器名称的正则表达式匹配、虚拟服务器配置以及服务器名称字符串。
 * 在处理HTTP请求时，这个结构体用于确定请求应该由哪个虚拟服务器处理。
 */
/**
 * @brief HTTP服务器名称结构体
 *
 * 该结构体用于存储和管理HTTP服务器的名称配置信息。
 */
typedef struct {
#if (NGX_PCRE)
    ngx_http_regex_t          *regex;    /* 用于正则表达式匹配的结构体指针 */
#endif
    ngx_http_core_srv_conf_t  *server;   /* 虚拟服务器配置指针 */
    ngx_str_t                  name;     /* 服务器名称字符串 */
} ngx_http_server_name_t;


/*
 * HTTP虚拟主机名称结构体
 *
 * 此结构体用于管理HTTP服务器的虚拟主机名称配置。
 * 它包含了用于快速查找服务器名称的哈希表，以及正则表达式匹配的相关信息。
 * 这个结构体在处理请求时用于确定应该使用哪个虚拟主机的配置。
 */
typedef struct {
    ngx_hash_combined_t        names;    /* 用于快速查找服务器名称的组合哈希表 */

    ngx_uint_t                 nregex;   /* 正则表达式服务器名称的数量 */
    ngx_http_server_name_t    *regex;    /* 指向正则表达式服务器名称数组的指针 */
} ngx_http_virtual_names_t;


/*
 * HTTP地址配置结构体
 *
 * 此结构体用于存储和管理与特定IP地址和端口相关的HTTP服务器配置信息。
 * 它包含了默认服务器配置、虚拟主机名称配置以及一些标志位，
 * 用于指示该地址是否支持SSL、HTTP/2、QUIC和代理协议等特性。
 */
/**
 * @brief HTTP地址配置结构体
 *
 * 此结构体用于存储和管理与特定IP地址和端口相关的HTTP服务器配置信息。
 */
struct ngx_http_addr_conf_s {
    /* 该地址:端口的默认服务器配置 */
    ngx_http_core_srv_conf_t  *default_server;

    /* 虚拟主机名称配置 */
    ngx_http_virtual_names_t  *virtual_names;

    unsigned                   ssl:1;            /* 是否支持SSL */
    unsigned                   http2:1;          /* 是否支持HTTP/2 */
    unsigned                   quic:1;           /* 是否支持QUIC */
    unsigned                   proxy_protocol:1; /* 是否支持代理协议 */
};


/*
 * HTTP IPv4地址配置结构体
 *
 * 此结构体用于存储和管理与IPv4地址相关的HTTP服务器配置信息。
 * 它包含了IPv4地址和相应的HTTP地址配置。
 * 在处理IPv4连接时，这个结构体用于快速访问特定IP地址的配置。
 */
typedef struct {
    in_addr_t                  addr;
    ngx_http_addr_conf_t       conf;
} ngx_http_in_addr_t;


#if (NGX_HAVE_INET6)

/*
 * IPv6地址配置结构体
 *
 * 此结构体用于存储和管理与IPv6地址相关的HTTP服务器配置信息。
 * 它包含了IPv6地址和相应的HTTP地址配置。
 * 在支持IPv6的系统中，这个结构体用于处理IPv6连接的特定配置。
 */
typedef struct {
    struct in6_addr            addr6;
    ngx_http_addr_conf_t       conf;
} ngx_http_in6_addr_t;

#endif


/*
 * HTTP端口结构体
 *
 * 此结构体定义了HTTP服务器配置中与特定端口相关的信息。
 * 它包含了地址数组和地址数量，用于存储和管理与该端口关联的所有IP地址配置。
 */
typedef struct {
    /* ngx_http_in_addr_t或ngx_http_in6_addr_t类型的指针，用于存储IPv4或IPv6地址配置 */
    void                      *addrs;
    
    /* 存储在addrs中的地址数量 */
    ngx_uint_t                 naddrs;
} ngx_http_port_t;


/*
 * HTTP配置端口结构体
 *
 * 此结构体定义了HTTP服务器配置中与特定端口相关的信息。
 * 它包含了地址族、套接字类型、端口号以及地址数组等重要配置数据。
 * 这个结构体在处理HTTP请求时用于存储和访问端口特定的配置信息。
 */
typedef struct {
    ngx_int_t                  family;    /* 地址族(如AF_INET, AF_INET6) */
    ngx_int_t                  type;      /* 套接字类型(如SOCK_STREAM) */
    in_port_t                  port;      /* 端口号 */
    ngx_array_t                addrs;     /* ngx_http_conf_addr_t类型的数组，存储该端口的所有地址配置 */
} ngx_http_conf_port_t;


/*
 * HTTP配置地址结构体
 *
 * 此结构体定义了HTTP服务器配置中与特定地址相关的信息。
 * 它包含了监听选项、协议设置、服务器名称哈希表等重要配置数据。
 * 这个结构体在处理HTTP请求时用于存储和访问地址特定的配置信息。
 */
typedef struct {
    ngx_http_listen_opt_t      opt;          /* 监听选项配置 */

    unsigned                   protocols:3;  /* 支持的协议标志（如HTTP/1.0, HTTP/1.1, HTTP/2等） */
    unsigned                   protocols_set:1;    /* 协议设置标志 */
    unsigned                   protocols_changed:1;  /* 协议变更标志 */

    ngx_hash_t                 hash;         /* 服务器名称哈希表 */
    ngx_hash_wildcard_t       *wc_head;      /* 通配符服务器名称（前缀匹配）哈希表 */
    ngx_hash_wildcard_t       *wc_tail;      /* 通配符服务器名称（后缀匹配）哈希表 */

#if (NGX_PCRE)
    ngx_uint_t                 nregex;       /* 正则表达式服务器名称数量 */
    ngx_http_server_name_t    *regex;        /* 正则表达式服务器名称数组 */
#endif

    /* 此地址:端口的默认服务器配置 */
    ngx_http_core_srv_conf_t  *default_server;
    ngx_array_t                servers;  /* ngx_http_core_srv_conf_t 类型的数组，存储所有服务器配置 */
} ngx_http_conf_addr_t;


/*
 * HTTP错误页面配置结构体
 *
 * 此结构体定义了HTTP错误页面的配置信息。
 * 它包含了错误状态码、是否覆盖原有配置、错误页面内容以及附加参数等信息。
 * 这个结构体用于自定义HTTP服务器的错误响应页面。
 */
typedef struct {
    ngx_int_t                  status;      /* HTTP错误状态码 */
    ngx_int_t                  overwrite;   /* 是否覆盖原有错误页面配置 */
    ngx_http_complex_value_t   value;       /* 错误页面内容或重定向URL */
    ngx_str_t                  args;        /* 附加到重定向URL的查询参数 */
} ngx_http_err_page_t;


/*
 * HTTP核心位置配置结构体
 *
 * 此结构体定义了HTTP模块的核心位置配置。
 * 它包含了与特定位置（location）相关的各种配置选项和标志。
 * 这个结构体在处理HTTP请求时用于存储和访问位置特定的配置信息。
 */
struct ngx_http_core_loc_conf_s {
    ngx_str_t     name;          /* location名称 */
    ngx_str_t     escaped_name;  /* 转义后的location名称 */

#if (NGX_PCRE)
    ngx_http_regex_t  *regex;    /* 正则表达式匹配 */
#endif

    unsigned      noname:1;   /* "if () {}" 块或 limit_except */
    unsigned      lmt_excpt:1;  /* 是否为limit_except块 */
    unsigned      named:1;    /* 是否为命名location */

    unsigned      exact_match:1;  /* 是否为精确匹配 */
    unsigned      noregex:1;    /* 是否禁用正则表达式 */

    unsigned      auto_redirect:1;  /* 是否自动重定向 */
#if (NGX_HTTP_GZIP)
    unsigned      gzip_disable_msie6:2;  /* 对MSIE6禁用gzip */
    unsigned      gzip_disable_degradation:2;  /* 禁用gzip降级 */
#endif

    ngx_http_location_tree_node_t   *static_locations;  /* 静态location树 */
#if (NGX_PCRE)
    ngx_http_core_loc_conf_t       **regex_locations;  /* 正则location数组 */
#endif

    /* 指向模块特定loc_conf的指针数组 */
    void        **loc_conf;

    uint32_t      limit_except;  /* limit_except指令的标志 */
    void        **limit_except_loc_conf;  /* limit_except块的配置 */

    ngx_http_handler_pt  handler;  /* 请求处理函数 */

    /* 继承别名的包含location的名称长度 */
    size_t        alias;
    ngx_str_t     root;                    /* root或alias指令的值 */
    ngx_str_t     post_action;  /* post_action指令的值 */

    ngx_array_t  *root_lengths;  /* root指令的复杂值长度 */
    ngx_array_t  *root_values;   /* root指令的复杂值 */

    ngx_array_t  *types;  /* MIME类型数组 */
    ngx_hash_t    types_hash;  /* MIME类型哈希表 */
    ngx_str_t     default_type;  /* 默认MIME类型 */

    off_t         client_max_body_size;    /* client_max_body_size指令的值 */
    off_t         directio;                /* directio指令的值 */
    off_t         directio_alignment;      /* directio_alignment指令的值 */

    size_t        client_body_buffer_size; /* client_body_buffer_size指令的值 */
    size_t        send_lowat;              /* send_lowat指令的值 */
    size_t        postpone_output;         /* postpone_output指令的值 */
    size_t        sendfile_max_chunk;      /* sendfile_max_chunk指令的值 */
    size_t        read_ahead;              /* read_ahead指令的值 */
    size_t        subrequest_output_buffer_size;  /* subrequest_output_buffer_size指令的值 */

    ngx_http_complex_value_t  *limit_rate; /* limit_rate指令的复杂值 */
    ngx_http_complex_value_t  *limit_rate_after; /* limit_rate_after指令的复杂值 */

    ngx_msec_t    client_body_timeout;     /* client_body_timeout指令的值 */
    ngx_msec_t    send_timeout;            /* send_timeout指令的值 */
    ngx_msec_t    keepalive_time;          /* keepalive_time指令的值 */
    ngx_msec_t    keepalive_timeout;       /* keepalive_timeout指令的值 */
    ngx_msec_t    lingering_time;          /* lingering_time指令的值 */
    ngx_msec_t    lingering_timeout;       /* lingering_timeout指令的值 */
    ngx_msec_t    resolver_timeout;        /* resolver_timeout指令的值 */
    ngx_msec_t    auth_delay;              /* auth_delay指令的值 */

    ngx_resolver_t  *resolver;             /* resolver指令的值 */

    time_t        keepalive_header;        /* keepalive_timeout指令的header值 */

    ngx_uint_t    keepalive_requests;      /* keepalive_requests指令的值 */
    ngx_uint_t    keepalive_disable;       /* keepalive_disable指令的值 */
    ngx_uint_t    satisfy;                 /* satisfy指令的值 */
    ngx_uint_t    lingering_close;         /* lingering_close指令的值 */
    ngx_uint_t    if_modified_since;       /* if_modified_since指令的值 */
    ngx_uint_t    max_ranges;              /* max_ranges指令的值 */
    ngx_uint_t    client_body_in_file_only; /* client_body_in_file_only指令的值 */

    ngx_flag_t    client_body_in_single_buffer;  /* client_body_in_single_buffer指令的值 */
    ngx_flag_t    internal;                /* internal指令的值 */
    ngx_flag_t    sendfile;                /* sendfile指令的值 */
    ngx_flag_t    aio;                     /* aio指令的值 */
    ngx_flag_t    aio_write;               /* aio_write指令的值 */
    ngx_flag_t    tcp_nopush;              /* tcp_nopush指令的值 */
    ngx_flag_t    tcp_nodelay;             /* tcp_nodelay指令的值 */
    ngx_flag_t    reset_timedout_connection; /* reset_timedout_connection指令的值 */
    ngx_flag_t    absolute_redirect;       /* absolute_redirect指令的值 */
    ngx_flag_t    server_name_in_redirect; /* server_name_in_redirect指令的值 */
    ngx_flag_t    port_in_redirect;        /* port_in_redirect指令的值 */
    ngx_flag_t    msie_padding;            /* msie_padding指令的值 */
    ngx_flag_t    msie_refresh;            /* msie_refresh指令的值 */
    ngx_flag_t    log_not_found;           /* log_not_found指令的值 */
    ngx_flag_t    log_subrequest;          /* log_subrequest指令的值 */
    ngx_flag_t    recursive_error_pages;   /* recursive_error_pages指令的值 */
    ngx_uint_t    server_tokens;           /* server_tokens指令的值 */
    ngx_flag_t    chunked_transfer_encoding; /* chunked_transfer_encoding指令的值 */
    ngx_flag_t    etag;                    /* etag指令的值 */

#if (NGX_HTTP_GZIP)
    ngx_flag_t    gzip_vary;               /* gzip_vary指令的值 */

    ngx_uint_t    gzip_http_version;       /* gzip_http_version指令的值 */
    ngx_uint_t    gzip_proxied;            /* gzip_proxied指令的值 */

#if (NGX_PCRE)
    ngx_array_t  *gzip_disable;            /* gzip_disable指令的值 */
#endif
#endif

#if (NGX_THREADS || NGX_COMPAT)
    ngx_thread_pool_t         *thread_pool;  /* 线程池 */
    ngx_http_complex_value_t  *thread_pool_value;  /* 线程池的复杂值 */
#endif

#if (NGX_HAVE_OPENAT)
    ngx_uint_t    disable_symlinks;        /* disable_symlinks指令的值 */
    ngx_http_complex_value_t  *disable_symlinks_from;  /* disable_symlinks_from指令的复杂值 */
#endif

    ngx_array_t  *error_pages;             /* error_page指令的值数组 */

    ngx_path_t   *client_body_temp_path;   /* client_body_temp_path指令的值 */

    ngx_open_file_cache_t  *open_file_cache;  /* 打开文件缓存 */
    time_t        open_file_cache_valid;  /* open_file_cache_valid指令的值 */
    ngx_uint_t    open_file_cache_min_uses;  /* open_file_cache_min_uses指令的值 */
    ngx_flag_t    open_file_cache_errors;  /* open_file_cache_errors指令的值 */
    ngx_flag_t    open_file_cache_events;  /* open_file_cache_events指令的值 */

    ngx_log_t    *error_log;  /* 错误日志 */

    ngx_uint_t    types_hash_max_size;  /* types_hash_max_size指令的值 */
    ngx_uint_t    types_hash_bucket_size;  /* types_hash_bucket_size指令的值 */

    ngx_queue_t  *locations;  /* location队列 */

#if 0
    ngx_http_core_loc_conf_t  *prev_location;  /* 前一个location配置 */
#endif
};


/*
 * HTTP位置队列结构体
 *
 * 此结构体用于管理HTTP服务器中的位置配置。
 * 它包含了位置的精确匹配、包含匹配、名称等信息，
 * 以及用于在队列中组织位置的相关字段。
 */
/**
 * @brief HTTP位置队列结构体
 *
 * 此结构体用于管理HTTP服务器中的位置配置。
 * 它包含了位置的精确匹配、包含匹配、名称等信息，
 * 以及用于在队列中组织位置的相关字段。
 */
typedef struct {
    ngx_queue_t                      queue;      /* 用于将位置组织成队列的队列节点 */
    ngx_http_core_loc_conf_t        *exact;      /* 指向精确匹配的位置配置 */
    ngx_http_core_loc_conf_t        *inclusive;  /* 指向包含匹配的位置配置 */
    ngx_str_t                       *name;       /* 位置的名称 */
    u_char                          *file_name;  /* 定义此位置的配置文件名 */
    ngx_uint_t                       line;       /* 在配置文件中的行号 */
    ngx_queue_t                      list;       /* 用于管理子位置的队列 */
} ngx_http_location_queue_t;


/*
 * HTTP位置树节点结构体
 *
 * 此结构体定义了HTTP位置树中的一个节点。
 * 位置树用于高效地匹配和管理HTTP请求的URI路径。
 * 每个节点包含左右子节点、精确匹配配置、包含匹配配置等信息。
 */
struct ngx_http_location_tree_node_s {
    ngx_http_location_tree_node_t   *left;        /* 左子节点指针 */
    ngx_http_location_tree_node_t   *right;       /* 右子节点指针 */
    ngx_http_location_tree_node_t   *tree;        /* 子树根节点指针 */

    ngx_http_core_loc_conf_t        *exact;       /* 精确匹配的位置配置 */
    ngx_http_core_loc_conf_t        *inclusive;   /* 包含匹配的位置配置 */

    u_short                          len;         /* 名称长度 */
    u_char                           auto_redirect; /* 自动重定向标志 */
    u_char                           name[1];     /* 位置名称（柔性数组成员） */
};


/*
 * 运行HTTP请求处理的各个阶段
 *
 * @param r HTTP请求结构体指针
 *
 * 此函数负责按顺序执行HTTP请求处理的所有阶段。
 * 它会遍历配置的处理阶段，并为每个阶段调用相应的处理函数。
 * 这是HTTP请求处理的核心流程，确保请求按照预定义的顺序经过所有必要的处理步骤。
 */
void ngx_http_core_run_phases(ngx_http_request_t *r);
/*
 * HTTP核心通用阶段处理函数
 *
 * @param r HTTP请求结构体指针
 * @param ph 阶段处理器结构体指针
 * @return NGX_OK 成功完成处理
 *         NGX_DECLINED 拒绝处理，传递给下一个处理器
 *         NGX_ERROR 处理过程中出错
 *
 * 此函数作为HTTP请求处理的通用阶段处理器。
 * 它可以用于处理各种不同的HTTP处理阶段，
 * 提供了一个灵活的框架来执行通用的请求处理逻辑。
 * 具体的行为可能会根据当前的处理阶段和配置而有所不同。
 */
ngx_int_t ngx_http_core_generic_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
/*
 * HTTP核心重写阶段处理函数
 *
 * @param r HTTP请求结构体指针
 * @param ph 阶段处理器结构体指针
 * @return NGX_OK 成功完成重写
 *         NGX_DECLINED 拒绝重写，传递给下一个处理器
 *         NGX_ERROR 处理过程中出错
 *
 * 此函数负责处理HTTP请求的URL重写阶段。
 * 它可能会根据配置的重写规则修改请求的URI，
 * 执行重定向，或者进行其他与URL处理相关的操作。
 * 这个阶段对于实现灵活的URL处理和请求路由非常重要。
 */
ngx_int_t ngx_http_core_rewrite_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
/*
 * HTTP核心配置查找阶段处理函数
 *
 * @param r HTTP请求结构体指针
 * @param ph 阶段处理器结构体指针
 * @return NGX_OK 成功找到配置
 *         NGX_DECLINED 未找到配置，传递给下一个处理器
 *         NGX_ERROR 处理过程中出错
 *
 * 此函数负责在HTTP请求处理的配置查找阶段执行。
 * 它会根据请求的URI和服务器配置，查找并设置适用于该请求的配置。
 * 这个阶段对于确定请求应该如何被处理至关重要，因为它决定了后续阶段使用的配置。
 */
ngx_int_t ngx_http_core_find_config_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
/*
 * HTTP核心后重写阶段处理函数
 *
 * @param r HTTP请求结构体指针
 * @param ph 阶段处理器结构体指针
 * @return NGX_OK 成功处理后重写阶段
 *         NGX_DECLINED 拒绝处理，传递给下一个处理器
 *         NGX_ERROR 处理过程中出错
 *
 * 此函数在URL重写阶段之后执行，用于处理重写后的请求。
 * 它可能会执行额外的URL检查、重定向逻辑或其他必要的后处理操作。
 * 这个阶段为开发者提供了一个在主要重写操作之后但在后续处理之前执行自定义逻辑的机会。
 */
ngx_int_t ngx_http_core_post_rewrite_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
/*
 * HTTP核心访问控制阶段处理函数
 *
 * @param r HTTP请求结构体指针
 * @param ph 阶段处理器结构体指针
 * @return NGX_OK 成功通过访问控制
 *         NGX_DECLINED 拒绝访问，传递给下一个处理器
 *         NGX_ERROR 处理过程中出错
 *
 * 此函数负责处理HTTP请求的访问控制阶段。
 * 它会检查请求是否有权限访问所请求的资源，
 * 可能会执行IP地址检查、认证验证等操作。
 * 这是实现访问控制和安全策略的关键阶段。
 */
ngx_int_t ngx_http_core_access_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
/*
 * HTTP核心后访问阶段处理函数
 *
 * @param r HTTP请求结构体指针
 * @param ph 阶段处理器结构体指针
 * @return NGX_OK 成功处理后访问阶段
 *         NGX_DECLINED 拒绝处理，传递给下一个处理器
 *         NGX_ERROR 处理过程中出错
 *
 * 此函数在访问控制阶段之后执行，用于处理一些后续的访问控制逻辑。
 * 它可能会执行额外的安全检查、日志记录或其他必要的后处理操作。
 * 这个阶段为开发者提供了一个在主要访问控制之后但在内容处理之前执行自定义逻辑的机会。
 */
ngx_int_t ngx_http_core_post_access_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
/*
 * HTTP核心内容处理阶段函数
 *
 * @param r HTTP请求结构体指针
 * @param ph 阶段处理器结构体指针
 * @return NGX_OK 成功处理内容阶段
 *         NGX_DECLINED 拒绝处理，传递给下一个处理器
 *         NGX_ERROR 处理过程中出错
 *
 * 此函数是HTTP请求处理的内容阶段的核心函数。
 * 它负责生成或处理HTTP响应的主体内容。
 * 在这个阶段，可能会调用各种内容处理程序，如静态文件服务、动态内容生成等。
 * 函数会根据请求的具体情况和配置来决定如何生成响应内容。
 */
ngx_int_t ngx_http_core_content_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);


/*
 * 测试HTTP请求的内容类型
 *
 * @param r HTTP请求结构体指针
 * @param types_hash 内容类型哈希表
 * @return 如果找到匹配的内容类型，返回对应的配置指针；否则返回NULL
 *
 * 此函数用于检查HTTP请求的内容类型是否在给定的类型哈希表中。
 * 它通过分析请求的文件扩展名或其他相关信息来确定内容类型。
 * 这个函数在处理HTTP请求时很有用，可以用于内容协商或决定如何处理请求体。
 */
void *ngx_http_test_content_type(ngx_http_request_t *r, ngx_hash_t *types_hash);
/*
 * 设置HTTP响应的内容类型
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功设置内容类型
 *         NGX_ERROR 设置内容类型失败
 *
 * 此函数用于根据请求的URI和配置信息设置HTTP响应的Content-Type头。
 * 它会分析请求的文件扩展名，并根据预定义的MIME类型映射来确定适当的内容类型。
 * 如果无法确定内容类型，则可能会使用默认类型。
 * 这个函数在处理HTTP请求的过程中扮演着重要角色，确保客户端能够正确解释响应内容。
 */
ngx_int_t ngx_http_set_content_type(ngx_http_request_t *r);
/*
 * 设置HTTP请求的文件扩展名
 *
 * @param r HTTP请求结构体指针
 * @return 无返回值
 *
 * 此函数用于分析HTTP请求的URI，并设置相应的文件扩展名。
 * 它会从URI的末尾开始查找最后一个点号，并将点号之后的部分设置为文件扩展名。
 * 这个扩展名信息可以用于后续的内容类型判断和处理。
 */
void ngx_http_set_exten(ngx_http_request_t *r);
/*
 * 设置HTTP响应的ETag头
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功设置ETag
 *         NGX_ERROR 设置ETag失败
 *
 * 此函数用于为HTTP响应生成并设置ETag头。
 * ETag是一个用于缓存验证的HTTP响应头，它允许客户端进行条件性请求。
 * 函数会根据响应内容生成一个唯一标识符，并将其设置为ETag头的值。
 * 这有助于提高缓存效率和减少不必要的数据传输。
 */
ngx_int_t ngx_http_set_etag(ngx_http_request_t *r);
/*
 * 生成弱ETag
 *
 * @param r HTTP请求结构体指针
 * @return 无返回值
 *
 * 此函数用于为HTTP响应生成一个弱ETag。
 * 弱ETag允许内容有细微差异，但在语义上仍被视为相同。
 * 它通常用于动态生成的内容，其中内容可能略有不同但本质上相同。
 * 函数会修改请求结构体中的相关字段来设置弱ETag。
 */
void ngx_http_weak_etag(ngx_http_request_t *r);
/*
 * 发送HTTP响应
 *
 * @param r HTTP请求结构体指针
 * @param status HTTP状态码
 * @param ct 内容类型字符串指针
 * @param cv 复杂值结构体指针，用于生成响应体
 * @return NGX_OK 成功发送响应
 *         NGX_ERROR 发送响应失败
 *
 * 此函数用于构建并发送HTTP响应。它设置响应的状态码、
 * 内容类型，并根据提供的复杂值生成响应体。
 * 这是一个核心函数，用于处理各种HTTP响应场景。
 */
ngx_int_t ngx_http_send_response(ngx_http_request_t *r, ngx_uint_t status,
    ngx_str_t *ct, ngx_http_complex_value_t *cv);
/*
 * 将URI映射到文件系统路径
 *
 * @param r HTTP请求结构体指针
 * @param name 用于存储生成的文件系统路径
 * @param root_length 指向用于存储根路径长度的变量的指针
 * @param reserved 保留的额外空间大小
 * @return 指向生成的文件系统路径的指针，如果失败则返回NULL
 *
 * 此函数将HTTP请求中的URI转换为服务器文件系统上的实际路径。
 * 它考虑了配置的根目录、别名等因素，生成对应的文件系统路径。
 * 生成的路径存储在name参数中，root_length用于记录根路径的长度。
 */
u_char *ngx_http_map_uri_to_path(ngx_http_request_t *r, ngx_str_t *name,
    size_t *root_length, size_t reserved);
/*
 * 获取HTTP基本认证的用户名
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功获取用户名
 *         NGX_ERROR 获取用户名失败
 *         NGX_DECLINED 请求中没有提供认证信息
 *
 * 此函数用于从HTTP请求的Authorization头部提取基本认证的用户名。
 * 它会解析Authorization头部，提取并解码用户名信息。
 * 提取的用户名将被存储在请求结构体中，以便后续的认证处理使用。
 */
ngx_int_t ngx_http_auth_basic_user(ngx_http_request_t *r);
#if (NGX_HTTP_GZIP)
/*
 * 检查是否可以对请求进行gzip压缩
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 可以进行gzip压缩
 *         NGX_DECLINED 不应进行gzip压缩
 *
 * 此函数用于判断是否应该对当前HTTP请求的响应内容进行gzip压缩。
 * 它会检查各种条件，如客户端是否支持gzip、内容类型是否适合压缩等。
 * 这个函数通常在处理响应内容之前被调用，以决定是否启用gzip压缩。
 */
ngx_int_t ngx_http_gzip_ok(ngx_http_request_t *r);
#endif


/*
 * 创建并执行HTTP子请求
 *
 * @param r 父HTTP请求结构体指针
 * @param uri 子请求的URI
 * @param args 子请求的查询参数
 * @param psr 用于存储创建的子请求指针的指针
 * @param ps 子请求完成后的回调处理结构体
 * @param flags 子请求的标志位
 * @return NGX_OK 子请求创建成功
 *         NGX_ERROR 子请求创建失败
 *
 * 此函数用于创建和执行HTTP子请求。子请求是在处理主请求过程中
 * 创建的附加请求，通常用于获取额外的资源或执行辅助操作。
 */
ngx_int_t ngx_http_subrequest(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args, ngx_http_request_t **psr,
    ngx_http_post_subrequest_t *ps, ngx_uint_t flags);
/*
 * 执行HTTP内部重定向
 *
 * @param r HTTP请求结构体指针
 * @param uri 重定向的目标URI
 * @param args 重定向的查询参数
 * @return NGX_OK 重定向成功
 *         NGX_ERROR 重定向失败
 *
 * 此函数用于执行HTTP内部重定向。
 * 内部重定向不会向客户端发送重定向响应，而是在服务器内部处理新的URI。
 * 这对于实现URL重写、内部转发等功能非常有用。
 */
ngx_int_t ngx_http_internal_redirect(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args);
/*
 * 执行命名位置的内部重定向
 *
 * @param r HTTP请求结构体指针
 * @param name 命名位置的名称
 * @return NGX_OK 重定向成功
 *         NGX_ERROR 重定向失败
 *
 * 此函数用于执行到指定命名位置的内部重定向。
 * 命名位置是在Nginx配置中使用@符号定义的特殊位置。
 * 这种重定向不会改变客户端的URL，而是在服务器内部处理请求。
 */
ngx_int_t ngx_http_named_location(ngx_http_request_t *r, ngx_str_t *name);


/*
 * 为HTTP请求添加清理回调函数
 *
 * @param r HTTP请求结构体指针
 * @param size 清理回调函数所需的额外内存大小
 * @return 返回ngx_http_cleanup_t结构体指针，如果分配失败则返回NULL
 *
 * 此函数用于为HTTP请求添加一个清理回调函数。清理回调函数会在请求结束时被调用，
 * 用于释放资源或执行其他清理操作。size参数指定了清理回调函数可能需要的额外内存大小。
 */
ngx_http_cleanup_t *ngx_http_cleanup_add(ngx_http_request_t *r, size_t size);


/*
 * HTTP响应头过滤器函数指针类型定义
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功处理响应头
 *         NGX_ERROR 处理响应头失败
 *
 * 这个函数指针类型用于定义HTTP响应头的过滤器函数。
 * 过滤器可以对响应头进行检查、修改或其他处理。
 * 它在Nginx发送HTTP响应头之前被调用，允许模块对响应头进行自定义处理。
 */
typedef ngx_int_t (*ngx_http_output_header_filter_pt)(ngx_http_request_t *r);
/*
 * HTTP响应体过滤器函数指针类型定义
 *
 * @param r HTTP请求结构体指针
 * @param chain 包含响应体数据的链表
 * @return NGX_OK 成功处理响应体
 *         NGX_ERROR 处理响应体失败
 *
 * 这个函数指针类型用于定义HTTP响应体的过滤器函数。
 * 过滤器可以对响应体进行检查、修改或其他处理。
 * 它在Nginx发送HTTP响应体时被调用，允许模块对响应体进行自定义处理。
 */
typedef ngx_int_t (*ngx_http_output_body_filter_pt)
    (ngx_http_request_t *r, ngx_chain_t *chain);
/*
 * HTTP请求体过滤器函数指针类型定义
 *
 * @param r HTTP请求结构体指针
 * @param chain 包含请求体数据的链表
 * @return NGX_OK 成功处理请求体
 *         NGX_ERROR 处理请求体失败
 *
 * 这个函数指针类型用于定义HTTP请求体的过滤器函数。
 * 过滤器可以对请求体进行检查、修改或其他处理。
 * 它在Nginx处理HTTP请求体时被调用，允许模块对请求体进行自定义处理。
 */
typedef ngx_int_t (*ngx_http_request_body_filter_pt)
    (ngx_http_request_t *r, ngx_chain_t *chain);


/*
 * HTTP输出过滤器函数
 *
 * @param r HTTP请求结构体指针
 * @param chain 包含响应数据的链表
 * @return NGX_OK 成功处理输出
 *         NGX_ERROR 处理输出失败
 *
 * 这个函数是Nginx的主要输出过滤器。它负责处理HTTP响应数据，
 * 并将数据传递给后续的过滤器链。这个函数通常在发送响应体之前被调用，
 * 允许对输出进行最后的处理或修改。
 */
ngx_int_t ngx_http_output_filter(ngx_http_request_t *r, ngx_chain_t *chain);
/*
 * HTTP写入过滤器函数
 *
 * @param r HTTP请求结构体指针
 * @param chain 包含要写入的数据的链表
 * @return NGX_OK 成功写入数据
 *         NGX_ERROR 写入数据失败
 *         NGX_AGAIN 需要稍后重试写入
 *
 * 这个函数是Nginx的核心写入过滤器。它负责将处理后的HTTP响应数据写入到客户端连接。
 * 函数会遍历chain链表，尝试将数据写入到连接的发送缓冲区中。
 * 如果无法立即写入所有数据（例如，由于网络原因），函数会返回NGX_AGAIN，
 * 表示需要稍后重试写入操作。
 */
ngx_int_t ngx_http_write_filter(ngx_http_request_t *r, ngx_chain_t *chain);
/*
 * HTTP请求体保存过滤器函数
 *
 * @param r HTTP请求结构体指针
 * @param chain 包含请求体数据的链表
 * @return ngx_int_t 返回处理结果的状态码
 *
 * 此函数用于过滤和保存HTTP请求体。它在处理请求体时被调用，
 * 可以对请求体进行检查、修改，并将其保存到临时文件或内存中。
 * 这个过滤器通常用于处理大型请求体或实现特定的请求体处理逻辑。
 */
ngx_int_t ngx_http_request_body_save_filter(ngx_http_request_t *r,
    ngx_chain_t *chain);


/*
 * 设置禁用符号链接的函数
 *
 * @param r HTTP请求结构体指针
 * @param clcf HTTP核心位置配置结构体指针
 * @param path 路径字符串指针
 * @param of 打开文件信息结构体指针
 * @return ngx_int_t 返回设置结果的状态码
 *
 * 此函数用于设置是否禁用符号链接。它在处理HTTP请求时被调用，
 * 可以根据配置和请求信息决定是否允许访问符号链接。
 * 这个函数通常用于增强服务器的安全性，防止通过符号链接访问未授权的文件。
 */
ngx_int_t ngx_http_set_disable_symlinks(ngx_http_request_t *r,
    ngx_http_core_loc_conf_t *clcf, ngx_str_t *path, ngx_open_file_info_t *of);

/*
 * 获取转发地址函数
 *
 * @param r HTTP请求结构体指针
 * @param addr 用于存储获取到的地址信息的结构体指针
 * @param headers 请求头部信息
 * @param value 用于存储特定头部值的字符串指针
 * @param proxies 代理服务器列表
 * @param recursive 是否递归处理标志
 * @return ngx_int_t 返回处理结果的状态码
 *
 * 此函数用于获取HTTP请求的转发地址。它在处理经过代理转发的请求时被调用，
 * 可以从请求头部中提取原始客户端的地址信息。这个函数通常用于
 * 在使用反向代理或负载均衡器时正确识别客户端的真实IP地址。
 */
ngx_int_t ngx_http_get_forwarded_addr(ngx_http_request_t *r, ngx_addr_t *addr,
    ngx_table_elt_t *headers, ngx_str_t *value, ngx_array_t *proxies,
    int recursive);

/*
 * 链接多个相同名称的HTTP头部
 *
 * @param r HTTP请求结构体指针
 * @return ngx_int_t 返回处理结果的状态码
 *
 * 此函数用于处理HTTP请求中存在多个相同名称的头部字段的情况。
 * 它将这些头部字段链接在一起，以便于后续处理。
 * 这对于处理某些特殊的HTTP头部（如Set-Cookie）特别有用，
 * 因为这些头部可能在一个请求中出现多次。
 */
ngx_int_t ngx_http_link_multi_headers(ngx_http_request_t *r);


/*
 * 声明 ngx_http_core_module 模块
 *
 * 这是一个外部声明，表示 ngx_http_core_module 是在其他文件中定义的模块。
 * ngx_http_core_module 是 Nginx HTTP 核心模块，它提供了 HTTP 服务器的基本功能。
 * 这个模块负责处理 HTTP 请求的主要流程，包括请求的接收、处理和响应。
 * 通过这个外部声明，其他源文件可以引用和使用这个核心模块。
 */
extern ngx_module_t  ngx_http_core_module;

/*
 * 声明 ngx_http_max_module 变量
 *
 * 这是一个外部声明，表示 ngx_http_max_module 是在其他文件中定义的全局变量。
 * ngx_http_max_module 用于存储已加载的 HTTP 模块的最大数量。
 * 这个变量在 Nginx 的模块系统中起着重要作用，它用于确定模块数组的大小，
 * 并在模块初始化和请求处理过程中用于遍历所有已加载的 HTTP 模块。
 * 通过这个外部声明，其他源文件可以访问和使用这个变量，
 * 从而实现对 HTTP 模块的管理和操作。
 */
extern ngx_uint_t ngx_http_max_module;

/*
 * 声明 ngx_http_core_get_method 变量
 *
 * 这是一个外部声明，表示 ngx_http_core_get_method 是在其他文件中定义的全局变量。
 * ngx_http_core_get_method 是一个 ngx_str_t 类型的变量，用于表示 HTTP GET 方法。
 * 在 Nginx 的 HTTP 模块中，这个变量可能被用于快速比较或设置请求方法。
 * 通过这个外部声明，其他源文件可以访问和使用这个预定义的 GET 方法字符串，
 * 从而在处理 HTTP 请求时提供便利。
 */
extern ngx_str_t  ngx_http_core_get_method;


/*
 * 清除HTTP响应的Content-Length头部
 *
 * @param r HTTP请求结构体指针
 *
 * 此宏用于清除HTTP响应中的Content-Length头部信息。
 * 它将content_length_n设置为-1，并清除content_length指针。
 * 这通常在需要重新计算或移除Content-Length头部时使用。
 */
#define ngx_http_clear_content_length(r)                                      \
                                                                              \
    r->headers_out.content_length_n = -1;                                     \
    if (r->headers_out.content_length) {                                      \
        r->headers_out.content_length->hash = 0;                              \
        r->headers_out.content_length = NULL;                                 \
    }

/*
 * 清除HTTP响应的Accept-Ranges头部
 *
 * @param r HTTP请求结构体指针
 *
 * 此宏用于清除HTTP响应中的Accept-Ranges头部信息。
 * 它将allow_ranges标志设置为0，并清除accept_ranges头部指针。
 * 这通常在需要禁用或重置范围请求支持时使用。
 */
#define ngx_http_clear_accept_ranges(r)                                       \
                                                                              \
    r->allow_ranges = 0;                                                      \
    if (r->headers_out.accept_ranges) {                                       \
        r->headers_out.accept_ranges->hash = 0;                               \
        r->headers_out.accept_ranges = NULL;                                  \
    }

/*
 * 清除HTTP响应的Last-Modified头部
 *
 * @param r HTTP请求结构体指针
 *
 * 此宏用于清除HTTP响应中的Last-Modified头部信息。
 * 它将last_modified_time设置为-1，并清除last_modified头部指针。
 * 这通常在需要重新设置或移除Last-Modified头部时使用，
 * 例如当资源的最后修改时间发生变化或不再适用时。
 */
#define ngx_http_clear_last_modified(r)                                       \
                                                                              \
    r->headers_out.last_modified_time = -1;                                   \
    if (r->headers_out.last_modified) {                                       \
        r->headers_out.last_modified->hash = 0;                               \
        r->headers_out.last_modified = NULL;                                  \
    }

/*
 * 清除HTTP响应的Location头部
 *
 * @param r HTTP请求结构体指针
 *
 * 此宏用于清除HTTP响应中的Location头部信息。
 * 它将location头部指针设置为NULL，并清除其哈希值。
 * 这通常在需要移除或重置重定向URL时使用。
 */
#define ngx_http_clear_location(r)                                            \
                                                                              \
    if (r->headers_out.location) {                                            \
        r->headers_out.location->hash = 0;                                    \
        r->headers_out.location = NULL;                                       \
    }

/*
 * 清除HTTP响应的ETag头部
 *
 * @param r HTTP请求结构体指针
 *
 * 此宏用于清除HTTP响应中的ETag头部信息。
 * 它将etag头部指针设置为NULL，并清除其哈希值。
 * 这通常在需要移除或重置实体标签时使用，
 * 例如当资源内容发生变化或不再需要缓存验证时。
 */
#define ngx_http_clear_etag(r)                                                \
                                                                              \
    if (r->headers_out.etag) {                                                \
        r->headers_out.etag->hash = 0;                                        \
        r->headers_out.etag = NULL;                                           \
    }


#endif /* _NGX_HTTP_CORE_H_INCLUDED_ */
