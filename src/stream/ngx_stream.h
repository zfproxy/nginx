
/*
 * Copyright (C) Roman Arutyunyan
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_STREAM_H_INCLUDED_
#define _NGX_STREAM_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

#if (NGX_STREAM_SSL)
#include <ngx_stream_ssl_module.h>
#endif


typedef struct ngx_stream_session_s  ngx_stream_session_t;


#include <ngx_stream_variables.h>
#include <ngx_stream_script.h>
#include <ngx_stream_upstream.h>
#include <ngx_stream_upstream_round_robin.h>


/* 定义流模块的状态码 */

/* 请求成功 */
#define NGX_STREAM_OK                        200

/* 错误的请求 */
#define NGX_STREAM_BAD_REQUEST               400

/* 禁止访问 */
#define NGX_STREAM_FORBIDDEN                 403

/* 服务器内部错误 */
#define NGX_STREAM_INTERNAL_SERVER_ERROR     500

/* 网关错误 */
#define NGX_STREAM_BAD_GATEWAY               502

/* 服务不可用 */
#define NGX_STREAM_SERVICE_UNAVAILABLE       503


/**
 * @brief 流模块配置上下文结构体
 *
 * 该结构体用于存储流模块的配置信息。
 */
typedef struct {
    void                         **main_conf;  /**< 指向主配置数组的指针 */
    void                         **srv_conf;   /**< 指向服务器配置数组的指针 */
} ngx_stream_conf_ctx_t;

/**
 * @brief 流模块监听选项结构体
 *
 * 该结构体用于存储流模块的监听选项配置信息。
 * 包含了套接字地址、各种标志位以及一些网络相关的参数设置。
 */
typedef struct {
    struct sockaddr               *sockaddr;    /* 指向套接字地址结构的指针 */
    socklen_t                      socklen;     /* 套接字地址结构的长度 */
    ngx_str_t                      addr_text;   /* 地址的文本表示 */

    unsigned                       set:1;             /* 标记是否已设置 */
    unsigned                       default_server:1;  /* 是否为默认服务器 */
    unsigned                       bind:1;            /* 是否绑定地址 */
    unsigned                       wildcard:1;        /* 是否使用通配符地址 */
    unsigned                       ssl:1;             /* 是否启用SSL */
#if (NGX_HAVE_INET6)
    unsigned                       ipv6only:1;        /* 是否仅支持IPv6 */
#endif
    unsigned                       deferred_accept:1; /* 是否启用延迟接受 */
    unsigned                       reuseport:1;       /* 是否启用端口重用 */
    unsigned                       so_keepalive:2;    /* TCP keepalive选项 */
    unsigned                       proxy_protocol:1;  /* 是否启用代理协议 */

    int                            backlog;     /* 监听队列的最大长度 */
    int                            rcvbuf;      /* 接收缓冲区大小 */
    int                            sndbuf;      /* 发送缓冲区大小 */
    int                            type;        /* 套接字类型 */
#if (NGX_HAVE_SETFIB)
    int                            setfib;      /* 设置FIB（转发信息库）*/
#endif
#if (NGX_HAVE_TCP_FASTOPEN)
    int                            fastopen;    /* TCP快速打开选项 */
#endif
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int                            tcp_keepidle;  /* TCP keepalive空闲时间 */
    int                            tcp_keepintvl; /* TCP keepalive间隔 */
    int                            tcp_keepcnt;   /* TCP keepalive探测次数 */
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    char                          *accept_filter; /* 接受过滤器名称 */
#endif
} ngx_stream_listen_opt_t;


/**
 * @brief 定义流模块处理阶段的枚举
 *
 * 这个枚举定义了流模块处理请求时的各个阶段，按照处理顺序排列。
 * 每个阶段都有特定的用途和处理逻辑。
 */
typedef enum {
    NGX_STREAM_POST_ACCEPT_PHASE = 0,  /**< 接受连接后的处理阶段 */
    NGX_STREAM_PREACCESS_PHASE,        /**< 访问控制前的预处理阶段 */
    NGX_STREAM_ACCESS_PHASE,           /**< 访问控制阶段 */
    NGX_STREAM_SSL_PHASE,              /**< SSL/TLS处理阶段 */
    NGX_STREAM_PREREAD_PHASE,          /**< 读取数据前的预处理阶段 */
    NGX_STREAM_CONTENT_PHASE,          /**< 内容处理阶段 */
    NGX_STREAM_LOG_PHASE               /**< 日志记录阶段 */
} ngx_stream_phases;


typedef struct ngx_stream_phase_handler_s  ngx_stream_phase_handler_t;

/**
 * @brief 定义流模块阶段处理器函数指针类型
 *
 * @param s 流会话对象
 * @param ph 阶段处理器对象
 * @return ngx_int_t 处理结果
 */
typedef ngx_int_t (*ngx_stream_phase_handler_pt)(ngx_stream_session_t *s,
    ngx_stream_phase_handler_t *ph);

/**
 * @brief 定义流模块处理器函数指针类型
 *
 * @param s 流会话对象
 * @return ngx_int_t 处理结果
 */
typedef ngx_int_t (*ngx_stream_handler_pt)(ngx_stream_session_t *s);

/**
 * @brief 定义流模块内容处理器函数指针类型
 *
 * @param s 流会话对象
 */
typedef void (*ngx_stream_content_handler_pt)(ngx_stream_session_t *s);


/**
 * @brief 定义流模块阶段处理器结构体
 *
 * 该结构体用于表示流模块处理过程中的一个阶段处理器。
 * 每个阶段处理器包含一个检查函数、一个处理函数和下一个处理器的索引。
 */
struct ngx_stream_phase_handler_s {
    ngx_stream_phase_handler_pt    checker;  /**< 检查函数,用于决定是否执行处理函数 */
    ngx_stream_handler_pt          handler;  /**< 处理函数,执行实际的处理逻辑 */
    ngx_uint_t                     next;     /**< 下一个要执行的处理器的索引 */
};


/**
 * @brief 定义流模块阶段引擎结构体
 *
 * 该结构体用于管理流模块处理过程中的所有阶段处理器。
 * 它包含一个指向阶段处理器数组的指针，这些处理器按顺序执行以完成流处理。
 */
typedef struct {
    ngx_stream_phase_handler_t    *handlers;  /**< 指向阶段处理器数组的指针 */
} ngx_stream_phase_engine_t;


/**
 * @brief 定义流模块阶段结构体
 *
 * 该结构体用于表示流模块处理过程中的一个阶段。
 * 每个阶段包含一个处理器数组，用于存储该阶段的所有处理器。
 */
typedef struct {
    ngx_array_t                    handlers;  /**< 处理器数组，存储该阶段的所有处理器 */
} ngx_stream_phase_t;


/**
 * @brief 流模块核心主配置结构体
 *
 * 该结构体包含了流模块的主要配置信息,用于管理整个流模块的全局设置。
 */
typedef struct {
    ngx_array_t                    servers;     /* ngx_stream_core_srv_conf_t */  /**< 服务器配置数组 */

    ngx_stream_phase_engine_t      phase_engine;  /**< 阶段引擎,用于管理处理阶段 */

    ngx_hash_t                     variables_hash;  /**< 变量哈希表 */

    ngx_array_t                    variables;        /* ngx_stream_variable_t */  /**< 变量数组 */
    ngx_array_t                    prefix_variables; /* ngx_stream_variable_t */  /**< 前缀变量数组 */
    ngx_uint_t                     ncaptures;  /**< 捕获数量 */

    ngx_uint_t                     server_names_hash_max_size;  /**< 服务器名称哈希表最大大小 */
    ngx_uint_t                     server_names_hash_bucket_size;  /**< 服务器名称哈希桶大小 */

    ngx_uint_t                     variables_hash_max_size;  /**< 变量哈希表最大大小 */
    ngx_uint_t                     variables_hash_bucket_size;  /**< 变量哈希桶大小 */

    ngx_hash_keys_arrays_t        *variables_keys;  /**< 变量键数组 */

    ngx_array_t                   *ports;  /**< 端口数组 */

    ngx_stream_phase_t             phases[NGX_STREAM_LOG_PHASE + 1];  /**< 处理阶段数组 */
} ngx_stream_core_main_conf_t;


typedef struct {
    /* 服务器名称数组，存储"server_name"指令配置的服务器名称 */
    ngx_array_t                    server_names;

    /* 内容处理函数指针，用于处理流会话的主要逻辑 */
    ngx_stream_content_handler_pt  handler;

    /* 配置上下文，包含当前服务器块的所有配置信息 */
    ngx_stream_conf_ctx_t         *ctx;

    /* 配置文件名，用于错误日志和调试 */
    u_char                        *file_name;
    /* 配置在文件中的行号，用于错误日志和调试 */
    ngx_uint_t                     line;

    /* 主服务器名称 */
    ngx_str_t                      server_name;

    /* TCP_NODELAY选项标志 */
    ngx_flag_t                     tcp_nodelay;
    /* 预读缓冲区大小 */
    size_t                         preread_buffer_size;
    /* 预读超时时间 */
    ngx_msec_t                     preread_timeout;

    /* 错误日志对象 */
    ngx_log_t                     *error_log;

    /* 解析器超时时间 */
    ngx_msec_t                     resolver_timeout;
    /* 解析器对象 */
    ngx_resolver_t                *resolver;

    /* 代理协议超时时间 */
    ngx_msec_t                     proxy_protocol_timeout;

    /* 是否配置了listen指令的标志 */
    unsigned                       listen:1;
#if (NGX_PCRE)
    /* 是否启用了正则表达式捕获的标志 */
    unsigned                       captures:1;
#endif
} ngx_stream_core_srv_conf_t;


/* list of structures to find core_srv_conf quickly at run time */


/**
 * @brief 定义流模块服务器名称结构体
 *
 * 该结构体用于存储流模块中服务器名称相关的信息。
 */
typedef struct {
#if (NGX_PCRE)
    ngx_stream_regex_t            *regex;    /**< 正则表达式对象指针，用于支持正则匹配的服务器名称 */
#endif
    ngx_stream_core_srv_conf_t    *server;   /**< 指向虚拟服务器配置的指针 */
    ngx_str_t                      name;     /**< 服务器名称字符串 */
} ngx_stream_server_name_t;


/**
 * @brief 定义流模块虚拟名称结构体
 *
 * 该结构体用于存储流模块中虚拟服务器名称相关的信息。
 */
typedef struct {
    ngx_hash_combined_t            names;    /**< 组合哈希表，用于快速查找服务器名称 */

    ngx_uint_t                     nregex;   /**< 正则表达式服务器名称的数量 */
    ngx_stream_server_name_t      *regex;    /**< 指向正则表达式服务器名称数组的指针 */
} ngx_stream_virtual_names_t;


/**
 * @brief 定义流模块地址配置结构体
 *
 * 该结构体用于存储特定地址和端口的服务器配置信息。
 */
typedef struct {
    /* 该地址:端口的默认服务器配置 */
    ngx_stream_core_srv_conf_t    *default_server;

    /* 虚拟服务器名称配置 */
    ngx_stream_virtual_names_t    *virtual_names;

    unsigned                       ssl:1;            /**< 是否启用SSL/TLS */
    unsigned                       proxy_protocol:1; /**< 是否启用代理协议 */
} ngx_stream_addr_conf_t;


/**
 * @brief 定义流模块IPv4地址结构体
 *
 * 该结构体用于存储IPv4地址及其对应的配置信息。
 */
typedef struct {
    in_addr_t                      addr;    /**< IPv4地址 */
    ngx_stream_addr_conf_t         conf;    /**< 对应的地址配置信息 */
} ngx_stream_in_addr_t;


#if (NGX_HAVE_INET6)

/**
 * @brief 定义流模块IPv6地址结构体
 *
 * 该结构体用于存储IPv6地址及其对应的配置信息。
 */
typedef struct {
    struct in6_addr                addr6;    /**< IPv6地址 */
    ngx_stream_addr_conf_t         conf;     /**< 对应的地址配置信息 */
} ngx_stream_in6_addr_t;

#endif


/**
 * @brief 定义流模块端口结构体
 *
 * 该结构体用于存储与特定端口相关的地址信息。
 */
typedef struct {
    /* ngx_stream_in_addr_t 或 ngx_stream_in6_addr_t */
    void                          *addrs;    /**< 指向地址数组的指针，可能是IPv4或IPv6地址 */
    ngx_uint_t                     naddrs;   /**< 地址数组中的地址数量 */
} ngx_stream_port_t;


/**
 * @brief 定义流模块配置端口结构体
 *
 * 该结构体用于存储与特定端口相关的配置信息。
 */
typedef struct {
    int                            family;    /**< 地址族（如 AF_INET, AF_INET6） */
    int                            type;      /**< 套接字类型（如 SOCK_STREAM, SOCK_DGRAM） */
    in_port_t                      port;      /**< 端口号 */
    ngx_array_t                    addrs;     /**< ngx_stream_conf_addr_t 类型的数组，存储与该端口关联的地址配置 */
} ngx_stream_conf_port_t;


/**
 * @brief 定义流模块配置地址结构体
 *
 * 该结构体用于存储与特定地址和端口相关的配置信息。
 */
typedef struct {
    ngx_stream_listen_opt_t        opt;            /**< 监听选项 */

    unsigned                       protocols:3;    /**< 支持的协议标志 */
    unsigned                       protocols_set:1;    /**< 协议是否已设置标志 */
    unsigned                       protocols_changed:1;    /**< 协议是否已更改标志 */

    ngx_hash_t                     hash;           /**< 服务器名称哈希表 */
    ngx_hash_wildcard_t           *wc_head;        /**< 通配符服务器名称（前缀匹配）*/
    ngx_hash_wildcard_t           *wc_tail;        /**< 通配符服务器名称（后缀匹配）*/

#if (NGX_PCRE)
    ngx_uint_t                     nregex;         /**< 正则表达式服务器名称数量 */
    ngx_stream_server_name_t      *regex;          /**< 正则表达式服务器名称数组 */
#endif

    /* 该地址:端口的默认服务器配置 */
    ngx_stream_core_srv_conf_t    *default_server;
    ngx_array_t                    servers;        /**< ngx_stream_core_srv_conf_t 数组 */
} ngx_stream_conf_addr_t;


/**
 * @brief 定义流会话结构体
 *
 * 该结构体包含了一个流会话的所有相关信息和状态。
 */
struct ngx_stream_session_s {
    uint32_t                       signature;         /* "STRM" */  /**< 会话签名，固定为"STRM" */

    ngx_connection_t              *connection;        /**< 指向与该会话关联的连接对象 */

    off_t                          received;          /**< 已接收的字节数 */
    time_t                         start_sec;         /**< 会话开始时间（秒） */
    ngx_msec_t                     start_msec;        /**< 会话开始时间（毫秒） */

    ngx_log_handler_pt             log_handler;       /**< 日志处理函数指针 */

    void                         **ctx;               /**< 模块上下文数组 */
    void                         **main_conf;         /**< 主配置数组 */
    void                         **srv_conf;          /**< 服务器配置数组 */

    ngx_stream_virtual_names_t    *virtual_names;     /**< 虚拟主机名称结构 */

    ngx_stream_upstream_t         *upstream;          /**< 上游服务器信息 */
    ngx_array_t                   *upstream_states;   /**< 上游服务器状态数组 */
                                           /* of ngx_stream_upstream_state_t */
    ngx_stream_variable_value_t   *variables;         /**< 变量值数组 */

#if (NGX_PCRE)
    ngx_uint_t                     ncaptures;         /**< 正则表达式捕获数量 */
    int                           *captures;          /**< 正则表达式捕获结果 */
    u_char                        *captures_data;     /**< 正则表达式捕获数据 */
#endif

    ngx_int_t                      phase_handler;     /**< 当前阶段处理器索引 */
    ngx_uint_t                     status;            /**< 会话状态 */

    unsigned                       ssl:1;             /**< SSL标志 */

    unsigned                       stat_processing:1; /**< 统计处理标志 */

    unsigned                       health_check:1;    /**< 健康检查标志 */

    unsigned                       limit_conn_status:2; /**< 连接限制状态 */
};


/**
 * @brief 定义流模块结构体
 *
 * 该结构体定义了流模块的各个回调函数，用于模块的配置和初始化过程。
 */
typedef struct {
    /**
     * @brief 预配置函数
     * 在配置解析开始前调用，用于初始化一些必要的数据结构
     */
    ngx_int_t                    (*preconfiguration)(ngx_conf_t *cf);

    /**
     * @brief 后配置函数
     * 在所有配置解析完成后调用，用于最终的设置和检查
     */
    ngx_int_t                    (*postconfiguration)(ngx_conf_t *cf);

    /**
     * @brief 创建主配置函数
     * 用于创建模块的主配置结构
     */
    void                        *(*create_main_conf)(ngx_conf_t *cf);

    /**
     * @brief 初始化主配置函数
     * 用于初始化模块的主配置
     */
    char                        *(*init_main_conf)(ngx_conf_t *cf, void *conf);

    /**
     * @brief 创建服务器配置函数
     * 用于创建模块的服务器配置结构
     */
    void                        *(*create_srv_conf)(ngx_conf_t *cf);

    /**
     * @brief 合并服务器配置函数
     * 用于合并不同级别的服务器配置
     */
    char                        *(*merge_srv_conf)(ngx_conf_t *cf, void *prev,
                                                   void *conf);
} ngx_stream_module_t;


/* 定义流模块的标识符，ASCII码为"STRM" */
#define NGX_STREAM_MODULE       0x4d525453     /* "STRM" */

/* 定义流模块配置的类型标志 */
#define NGX_STREAM_MAIN_CONF    0x02000000  /* 主配置 */
#define NGX_STREAM_SRV_CONF     0x04000000  /* 服务器配置 */
#define NGX_STREAM_UPS_CONF     0x08000000  /* 上游配置 */

/* 定义获取流模块配置结构体中主配置和服务器配置的偏移量 */
#define NGX_STREAM_MAIN_CONF_OFFSET  offsetof(ngx_stream_conf_ctx_t, main_conf)
#define NGX_STREAM_SRV_CONF_OFFSET   offsetof(ngx_stream_conf_ctx_t, srv_conf)

/* 定义获取、设置和删除模块上下文的宏 */
#define ngx_stream_get_module_ctx(s, module)   (s)->ctx[module.ctx_index]
#define ngx_stream_set_ctx(s, c, module)       s->ctx[module.ctx_index] = c;
#define ngx_stream_delete_ctx(s, module)       s->ctx[module.ctx_index] = NULL;

/* 定义获取模块主配置和服务器配置的宏 */
#define ngx_stream_get_module_main_conf(s, module)                             \
    (s)->main_conf[module.ctx_index]
#define ngx_stream_get_module_srv_conf(s, module)                              \
    (s)->srv_conf[module.ctx_index]

/* 定义从配置上下文中获取模块主配置和服务器配置的宏 */
#define ngx_stream_conf_get_module_main_conf(cf, module)                       \
    ((ngx_stream_conf_ctx_t *) cf->ctx)->main_conf[module.ctx_index]
#define ngx_stream_conf_get_module_srv_conf(cf, module)                        \
    ((ngx_stream_conf_ctx_t *) cf->ctx)->srv_conf[module.ctx_index]

/* 定义从cycle中获取模块主配置的宏 */
#define ngx_stream_cycle_get_module_main_conf(cycle, module)                   \
    (cycle->conf_ctx[ngx_stream_module.index] ?                                \
        ((ngx_stream_conf_ctx_t *) cycle->conf_ctx[ngx_stream_module.index])   \
            ->main_conf[module.ctx_index]:                                     \
        NULL)

/* 定义流模块写缓冲标志 */
#define NGX_STREAM_WRITE_BUFFERED  0x10


/**
 * @brief 添加监听配置
 * @param cf 配置上下文
 * @param cscf 流模块核心服务器配置
 * @param lsopt 监听选项
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_stream_add_listen(ngx_conf_t *cf,
    ngx_stream_core_srv_conf_t *cscf, ngx_stream_listen_opt_t *lsopt);

/**
 * @brief 运行流模块的所有处理阶段
 * @param s 流会话
 */
void ngx_stream_core_run_phases(ngx_stream_session_t *s);

/**
 * @brief 通用阶段处理函数
 * @param s 流会话
 * @param ph 阶段处理器
 * @return ngx_int_t 处理结果
 */
ngx_int_t ngx_stream_core_generic_phase(ngx_stream_session_t *s,
    ngx_stream_phase_handler_t *ph);

/**
 * @brief 预读阶段处理函数
 * @param s 流会话
 * @param ph 阶段处理器
 * @return ngx_int_t 处理结果
 */
ngx_int_t ngx_stream_core_preread_phase(ngx_stream_session_t *s,
    ngx_stream_phase_handler_t *ph);

/**
 * @brief 内容处理阶段函数
 * @param s 流会话
 * @param ph 阶段处理器
 * @return ngx_int_t 处理结果
 */
ngx_int_t ngx_stream_core_content_phase(ngx_stream_session_t *s,
    ngx_stream_phase_handler_t *ph);

/**
 * @brief 验证主机名是否合法
 *
 * @param host 要验证的主机名
 * @param pool 内存池，用于分配内存
 * @param alloc 是否需要分配新内存的标志
 * @return ngx_int_t 验证结果，NGX_OK表示合法，NGX_ERROR表示非法
 */
ngx_int_t ngx_stream_validate_host(ngx_str_t *host, ngx_pool_t *pool,
    ngx_uint_t alloc);

/**
 * @brief 根据主机名查找对应的虚拟服务器配置
 *
 * @param s 当前的流会话
 * @param host 要查找的主机名
 * @param cscfp 用于存储找到的服务器配置的指针
 * @return ngx_int_t 查找结果，NGX_OK表示成功找到，NGX_ERROR表示未找到
 */
ngx_int_t ngx_stream_find_virtual_server(ngx_stream_session_t *s,
    ngx_str_t *host, ngx_stream_core_srv_conf_t **cscfp);

/* 初始化流连接 */
void ngx_stream_init_connection(ngx_connection_t *c);

/* 处理流会话事件 */
void ngx_stream_session_handler(ngx_event_t *rev);

/* 结束流会话 */
void ngx_stream_finalize_session(ngx_stream_session_t *s, ngx_uint_t rc);


/* 声明流模块的外部变量 */
extern ngx_module_t  ngx_stream_module;

/* 声明流模块的最大数量 */
extern ngx_uint_t    ngx_stream_max_module;

/* 声明流模块的核心模块 */
extern ngx_module_t  ngx_stream_core_module;


/**
 * @brief 定义流模块过滤器函数指针类型
 *
 * @param s 流会话对象
 * @param chain 数据链
 * @param from_upstream 是否来自上游的标志
 * @return ngx_int_t 过滤结果
 */
typedef ngx_int_t (*ngx_stream_filter_pt)(ngx_stream_session_t *s,
    ngx_chain_t *chain, ngx_uint_t from_upstream);


/**
 * @brief 声明顶层流模块过滤器
 *
 * 这是一个全局变量,指向当前注册的最顶层的流模块过滤器函数
 */
extern ngx_stream_filter_pt  ngx_stream_top_filter;


#endif /* _NGX_STREAM_H_INCLUDED_ */
