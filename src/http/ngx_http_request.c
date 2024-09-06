
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_request.c
 *
 * 该文件实现了Nginx处理HTTP请求的核心功能。
 *
 * 支持的功能:
 * 1. HTTP请求的接收和解析
 * 2. HTTP请求头的处理
 * 3. HTTP请求体的读取
 * 4. 请求的路由和处理
 * 5. 响应的生成和发送
 * 6. 请求的生命周期管理
 * 7. 长连接和管道化请求的处理
 * 8. 请求限制和超时处理
 * 9. 错误处理和日志记录
 * 10. 内部重定向和子请求的处理
 *
 * 支持的指令:
 * - client_header_buffer_size: 设置读取客户端请求头的缓冲区大小
 *   语法: client_header_buffer_size size;
 *   默认值: client_header_buffer_size 1k;
 *   上下文: http, server
 *
 * - client_header_timeout: 定义读取客户端请求头的超时时间
 *   语法: client_header_timeout time;
 *   默认值: client_header_timeout 60s;
 *   上下文: http, server
 *
 * - large_client_header_buffers: 设置读取大型客户端请求头的缓冲区数量和大小
 *   语法: large_client_header_buffers number size;
 *   默认值: large_client_header_buffers 4 8k;
 *   上下文: http, server
 *
 * 支持的变量:
 * - $request: 完整的原始请求行
 * - $request_method: 请求方法
 * - $request_uri: 请求URI
 * - $request_length: 请求的总字节数
 * - $request_time: 请求处理时间
 *
 * 使用注意点:
 * 1. 合理配置缓冲区大小，避免频繁的内存分配
 * 2. 注意设置适当的超时时间，防止慢速攻击
 * 3. 在处理大量并发请求时，注意内存使用情况
 * 4. 对于复杂的请求处理逻辑，考虑使用子请求来模块化处理
 * 5. 在处理请求头时注意潜在的安全风险，如头部注入攻击
 * 6. 使用长连接时，注意连接的生命周期管理
 * 7. 在记录日志时，注意敏感信息的保护
 * 8. 对于大文件上传，考虑使用流式处理而非完全缓冲
 * 9. 在高并发环境下，注意请求处理的性能优化
 * 10. 定期检查和更新请求处理逻辑，以适应新的HTTP标准和安全要求
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/*
 * 等待请求处理函数
 * 当连接建立后，等待客户端发送请求数据时调用此函数
 * 参数:
 *   ev: 事件结构体指针，包含了连接相关信息
 */
static void ngx_http_wait_request_handler(ngx_event_t *ev);

/*
 * 分配请求结构体
 * 参数:
 *   c: 连接结构体指针
 * 返回值:
 *   HTTP请求结构体指针
 */
static ngx_http_request_t *ngx_http_alloc_request(ngx_connection_t *c);

/*
 * 处理请求行
 * 参数:
 *   rev: 事件结构体指针，包含了连接相关信息
 */
static void ngx_http_process_request_line(ngx_event_t *rev);

/*
 * 处理请求头
 * 参数:
 *   rev: 事件结构体指针，包含了连接相关信息
 * 功能:
 *   解析和处理HTTP请求头部信息
 */
static void ngx_http_process_request_headers(ngx_event_t *rev);

/*
 * 读取请求头部
 * 参数:
 *   r: HTTP请求结构体指针
 * 返回值:
 *   读取到的头部长度
 */
static ssize_t ngx_http_read_request_header(ngx_http_request_t *r);
/*
 * 分配大型头部缓冲区
 * 参数:
 *   r: HTTP请求结构体指针
 *   request_line: 标志位，指示是否为请求行
 * 返回值:
 *   NGX_OK: 成功
 *   NGX_ERROR: 错误
 * 功能:
 *   为大型HTTP头部分配足够的缓冲区空间
 */
static ngx_int_t ngx_http_alloc_large_header_buffer(ngx_http_request_t *r,
    ngx_uint_t request_line);

/*
 * 处理HTTP请求头部行
 * 参数:
 *   r: HTTP请求结构体指针
 *   h: 头部字段结构体指针
 *   offset: 头部字段在请求结构体中的偏移量
 * 返回值:
 *   NGX_OK: 成功处理
 *   NGX_ERROR: 处理出错
 *   NGX_DECLINED: 拒绝处理该头部
 * 功能:
 *   解析和处理单个HTTP请求头部行
 */
static ngx_int_t ngx_http_process_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/*
 * 处理唯一的HTTP头部行
 * 参数:
 *   r: HTTP请求结构体指针
 *   h: 头部字段结构体指针（在函数内部使用）
 *   offset: 头部字段在请求结构体中的偏移量（在函数内部使用）
 * 返回值:
 *   NGX_OK: 成功处理
 *   NGX_ERROR: 处理出错
 *   NGX_DECLINED: 拒绝处理该头部
 * 功能:
 *   解析和处理只允许出现一次的HTTP请求头部行
 */
static ngx_int_t ngx_http_process_unique_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/*
 * 处理HTTP请求的Host头部
 * 参数:
 *   r: HTTP请求结构体指针
 *   h: Host头部字段结构体指针（在函数内部使用）
 *   offset: Host头部字段在请求结构体中的偏移量（在函数内部使用）
 * 返回值:
 *   NGX_OK: 成功处理
 *   NGX_ERROR: 处理出错
 *   NGX_DECLINED: 拒绝处理该头部
 * 功能:
 *   解析和处理HTTP请求中的Host头部，设置虚拟主机
 */
static ngx_int_t ngx_http_process_host(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/*
 * 处理HTTP请求的Connection头部
 * 参数:
 *   r: HTTP请求结构体指针
 *   h: Connection头部字段结构体指针（在函数内部使用）
 *   offset: Connection头部字段在请求结构体中的偏移量（在函数内部使用）
 * 返回值:
 *   NGX_OK: 成功处理
 *   NGX_ERROR: 处理出错
 *   NGX_DECLINED: 拒绝处理该头部
 * 功能:
 *   解析和处理HTTP请求中的Connection头部，设置连接属性
 */
static ngx_int_t ngx_http_process_connection(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/*
 * 处理HTTP请求的User-Agent头部
 * 参数:
 *   r: HTTP请求结构体指针
 *   h: User-Agent头部字段结构体指针（在函数内部使用）
 *   offset: User-Agent头部字段在请求结构体中的偏移量（在函数内部使用）
 * 返回值:
 *   NGX_OK: 成功处理
 *   NGX_ERROR: 处理出错
 *   NGX_DECLINED: 拒绝处理该头部
 * 功能:
 *   解析和处理HTTP请求中的User-Agent头部，记录客户端信息
 */
static ngx_int_t ngx_http_process_user_agent(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);

/*
 * 查找虚拟服务器
 * 参数:
 *   c: 连接结构体指针
 *   virtual_names: 虚拟主机名称结构体指针
 *   host: 主机名字符串指针
 *   r: HTTP请求结构体指针
 *   cscfp: 核心服务器配置结构体指针的指针
 * 返回值:
 *   NGX_OK: 成功找到匹配的虚拟服务器
 *   NGX_ERROR: 查找过程中出错
 *   NGX_DECLINED: 未找到匹配的虚拟服务器
 * 功能:
 *   根据给定的主机名在虚拟主机列表中查找匹配的服务器配置
 */
static ngx_int_t ngx_http_find_virtual_server(ngx_connection_t *c,
    ngx_http_virtual_names_t *virtual_names, ngx_str_t *host,
    ngx_http_request_t *r, ngx_http_core_srv_conf_t **cscfp);

/*
 * HTTP请求处理函数
 * 参数:
 *   ev: 事件结构体指针，包含了连接相关信息
 * 功能:
 *   处理HTTP请求的主要函数，根据请求的状态执行相应的操作
 */
static void ngx_http_request_handler(ngx_event_t *ev);
/*
 * 终止HTTP请求处理
 * 参数:
 *   r: HTTP请求结构体指针
 *   rc: 终止请求的返回码
 * 功能:
 *   根据给定的返回码终止当前HTTP请求的处理,
 *   清理相关资源并可能关闭连接
 */
static void ngx_http_terminate_request(ngx_http_request_t *r, ngx_int_t rc);
/*
 * HTTP请求终止处理函数
 * 参数:
 *   r: HTTP请求结构体指针
 * 功能:
 *   处理HTTP请求终止时的相关操作，
 *   可能包括清理资源、记录日志等
 */
static void ngx_http_terminate_handler(ngx_http_request_t *r);
/*
 * 完成HTTP连接处理
 * 参数:
 *   r: HTTP请求结构体指针
 * 功能:
 *   完成HTTP连接的处理，可能包括清理资源、记录日志等
 */
static void ngx_http_finalize_connection(ngx_http_request_t *r);

/*
 * 设置HTTP请求的写处理函数
 * 参数:
 *   r: HTTP请求结构体指针
 * 返回值:
 *   NGX_OK: 成功设置写处理函数
 *   NGX_ERROR: 设置过程中出错
 * 功能:
 *   为给定的HTTP请求设置写处理函数，
 *   用于处理响应数据的发送
 */
static ngx_int_t ngx_http_set_write_handler(ngx_http_request_t *r);

/*
 * HTTP写处理函数
 * 参数:
 *   r: HTTP请求结构体指针
 * 功能:
 *   处理HTTP响应数据的发送，
 *   可能包括将响应数据写入连接或发送缓冲区
 */
static void ngx_http_writer(ngx_http_request_t *r);

/*
 * HTTP请求终结器
 * 参数:
 *   r: HTTP请求结构体指针
 * 功能:
 *   处理HTTP请求的终结，可能包括清理资源、记录日志等
 */
static void ngx_http_request_finalizer(ngx_http_request_t *r);

/*
 * 设置HTTP请求的保持活动状态
 * 参数:
 *   r: HTTP请求结构体指针
 * 功能:
 *   设置HTTP请求的保持活动状态，
 *   可能包括设置连接的保持活动状态和相关处理函数
 */
static void ngx_http_set_keepalive(ngx_http_request_t *r);

/*
 * 保持活动处理函数
 * 参数:
 *   ev: 事件结构体指针，包含了连接相关信息
 * 功能:
 *   处理保持活动状态的连接，可能包括发送数据或关闭连接
 */
static void ngx_http_keepalive_handler(ngx_event_t *ev);

/*
 * 设置连接的延迟关闭
 * 参数:
 *   c: 连接结构体指针
 * 功能:
 *   设置连接的延迟关闭，可能包括发送数据或关闭连接
 */
static void ngx_http_set_lingering_close(ngx_connection_t *c);


/*
 * 延迟关闭处理函数
 * 参数:
 *   ev: 事件结构体指针，包含了连接相关信息
 * 功能:
 *   处理延迟关闭的连接，可能包括读取剩余数据或关闭连接
 */
static void ngx_http_lingering_close_handler(ngx_event_t *ev);

/*
 * 执行HTTP请求的后处理操作
 * 参数:
 *   r: HTTP请求结构体指针
 * 返回值:
 *   NGX_OK: 成功执行后处理操作
 *   NGX_ERROR: 执行过程中出错
 */
static ngx_int_t ngx_http_post_action(ngx_http_request_t *r);

/*
 * 记录HTTP请求日志
 * 参数:
 *   r: HTTP请求结构体指针
 * 功能:
 *   记录HTTP请求的日志信息，可能包括请求方法、URI、状态码等
 */
static void ngx_http_log_request(ngx_http_request_t *r);

/*
 * HTTP错误日志记录函数
 * 参数:
 *   log: 日志结构体指针
 *   buf: 用于存储日志信息的缓冲区
 *   len: 缓冲区长度
 * 返回值:
 *   u_char*: 指向写入日志信息后的缓冲区位置的指针
 * 功能:
 *   记录HTTP请求处理过程中的错误信息到日志中
 */
static u_char *ngx_http_log_error(ngx_log_t *log, u_char *buf, size_t len);
/*
 * HTTP错误日志处理函数
 * 参数:
 *   r: 当前HTTP请求结构体指针
 *   sr: 子请求结构体指针（如果有的话）
 *   buf: 用于存储日志信息的缓冲区
 *   len: 缓冲区长度
 * 返回值:
 *   u_char*: 指向写入日志信息后的缓冲区位置的指针
 * 功能:
 *   处理HTTP请求过程中的错误，并将错误信息记录到日志中
 */
static u_char *ngx_http_log_error_handler(ngx_http_request_t *r,
    ngx_http_request_t *sr, u_char *buf, size_t len);

#if (NGX_HTTP_SSL)
/*
 * SSL握手处理函数
 * 参数:
 *   rev: 读事件结构体指针
 * 功能:
 *   处理SSL/TLS握手过程，建立安全连接
 * 说明:
 *   此函数在SSL连接初始化时被调用，用于完成SSL握手
 */
static void ngx_http_ssl_handshake(ngx_event_t *rev);
/*
 * SSL握手处理函数
 * 参数:
 *   c: 连接结构体指针
 * 功能:
 *   处理SSL/TLS握手过程，建立安全连接
 * 说明:
 *   此函数在SSL连接初始化时被调用，用于完成SSL握手
 */
static void ngx_http_ssl_handshake_handler(ngx_connection_t *c);
#endif


/*
 * 客户端错误信息数组
 * 用于存储客户端发送的错误信息
 */
static char *ngx_http_client_errors[] = {

    /* NGX_HTTP_PARSE_INVALID_METHOD */
    "client sent invalid method",

    /* NGX_HTTP_PARSE_INVALID_REQUEST */
    "client sent invalid request",

    /* NGX_HTTP_PARSE_INVALID_VERSION */
    "client sent invalid version",

    /* NGX_HTTP_PARSE_INVALID_09_METHOD */
    "client sent invalid method in HTTP/0.9 request"
};


/*
 * HTTP请求头部处理函数数组
 * 用于处理HTTP请求头部字段
 */
ngx_http_header_t  ngx_http_headers_in[] = {
    /*
     * Host头部处理
     * 用于处理HTTP请求中的Host头部字段
     */
    { ngx_string("Host"), offsetof(ngx_http_headers_in_t, host),
                 ngx_http_process_host },

    /*
     * Connection头部处理
     * 用于处理HTTP请求中的Connection头部字段
     */
    { ngx_string("Connection"), offsetof(ngx_http_headers_in_t, connection),
                 ngx_http_process_connection },

    /*
     * If-Modified-Since头部处理
     * 用于处理HTTP请求中的If-Modified-Since头部字段
     */
    { ngx_string("If-Modified-Since"),
                 offsetof(ngx_http_headers_in_t, if_modified_since),
                 ngx_http_process_unique_header_line },

    /*
     * If-Unmodified-Since头部处理
     * 用于处理HTTP请求中的If-Unmodified-Since头部字段
     */
    { ngx_string("If-Unmodified-Since"),
                 offsetof(ngx_http_headers_in_t, if_unmodified_since),
                 ngx_http_process_unique_header_line },

    /*
     * If-Match头部处理
     * 用于处理HTTP请求中的If-Match头部字段
     */
    { ngx_string("If-Match"),
                 offsetof(ngx_http_headers_in_t, if_match),
                 ngx_http_process_unique_header_line },

    /*
     * If-None-Match头部处理
     * 用于处理HTTP请求中的If-None-Match头部字段
     */
    { ngx_string("If-None-Match"),
                 offsetof(ngx_http_headers_in_t, if_none_match),
                 ngx_http_process_unique_header_line },

    /*
     * User-Agent头部处理
     * 用于处理HTTP请求中的User-Agent头部字段
     */
    { ngx_string("User-Agent"), offsetof(ngx_http_headers_in_t, user_agent),
                 ngx_http_process_user_agent },

    /*
     * Referer头部处理
     * 用于处理HTTP请求中的Referer头部字段
     */
    { ngx_string("Referer"), offsetof(ngx_http_headers_in_t, referer),
                 ngx_http_process_header_line },

    /*
     * Content-Length头部处理
     * 用于处理HTTP请求中的Content-Length头部字段
     */
    { ngx_string("Content-Length"),
                 offsetof(ngx_http_headers_in_t, content_length),
                 ngx_http_process_unique_header_line },

    /*
     * Content-Range头部处理
     * 用于处理HTTP请求中的Content-Range头部字段
     */
    { ngx_string("Content-Range"),
                 offsetof(ngx_http_headers_in_t, content_range),
                 ngx_http_process_unique_header_line },

    /*
     * Content-Type头部处理
     * 用于处理HTTP请求中的Content-Type头部字段
     */
    { ngx_string("Content-Type"),
                 offsetof(ngx_http_headers_in_t, content_type),
                 ngx_http_process_header_line },

    /*
     * Range头部处理
     * 用于处理HTTP请求中的Range头部字段
     */
    { ngx_string("Range"), offsetof(ngx_http_headers_in_t, range),
                 ngx_http_process_header_line },

    /*
     * If-Range头部处理
     * 用于处理HTTP请求中的If-Range头部字段
     */
    { ngx_string("If-Range"),
                 offsetof(ngx_http_headers_in_t, if_range),
                 ngx_http_process_unique_header_line },

    /*
     * Transfer-Encoding头部处理
     * 用于处理HTTP请求中的Transfer-Encoding头部字段
     */
    { ngx_string("Transfer-Encoding"),
                 offsetof(ngx_http_headers_in_t, transfer_encoding),
                 ngx_http_process_unique_header_line },

    /*
     * TE头部处理
     * 用于处理HTTP请求中的TE头部字段
     */
    { ngx_string("TE"),
                 offsetof(ngx_http_headers_in_t, te),
                 ngx_http_process_header_line },

    /*
     * Expect头部处理
     * 用于处理HTTP请求中的Expect头部字段
     */
    { ngx_string("Expect"),
                 offsetof(ngx_http_headers_in_t, expect),
                 ngx_http_process_unique_header_line },

    /*
     * Upgrade头部处理
     * 用于处理HTTP请求中的Upgrade头部字段
     */
    { ngx_string("Upgrade"),
                 offsetof(ngx_http_headers_in_t, upgrade),
                 ngx_http_process_header_line },

#if (NGX_HTTP_GZIP || NGX_HTTP_HEADERS)
    /*
     * Accept-Encoding头部处理
     * 用于处理HTTP请求中的Accept-Encoding头部字段
     */
    { ngx_string("Accept-Encoding"),
                 offsetof(ngx_http_headers_in_t, accept_encoding),
                 ngx_http_process_header_line },

    /*
     * Via头部处理
     * 用于处理HTTP请求中的Via头部字段
     */
    { ngx_string("Via"), offsetof(ngx_http_headers_in_t, via),
                 ngx_http_process_header_line },
#endif

    /*
     * Authorization头部处理
     * 用于处理HTTP请求中的Authorization头部字段
     */
    { ngx_string("Authorization"),
                 offsetof(ngx_http_headers_in_t, authorization),
                 ngx_http_process_unique_header_line },

    /*
     * Keep-Alive头部处理
     * 用于处理HTTP请求中的Keep-Alive头部字段
     */
    { ngx_string("Keep-Alive"), offsetof(ngx_http_headers_in_t, keep_alive),
                 ngx_http_process_header_line },

#if (NGX_HTTP_X_FORWARDED_FOR)
    /*
     * X-Forwarded-For头部处理
     * 用于处理HTTP请求中的X-Forwarded-For头部字段
     */
    { ngx_string("X-Forwarded-For"),
                 offsetof(ngx_http_headers_in_t, x_forwarded_for),
                 ngx_http_process_header_line },
#endif

#if (NGX_HTTP_REALIP)
    /*
     * X-Real-IP头部处理
     * 用于处理HTTP请求中的X-Real-IP头部字段
     */
    { ngx_string("X-Real-IP"),
                 offsetof(ngx_http_headers_in_t, x_real_ip),
                 ngx_http_process_header_line },
#endif

#if (NGX_HTTP_HEADERS)
    /*
     * Accept头部处理
     * 用于处理HTTP请求中的Accept头部字段
     */
    { ngx_string("Accept"), offsetof(ngx_http_headers_in_t, accept),
                 ngx_http_process_header_line },

    /*
     * Accept-Language头部处理
     * 用于处理HTTP请求中的Accept-Language头部字段
     */
    { ngx_string("Accept-Language"),
                 offsetof(ngx_http_headers_in_t, accept_language),
                 ngx_http_process_header_line },
#endif

#if (NGX_HTTP_DAV)
    /*
     * Depth头部处理
     * 用于处理HTTP请求中的Depth头部字段
     */
    { ngx_string("Depth"), offsetof(ngx_http_headers_in_t, depth),
                 ngx_http_process_header_line },

    /*
     * Destination头部处理
     * 用于处理HTTP请求中的Destination头部字段
     */
    { ngx_string("Destination"), offsetof(ngx_http_headers_in_t, destination),
                 ngx_http_process_header_line },

    /*
     * Overwrite头部处理
     * 用于处理HTTP请求中的Overwrite头部字段
     */
    { ngx_string("Overwrite"), offsetof(ngx_http_headers_in_t, overwrite),
                 ngx_http_process_header_line },

    /*
     * Date头部处理
     * 用于处理HTTP请求中的Date头部字段
     */
    { ngx_string("Date"), offsetof(ngx_http_headers_in_t, date),
                 ngx_http_process_header_line },
#endif

    /*
     * Cookie头部处理
     * 用于处理HTTP请求中的Cookie头部字段
     */
    { ngx_string("Cookie"), offsetof(ngx_http_headers_in_t, cookie),
                 ngx_http_process_header_line },

    { ngx_null_string, 0, NULL }
};


/*
 * 初始化HTTP连接
 * 参数:
 *   c: 连接结构体指针
 * 功能:
 *   为新建立的HTTP连接进行初始化设置
 *   包括分配内存、查找服务器配置、设置事件处理函数等
 */
void
ngx_http_init_connection(ngx_connection_t *c)
{
    ngx_uint_t                 i;
    ngx_event_t               *rev;
    struct sockaddr_in        *sin;
    ngx_http_port_t           *port;
    ngx_http_in_addr_t        *addr;
    ngx_http_log_ctx_t        *ctx;
    ngx_http_connection_t     *hc;
    ngx_http_core_srv_conf_t  *cscf;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6       *sin6;
    ngx_http_in6_addr_t       *addr6;
#endif

    // 为HTTP连接分配内存
    hc = ngx_pcalloc(c->pool, sizeof(ngx_http_connection_t));
    if (hc == NULL) {
        ngx_http_close_connection(c);
        return;
    }

    // 将HTTP连接结构体与nginx连接关联
    c->data = hc;

    // 查找服务器配置

    port = c->listening->servers;

    if (port->naddrs > 1) {
        // 如果端口有多个地址，需要确定具体的服务器地址

        if (ngx_connection_local_sockaddr(c, NULL, 0) != NGX_OK) {
            ngx_http_close_connection(c);
            return;
        }

        switch (c->local_sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            // IPv6地址处理
            sin6 = (struct sockaddr_in6 *) c->local_sockaddr;
            addr6 = port->addrs;

            // 查找匹配的IPv6地址
            for (i = 0; i < port->naddrs - 1; i++) {
                if (ngx_memcmp(&addr6[i].addr6, &sin6->sin6_addr, 16) == 0) {
                    break;
                }
            }

            hc->addr_conf = &addr6[i].conf;
            break;
#endif

        default: /* AF_INET */
            // IPv4地址处理
            sin = (struct sockaddr_in *) c->local_sockaddr;
            addr = port->addrs;

            // 查找匹配的IPv4地址
            for (i = 0; i < port->naddrs - 1; i++) {
                if (addr[i].addr == sin->sin_addr.s_addr) {
                    break;
                }
            }

            hc->addr_conf = &addr[i].conf;
            break;
        }

    } else {
        // 如果端口只有一个地址，直接使用该地址的配置

        switch (c->local_sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            addr6 = port->addrs;
            hc->addr_conf = &addr6[0].conf;
            break;
#endif

        default: /* AF_INET */
            addr = port->addrs;
            hc->addr_conf = &addr[0].conf;
            break;
        }
    }

    // 设置默认的服务器配置
    hc->conf_ctx = hc->addr_conf->default_server->ctx;

    // 分配日志上下文
    ctx = ngx_palloc(c->pool, sizeof(ngx_http_log_ctx_t));
    if (ctx == NULL) {
        ngx_http_close_connection(c);
        return;
    }

    // 初始化日志上下文
    ctx->connection = c;
    ctx->request = NULL;
    ctx->current_request = NULL;

    // 设置日志相关参数
    c->log->connection = c->number;
    c->log->handler = ngx_http_log_error;
    c->log->data = ctx;
    c->log->action = "waiting for request";

    c->log_error = NGX_ERROR_INFO;

    // 设置读写事件处理函数
    rev = c->read;
    rev->handler = ngx_http_wait_request_handler;
    c->write->handler = ngx_http_empty_handler;

#if (NGX_HTTP_V3)
    // HTTP/3 初始化
    if (hc->addr_conf->quic) {
        ngx_http_v3_init_stream(c);
        return;
    }
#endif

#if (NGX_HTTP_SSL)
    // SSL 初始化
    if (hc->addr_conf->ssl) {
        hc->ssl = 1;
        c->log->action = "SSL handshaking";
        rev->handler = ngx_http_ssl_handshake;
    }
#endif

    // 代理协议处理
    if (hc->addr_conf->proxy_protocol) {
        hc->proxy_protocol = 1;
        c->log->action = "reading PROXY protocol";
    }

    if (rev->ready) {
        // 如果读事件已就绪（deferred accept或iocp）

        if (ngx_use_accept_mutex) {
            ngx_post_event(rev, &ngx_posted_events);
            return;
        }

        rev->handler(rev);
        return;
    }

    // 获取核心服务器配置
    cscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_core_module);

    // 设置客户端头部超时定时器
    ngx_add_timer(rev, cscf->client_header_timeout);
    ngx_reusable_connection(c, 1);

    // 处理读事件
    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        ngx_http_close_connection(c);
        return;
    }
}


/*
 * 函数: ngx_http_wait_request_handler
 * 功能: 处理等待HTTP请求的事件
 * 参数:
 *   rev: 读事件结构体指针，包含了与连接相关的信息
 * 描述:
 *   这个函数是HTTP模块中等待请求到来时的事件处理器。
 *   当有新的客户端连接建立，但还未收到HTTP请求数据时，
 *   这个函数会被调用来等待和处理incoming请求。
 */
static void
ngx_http_wait_request_handler(ngx_event_t *rev)
{
    u_char                    *p;
    size_t                     size;
    ssize_t                    n;
    ngx_buf_t                 *b;
    ngx_connection_t          *c;
    ngx_http_connection_t     *hc;
#if (NGX_HTTP_V2)
    ngx_http_v2_srv_conf_t    *h2scf;
#endif
    ngx_http_core_srv_conf_t  *cscf;

    c = rev->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "http wait request handler");

    // 检查连接是否超时
    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        ngx_http_close_connection(c);
        return;
    }

    // 检查连接是否已关闭
    if (c->close) {
        ngx_http_close_connection(c);
        return;
    }

    hc = c->data;
    cscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_core_module);

    // 获取客户端头部缓冲区大小
    size = cscf->client_header_buffer_size;

    b = c->buffer;

    // 如果缓冲区不存在，创建一个新的临时缓冲区
    if (b == NULL) {
        b = ngx_create_temp_buf(c->pool, size);
        if (b == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        c->buffer = b;

    } else if (b->start == NULL) {
        // 如果缓冲区存在但起始位置为空，分配内存
        b->start = ngx_palloc(c->pool, size);
        if (b->start == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        b->pos = b->start;
        b->last = b->start;
        b->end = b->last + size;
    }

    size = b->end - b->last;

    // 从连接中接收数据
    n = c->recv(c, b->last, size);

    if (n == NGX_AGAIN) {
        // 如果没有立即接收到数据，设置定时器并处理读事件
        if (!rev->timer_set) {
            ngx_add_timer(rev, cscf->client_header_timeout);
            ngx_reusable_connection(c, 1);
        }

        if (ngx_handle_read_event(rev, 0) != NGX_OK) {
            ngx_http_close_connection(c);
            return;
        }

        // 如果缓冲区为空，尝试释放内存
        if (b->pos == b->last) {
            if (ngx_pfree(c->pool, b->start) == NGX_OK) {
                b->start = NULL;
            }
        }

        return;
    }

    // 处理接收错误
    if (n == NGX_ERROR) {
        ngx_http_close_connection(c);
        return;
    }

    // 处理客户端关闭连接
    if (n == 0) {
        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "client closed connection");
        ngx_http_close_connection(c);
        return;
    }

    b->last += n;

    // 处理代理协议
    if (hc->proxy_protocol) {
        hc->proxy_protocol = 0;

        p = ngx_proxy_protocol_read(c, b->pos, b->last);

        if (p == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        b->pos = p;

        if (b->pos == b->last) {
            c->log->action = "waiting for request";
            b->pos = b->start;
            b->last = b->start;
            ngx_post_event(rev, &ngx_posted_events);
            return;
        }
    }

#if (NGX_HTTP_V2)
    // 处理HTTP/2协议
    h2scf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_v2_module);

    if (!hc->ssl && (h2scf->enable || hc->addr_conf->http2)) {

        size = ngx_min(sizeof(NGX_HTTP_V2_PREFACE) - 1,
                       (size_t) (b->last - b->pos));

        if (ngx_memcmp(b->pos, NGX_HTTP_V2_PREFACE, size) == 0) {

            if (size == sizeof(NGX_HTTP_V2_PREFACE) - 1) {
                ngx_http_v2_init(rev);
                return;
            }

            ngx_post_event(rev, &ngx_posted_events);
            return;
        }
    }

#endif

    c->log->action = "reading client request line";

    ngx_reusable_connection(c, 0);

    // 创建HTTP请求
    c->data = ngx_http_create_request(c);
    if (c->data == NULL) {
        ngx_http_close_connection(c);
        return;
    }

    // 设置下一步处理函数为处理请求行
    rev->handler = ngx_http_process_request_line;
    ngx_http_process_request_line(rev);
}


/*
 * 函数: ngx_http_create_request
 * 功能: 为给定的连接创建一个新的HTTP请求
 * 参数:
 *   c: ngx_connection_t类型的指针，表示客户端连接
 * 返回值: 
 *   ngx_http_request_t类型的指针，指向新创建的HTTP请求结构体
 *   如果创建失败，返回NULL
 * 说明:
 *   此函数负责初始化一个新的HTTP请求结构体，设置相关的日志上下文，
 *   并更新一些统计信息。它是处理新进入的HTTP请求的关键步骤之一。
 */
ngx_http_request_t *
ngx_http_create_request(ngx_connection_t *c)
{
    ngx_http_request_t        *r;
    ngx_http_log_ctx_t        *ctx;
    ngx_http_core_loc_conf_t  *clcf;

    // 分配新的HTTP请求结构
    r = ngx_http_alloc_request(c);
    if (r == NULL) {
        return NULL;
    }

    // 增加连接的请求计数
    c->requests++;

    // 获取HTTP核心模块的位置配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 设置连接的日志
    ngx_set_connection_log(c, clcf->error_log);

    // 更新日志上下文
    ctx = c->log->data;
    ctx->request = r;
    ctx->current_request = r;

#if (NGX_STAT_STUB)
    // 更新统计信息
    (void) ngx_atomic_fetch_add(ngx_stat_reading, 1);
    r->stat_reading = 1;
    (void) ngx_atomic_fetch_add(ngx_stat_requests, 1);
#endif

    return r;
}


/*
 * 函数: ngx_http_alloc_request
 * 功能: 为给定的连接分配一个新的HTTP请求结构
 * 参数:
 *   c: ngx_connection_t类型的指针，表示客户端连接
 * 返回值: 
 *   ngx_http_request_t类型的指针，指向新分配的HTTP请求结构体
 *   如果分配失败，返回NULL
 * 说明:
 *   此函数负责分配和初始化一个新的HTTP请求结构体。它是处理新进入的HTTP请求的内部函数，
 *   通常由ngx_http_create_request调用。
 */
static ngx_http_request_t *
ngx_http_alloc_request(ngx_connection_t *c)
{
    ngx_pool_t                 *pool;
    ngx_time_t                 *tp;
    ngx_http_request_t         *r;
    ngx_http_connection_t      *hc;
    ngx_http_core_srv_conf_t   *cscf;
    ngx_http_core_main_conf_t  *cmcf;

    hc = c->data;

    // 获取HTTP核心模块的服务器配置
    cscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_core_module);

    // 创建内存池
    pool = ngx_create_pool(cscf->request_pool_size, c->log);
    if (pool == NULL) {
        return NULL;
    }

    // 分配HTTP请求结构体
    r = ngx_pcalloc(pool, sizeof(ngx_http_request_t));
    if (r == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    // 初始化请求结构体的基本属性
    r->pool = pool;
    r->http_connection = hc;
    r->signature = NGX_HTTP_MODULE;
    r->connection = c;

    // 设置配置上下文
    r->main_conf = hc->conf_ctx->main_conf;
    r->srv_conf = hc->conf_ctx->srv_conf;
    r->loc_conf = hc->conf_ctx->loc_conf;

    // 设置读事件处理函数
    r->read_event_handler = ngx_http_block_reading;

    // 设置请求头缓冲区
    r->header_in = hc->busy ? hc->busy->buf : c->buffer;

    // 初始化输出头部列表
    if (ngx_list_init(&r->headers_out.headers, r->pool, 20,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        ngx_destroy_pool(r->pool);
        return NULL;
    }

    // 初始化尾部列表
    if (ngx_list_init(&r->headers_out.trailers, r->pool, 4,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        ngx_destroy_pool(r->pool);
        return NULL;
    }

    // 分配模块上下文数组
    r->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (r->ctx == NULL) {
        ngx_destroy_pool(r->pool);
        return NULL;
    }

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    // 分配变量数组
    r->variables = ngx_pcalloc(r->pool, cmcf->variables.nelts
                                        * sizeof(ngx_http_variable_value_t));
    if (r->variables == NULL) {
        ngx_destroy_pool(r->pool);
        return NULL;
    }

#if (NGX_HTTP_SSL)
    // SSL相关设置
    if (c->ssl && !c->ssl->sendfile) {
        r->main_filter_need_in_memory = 1;
    }
#endif

    // 设置请求的基本属性
    r->main = r;
    r->count = 1;

    // 记录请求开始时间
    tp = ngx_timeofday();
    r->start_sec = tp->sec;
    r->start_msec = tp->msec;

    // 初始化HTTP方法和版本
    r->method = NGX_HTTP_UNKNOWN;
    r->http_version = NGX_HTTP_VERSION_10;

    // 初始化头部信息
    r->headers_in.content_length_n = -1;
    r->headers_in.keep_alive_n = -1;
    r->headers_out.content_length_n = -1;
    r->headers_out.last_modified_time = -1;

    // 设置URI变更和子请求限制
    r->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
    r->subrequests = NGX_HTTP_MAX_SUBREQUESTS + 1;

    // 设置HTTP状态和日志处理函数
    r->http_state = NGX_HTTP_READING_REQUEST_STATE;
    r->log_handler = ngx_http_log_error_handler;

    return r;
}


#if (NGX_HTTP_SSL)

/*
 * 处理HTTP SSL握手
 *
 * @param rev 读事件结构体指针
 *
 * 该函数负责处理HTTP连接的SSL握手过程。它会检查连接状态,
 * 执行SSL握手,并根据握手结果进行相应处理。如果握手成功,
 * 将继续处理HTTP请求;如果失败,则关闭连接。
 */
static void
ngx_http_ssl_handshake(ngx_event_t *rev)
{
    u_char                    *p, buf[NGX_PROXY_PROTOCOL_MAX_HEADER + 1];
    size_t                     size;
    ssize_t                    n;
    ngx_err_t                  err;
    ngx_int_t                  rc;
    ngx_connection_t          *c;
    ngx_http_connection_t     *hc;
    ngx_http_ssl_srv_conf_t   *sscf;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;

    c = rev->data;
    hc = c->data;

    // 记录调试日志
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   "http check ssl handshake");

    // 检查连接是否超时
    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        ngx_http_close_connection(c);
        return;
    }

    // 检查连接是否已关闭
    if (c->close) {
        ngx_http_close_connection(c);
        return;
    }

    // 确定接收缓冲区大小
    size = hc->proxy_protocol ? sizeof(buf) : 1;

    // 尝试接收数据
    n = recv(c->fd, (char *) buf, size, MSG_PEEK);

    err = ngx_socket_errno;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, rev->log, 0, "http recv(): %z", n);

    // 处理接收错误
    if (n == -1) {
        if (err == NGX_EAGAIN) {
            rev->ready = 0;

            // 设置定时器
            if (!rev->timer_set) {
                cscf = ngx_http_get_module_srv_conf(hc->conf_ctx,
                                                    ngx_http_core_module);
                ngx_add_timer(rev, cscf->client_header_timeout);
                ngx_reusable_connection(c, 1);
            }

            // 处理读事件
            if (ngx_handle_read_event(rev, 0) != NGX_OK) {
                ngx_http_close_connection(c);
            }

            return;
        }

        // 记录接收错误并关闭连接
        ngx_connection_error(c, err, "recv() failed");
        ngx_http_close_connection(c);

        return;
    }

    // 处理代理协议
    if (hc->proxy_protocol) {
        hc->proxy_protocol = 0;

        p = ngx_proxy_protocol_read(c, buf, buf + n);

        if (p == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        size = p - buf;

        if (c->recv(c, buf, size) != (ssize_t) size) {
            ngx_http_close_connection(c);
            return;
        }

        c->log->action = "SSL handshaking";

        if (n == (ssize_t) size) {
            ngx_post_event(rev, &ngx_posted_events);
            return;
        }

        n = 1;
        buf[0] = *p;
    }

    // 检查是否为SSL/TLS连接
    if (n == 1) {
        if (buf[0] & 0x80 /* SSLv2 */ || buf[0] == 0x16 /* SSLv3/TLSv1 */) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                           "https ssl handshake: 0x%02Xd", buf[0]);

            // 获取核心位置配置
            clcf = ngx_http_get_module_loc_conf(hc->conf_ctx,
                                                ngx_http_core_module);

            // 设置TCP_NODELAY
            if (clcf->tcp_nodelay && ngx_tcp_nodelay(c) != NGX_OK) {
                ngx_http_close_connection(c);
                return;
            }

            // 获取SSL服务器配置
            sscf = ngx_http_get_module_srv_conf(hc->conf_ctx,
                                                ngx_http_ssl_module);

            // 创建SSL连接
            if (ngx_ssl_create_connection(&sscf->ssl, c, NGX_SSL_BUFFER)
                != NGX_OK)
            {
                ngx_http_close_connection(c);
                return;
            }

            ngx_reusable_connection(c, 0);

            // 执行SSL握手
            rc = ngx_ssl_handshake(c);

            if (rc == NGX_AGAIN) {
                // 如果握手未完成，设置定时器
                if (!rev->timer_set) {
                    cscf = ngx_http_get_module_srv_conf(hc->conf_ctx,
                                                        ngx_http_core_module);
                    ngx_add_timer(rev, cscf->client_header_timeout);
                }

                c->ssl->handler = ngx_http_ssl_handshake_handler;
                return;
            }

            // 握手完成，调用处理函数
            ngx_http_ssl_handshake_handler(c);

            return;
        }

        // 非SSL连接，切换到普通HTTP处理
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0, "plain http");

        c->log->action = "waiting for request";

        rev->handler = ngx_http_wait_request_handler;
        ngx_http_wait_request_handler(rev);

        return;
    }

    // 客户端关闭连接
    ngx_log_error(NGX_LOG_INFO, c->log, 0, "client closed connection");
    ngx_http_close_connection(c);
}


/*
 * 函数: ngx_http_ssl_handshake_handler
 * 功能: 处理SSL握手完成后的操作
 * 参数:
 *   c: 连接结构体指针，表示当前的客户端连接
 * 描述:
 *   此函数在SSL握手完成后被调用，负责设置后续的HTTP处理流程。
 *   它会检查握手是否成功，设置适当的读写处理函数，并可能初始化HTTP/2连接。
 */
static void
ngx_http_ssl_handshake_handler(ngx_connection_t *c)
{
    // 检查SSL握手是否已完成
    if (c->ssl->handshaked) {

        /*
         * 大多数浏览器不发送"close notify"警报。
         * 其中包括MSIE、旧版Mozilla、Netscape 4、Konqueror和Links。
         * 更重要的是，MSIE会忽略服务器的警报。
         *
         * Opera和最新的Mozilla会发送警报。
         */

        // 设置不等待关闭通知
        c->ssl->no_wait_shutdown = 1;

#if (NGX_HTTP_V2                                                              \
     && defined TLSEXT_TYPE_application_layer_protocol_negotiation)
        {
        unsigned int             len;
        const unsigned char     *data;
        ngx_http_connection_t   *hc;
        ngx_http_v2_srv_conf_t  *h2scf;

        // 获取HTTP连接结构
        hc = c->data;

        // 获取HTTP/2模块的服务器配置
        h2scf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_v2_module);

        // 检查是否启用HTTP/2
        if (h2scf->enable || hc->addr_conf->http2) {

            // 获取ALPN协商结果
            SSL_get0_alpn_selected(c->ssl->connection, &data, &len);

            // 如果协商结果为"h2"，初始化HTTP/2连接
            if (len == 2 && data[0] == 'h' && data[1] == '2') {
                ngx_http_v2_init(c->read);
                return;
            }
        }
        }
#endif

        // 设置连接状态为等待请求
        c->log->action = "waiting for request";

        // 设置读事件处理函数
        c->read->handler = ngx_http_wait_request_handler;
        /* STUB: epoll edge */ c->write->handler = ngx_http_empty_handler;

        // 标记连接为可重用
        ngx_reusable_connection(c, 1);

        // 调用等待请求处理函数
        ngx_http_wait_request_handler(c->read);

        return;
    }

    // 检查读取是否超时
    if (c->read->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
    }

    // 关闭HTTP连接
    ngx_http_close_connection(c);
}


#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME

/*
 * 处理SSL服务器名称指示(SNI)扩展的回调函数
 * 
 * @param ssl_conn SSL连接对象
 * @param ad       用于存储警告描述符的指针
 * @param arg      额外的参数（在这里未使用）
 * @return         SSL扩展错误代码
 */
int
ngx_http_ssl_servername(ngx_ssl_conn_t *ssl_conn, int *ad, void *arg)
{
    ngx_int_t                  rc;
    ngx_str_t                  host;
    const char                *servername;
    ngx_connection_t          *c;
    ngx_http_connection_t     *hc;
    ngx_http_ssl_srv_conf_t   *sscf;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;

    c = ngx_ssl_get_connection(ssl_conn);

    if (c->ssl->handshaked) {
        *ad = SSL_AD_NO_RENEGOTIATION;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    hc = c->data;

    servername = SSL_get_servername(ssl_conn, TLSEXT_NAMETYPE_host_name);

    if (servername == NULL) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "SSL server name: null");
        goto done;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "SSL server name: \"%s\"", servername);

    host.len = ngx_strlen(servername);

    if (host.len == 0) {
        goto done;
    }

    host.data = (u_char *) servername;

    rc = ngx_http_validate_host(&host, c->pool, 1);

    if (rc == NGX_ERROR) {
        goto error;
    }

    if (rc == NGX_DECLINED) {
        goto done;
    }

    rc = ngx_http_find_virtual_server(c, hc->addr_conf->virtual_names, &host,
                                      NULL, &cscf);

    if (rc == NGX_ERROR) {
        goto error;
    }

    if (rc == NGX_DECLINED) {
        goto done;
    }

    hc->ssl_servername = ngx_palloc(c->pool, sizeof(ngx_str_t));
    if (hc->ssl_servername == NULL) {
        goto error;
    }

    *hc->ssl_servername = host;

    hc->conf_ctx = cscf->ctx;

    clcf = ngx_http_get_module_loc_conf(hc->conf_ctx, ngx_http_core_module);

    ngx_set_connection_log(c, clcf->error_log);

    sscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_ssl_module);

    c->ssl->buffer_size = sscf->buffer_size;

    if (sscf->ssl.ctx) {
        if (SSL_set_SSL_CTX(ssl_conn, sscf->ssl.ctx) == NULL) {
            goto error;
        }

        /*
         * SSL_set_SSL_CTX() only changes certs as of 1.0.0d
         * adjust other things we care about
         */

        SSL_set_verify(ssl_conn, SSL_CTX_get_verify_mode(sscf->ssl.ctx),
                       SSL_CTX_get_verify_callback(sscf->ssl.ctx));

        SSL_set_verify_depth(ssl_conn, SSL_CTX_get_verify_depth(sscf->ssl.ctx));

#if OPENSSL_VERSION_NUMBER >= 0x009080dfL
        /* only in 0.9.8m+ */
        SSL_clear_options(ssl_conn, SSL_get_options(ssl_conn) &
                                    ~SSL_CTX_get_options(sscf->ssl.ctx));
#endif

        SSL_set_options(ssl_conn, SSL_CTX_get_options(sscf->ssl.ctx));

#ifdef SSL_OP_NO_RENEGOTIATION
        SSL_set_options(ssl_conn, SSL_OP_NO_RENEGOTIATION);
#endif

#ifdef SSL_OP_ENABLE_MIDDLEBOX_COMPAT
#if (NGX_HTTP_V3)
        if (c->listening->quic) {
            SSL_clear_options(ssl_conn, SSL_OP_ENABLE_MIDDLEBOX_COMPAT);
        }
#endif
#endif
    }

done:

    sscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_ssl_module);

    if (sscf->reject_handshake) {
        c->ssl->handshake_rejected = 1;
        *ad = SSL_AD_UNRECOGNIZED_NAME;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    return SSL_TLSEXT_ERR_OK;

error:

    *ad = SSL_AD_INTERNAL_ERROR;
    return SSL_TLSEXT_ERR_ALERT_FATAL;
}

#endif


#ifdef SSL_R_CERT_CB_ERROR

/*
 * 函数: ngx_http_ssl_certificate
 * 功能: 处理SSL证书的回调函数
 * 参数:
 *   ssl_conn: SSL连接对象
 *   arg: 附加参数，通常是SSL配置信息
 * 返回值: 成功返回0，失败返回其他值
 * 说明:
 *   此函数在SSL握手过程中被调用，用于动态设置SSL证书。
 *   它会根据配置信息选择合适的证书和私钥，并将其应用到当前的SSL连接。
 */
int
ngx_http_ssl_certificate(ngx_ssl_conn_t *ssl_conn, void *arg)
{
    ngx_str_t                  cert, key;
    ngx_uint_t                 i, nelts;
    ngx_connection_t          *c;
    ngx_http_request_t        *r;
    ngx_http_ssl_srv_conf_t   *sscf;
    ngx_http_complex_value_t  *certs, *keys;

    c = ngx_ssl_get_connection(ssl_conn);

    if (c->ssl->handshaked) {
        return 0;
    }

    r = ngx_http_alloc_request(c);
    if (r == NULL) {
        return 0;
    }

    r->logged = 1;

    sscf = arg;

    nelts = sscf->certificate_values->nelts;
    certs = sscf->certificate_values->elts;
    keys = sscf->certificate_key_values->elts;

    for (i = 0; i < nelts; i++) {

        if (ngx_http_complex_value(r, &certs[i], &cert) != NGX_OK) {
            goto failed;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "ssl cert: \"%s\"", cert.data);

        if (ngx_http_complex_value(r, &keys[i], &key) != NGX_OK) {
            goto failed;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "ssl key: \"%s\"", key.data);

        if (ngx_ssl_connection_certificate(c, r->pool, &cert, &key,
                                           sscf->passwords)
            != NGX_OK)
        {
            goto failed;
        }
    }

    ngx_http_free_request(r, 0);
    c->log->action = "SSL handshaking";
    c->destroyed = 0;
    return 1;

failed:

    ngx_http_free_request(r, 0);
    c->log->action = "SSL handshaking";
    c->destroyed = 0;
    return 0;
}

#endif

#endif


/*
 * 处理HTTP请求行
 *
 * @param rev 读事件结构体指针
 *
 * 该函数负责处理HTTP请求的第一行，即请求行。
 * 它会读取请求数据，解析请求行，并设置相关的请求属性。
 * 如果解析成功，会进一步处理请求头部。
 */
static void
ngx_http_process_request_line(ngx_event_t *rev)
{
    ssize_t              n;
    ngx_int_t            rc, rv;
    ngx_str_t            host;
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    c = rev->data;
    r = c->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   "http process request line");

    // 检查是否超时
    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        c->timedout = 1;
        ngx_http_close_request(r, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    rc = NGX_AGAIN;

    for ( ;; ) {

        if (rc == NGX_AGAIN) {
            // 读取请求头
            n = ngx_http_read_request_header(r);

            if (n == NGX_AGAIN || n == NGX_ERROR) {
                break;
            }
        }

        // 解析请求行
        rc = ngx_http_parse_request_line(r, r->header_in);

        if (rc == NGX_OK) {

            // 请求行解析成功

            // 设置请求行相关属性
            r->request_line.len = r->request_end - r->request_start;
            r->request_line.data = r->request_start;
            r->request_length = r->header_in->pos - r->request_start;

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http request line: \"%V\"", &r->request_line);

            // 设置请求方法
            r->method_name.len = r->method_end - r->request_start + 1;
            r->method_name.data = r->request_line.data;

            // 设置HTTP协议版本
            if (r->http_protocol.data) {
                r->http_protocol.len = r->request_end - r->http_protocol.data;
            }

            // 处理请求URI
            if (ngx_http_process_request_uri(r) != NGX_OK) {
                break;
            }

            // 设置schema
            if (r->schema_end) {
                r->schema.len = r->schema_end - r->schema_start;
                r->schema.data = r->schema_start;
            }

            // 处理Host头
            if (r->host_end) {

                host.len = r->host_end - r->host_start;
                host.data = r->host_start;

                // 验证Host
                rc = ngx_http_validate_host(&host, r->pool, 0);

                if (rc == NGX_DECLINED) {
                    ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                  "client sent invalid host in request line");
                    ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
                    break;
                }

                if (rc == NGX_ERROR) {
                    ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                    break;
                }

                // 设置虚拟服务器
                if (ngx_http_set_virtual_server(r, &host) == NGX_ERROR) {
                    break;
                }

                r->headers_in.server = host;
            }

            // 处理HTTP/0.9请求
            if (r->http_version < NGX_HTTP_VERSION_10) {

                if (r->headers_in.server.len == 0
                    && ngx_http_set_virtual_server(r, &r->headers_in.server)
                       == NGX_ERROR)
                {
                    break;
                }

                ngx_http_process_request(r);
                break;
            }

            // 初始化请求头列表
            if (ngx_list_init(&r->headers_in.headers, r->pool, 20,
                              sizeof(ngx_table_elt_t))
                != NGX_OK)
            {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                break;
            }

            c->log->action = "reading client request headers";

            // 设置下一步处理函数为处理请求头
            rev->handler = ngx_http_process_request_headers;
            ngx_http_process_request_headers(rev);

            break;
        }

        if (rc != NGX_AGAIN) {

            // 请求行解析出错

            ngx_log_error(NGX_LOG_INFO, c->log, 0,
                          ngx_http_client_errors[rc - NGX_HTTP_CLIENT_ERROR]);

            if (rc == NGX_HTTP_PARSE_INVALID_VERSION) {
                ngx_http_finalize_request(r, NGX_HTTP_VERSION_NOT_SUPPORTED);

            } else {
                ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            }

            break;
        }

        // 请求行解析未完成

        if (r->header_in->pos == r->header_in->end) {

            // 分配大缓冲区
            rv = ngx_http_alloc_large_header_buffer(r, 1);

            if (rv == NGX_ERROR) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                break;
            }

            if (rv == NGX_DECLINED) {
                r->request_line.len = r->header_in->end - r->request_start;
                r->request_line.data = r->request_start;

                ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "client sent too long URI");
                ngx_http_finalize_request(r, NGX_HTTP_REQUEST_URI_TOO_LARGE);
                break;
            }
        }
    }

    // 运行已发布的请求
    ngx_http_run_posted_requests(c);
}


/**
 * @brief 处理HTTP请求的URI
 * 
 * 该函数负责处理和解析HTTP请求中的URI部分。它会处理复杂URI、带引号的URI以及空路径URI等特殊情况。
 * 
 * @param r 指向ngx_http_request_t结构的指针，包含了当前HTTP请求的所有相关信息
 * @return ngx_int_t 返回处理结果，可能的值包括NGX_OK（成功）或NGX_ERROR（失败）
 */
ngx_int_t
ngx_http_process_request_uri(ngx_http_request_t *r)
{
    ngx_http_core_srv_conf_t  *cscf;

    // 计算URI的长度
    if (r->args_start) {
        r->uri.len = r->args_start - 1 - r->uri_start;
    } else {
        r->uri.len = r->uri_end - r->uri_start;
    }

    // 处理复杂URI、带引号的URI或空路径URI
    if (r->complex_uri || r->quoted_uri || r->empty_path_in_uri) {

        if (r->empty_path_in_uri) {
            r->uri.len++;
        }

        // 为URI分配内存
        r->uri.data = ngx_pnalloc(r->pool, r->uri.len);
        if (r->uri.data == NULL) {
            ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_ERROR;
        }

        cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

        // 解析复杂URI
        if (ngx_http_parse_complex_uri(r, cscf->merge_slashes) != NGX_OK) {
            r->uri.len = 0;

            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "client sent invalid request");
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return NGX_ERROR;
        }

    } else {
        // 简单URI，直接使用原始数据
        r->uri.data = r->uri_start;
    }

    // 设置未解析的URI
    r->unparsed_uri.len = r->uri_end - r->uri_start;
    r->unparsed_uri.data = r->uri_start;

    r->valid_unparsed_uri = r->empty_path_in_uri ? 0 : 1;

    // 处理URI扩展名
    if (r->uri_ext) {
        if (r->args_start) {
            r->exten.len = r->args_start - 1 - r->uri_ext;
        } else {
            r->exten.len = r->uri_end - r->uri_ext;
        }

        r->exten.data = r->uri_ext;
    }

    // 处理查询参数
    if (r->args_start && r->uri_end > r->args_start) {
        r->args.len = r->uri_end - r->args_start;
        r->args.data = r->args_start;
    }

#if (NGX_WIN32)
    {
    u_char  *p, *last;

    p = r->uri.data;
    last = r->uri.data + r->uri.len;

    // 检查Windows特定的不安全URI
    while (p < last) {

        if (*p++ == ':') {

            /*
             * 这个检查涵盖了 "::$data", "::$index_allocation" 和
             * ":$i30:$index_allocation" 等情况
             */

            if (p < last && *p == '$') {
                ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                              "client sent unsafe win32 URI");
                ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
                return NGX_ERROR;
            }
        }
    }

    // 处理URI末尾的空格和点
    p = r->uri.data + r->uri.len - 1;

    while (p > r->uri.data) {

        if (*p == ' ') {
            p--;
            continue;
        }

        if (*p == '.') {
            p--;
            continue;
        }

        break;
    }

    if (p != r->uri.data + r->uri.len - 1) {
        r->uri.len = p + 1 - r->uri.data;
        ngx_http_set_exten(r);
    }

    }
#endif

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http uri: \"%V\"", &r->uri);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http args: \"%V\"", &r->args);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http exten: \"%V\"", &r->exten);

    return NGX_OK;
}



/*
 * 函数: ngx_http_process_request_headers
 * 功能: 处理HTTP请求头
 * 参数:
 *   rev: 事件结构体指针，通常是读事件
 * 描述:
 *   这个函数负责解析和处理HTTP请求的头部信息。
 *   它会逐行读取请求头，解析每个头部字段，并进行相应的处理。
 *   函数还会处理一些特殊情况，如超时、缓冲区分配等。
 */
static void
ngx_http_process_request_headers(ngx_event_t *rev)
{
    u_char                     *p;
    size_t                      len;
    ssize_t                     n;
    ngx_int_t                   rc, rv;
    ngx_table_elt_t            *h;
    ngx_connection_t           *c;
    ngx_http_header_t          *hh;
    ngx_http_request_t         *r;
    ngx_http_core_srv_conf_t   *cscf;
    ngx_http_core_main_conf_t  *cmcf;

    c = rev->data;
    r = c->data;

    // 记录调试日志
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   "http process request header line");

    // 检查是否超时
    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        c->timedout = 1;
        ngx_http_close_request(r, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    // 获取主配置
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    rc = NGX_AGAIN;

    for ( ;; ) {

        if (rc == NGX_AGAIN) {

            // 检查缓冲区是否已满
            if (r->header_in->pos == r->header_in->end) {

                // 分配大型头部缓冲区
                rv = ngx_http_alloc_large_header_buffer(r, 0);

                if (rv == NGX_ERROR) {
                    ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                    break;
                }

                if (rv == NGX_DECLINED) {
                    p = r->header_name_start;

                    r->lingering_close = 1;

                    if (p == NULL) {
                        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                      "client sent too large request");
                        ngx_http_finalize_request(r,
                                            NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
                        break;
                    }

                    len = r->header_in->end - p;

                    if (len > NGX_MAX_ERROR_STR - 300) {
                        len = NGX_MAX_ERROR_STR - 300;
                    }

                    ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                "client sent too long header line: \"%*s...\"",
                                len, r->header_name_start);

                    ngx_http_finalize_request(r,
                                            NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
                    break;
                }
            }

            // 读取请求头
            n = ngx_http_read_request_header(r);

            if (n == NGX_AGAIN || n == NGX_ERROR) {
                break;
            }
        }

        // 获取服务器配置上下文
        cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

        // 解析头部行
        rc = ngx_http_parse_header_line(r, r->header_in,
                                        cscf->underscores_in_headers);

        if (rc == NGX_OK) {

            r->request_length += r->header_in->pos - r->header_name_start;

            // 处理无效头部
            if (r->invalid_header && cscf->ignore_invalid_headers) {
                ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "client sent invalid header line: \"%*s\"",
                              r->header_end - r->header_name_start,
                              r->header_name_start);
                continue;
            }

            // 成功解析头部行，创建头部结构体
            h = ngx_list_push(&r->headers_in.headers);
            if (h == NULL) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                break;
            }

            // 设置头部信息
            h->hash = r->header_hash;

            h->key.len = r->header_name_end - r->header_name_start;
            h->key.data = r->header_name_start;
            h->key.data[h->key.len] = '\0';

            h->value.len = r->header_end - r->header_start;
            h->value.data = r->header_start;
            h->value.data[h->value.len] = '\0';

            // 分配并设置小写键
            h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
            if (h->lowcase_key == NULL) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                break;
            }

            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

            // 查找并处理特殊头部
            hh = ngx_hash_find(&cmcf->headers_in_hash, h->hash,
                               h->lowcase_key, h->key.len);

            if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
                break;
            }

            // 记录调试日志
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http header: \"%V: %V\"",
                           &h->key, &h->value);

            continue;
        }

        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {

            // 头部解析完成
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http header done");

            r->request_length += r->header_in->pos - r->header_name_start;

            r->http_state = NGX_HTTP_PROCESS_REQUEST_STATE;

            // 处理请求头
            rc = ngx_http_process_request_header(r);

            if (rc != NGX_OK) {
                break;
            }

            // 处理请求
            ngx_http_process_request(r);

            break;
        }

        if (rc == NGX_AGAIN) {
            // 头部行解析未完成，继续循环
            continue;
        }

        // 无效头部处理
        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "client sent invalid header line: \"%*s\\x%02xd...\"",
                      r->header_end - r->header_name_start,
                      r->header_name_start, *r->header_end);

        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        break;
    }

    // 运行已发布的请求
    ngx_http_run_posted_requests(c);
}


/*
 * 函数名: ngx_http_read_request_header
 * 功能: 读取HTTP请求头
 * 参数: ngx_http_request_t *r - HTTP请求结构体指针
 * 返回值: ssize_t - 读取的字节数或错误码
 */
static ssize_t
ngx_http_read_request_header(ngx_http_request_t *r)
{
    ssize_t                    n;
    ngx_event_t               *rev;
    ngx_connection_t          *c;
    ngx_http_core_srv_conf_t  *cscf;

    c = r->connection;
    rev = c->read;

    // 计算当前缓冲区中可读取的数据长度
    n = r->header_in->last - r->header_in->pos;

    // 如果缓冲区中有数据，直接返回可读取的数据长度
    if (n > 0) {
        return n;
    }

    // 如果读事件就绪，尝试从连接中接收数据
    if (rev->ready) {
        n = c->recv(c, r->header_in->last,
                    r->header_in->end - r->header_in->last);
    } else {
        // 如果读事件未就绪，返回NGX_AGAIN
        n = NGX_AGAIN;
    }

    // 处理NGX_AGAIN情况，表示需要等待更多数据
    if (n == NGX_AGAIN) {
        // 如果定时器未设置，添加客户端头部超时定时器
        if (!rev->timer_set) {
            cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
            ngx_add_timer(rev, cscf->client_header_timeout);
        }

        // 处理读事件，如果失败则关闭请求
        if (ngx_handle_read_event(rev, 0) != NGX_OK) {
            ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_ERROR;
        }

        return NGX_AGAIN;
    }

    // 处理连接关闭的情况
    if (n == 0) {
        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "client prematurely closed connection");
    }

    // 处理读取错误或连接关闭的情况
    if (n == 0 || n == NGX_ERROR) {
        c->error = 1;
        c->log->action = "reading client request headers";

        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
    }

    // 更新缓冲区中的数据长度
    r->header_in->last += n;

    // 返回实际读取的数据长度
    return n;
}


/*
 * 函数: ngx_http_alloc_large_header_buffer
 * 功能: 为HTTP请求分配大型头部缓冲区
 * 参数:
 *   r: HTTP请求结构体指针
 *   request_line: 标志位，指示是否为请求行分配缓冲区
 * 返回值: ngx_int_t类型，表示分配操作的结果
 * 描述:
 *   当HTTP请求头部或请求行需要更大的缓冲区时，此函数被调用。
 *   它负责分配或重用大型缓冲区，以容纳较长的HTTP头部。
 */
static ngx_int_t
ngx_http_alloc_large_header_buffer(ngx_http_request_t *r,
    ngx_uint_t request_line)
{
    u_char                    *old, *new;
    ngx_buf_t                 *b;
    ngx_chain_t               *cl;
    ngx_http_connection_t     *hc;
    ngx_http_core_srv_conf_t  *cscf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http alloc large header buffer");

    // 检查是否为请求行且请求状态为初始状态
    if (request_line && r->state == 0) {

        /* the client fills up the buffer with "\r\n" */

        // 重置header_in缓冲区的位置指针
        r->header_in->pos = r->header_in->start;
        r->header_in->last = r->header_in->start;

        return NGX_OK;
    }

    // 确定旧缓冲区的起始位置
    old = request_line ? r->request_start : r->header_name_start;

    // 获取HTTP核心模块的服务器配置
    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

    // 检查是否超过了大型客户端头部缓冲区的大小限制
    if (r->state != 0
        && (size_t) (r->header_in->pos - old)
                                     >= cscf->large_client_header_buffers.size)
    {
        return NGX_DECLINED;
    }

    // 获取HTTP连接结构体
    hc = r->http_connection;

    // 尝试从空闲链表中获取缓冲区
    if (hc->free) {
        cl = hc->free;
        hc->free = cl->next;

        b = cl->buf;

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http large header free: %p %uz",
                       b->pos, b->end - b->last);

    } else if (hc->nbusy < cscf->large_client_header_buffers.num) {
        // 如果没有空闲缓冲区，且未达到最大缓冲区数量限制，则创建新的缓冲区

        // 创建临时缓冲区
        b = ngx_create_temp_buf(r->connection->pool,
                                cscf->large_client_header_buffers.size);
        if (b == NULL) {
            return NGX_ERROR;
        }

        // 分配新的链表节点
        cl = ngx_alloc_chain_link(r->connection->pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = b;

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http large header alloc: %p %uz",
                       b->pos, b->end - b->last);

    } else {
        // 如果达到最大缓冲区数量限制，则拒绝分配
        return NGX_DECLINED;
    }

    // 将新分配的缓冲区添加到忙碌链表中
    cl->next = hc->busy;
    hc->busy = cl;
    hc->nbusy++;

    // 如果请求状态为初始状态，直接使用新缓冲区
    if (r->state == 0) {
        /*
         * r->state == 0 means that a header line was parsed successfully
         * and we do not need to copy incomplete header line and
         * to relocate the parser header pointers
         */

        r->header_in = b;

        return NGX_OK;
    }

    // 记录日志，显示需要复制的数据大小
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http large header copy: %uz", r->header_in->pos - old);

    // 检查是否超出新缓冲区的容量
    if (r->header_in->pos - old > b->end - b->start) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "too large header to copy");
        return NGX_ERROR;
    }

    // 获取新缓冲区的起始位置
    new = b->start;

    // 将旧缓冲区的数据复制到新缓冲区
    ngx_memcpy(new, old, r->header_in->pos - old);

    // 更新新缓冲区的位置指针
    b->pos = new + (r->header_in->pos - old);
    b->last = new + (r->header_in->pos - old);
    // 如果是处理请求行
    if (request_line) {
        // 更新请求起始位置
        r->request_start = new;

        // 更新请求结束位置
        if (r->request_end) {
            r->request_end = new + (r->request_end - old);
        }

        // 更新方法结束位置
        if (r->method_end) {
            r->method_end = new + (r->method_end - old);
        }

        // 更新URI起始位置
        if (r->uri_start) {
            r->uri_start = new + (r->uri_start - old);
        }

        // 更新URI结束位置
        if (r->uri_end) {
            r->uri_end = new + (r->uri_end - old);
        }

        // 更新schema起始和结束位置
        if (r->schema_start) {
            r->schema_start = new + (r->schema_start - old);
            if (r->schema_end) {
                r->schema_end = new + (r->schema_end - old);
            }
        }

        // 更新host起始和结束位置
        if (r->host_start) {
            r->host_start = new + (r->host_start - old);
            if (r->host_end) {
                r->host_end = new + (r->host_end - old);
            }
        }

        // 更新URI扩展名位置
        if (r->uri_ext) {
            r->uri_ext = new + (r->uri_ext - old);
        }

        // 更新参数起始位置
        if (r->args_start) {
            r->args_start = new + (r->args_start - old);
        }

        // 更新HTTP协议数据位置
        if (r->http_protocol.data) {
            r->http_protocol.data = new + (r->http_protocol.data - old);
        }

    } else {
        // 如果是处理头部
        // 更新头部名称起始位置
        r->header_name_start = new;

        // 更新头部名称结束位置
        if (r->header_name_end) {
            r->header_name_end = new + (r->header_name_end - old);
        }

        // 更新头部起始位置
        if (r->header_start) {
            r->header_start = new + (r->header_start - old);
        }

        // 更新头部结束位置
        if (r->header_end) {
            r->header_end = new + (r->header_end - old);
        }
    }

    // 更新请求的header_in指针为新分配的缓冲区
    r->header_in = b;

    return NGX_OK;
}


/*
 * 处理HTTP请求头行的函数
 * 
 * @param r HTTP请求结构体
 * @param h 头部元素结构体
 * @param offset 头部在headers_in结构中的偏移量
 * @return NGX_OK 处理成功
 */
static ngx_int_t
ngx_http_process_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  **ph;

    // 计算头部在headers_in结构中的偏移地址
    ph = (ngx_table_elt_t **) ((char *) &r->headers_in + offset);

    // 遍历链表，找到最后一个节点
    while (*ph) { ph = &(*ph)->next; }

    // 将新的头部添加到链表末尾
    *ph = h;
    h->next = NULL;

    return NGX_OK;
}


/*
 * 处理唯一HTTP头部行的函数
 * 
 * @param r HTTP请求结构体
 * @param h 头部元素结构体
 * @param offset 头部在headers_in结构中的偏移量
 * @return NGX_OK 处理成功，NGX_ERROR 处理失败
 */
/*
 * 处理唯一HTTP头部行的函数
 * 
 * @param r HTTP请求结构体
 * @param h 头部元素结构体
 * @param offset 头部在headers_in结构中的偏移量
 * @return NGX_OK 处理成功，NGX_ERROR 处理失败
 */
static ngx_int_t
ngx_http_process_unique_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  **ph;

    // 计算头部在headers_in结构中的偏移地址
    ph = (ngx_table_elt_t **) ((char *) &r->headers_in + offset);

    // 如果该头部还未设置，则设置它
    if (*ph == NULL) {
        *ph = h;
        h->next = NULL;
        return NGX_OK;
    }

    // 如果该头部已经存在，记录错误日志
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                  "client sent duplicate header line: \"%V: %V\", "
                  "previous value: \"%V: %V\"",
                  &h->key, &h->value, &(*ph)->key, &(*ph)->value);

    // 终止请求处理，返回400 Bad Request
    ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);

    return NGX_ERROR;
}


/*
 * 处理HTTP Host头部的函数
 * 
 * @param r HTTP请求结构体
 * @param h Host头部元素结构体
 * @param offset 头部在headers_in结构中的偏移量（此参数在函数中未使用）
 * @return NGX_OK 处理成功，NGX_ERROR 处理失败
 */
static ngx_int_t
ngx_http_process_host(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_int_t  rc;
    ngx_str_t  host;

    // 检查是否已存在Host头部
    if (r->headers_in.host) {
        // 如果存在，记录错误日志并返回400 Bad Request
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "client sent duplicate host header: \"%V: %V\", "
                      "previous value: \"%V: %V\"",
                      &h->key, &h->value, &r->headers_in.host->key,
                      &r->headers_in.host->value);
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
    }

    // 设置Host头部
    r->headers_in.host = h;
    h->next = NULL;

    // 获取Host值
    host = h->value;

    // 验证Host值
    rc = ngx_http_validate_host(&host, r->pool, 0);

    if (rc == NGX_DECLINED) {
        // Host值无效，记录错误日志并返回400 Bad Request
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "client sent invalid host header");
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
    }

    if (rc == NGX_ERROR) {
        // 发生内部错误，关闭请求并返回500 Internal Server Error
        ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_ERROR;
    }

    // 如果已设置server名称，直接返回
    if (r->headers_in.server.len) {
        return NGX_OK;
    }

    // 设置虚拟服务器
    if (ngx_http_set_virtual_server(r, &host) == NGX_ERROR) {
        return NGX_ERROR;
    }

    // 设置server名称
    r->headers_in.server = host;

    return NGX_OK;
}


static ngx_int_t
ngx_http_process_connection(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    // 处理Connection头部
    if (ngx_http_process_header_line(r, h, offset) != NGX_OK) {
        return NGX_ERROR;
    }

    // 检查Connection头部的值
    if (ngx_strcasestrn(h->value.data, "close", 5 - 1)) {
        // 如果包含"close"，设置连接类型为关闭
        r->headers_in.connection_type = NGX_HTTP_CONNECTION_CLOSE;

    } else if (ngx_strcasestrn(h->value.data, "keep-alive", 10 - 1)) {
        // 如果包含"keep-alive"，设置连接类型为保持活动
        r->headers_in.connection_type = NGX_HTTP_CONNECTION_KEEP_ALIVE;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_process_user_agent(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    u_char  *user_agent, *msie;

    // 处理User-Agent头部
    if (ngx_http_process_header_line(r, h, offset) != NGX_OK) {
        return NGX_ERROR;
    }

    // 获取User-Agent字符串
    user_agent = h->value.data;

    // 检查是否为MSIE浏览器
    msie = ngx_strstrn(user_agent, "MSIE ", 5 - 1);

    if (msie && msie + 7 < user_agent + h->value.len) {
        // 标记为MSIE浏览器
        r->headers_in.msie = 1;

        if (msie[6] == '.') {
            // 检查MSIE版本
            switch (msie[5]) {
            case '4':
            case '5':
                r->headers_in.msie6 = 1;
                break;
            case '6':
                if (ngx_strstrn(msie + 8, "SV1", 3 - 1) == NULL) {
                    r->headers_in.msie6 = 1;
                }
                break;
            }
        }

#if 0
        /* MSIE ignores the SSL "close notify" alert */
        if (c->ssl) {
            c->ssl->no_send_shutdown = 1;
        }
#endif
    }

    // 检查是否为Opera浏览器
    if (ngx_strstrn(user_agent, "Opera", 5 - 1)) {
        r->headers_in.opera = 1;
        r->headers_in.msie = 0;
        r->headers_in.msie6 = 0;
    }

    // 检查其他浏览器
    if (!r->headers_in.msie && !r->headers_in.opera) {
        // 检查Gecko内核（如Firefox）
        if (ngx_strstrn(user_agent, "Gecko/", 6 - 1)) {
            r->headers_in.gecko = 1;

        // 检查Chrome浏览器
        } else if (ngx_strstrn(user_agent, "Chrome/", 7 - 1)) {
            r->headers_in.chrome = 1;

        // 检查Safari浏览器（Mac OS X）
        } else if (ngx_strstrn(user_agent, "Safari/", 7 - 1)
                   && ngx_strstrn(user_agent, "Mac OS X", 8 - 1))
        {
            r->headers_in.safari = 1;

        // 检查Konqueror浏览器
        } else if (ngx_strstrn(user_agent, "Konqueror", 9 - 1)) {
            r->headers_in.konqueror = 1;
        }
    }

    return NGX_OK;
}


/*
 * 函数名: ngx_http_process_request_header
 * 功能: 处理HTTP请求头
 * 参数: r - 指向ngx_http_request_t结构的指针，表示当前的HTTP请求
 * 返回值: NGX_OK 表示处理成功，NGX_ERROR 表示处理失败
 * 描述: 这个函数负责处理HTTP请求头的各个方面，包括服务器名称、Host头、Content-Length、
 *       Transfer-Encoding等，并进行相应的验证和处理。
 */
ngx_int_t
ngx_http_process_request_header(ngx_http_request_t *r)
{
    // 检查并设置虚拟服务器
    if (r->headers_in.server.len == 0
        && ngx_http_set_virtual_server(r, &r->headers_in.server)
           == NGX_ERROR)
    {
        return NGX_ERROR;
    }

    // 检查HTTP/1.1请求是否包含Host头
    if (r->headers_in.host == NULL && r->http_version > NGX_HTTP_VERSION_10) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                   "client sent HTTP/1.1 request without \"Host\" header");
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
    }

    // 处理Content-Length头
    if (r->headers_in.content_length) {
        r->headers_in.content_length_n =
                            ngx_atoof(r->headers_in.content_length->value.data,
                                      r->headers_in.content_length->value.len);

        if (r->headers_in.content_length_n == NGX_ERROR) {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "client sent invalid \"Content-Length\" header");
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return NGX_ERROR;
        }
    }

    // 处理Transfer-Encoding头
    if (r->headers_in.transfer_encoding) {
        // 检查HTTP版本
        if (r->http_version < NGX_HTTP_VERSION_11) {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "client sent HTTP/1.0 request with "
                          "\"Transfer-Encoding\" header");
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return NGX_ERROR;
        }

        // 检查是否为chunked编码
        if (r->headers_in.transfer_encoding->value.len == 7
            && ngx_strncasecmp(r->headers_in.transfer_encoding->value.data,
                               (u_char *) "chunked", 7) == 0)
        {
            // 检查是否同时存在Content-Length头
            if (r->headers_in.content_length) {
                ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                              "client sent \"Content-Length\" and "
                              "\"Transfer-Encoding\" headers "
                              "at the same time");
                ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
                return NGX_ERROR;
            }

            r->headers_in.chunked = 1;

        } else {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "client sent unknown \"Transfer-Encoding\": \"%V\"",
                          &r->headers_in.transfer_encoding->value);
            ngx_http_finalize_request(r, NGX_HTTP_NOT_IMPLEMENTED);
            return NGX_ERROR;
        }
    }

    // 处理Keep-Alive连接
    if (r->headers_in.connection_type == NGX_HTTP_CONNECTION_KEEP_ALIVE) {
        if (r->headers_in.keep_alive) {
            r->headers_in.keep_alive_n =
                            ngx_atotm(r->headers_in.keep_alive->value.data,
                                      r->headers_in.keep_alive->value.len);
        }
    }

    // 禁止CONNECT方法
    if (r->method == NGX_HTTP_CONNECT) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "client sent CONNECT method");
        ngx_http_finalize_request(r, NGX_HTTP_NOT_ALLOWED);
        return NGX_ERROR;
    }

    // 禁止TRACE方法
    if (r->method == NGX_HTTP_TRACE) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "client sent TRACE method");
        ngx_http_finalize_request(r, NGX_HTTP_NOT_ALLOWED);
        return NGX_ERROR;
    }

    return NGX_OK;
}


/*
 * 函数名: ngx_http_process_request
 * 功能: 处理HTTP请求
 * 参数: r - 指向ngx_http_request_t结构的指针，表示当前的HTTP请求
 * 返回值: 无
 * 描述: 这个函数负责处理HTTP请求的主要逻辑
 */
void
ngx_http_process_request(ngx_http_request_t *r)
{
    ngx_connection_t  *c;

    c = r->connection;

#if (NGX_HTTP_SSL)

    // 如果是HTTPS连接
    if (r->http_connection->ssl) {
        long                      rc;
        X509                     *cert;
        const char               *s;
        ngx_http_ssl_srv_conf_t  *sscf;

        // 检查是否通过SSL连接
        if (c->ssl == NULL) {
            ngx_log_error(NGX_LOG_INFO, c->log, 0,
                          "client sent plain HTTP request to HTTPS port");
            ngx_http_finalize_request(r, NGX_HTTP_TO_HTTPS);
            return;
        }

        sscf = ngx_http_get_module_srv_conf(r, ngx_http_ssl_module);

        // 验证客户端证书
        if (sscf->verify) {
            rc = SSL_get_verify_result(c->ssl->connection);

            if (rc != X509_V_OK
                && (sscf->verify != 3 || !ngx_ssl_verify_error_optional(rc)))
            {
                ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "client SSL certificate verify error: (%l:%s)",
                              rc, X509_verify_cert_error_string(rc));

                ngx_ssl_remove_cached_session(c->ssl->session_ctx,
                                       (SSL_get0_session(c->ssl->connection)));

                ngx_http_finalize_request(r, NGX_HTTPS_CERT_ERROR);
                return;
            }

            // 检查客户端是否提供了证书（如果要求的话）
            if (sscf->verify == 1) {
                cert = SSL_get_peer_certificate(c->ssl->connection);

                if (cert == NULL) {
                    ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                  "client sent no required SSL certificate");

                    ngx_ssl_remove_cached_session(c->ssl->session_ctx,
                                       (SSL_get0_session(c->ssl->connection)));

                    ngx_http_finalize_request(r, NGX_HTTPS_NO_CERT);
                    return;
                }

                X509_free(cert);
            }

            // 检查OCSP状态
            if (ngx_ssl_ocsp_get_status(c, &s) != NGX_OK) {
                ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "client SSL certificate verify error: %s", s);

                ngx_ssl_remove_cached_session(c->ssl->session_ctx,
                                       (SSL_get0_session(c->ssl->connection)));

                ngx_http_finalize_request(r, NGX_HTTPS_CERT_ERROR);
                return;
            }
        }
    }

#endif

    // 移除读取超时定时器
    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

#if (NGX_STAT_STUB)
    // 更新统计信息
    (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
    r->stat_reading = 0;
    (void) ngx_atomic_fetch_add(ngx_stat_writing, 1);
    r->stat_writing = 1;
#endif

    // 设置读写事件处理函数
    c->read->handler = ngx_http_request_handler;
    c->write->handler = ngx_http_request_handler;
    r->read_event_handler = ngx_http_block_reading;

    // 调用HTTP请求处理函数
    ngx_http_handler(r);
}


// 验证主机名的函数
ngx_int_t
ngx_http_validate_host(ngx_str_t *host, ngx_pool_t *pool, ngx_uint_t alloc)
{
    u_char  *h, ch;
    size_t   i, dot_pos, host_len;

    // 定义状态枚举
    enum {
        sw_usual = 0,    // 普通状态
        sw_literal,      // 字面量状态（用于IPv6地址）
        sw_rest          // 剩余部分状态
    } state;

    dot_pos = host->len;
    host_len = host->len;

    h = host->data;

    state = sw_usual;

    // 遍历主机名的每个字符
    for (i = 0; i < host->len; i++) {
        ch = h[i];

        switch (ch) {

        case '.':
            // 检查连续的点
            if (dot_pos == i - 1) {
                return NGX_DECLINED;
            }
            dot_pos = i;
            break;

        case ':':
            // 处理端口分隔符
            if (state == sw_usual) {
                host_len = i;
                state = sw_rest;
            }
            break;

        case '[':
            // IPv6地址的开始
            if (i == 0) {
                state = sw_literal;
            }
            break;

        case ']':
            // IPv6地址的结束
            if (state == sw_literal) {
                host_len = i + 1;
                state = sw_rest;
            }
            break;

        default:
            // 检查非法字符
            if (ngx_path_separator(ch)) {
                return NGX_DECLINED;
            }

            if (ch <= 0x20 || ch == 0x7f) {
                return NGX_DECLINED;
            }

            // 检查是否需要转换为小写
            if (ch >= 'A' && ch <= 'Z') {
                alloc = 1;
            }

            break;
        }
    }

    // 移除尾部的点
    if (dot_pos == host_len - 1) {
        host_len--;
    }

    // 检查主机名长度
    if (host_len == 0) {
        return NGX_DECLINED;
    }

    // 如果需要，分配新的内存并转换为小写
    if (alloc) {
        host->data = ngx_pnalloc(pool, host_len);
        if (host->data == NULL) {
            return NGX_ERROR;
        }

        ngx_strlow(host->data, h, host_len);
    }

    host->len = host_len;

    return NGX_OK;
}


// 设置虚拟服务器的函数
ngx_int_t
ngx_http_set_virtual_server(ngx_http_request_t *r, ngx_str_t *host)
{
    ngx_int_t                  rc;
    ngx_http_connection_t     *hc;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;

#if (NGX_SUPPRESS_WARN)
    cscf = NULL;
#endif

    hc = r->http_connection;

#if (NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME)

    // 检查SSL服务器名称
    if (hc->ssl_servername) {
        if (hc->ssl_servername->len == host->len
            && ngx_strncmp(hc->ssl_servername->data,
                           host->data, host->len) == 0)
        {
#if (NGX_PCRE)
            // 执行正则表达式匹配
            if (hc->ssl_servername_regex
                && ngx_http_regex_exec(r, hc->ssl_servername_regex,
                                          hc->ssl_servername) != NGX_OK)
            {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_ERROR;
            }
#endif
            return NGX_OK;
        }
    }

#endif

    // 查找虚拟服务器
    rc = ngx_http_find_virtual_server(r->connection,
                                      hc->addr_conf->virtual_names,
                                      host, r, &cscf);

    if (rc == NGX_ERROR) {
        ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_ERROR;
    }

#if (NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME)

    // 处理SSL服务器名称
    if (hc->ssl_servername) {
        ngx_http_ssl_srv_conf_t  *sscf;

        if (rc == NGX_DECLINED) {
            cscf = hc->addr_conf->default_server;
            rc = NGX_OK;
        }

        sscf = ngx_http_get_module_srv_conf(cscf->ctx, ngx_http_ssl_module);

        // 验证SSL服务器名称
        if (sscf->verify) {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "client attempted to request the server name "
                          "different from the one that was negotiated");
            ngx_http_finalize_request(r, NGX_HTTP_MISDIRECTED_REQUEST);
            return NGX_ERROR;
        }
    }

#endif

    if (rc == NGX_DECLINED) {
        return NGX_OK;
    }

    // 设置服务器和位置配置
    r->srv_conf = cscf->ctx->srv_conf;
    r->loc_conf = cscf->ctx->loc_conf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 设置连接日志
    ngx_set_connection_log(r->connection, clcf->error_log);

    return NGX_OK;
}


static ngx_int_t
ngx_http_find_virtual_server(ngx_connection_t *c,
    ngx_http_virtual_names_t *virtual_names, ngx_str_t *host,
    ngx_http_request_t *r, ngx_http_core_srv_conf_t **cscfp)
{
    ngx_http_core_srv_conf_t  *cscf;

    // 如果没有虚拟主机配置，直接返回NGX_DECLINED
    if (virtual_names == NULL) {
        return NGX_DECLINED;
    }

    // 在哈希表中查找匹配的服务器配置
    cscf = ngx_hash_find_combined(&virtual_names->names,
                                  ngx_hash_key(host->data, host->len),
                                  host->data, host->len);

    // 如果找到匹配的配置，设置cscfp并返回NGX_OK
    if (cscf) {
        *cscfp = cscf;
        return NGX_OK;
    }

#if (NGX_PCRE)

    // 如果启用了PCRE，并且有正则表达式配置
    if (host->len && virtual_names->nregex) {
        ngx_int_t                n;
        ngx_uint_t               i;
        ngx_http_server_name_t  *sn;

        sn = virtual_names->regex;

#if (NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME)

        // SSL SNI处理
        if (r == NULL) {
            ngx_http_connection_t  *hc;

            for (i = 0; i < virtual_names->nregex; i++) {

                // 尝试匹配正则表达式
                n = ngx_regex_exec(sn[i].regex->regex, host, NULL, 0);

                if (n == NGX_REGEX_NO_MATCHED) {
                    continue;
                }

                if (n >= 0) {
                    hc = c->data;
                    hc->ssl_servername_regex = sn[i].regex;

                    *cscfp = sn[i].server;
                    return NGX_OK;
                }

                // 正则表达式执行失败，记录错误日志
                ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                              ngx_regex_exec_n " failed: %i "
                              "on \"%V\" using \"%V\"",
                              n, host, &sn[i].regex->name);

                return NGX_ERROR;
            }

            return NGX_DECLINED;
        }

#endif /* NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME */

        // 非SSL请求的正则表达式匹配
        for (i = 0; i < virtual_names->nregex; i++) {

            n = ngx_http_regex_exec(r, sn[i].regex, host);

            if (n == NGX_DECLINED) {
                continue;
            }

            if (n == NGX_OK) {
                *cscfp = sn[i].server;
                return NGX_OK;
            }

            return NGX_ERROR;
        }
    }

#endif /* NGX_PCRE */

    // 如果没有找到匹配的配置，返回NGX_DECLINED
    return NGX_DECLINED;
}


// 处理HTTP请求的主要函数
static void
ngx_http_request_handler(ngx_event_t *ev)
{
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    c = ev->data;
    r = c->data;

    // 设置日志请求
    ngx_http_set_log_request(c->log, r);

    // 记录调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http run request: \"%V?%V\"", &r->uri, &r->args);

    // 如果连接已关闭，终止请求并运行已发布的请求
    if (c->close) {
        r->main->count++;
        ngx_http_terminate_request(r, 0);
        ngx_http_run_posted_requests(c);
        return;
    }

    // 处理延迟和超时事件
    if (ev->delayed && ev->timedout) {
        ev->delayed = 0;
        ev->timedout = 0;
    }

    // 根据事件类型调用相应的处理函数
    if (ev->write) {
        r->write_event_handler(r);
    } else {
        r->read_event_handler(r);
    }

    // 运行已发布的请求
    ngx_http_run_posted_requests(c);
}


// 运行已发布的请求
void
ngx_http_run_posted_requests(ngx_connection_t *c)
{
    ngx_http_request_t         *r;
    ngx_http_posted_request_t  *pr;

    for ( ;; ) {
        // 如果连接已销毁，退出循环
        if (c->destroyed) {
            return;
        }

        r = c->data;
        pr = r->main->posted_requests;

        // 如果没有已发布的请求，退出循环
        if (pr == NULL) {
            return;
        }

        // 移除当前已发布的请求
        r->main->posted_requests = pr->next;

        r = pr->request;

        // 设置日志请求
        ngx_http_set_log_request(c->log, r);

        // 记录调试日志
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http posted request: \"%V?%V\"", &r->uri, &r->args);

        // 调用写事件处理函数
        r->write_event_handler(r);
    }
}


// 发布HTTP请求
ngx_int_t
ngx_http_post_request(ngx_http_request_t *r, ngx_http_posted_request_t *pr)
{
    ngx_http_posted_request_t  **p;

    // 如果没有提供已发布的请求结构，则分配一个新的
    if (pr == NULL) {
        pr = ngx_palloc(r->pool, sizeof(ngx_http_posted_request_t));
        if (pr == NULL) {
            return NGX_ERROR;
        }
    }

    // 设置请求和下一个指针
    pr->request = r;
    pr->next = NULL;

    // 找到已发布请求链表的末尾
    for (p = &r->main->posted_requests; *p; p = &(*p)->next) { /* void */ }

    // 将新的已发布请求添加到链表末尾
    *p = pr;

    return NGX_OK;
}


void
ngx_http_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_connection_t          *c;
    ngx_http_request_t        *pr;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;

    // 记录调试日志
    ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http finalize request: %i, \"%V?%V\" a:%d, c:%d",
                   rc, &r->uri, &r->args, r == c->data, r->main->count);

    // 如果返回码为NGX_DONE，结束连接并返回
    if (rc == NGX_DONE) {
        ngx_http_finalize_connection(r);
        return;
    }

    // 如果返回码为NGX_OK且设置了filter_finalize标志，标记连接出错
    if (rc == NGX_OK && r->filter_finalize) {
        c->error = 1;
    }

    // 如果返回码为NGX_DECLINED，重置处理函数并重新运行阶段
    if (rc == NGX_DECLINED) {
        r->content_handler = NULL;
        r->write_event_handler = ngx_http_core_run_phases;
        ngx_http_core_run_phases(r);
        return;
    }

    // 处理子请求的后续操作
    if (r != r->main && r->post_subrequest) {
        rc = r->post_subrequest->handler(r, r->post_subrequest->data, rc);
    }

    // 处理错误情况
    if (rc == NGX_ERROR
        || rc == NGX_HTTP_REQUEST_TIME_OUT
        || rc == NGX_HTTP_CLIENT_CLOSED_REQUEST
        || c->error)
    {
        if (ngx_http_post_action(r) == NGX_OK) {
            return;
        }

        ngx_http_terminate_request(r, rc);
        return;
    }

    // 处理特殊响应
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE
        || rc == NGX_HTTP_CREATED
        || rc == NGX_HTTP_NO_CONTENT)
    {
        if (rc == NGX_HTTP_CLOSE) {
            c->timedout = 1;
            ngx_http_terminate_request(r, rc);
            return;
        }

        if (r == r->main) {
            if (c->read->timer_set) {
                ngx_del_timer(c->read);
            }

            if (c->write->timer_set) {
                ngx_del_timer(c->write);
            }
        }

        c->read->handler = ngx_http_request_handler;
        c->write->handler = ngx_http_request_handler;

        ngx_http_finalize_request(r, ngx_http_special_response_handler(r, rc));
        return;
    }

    // 处理子请求
    if (r != r->main) {

        // 如果子请求有缓冲或延迟的数据
        if (r->buffered || r->postponed) {

            if (ngx_http_set_write_handler(r) != NGX_OK) {
                ngx_http_terminate_request(r, 0);
            }

            return;
        }

        pr = r->parent;

        // 处理活跃的子请求或后台子请求
        if (r == c->data || r->background) {

            if (!r->logged) {

                clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

                if (clcf->log_subrequest) {
                    ngx_http_log_request(r);
                }

                r->logged = 1;

            } else {
                ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                              "subrequest: \"%V?%V\" logged again",
                              &r->uri, &r->args);
            }

            r->done = 1;

            if (r->background) {
                ngx_http_finalize_connection(r);
                return;
            }

            r->main->count--;

            if (pr->postponed && pr->postponed->request == r) {
                pr->postponed = pr->postponed->next;
            }

            c->data = pr;

        } else {

            // 处理非活跃的子请求
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http finalize non-active request: \"%V?%V\"",
                           &r->uri, &r->args);

            r->write_event_handler = ngx_http_request_finalizer;

            if (r->waited) {
                r->done = 1;
            }
        }

        // 发布父请求
        if (ngx_http_post_request(pr, NULL) != NGX_OK) {
            r->main->count++;
            ngx_http_terminate_request(r, 0);
            return;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http wake parent request: \"%V?%V\"",
                       &pr->uri, &pr->args);

        return;
    }

    // 处理主请求
    if (r->buffered || c->buffered || r->postponed) {

        if (ngx_http_set_write_handler(r) != NGX_OK) {
            ngx_http_terminate_request(r, 0);
        }

        return;
    }

    if (r != c->data) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "http finalize non-active request: \"%V?%V\"",
                      &r->uri, &r->args);
        return;
    }

    r->done = 1;

    r->read_event_handler = ngx_http_block_reading;
    r->write_event_handler = ngx_http_request_empty_handler;

    if (!r->post_action) {
        r->request_complete = 1;
    }

    if (ngx_http_post_action(r) == NGX_OK) {
        return;
    }

    // 清理定时器
    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    if (c->write->timer_set) {
        c->write->delayed = 0;
        ngx_del_timer(c->write);
    }

    // 结束连接
    ngx_http_finalize_connection(r);
}


static void
ngx_http_terminate_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_http_cleanup_t    *cln;
    ngx_http_request_t    *mr;
    ngx_http_ephemeral_t  *e;

    // 获取主请求
    mr = r->main;

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http terminate request count:%d", mr->count);

    // 标记请求已终止
    mr->terminated = 1;

    // 如果状态码大于0且未设置响应状态或未发送任何数据，则设置响应状态
    if (rc > 0 && (mr->headers_out.status == 0 || mr->connection->sent == 0)) {
        mr->headers_out.status = rc;
    }

    // 执行清理操作
    cln = mr->cleanup;
    mr->cleanup = NULL;

    while (cln) {
        if (cln->handler) {
            cln->handler(cln->data);
        }

        cln = cln->next;
    }

    // 记录清理后的调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http terminate cleanup count:%d blk:%d",
                   mr->count, mr->blocked);

    // 如果存在写事件处理器
    if (mr->write_event_handler) {

        // 如果请求被阻塞
        if (mr->blocked) {
            r = r->connection->data;

            r->connection->error = 1;
            r->write_event_handler = ngx_http_request_finalizer;

            return;
        }

        // 处理非阻塞情况
        e = ngx_http_ephemeral(mr);
        mr->posted_requests = NULL;
        mr->write_event_handler = ngx_http_terminate_handler;
        (void) ngx_http_post_request(mr, &e->terminal_posted_request);
        return;
    }

    // 关闭请求
    ngx_http_close_request(mr, rc);
}


static void
ngx_http_terminate_handler(ngx_http_request_t *r)
{
    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http terminate handler count:%d", r->count);

    // 设置请求计数为1
    r->count = 1;

    // 关闭请求
    ngx_http_close_request(r, 0);
}


static void
ngx_http_finalize_connection(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;

    // 处理HTTP/2流
#if (NGX_HTTP_V2)
    if (r->stream) {
        ngx_http_close_request(r, 0);
        return;
    }
#endif

    // 处理HTTP/3连接
#if (NGX_HTTP_V3)
    if (r->connection->quic) {
        ngx_http_close_request(r, 0);
        return;
    }
#endif

    // 获取核心位置配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 如果主请求计数不为1
    if (r->main->count != 1) {

        // 处理丢弃请求体的情况
        if (r->discard_body) {
            r->read_event_handler = ngx_http_discarded_request_body_handler;
            ngx_add_timer(r->connection->read, clcf->lingering_timeout);

            if (r->lingering_time == 0) {
                r->lingering_time = ngx_time()
                                      + (time_t) (clcf->lingering_time / 1000);
            }
        }

        // 关闭请求
        ngx_http_close_request(r, 0);
        return;
    }

    // 将r指向主请求
    r = r->main;

    // 如果连接已经读取到EOF
    if (r->connection->read->eof) {
        ngx_http_close_request(r, 0);
        return;
    }

    // 如果正在读取请求体
    if (r->reading_body) {
        r->keepalive = 0;
        r->lingering_close = 1;
    }

    // 处理保持连接的情况
    if (!ngx_terminate
         && !ngx_exiting
         && r->keepalive
         && clcf->keepalive_timeout > 0)
    {
        ngx_http_set_keepalive(r);
        return;
    }

    // 处理延迟关闭的情况
    if (clcf->lingering_close == NGX_HTTP_LINGERING_ALWAYS
        || (clcf->lingering_close == NGX_HTTP_LINGERING_ON
            && (r->lingering_close
                || r->header_in->pos < r->header_in->last
                || r->connection->read->ready
                || r->connection->pipeline)))
    {
        ngx_http_set_lingering_close(r->connection);
        return;
    }

    // 关闭请求
    ngx_http_close_request(r, 0);
}


static ngx_int_t
ngx_http_set_write_handler(ngx_http_request_t *r)
{
    ngx_event_t               *wev;
    ngx_http_core_loc_conf_t  *clcf;

    // 设置HTTP状态为写请求状态
    r->http_state = NGX_HTTP_WRITING_REQUEST_STATE;

    // 设置读事件处理器
    r->read_event_handler = r->discard_body ?
                                ngx_http_discarded_request_body_handler:
                                ngx_http_test_reading;
    // 设置写事件处理器
    r->write_event_handler = ngx_http_writer;

    wev = r->connection->write;

    // 如果写事件已就绪且被延迟
    if (wev->ready && wev->delayed) {
        return NGX_OK;
    }

    // 获取核心位置配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    // 如果写事件未被延迟，添加发送超时定时器
    if (!wev->delayed) {
        ngx_add_timer(wev, clcf->send_timeout);
    }

    // 处理写事件
    if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
        ngx_http_close_request(r, 0);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_http_writer(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_event_t               *wev;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    wev = c->write;

    // 记录调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, wev->log, 0,
                   "http writer handler: \"%V?%V\"", &r->uri, &r->args);

    // 获取核心位置配置
    clcf = ngx_http_get_module_loc_conf(r->main, ngx_http_core_module);

    // 检查写事件是否超时
    if (wev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT,
                      "client timed out");
        c->timedout = 1;

        // 超时时结束请求
        ngx_http_finalize_request(r, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    // 处理延迟写或异步I/O
    if (wev->delayed || r->aio) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, wev->log, 0,
                       "http writer delayed");

        // 如果写事件未被延迟，添加发送超时定时器
        if (!wev->delayed) {
            ngx_add_timer(wev, clcf->send_timeout);
        }

        // 处理写事件
        if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
            ngx_http_close_request(r, 0);
        }

        return;
    }

    // 输出过滤
    rc = ngx_http_output_filter(r, NULL);

    // 记录输出过滤结果的调试日志
    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http writer output filter: %i, \"%V?%V\"",
                   rc, &r->uri, &r->args);

    // 如果输出过滤出错，结束请求
    if (rc == NGX_ERROR) {
        ngx_http_finalize_request(r, rc);
        return;
    }

    // 处理缓冲、延迟或主请求的情况
    if (r->buffered || r->postponed || (r == r->main && c->buffered)) {

        // 如果写事件未被延迟，添加发送超时定时器
        if (!wev->delayed) {
            ngx_add_timer(wev, clcf->send_timeout);
        }

        // 处理写事件
        if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
            ngx_http_close_request(r, 0);
        }

        return;
    }

    // 记录写操作完成的调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, wev->log, 0,
                   "http writer done: \"%V?%V\"", &r->uri, &r->args);

    // 设置写事件处理器为空处理器
    r->write_event_handler = ngx_http_request_empty_handler;

    // 结束请求
    ngx_http_finalize_request(r, rc);
}


static void
ngx_http_request_finalizer(ngx_http_request_t *r)
{
    // 记录请求结束的调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http finalizer done: \"%V?%V\"", &r->uri, &r->args);

    // 结束请求
    ngx_http_finalize_request(r, 0);
}


void
ngx_http_block_reading(ngx_http_request_t *r)
{
    // 记录阻塞读取的调试日志
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http reading blocked");

    /* aio does not call this handler */

    // 如果使用水平触发事件，且读事件处于活跃状态
    if ((ngx_event_flags & NGX_USE_LEVEL_EVENT)
        && r->connection->read->active)
    {
        // 尝试删除读事件
        if (ngx_del_event(r->connection->read, NGX_READ_EVENT, 0) != NGX_OK) {
            ngx_http_close_request(r, 0);
        }
    }
}


void
ngx_http_test_reading(ngx_http_request_t *r)
{
    int                n;
    char               buf[1];
    ngx_err_t          err;
    ngx_event_t       *rev;
    ngx_connection_t  *c;

    c = r->connection;
    rev = c->read;

    // 记录测试读取的调试日志
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "http test reading");

#if (NGX_HTTP_V2)

    // 处理HTTP/2连接
    if (r->stream) {
        if (c->error) {
            err = 0;
            goto closed;
        }

        return;
    }

#endif

#if (NGX_HTTP_V3)

    // 处理HTTP/3连接
    if (c->quic) {
        if (rev->error) {
            c->error = 1;
            err = 0;
            goto closed;
        }

        return;
    }

#endif

#if (NGX_HAVE_KQUEUE)

    // 处理kqueue事件
    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {

        if (!rev->pending_eof) {
            return;
        }

        rev->eof = 1;
        c->error = 1;
        err = rev->kq_errno;

        goto closed;
    }

#endif

#if (NGX_HAVE_EPOLLRDHUP)

    // 处理epoll RDHUP事件
    if ((ngx_event_flags & NGX_USE_EPOLL_EVENT) && ngx_use_epoll_rdhup) {
        socklen_t  len;

        if (!rev->pending_eof) {
            return;
        }

        rev->eof = 1;
        c->error = 1;

        err = 0;
        len = sizeof(ngx_err_t);

        /*
         * BSDs和Linux返回0并在err中设置待处理错误
         * Solaris返回-1并设置errno
         */

        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
        {
            err = ngx_socket_errno;
        }

        goto closed;
    }

#endif

    // 尝试从套接字读取数据
    n = recv(c->fd, buf, 1, MSG_PEEK);

    if (n == 0) {
        // 连接已关闭
        rev->eof = 1;
        c->error = 1;
        err = 0;

        goto closed;

    } else if (n == -1) {
        // 发生错误
        err = ngx_socket_errno;

        if (err != NGX_EAGAIN) {
            rev->eof = 1;
            c->error = 1;

            goto closed;
        }
    }

    /* aio不调用此处理程序 */

    // 如果使用水平触发事件，且读事件处于活跃状态
    if ((ngx_event_flags & NGX_USE_LEVEL_EVENT) && rev->active) {

        // 尝试删除读事件
        if (ngx_del_event(rev, NGX_READ_EVENT, 0) != NGX_OK) {
            ngx_http_close_request(r, 0);
        }
    }

    return;

closed:

    if (err) {
        rev->error = 1;
    }

#if (NGX_HTTP_SSL)
    // 如果是SSL连接，设置不发送关闭通知
    if (c->ssl) {
        c->ssl->no_send_shutdown = 1;
    }
#endif

    // 记录客户端过早关闭连接的错误日志
    ngx_log_error(NGX_LOG_INFO, c->log, err,
                  "client prematurely closed connection");

    // 结束请求，返回客户端关闭请求的状态
    ngx_http_finalize_request(r, NGX_HTTP_CLIENT_CLOSED_REQUEST);
}


static void
ngx_http_set_keepalive(ngx_http_request_t *r)
{
    int                        tcp_nodelay;
    ngx_buf_t                 *b, *f;
    ngx_chain_t               *cl, *ln;
    ngx_event_t               *rev, *wev;
    ngx_connection_t          *c;
    ngx_http_connection_t     *hc;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    rev = c->read;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 记录设置HTTP保持连接处理程序的调试日志
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "set http keepalive handler");

    c->log->action = "closing request";

    hc = r->http_connection;
    b = r->header_in;

    if (b->pos < b->last) {

        // 处理管道请求

        if (b != c->buffer) {

            /*
             * 如果在处理前一个请求时分配了大型头部缓冲区，
             * 则我们不会为管道请求使用c->buffer
             * （参见ngx_http_create_request()）。
             *
             * 现在我们将大型头部缓冲区移动到空闲列表。
             */

            for (cl = hc->busy; cl; /* void */) {
                ln = cl;
                cl = cl->next;

                if (ln->buf == b) {
                    ngx_free_chain(c->pool, ln);
                    continue;
                }

                f = ln->buf;
                f->pos = f->start;
                f->last = f->start;

                ln->next = hc->free;
                hc->free = ln;
            }

            cl = ngx_alloc_chain_link(c->pool);
            if (cl == NULL) {
                ngx_http_close_request(r, 0);
                return;
            }

            cl->buf = b;
            cl->next = NULL;

            hc->busy = cl;
            hc->nbusy = 1;
        }
    }

    // 防止从ngx_http_finalize_connection()递归调用
    r->keepalive = 0;

    ngx_http_free_request(r, 0);

    c->data = hc;

    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        ngx_http_close_connection(c);
        return;
    }

    wev = c->write;
    wev->handler = ngx_http_empty_handler;

    if (b->pos < b->last) {

        // 处理管道请求
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "pipelined request");

        c->log->action = "reading client pipelined request line";

        r = ngx_http_create_request(c);
        if (r == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        r->pipeline = 1;

        c->data = r;

        c->sent = 0;
        c->destroyed = 0;
        c->pipeline = 1;

        if (rev->timer_set) {
            ngx_del_timer(rev);
        }

        rev->handler = ngx_http_process_request_line;
        ngx_post_event(rev, &ngx_posted_events);
        return;
    }

    /*
     * 为了使空闲的保持连接的内存占用尽可能小，
     * 我们尝试释放c->buffer的内存（如果它是在c->pool之外分配的）。
     * 大型头部缓冲区总是在c->pool之外分配，也会被释放。
     */

    b = c->buffer;

    if (ngx_pfree(c->pool, b->start) == NGX_OK) {

        /*
         * 为ngx_http_keepalive_handler()设置特殊标记，
         * 表示c->buffer的内存已被释放
         */

        b->pos = NULL;

    } else {
        b->pos = b->start;
        b->last = b->start;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0, "hc free: %p",
                   hc->free);

    if (hc->free) {
        for (cl = hc->free; cl; /* void */) {
            ln = cl;
            cl = cl->next;
            ngx_pfree(c->pool, ln->buf->start);
            ngx_free_chain(c->pool, ln);
        }

        hc->free = NULL;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0, "hc busy: %p %i",
                   hc->busy, hc->nbusy);

    if (hc->busy) {
        for (cl = hc->busy; cl; /* void */) {
            ln = cl;
            cl = cl->next;
            ngx_pfree(c->pool, ln->buf->start);
            ngx_free_chain(c->pool, ln);
        }

        hc->busy = NULL;
        hc->nbusy = 0;
    }

#if (NGX_HTTP_SSL)
    // 如果是SSL连接，释放SSL缓冲区
    if (c->ssl) {
        ngx_ssl_free_buffer(c);
    }
#endif

    rev->handler = ngx_http_keepalive_handler;

    if (wev->active && (ngx_event_flags & NGX_USE_LEVEL_EVENT)) {
        if (ngx_del_event(wev, NGX_WRITE_EVENT, 0) != NGX_OK) {
            ngx_http_close_connection(c);
            return;
        }
    }

    c->log->action = "keepalive";

    if (c->tcp_nopush == NGX_TCP_NOPUSH_SET) {
        if (ngx_tcp_push(c->fd) == -1) {
            ngx_connection_error(c, ngx_socket_errno, ngx_tcp_push_n " failed");
            ngx_http_close_connection(c);
            return;
        }

        c->tcp_nopush = NGX_TCP_NOPUSH_UNSET;
        tcp_nodelay = ngx_tcp_nodelay_and_tcp_nopush ? 1 : 0;

    } else {
        tcp_nodelay = 1;
    }

    if (tcp_nodelay && clcf->tcp_nodelay && ngx_tcp_nodelay(c) != NGX_OK) {
        ngx_http_close_connection(c);
        return;
    }

#if 0
    /* 如果ngx_http_request_t已被释放，则需要其他地方 */
    r->http_state = NGX_HTTP_KEEPALIVE_STATE;
#endif

    c->idle = 1;
    ngx_reusable_connection(c, 1);

    ngx_add_timer(rev, clcf->keepalive_timeout);

    if (rev->ready) {
        ngx_post_event(rev, &ngx_posted_events);
    }
}


static void
ngx_http_keepalive_handler(ngx_event_t *rev)
{
    size_t             size;
    ssize_t            n;
    ngx_buf_t         *b;
    ngx_connection_t  *c;

    c = rev->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "http keepalive handler");

    // 检查连接是否超时或已关闭
    if (rev->timedout || c->close) {
        ngx_http_close_connection(c);
        return;
    }

#if (NGX_HAVE_KQUEUE)

    // 对于kqueue事件模型的特殊处理
    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
        if (rev->pending_eof) {
            c->log->handler = NULL;
            ngx_log_error(NGX_LOG_INFO, c->log, rev->kq_errno,
                          "kevent() reported that client %V closed "
                          "keepalive connection", &c->addr_text);
#if (NGX_HTTP_SSL)
            if (c->ssl) {
                c->ssl->no_send_shutdown = 1;
            }
#endif
            ngx_http_close_connection(c);
            return;
        }
    }

#endif

    b = c->buffer;
    size = b->end - b->start;

    // 如果缓冲区内存已被释放，重新分配
    if (b->pos == NULL) {
        b->pos = ngx_palloc(c->pool, size);
        if (b->pos == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        b->start = b->pos;
        b->last = b->pos;
        b->end = b->pos + size;
    }

    // 忽略ECONNRESET错误，因为MSIE可能会用RST标志关闭keepalive连接
    c->log_error = NGX_ERROR_IGNORE_ECONNRESET;
    ngx_set_socket_errno(0);

    // 尝试接收数据
    n = c->recv(c, b->last, size);
    c->log_error = NGX_ERROR_INFO;

    if (n == NGX_AGAIN) {
        // 如果暂时无数据可读，设置读事件处理
        if (ngx_handle_read_event(rev, 0) != NGX_OK) {
            ngx_http_close_connection(c);
            return;
        }

        // 尝试释放缓冲区内存
        if (ngx_pfree(c->pool, b->start) == NGX_OK) {
            b->pos = NULL;
        }

        return;
    }

    if (n == NGX_ERROR) {
        ngx_http_close_connection(c);
        return;
    }

    c->log->handler = NULL;

    // 客户端关闭了连接
    if (n == 0) {
        ngx_log_error(NGX_LOG_INFO, c->log, ngx_socket_errno,
                      "client %V closed keepalive connection", &c->addr_text);
        ngx_http_close_connection(c);
        return;
    }

    // 更新缓冲区
    b->last += n;

    c->log->handler = ngx_http_log_error;
    c->log->action = "reading client request line";

    // 设置连接为非空闲状态
    c->idle = 0;
    ngx_reusable_connection(c, 0);

    // 创建新的HTTP请求
    c->data = ngx_http_create_request(c);
    if (c->data == NULL) {
        ngx_http_close_connection(c);
        return;
    }

    c->sent = 0;
    c->destroyed = 0;

    ngx_del_timer(rev);

    // 设置下一步处理函数为处理请求行
    rev->handler = ngx_http_process_request_line;
    ngx_http_process_request_line(rev);
}


static void
ngx_http_set_lingering_close(ngx_connection_t *c)
{
    ngx_event_t               *rev, *wev;
    ngx_http_request_t        *r;
    ngx_http_core_loc_conf_t  *clcf;

    r = c->data;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 设置lingering关闭的超时时间
    if (r->lingering_time == 0) {
        r->lingering_time = ngx_time() + (time_t) (clcf->lingering_time / 1000);
    }

#if (NGX_HTTP_SSL)
    // 处理SSL连接的关闭
    if (c->ssl) {
        ngx_int_t  rc;

        c->ssl->shutdown_without_free = 1;

        rc = ngx_ssl_shutdown(c);

        if (rc == NGX_ERROR) {
            ngx_http_close_request(r, 0);
            return;
        }

        if (rc == NGX_AGAIN) {
            c->ssl->handler = ngx_http_set_lingering_close;
            return;
        }
    }
#endif

    // 设置读事件处理函数
    rev = c->read;
    rev->handler = ngx_http_lingering_close_handler;

    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        ngx_http_close_request(r, 0);
        return;
    }

    // 设置写事件处理函数
    wev = c->write;
    wev->handler = ngx_http_empty_handler;

    // 如果写事件活跃，从epoll中删除
    if (wev->active && (ngx_event_flags & NGX_USE_LEVEL_EVENT)) {
        if (ngx_del_event(wev, NGX_WRITE_EVENT, 0) != NGX_OK) {
            ngx_http_close_request(r, 0);
            return;
        }
    }

    // 关闭socket的写端
    if (ngx_shutdown_socket(c->fd, NGX_WRITE_SHUTDOWN) == -1) {
        ngx_connection_error(c, ngx_socket_errno,
                             ngx_shutdown_socket_n " failed");
        ngx_http_close_request(r, 0);
        return;
    }

    // 设置连接为可重用状态
    c->close = 0;
    ngx_reusable_connection(c, 1);

    // 添加读事件定时器
    ngx_add_timer(rev, clcf->lingering_timeout);

    // 如果读事件就绪，立即处理
    if (rev->ready) {
        ngx_http_lingering_close_handler(rev);
    }
}


static void
ngx_http_lingering_close_handler(ngx_event_t *rev)
{
    ssize_t                    n;
    ngx_msec_t                 timer;
    ngx_connection_t          *c;
    ngx_http_request_t        *r;
    ngx_http_core_loc_conf_t  *clcf;
    u_char                     buffer[NGX_HTTP_LINGERING_BUFFER_SIZE];

    c = rev->data;
    r = c->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http lingering close handler");

    // 检查是否超时或连接已关闭
    if (rev->timedout || c->close) {
        ngx_http_close_request(r, 0);
        return;
    }

    // 计算剩余的lingering时间
    timer = (ngx_msec_t) r->lingering_time - (ngx_msec_t) ngx_time();
    if ((ngx_msec_int_t) timer <= 0) {
        ngx_http_close_request(r, 0);
        return;
    }

    // 循环读取剩余数据
    do {
        n = c->recv(c, buffer, NGX_HTTP_LINGERING_BUFFER_SIZE);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0, "lingering read: %z", n);

        if (n == NGX_AGAIN) {
            break;
        }

        if (n == NGX_ERROR || n == 0) {
            ngx_http_close_request(r, 0);
            return;
        }

    } while (rev->ready);

    // 重新设置读事件
    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        ngx_http_close_request(r, 0);
        return;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 调整定时器时间
    timer *= 1000;

    if (timer > clcf->lingering_timeout) {
        timer = clcf->lingering_timeout;
    }

    ngx_add_timer(rev, timer);
}


/**
 * 空的HTTP处理函数
 * 
 * @param wev 写事件结构体指针
 */
void
ngx_http_empty_handler(ngx_event_t *wev)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, wev->log, 0, "http empty handler");

    return;
}


/**
 * 空的HTTP请求处理函数
 * 
 * @param r HTTP请求结构体指针
 */
void
ngx_http_request_empty_handler(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http request empty handler");

    return;
}


/**
 * 发送特殊HTTP响应
 * 
 * @param r HTTP请求结构体指针
 * @param flags 特殊标志
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t
ngx_http_send_special(ngx_http_request_t *r, ngx_uint_t flags)
{
    ngx_buf_t    *b;
    ngx_chain_t   out;

    // 分配缓冲区
    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_ERROR;
    }

    // 设置last_buf标志
    if (flags & NGX_HTTP_LAST) {

        if (r == r->main && !r->post_action) {
            b->last_buf = 1;

        } else {
            b->sync = 1;
            b->last_in_chain = 1;
        }
    }

    // 设置flush标志
    if (flags & NGX_HTTP_FLUSH) {
        b->flush = 1;
    }

    // 构造输出链
    out.buf = b;
    out.next = NULL;

    // 输出过滤
    return ngx_http_output_filter(r, &out);
}


/**
 * 执行HTTP请求的后置动作
 * 
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功，NGX_DECLINED 无需执行后置动作
 */
static ngx_int_t
ngx_http_post_action(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;

    // 获取location配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 检查是否配置了post_action
    if (clcf->post_action.data == NULL) {
        return NGX_DECLINED;
    }

    // 检查是否已经执行过post_action
    if (r->post_action && r->uri_changes == 0) {
        return NGX_DECLINED;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post action: \"%V\"", &clcf->post_action);

    // 减少主请求计数
    r->main->count--;

    // 设置请求属性
    r->http_version = NGX_HTTP_VERSION_9;
    r->header_only = 1;
    r->post_action = 1;

    // 设置读事件处理函数
    r->read_event_handler = ngx_http_block_reading;

    // 执行内部重定向或命名location
    if (clcf->post_action.data[0] == '/') {
        ngx_http_internal_redirect(r, &clcf->post_action, NULL);

    } else {
        ngx_http_named_location(r, &clcf->post_action);
    }

    return NGX_OK;
}


/**
 * 关闭HTTP请求
 * 
 * @param r HTTP请求结构体指针
 * @param rc 返回码
 */
void
ngx_http_close_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_connection_t  *c;

    r = r->main;
    c = r->connection;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http request count:%d blk:%d", r->count, r->blocked);

    // 检查请求计数
    if (r->count == 0) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0, "http request count is zero");
    }

    r->count--;

    // 如果请求还在处理中，直接返回
    if (r->count || r->blocked) {
        return;
    }

#if (NGX_HTTP_V2)
    // 处理HTTP/2流
    if (r->stream) {
        ngx_http_v2_close_stream(r->stream, rc);
        return;
    }
#endif

    // 释放请求资源并关闭连接
    ngx_http_free_request(r, rc);
    ngx_http_close_connection(c);
}


/**
 * 释放HTTP请求资源
 * 
 * @param r HTTP请求结构体指针
 * @param rc 返回码
 */
void
ngx_http_free_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_t                 *log;
    ngx_pool_t                *pool;
    struct linger              linger;
    ngx_http_cleanup_t        *cln;
    ngx_http_log_ctx_t        *ctx;
    ngx_http_core_loc_conf_t  *clcf;

    log = r->connection->log;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, "http close request");

    // 检查请求池是否已释放
    if (r->pool == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0, "http request already closed");
        return;
    }

    // 执行清理函数
    cln = r->cleanup;
    r->cleanup = NULL;

    while (cln) {
        if (cln->handler) {
            cln->handler(cln->data);
        }

        cln = cln->next;
    }

#if (NGX_STAT_STUB)
    // 更新统计信息
    if (r->stat_reading) {
        (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
    }

    if (r->stat_writing) {
        (void) ngx_atomic_fetch_add(ngx_stat_writing, -1);
    }

#endif

    // 设置响应状态码
    if (rc > 0 && (r->headers_out.status == 0 || r->connection->sent == 0)) {
        r->headers_out.status = rc;
    }

    // 记录请求日志
    if (!r->logged) {
        log->action = "logging request";

        ngx_http_log_request(r);
    }

    log->action = "closing request";

    // 处理超时连接
    if (r->connection->timedout
#if (NGX_HTTP_V3)
        && r->connection->quic == NULL
#endif
       )
    {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->reset_timedout_connection) {
            linger.l_onoff = 1;
            linger.l_linger = 0;

            if (setsockopt(r->connection->fd, SOL_SOCKET, SO_LINGER,
                           (const void *) &linger, sizeof(struct linger)) == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, log, ngx_socket_errno,
                              "setsockopt(SO_LINGER) failed");
            }
        }
    }

    // 清理日志上下文
    ctx = log->data;
    ctx->request = NULL;

    r->request_line.len = 0;

    r->connection->destroyed = 1;

    // 销毁请求内存池
    pool = r->pool;
    r->pool = NULL;

    ngx_destroy_pool(pool);
}


/*
 * 函数名: ngx_http_log_request
 * 功能: 记录HTTP请求的日志
 * 参数: r - HTTP请求结构体指针
 */
static void
ngx_http_log_request(ngx_http_request_t *r)
{
    ngx_uint_t                  i, n;
    ngx_http_handler_pt        *log_handler;
    ngx_http_core_main_conf_t  *cmcf;

    // 获取HTTP核心模块的主配置
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    // 获取日志阶段的处理函数数组
    log_handler = cmcf->phases[NGX_HTTP_LOG_PHASE].handlers.elts;
    // 获取日志处理函数的数量
    n = cmcf->phases[NGX_HTTP_LOG_PHASE].handlers.nelts;

    // 遍历并执行所有的日志处理函数
    for (i = 0; i < n; i++) {
        log_handler[i](r);
    }
}


// 关闭HTTP连接的函数
void
ngx_http_close_connection(ngx_connection_t *c)
{
    ngx_pool_t  *pool;

    // 记录调试日志，显示正在关闭的连接文件描述符
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "close http connection: %d", c->fd);

#if (NGX_HTTP_SSL)

    // 如果是SSL连接，尝试关闭SSL
    if (c->ssl) {
        if (ngx_ssl_shutdown(c) == NGX_AGAIN) {
            c->ssl->handler = ngx_http_close_connection;
            return;
        }
    }

#endif

#if (NGX_HTTP_V3)
    // 如果是HTTP/3连接，重置QUIC流
    if (c->quic) {
        ngx_http_v3_reset_stream(c);
    }
#endif

#if (NGX_STAT_STUB)
    // 更新活跃连接统计
    (void) ngx_atomic_fetch_add(ngx_stat_active, -1);
#endif

    // 标记连接为已销毁
    c->destroyed = 1;

    // 保存连接的内存池
    pool = c->pool;

    // 关闭连接
    ngx_close_connection(c);

    // 销毁内存池
    ngx_destroy_pool(pool);
}


// 记录HTTP错误日志的函数
static u_char *
ngx_http_log_error(ngx_log_t *log, u_char *buf, size_t len)
{
    u_char              *p;
    ngx_http_request_t  *r;
    ngx_http_log_ctx_t  *ctx;

    // 如果有正在执行的动作，记录到日志中
    if (log->action) {
        p = ngx_snprintf(buf, len, " while %s", log->action);
        len -= p - buf;
        buf = p;
    }

    ctx = log->data;

    // 记录客户端地址
    p = ngx_snprintf(buf, len, ", client: %V", &ctx->connection->addr_text);
    len -= p - buf;

    r = ctx->request;

    // 如果存在请求，使用请求的日志处理函数
    if (r) {
        return r->log_handler(r, ctx->current_request, p, len);

    } else {
        // 如果不存在请求，记录服务器地址
        p = ngx_snprintf(p, len, ", server: %V",
                         &ctx->connection->listening->addr_text);
    }

    return p;
}


/**
 * @brief 处理HTTP错误日志的函数
 * 
 * @param r 当前HTTP请求
 * @param sr 子请求（如果存在）
 * @param buf 日志缓冲区
 * @param len 缓冲区长度
 * @return u_char* 更新后的缓冲区指针
 */
static u_char *
ngx_http_log_error_handler(ngx_http_request_t *r, ngx_http_request_t *sr,
    u_char *buf, size_t len)
{
    char                      *uri_separator;
    u_char                    *p;
    ngx_http_upstream_t       *u;
    ngx_http_core_srv_conf_t  *cscf;

    // 获取核心服务器配置
    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

    // 记录服务器名称
    p = ngx_snprintf(buf, len, ", server: %V", &cscf->server_name);
    len -= p - buf;
    buf = p;

    // 如果请求行未设置但请求已开始，则设置请求行
    if (r->request_line.data == NULL && r->request_start) {
        for (p = r->request_start; p < r->header_in->last; p++) {
            if (*p == CR || *p == LF) {
                break;
            }
        }

        r->request_line.len = p - r->request_start;
        r->request_line.data = r->request_start;
    }

    // 记录请求行
    if (r->request_line.len) {
        p = ngx_snprintf(buf, len, ", request: \"%V\"", &r->request_line);
        len -= p - buf;
        buf = p;
    }

    // 如果是子请求，记录子请求的URI
    if (r != sr) {
        p = ngx_snprintf(buf, len, ", subrequest: \"%V\"", &sr->uri);
        len -= p - buf;
        buf = p;
    }

    // 获取上游信息
    u = sr->upstream;

    // 记录上游信息（如果存在）
    if (u && u->peer.name) {

        uri_separator = "";

#if (NGX_HAVE_UNIX_DOMAIN)
        if (u->peer.sockaddr && u->peer.sockaddr->sa_family == AF_UNIX) {
            uri_separator = ":";
        }
#endif

        p = ngx_snprintf(buf, len, ", upstream: \"%V%V%s%V\"",
                         &u->schema, u->peer.name,
                         uri_separator, &u->uri);
        len -= p - buf;
        buf = p;
    }

    // 记录Host头
    if (r->headers_in.host) {
        p = ngx_snprintf(buf, len, ", host: \"%V\"",
                         &r->headers_in.host->value);
        len -= p - buf;
        buf = p;
    }

    // 记录Referer头
    if (r->headers_in.referer) {
        p = ngx_snprintf(buf, len, ", referrer: \"%V\"",
                         &r->headers_in.referer->value);
        buf = p;
    }

    return buf;
}
