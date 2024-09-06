
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_H_INCLUDED_
#define _NGX_HTTP_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* 
 * 定义 ngx_http_request_t 类型
 * 这是一个指向 ngx_http_request_s 结构体的别名
 * 用于表示 HTTP 请求的相关信息和状态
 */
typedef struct ngx_http_request_s     ngx_http_request_t;
/**
 * @brief 定义 ngx_http_upstream_t 类型
 *
 * 这是一个指向 ngx_http_upstream_s 结构体的别名
 * 用于表示 HTTP 上游服务器的相关信息和状态
 * 上游服务器通常指的是 Nginx 反向代理或负载均衡后面的实际处理请求的服务器
 */
typedef struct ngx_http_upstream_s    ngx_http_upstream_t;
/**
 * @brief 定义 ngx_http_cache_t 类型
 *
 * 这是一个指向 ngx_http_cache_s 结构体的别名
 * 用于表示 HTTP 缓存的相关信息和状态
 * 缓存机制可以提高服务器性能，减少对后端服务器的请求
 */
typedef struct ngx_http_cache_s       ngx_http_cache_t;
/**
 * @brief 定义 ngx_http_file_cache_t 类型
 *
 * 这是一个指向 ngx_http_file_cache_s 结构体的别名
 * 用于表示 HTTP 文件缓存的相关信息和状态
 * 文件缓存机制可以将响应内容保存在文件系统中，提高静态内容的访问速度
 */
typedef struct ngx_http_file_cache_s  ngx_http_file_cache_t;
/**
 * @brief 定义 ngx_http_log_ctx_t 类型
 *
 * 这是一个指向 ngx_http_log_ctx_s 结构体的别名
 * 用于表示 HTTP 日志上下文的相关信息
 * 日志上下文包含了记录日志时需要的各种信息，如连接、请求等
 */
typedef struct ngx_http_log_ctx_s     ngx_http_log_ctx_t;
/**
 * @brief 定义 ngx_http_chunked_t 类型
 *
 * 这是一个指向 ngx_http_chunked_s 结构体的别名
 * 用于表示 HTTP 分块传输编码的相关信息和状态
 * 分块传输编码允许服务器动态生成内容，并以块的形式发送，而不需要预先知道内容的总长度
 */
typedef struct ngx_http_chunked_s     ngx_http_chunked_t;
/**
 * @brief 定义 ngx_http_v2_stream_t 类型
 *
 * 这是一个指向 ngx_http_v2_stream_s 结构体的别名
 * 用于表示 HTTP/2 流的相关信息和状态
 * HTTP/2 流是 HTTP/2 协议中的基本单位，用于并行处理多个请求和响应
 */
typedef struct ngx_http_v2_stream_s   ngx_http_v2_stream_t;
/**
 * @brief 定义 ngx_http_v3_parse_t 类型
 *
 * 这是一个指向 ngx_http_v3_parse_s 结构体的别名
 * 用于表示 HTTP/3 解析相关的信息和状态
 * HTTP/3 是基于 QUIC 协议的新一代 HTTP 协议，这个结构体可能包含了解析 HTTP/3 请求和响应的相关数据
 */
typedef struct ngx_http_v3_parse_s    ngx_http_v3_parse_t;
/**
 * @brief 定义 ngx_http_v3_session_t 类型
 *
 * 这是一个指向 ngx_http_v3_session_s 结构体的别名
 * 用于表示 HTTP/3 会话的相关信息和状态
 * HTTP/3 会话管理 QUIC 连接上的多个 HTTP 请求和响应，
 * 包括流控制、优先级处理等功能
 */
typedef struct ngx_http_v3_session_s  ngx_http_v3_session_t;

/**
 * @brief 定义HTTP头部处理函数的函数指针类型
 *
 * 这个函数指针类型用于处理HTTP请求的头部字段。
 * 它接收一个HTTP请求对象指针和一个头部字段对象指针作为参数，
 * 并返回一个ngx_int_t类型的结果。
 *
 * @param r 指向当前HTTP请求对象的指针
 * @param h 指向当前正在处理的头部字段对象的指针
 * @param offset 头部字段在请求结构中的偏移量
 * @return 返回处理结果，通常是NGX_OK表示成功，或者错误码
 */
typedef ngx_int_t (*ngx_http_header_handler_pt)(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 定义HTTP日志处理函数的函数指针类型
 *
 * 这个函数指针类型用于处理HTTP请求的日志记录。
 * 它接收一个主HTTP请求对象指针和一个子请求对象指针（如果有的话）作为参数，
 * 以及一个缓冲区和长度，用于写入日志内容。
 *
 * @param r 指向主HTTP请求对象的指针
 * @param sr 指向子请求对象的指针（如果没有子请求，则为NULL）
 * @param buf 指向用于写入日志内容的缓冲区
 * @param len 缓冲区的长度
 * @return 返回写入日志后的缓冲区位置
 */
typedef u_char *(*ngx_http_log_handler_pt)(ngx_http_request_t *r,
    ngx_http_request_t *sr, u_char *buf, size_t len);


#include <ngx_http_variables.h>
#include <ngx_http_config.h>
#include <ngx_http_request.h>
#include <ngx_http_script.h>
#include <ngx_http_upstream.h>
#include <ngx_http_upstream_round_robin.h>
#include <ngx_http_core_module.h>

#if (NGX_HTTP_V2)
#include <ngx_http_v2.h>
#endif
#if (NGX_HTTP_V3)
#include <ngx_http_v3.h>
#endif
#if (NGX_HTTP_CACHE)
#include <ngx_http_cache.h>
#endif
#if (NGX_HTTP_SSI)
#include <ngx_http_ssi_filter_module.h>
#endif
#if (NGX_HTTP_SSL)
#include <ngx_http_ssl_module.h>
#endif


/* 
 * ngx_http_log_ctx_s 结构体定义了HTTP日志上下文
 * 用于存储与日志记录相关的信息
 */
struct ngx_http_log_ctx_s {
    ngx_connection_t    *connection;      /* 指向当前连接的指针 */
    ngx_http_request_t  *request;         /* 指向原始HTTP请求的指针 */
    ngx_http_request_t  *current_request; /* 指向当前正在处理的HTTP请求的指针，可能是原始请求或子请求 */
};


/*
 * ngx_http_chunked_s 结构体定义了HTTP分块传输编码的上下文
 * 用于处理HTTP分块传输编码的请求和响应
 */
struct ngx_http_chunked_s {
    ngx_uint_t           state;   /* 当前分块传输的状态 */
    off_t                size;    /* 当前分块的大小 */
    off_t                length;  /* 整个消息体的总长度 */
};


/*
 * ngx_http_status_t 结构体定义了HTTP状态码的结构
 * 用于表示HTTP响应的状态码和相关信息
 */
typedef struct {
    ngx_uint_t           http_version;  /* HTTP版本号 */
    ngx_uint_t           code;          /* HTTP状态码 */
    ngx_uint_t           count;         /* 状态行中状态码后面的空格数量 */
    u_char              *start;         /* 状态行的起始位置指针 */
    u_char              *end;           /* 状态行的结束位置指针 */
} ngx_http_status_t;


/*
 * 获取HTTP模块的上下文
 * 
 * @param r 请求结构体指针
 * @param module 模块结构体指针
 * @return 模块的上下文指针
 */
#define ngx_http_get_module_ctx(r, module)  (r)->ctx[module.ctx_index]


/*
 * 设置HTTP模块的上下文
 * 
 * @param r 请求结构体指针
 * @param c 上下文指针
 * @param module 模块结构体指针
 */
#define ngx_http_set_ctx(r, c, module)      r->ctx[module.ctx_index] = c;


/*
 * 添加HTTP位置配置
 * 
 * @param cf 配置结构体指针
 * @param locations 位置队列指针
 * @param clcf 核心位置配置结构体指针
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_http_add_location(ngx_conf_t *cf, ngx_queue_t **locations,
    ngx_http_core_loc_conf_t *clcf);
/*
 * 添加HTTP监听配置
 * 
 * @param cf 配置结构体指针
 * @param cscf HTTP核心服务器配置结构体指针
 * @param lsopt 监听选项结构体指针
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_http_add_listen(ngx_conf_t *cf, ngx_http_core_srv_conf_t *cscf,
    ngx_http_listen_opt_t *lsopt);


/*
 * 初始化HTTP连接
 * 
 * @param c 连接结构体指针
 */
void ngx_http_init_connection(ngx_connection_t *c);


/*
 * 关闭HTTP连接
 * 
 * @param c 连接结构体指针
 * @description 此函数用于关闭一个HTTP连接，释放相关资源
 */
void ngx_http_close_connection(ngx_connection_t *c);

#if (NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME)
/*
 * SSL服务器名称回调函数
 *
 * @param ssl_conn SSL连接对象
 * @param ad 警告描述符指针
 * @param arg 额外参数
 * @return 成功返回SSL_TLSEXT_ERR_OK，失败返回其他SSL错误码
 *
 * 此函数用于处理SSL/TLS服务器名称指示(SNI)扩展。
 * 当客户端在SSL握手时发送SNI时，此函数会被调用以选择适当的SSL上下文。
 */
int ngx_http_ssl_servername(ngx_ssl_conn_t *ssl_conn, int *ad, void *arg);
#endif
#if (NGX_HTTP_SSL && defined SSL_R_CERT_CB_ERROR)
/*
 * SSL证书回调函数
 *
 * @param ssl_conn SSL连接对象
 * @param arg 额外参数
 * @return 成功返回1，失败返回0
 *
 * 此函数用于处理SSL/TLS证书的动态选择或验证。
 * 当需要选择或验证SSL证书时，此函数会被调用。
 * 它允许根据连接的具体情况动态选择适当的证书。
 */
int ngx_http_ssl_certificate(ngx_ssl_conn_t *ssl_conn, void *arg);
#endif


/*
 * 解析HTTP请求行
 *
 * @param r HTTP请求结构体指针
 * @param b 包含请求行的缓冲区
 * @return 成功返回NGX_OK，失败返回错误码
 *
 * 此函数用于解析HTTP请求的第一行，即请求行。
 * 它会从给定的缓冲区中提取HTTP方法、URI和HTTP版本。
 */
ngx_int_t ngx_http_parse_request_line(ngx_http_request_t *r, ngx_buf_t *b);
/*
 * 解析HTTP请求的URI
 *
 * @param r HTTP请求结构体指针
 * @return 成功返回NGX_OK，失败返回错误码
 *
 * 此函数用于解析HTTP请求中的URI。
 * 它会处理URI中的各个组成部分，如协议、主机名、路径等。
 * 解析结果将存储在请求结构体中，以便后续处理使用。
 */
ngx_int_t ngx_http_parse_uri(ngx_http_request_t *r);
/*
 * 解析复杂的HTTP URI
 *
 * @param r HTTP请求结构体指针
 * @param merge_slashes 是否合并多个连续的斜杠
 * @return 成功返回NGX_OK，失败返回错误码
 *
 * 此函数用于解析复杂的HTTP URI。
 * 它能处理包含查询参数、片段标识符等的完整URI。
 * 如果merge_slashes为真，则会将多个连续的斜杠合并为一个。
 */
ngx_int_t ngx_http_parse_complex_uri(ngx_http_request_t *r,
    ngx_uint_t merge_slashes);
/*
 * 解析HTTP响应状态行
 *
 * @param r HTTP请求结构体指针
 * @param b 包含状态行的缓冲区
 * @param status 用于存储解析结果的状态结构体指针
 * @return 成功返回NGX_OK，失败返回错误码
 *
 * 此函数用于解析HTTP响应的状态行。
 * 它从给定的缓冲区中提取HTTP版本、状态码和状态消息。
 * 解析结果将存储在status结构体中，以便后续处理使用。
 */
ngx_int_t ngx_http_parse_status_line(ngx_http_request_t *r, ngx_buf_t *b,
    ngx_http_status_t *status);
/*
 * 解析不安全的HTTP URI
 *
 * @param r HTTP请求结构体指针
 * @param uri 待解析的URI字符串
 * @param args 用于存储解析出的查询参数
 * @param flags 解析标志位
 * @return 成功返回NGX_OK，失败返回错误码
 *
 * 此函数用于解析可能包含不安全字符的HTTP URI。
 * 它会处理URI中的特殊字符，如百分号编码等。
 * 解析结果将存储在相应的参数中，以便后续处理使用。
 */
ngx_int_t ngx_http_parse_unsafe_uri(ngx_http_request_t *r, ngx_str_t *uri,
    ngx_str_t *args, ngx_uint_t *flags);
/*
 * 解析HTTP头部行
 *
 * @param r HTTP请求结构体指针
 * @param b 包含头部行的缓冲区
 * @param allow_underscores 是否允许下划线出现在头部名称中
 * @return 成功返回NGX_OK，失败返回错误码
 *
 * 此函数用于解析单个HTTP头部行。
 * 它从给定的缓冲区中提取头部名称和值。
 * 如果allow_underscores为真，则允许头部名称中包含下划线。
 * 解析结果将存储在请求结构体中，以便后续处理使用。
 */
ngx_int_t ngx_http_parse_header_line(ngx_http_request_t *r, ngx_buf_t *b,
    ngx_uint_t allow_underscores);
/*
 * 解析多行HTTP头部
 *
 * @param r HTTP请求结构体指针
 * @param headers 头部数组
 * @param name 要查找的头部名称
 * @param value 用于存储找到的头部值
 * @return 找到的头部结构体指针，如果未找到则返回NULL
 *
 * 此函数用于解析可能跨多行的HTTP头部。
 * 它在给定的头部数组中查找指定名称的头部，
 * 并处理可能的多行值（如Set-Cookie头）。
 * 找到的头部值将存储在value参数中。
 */
ngx_table_elt_t *ngx_http_parse_multi_header_lines(ngx_http_request_t *r,
    ngx_table_elt_t *headers, ngx_str_t *name, ngx_str_t *value);
/*
 * 解析Set-Cookie头部行
 *
 * @param r HTTP请求结构体指针
 * @param headers 头部数组
 * @param name 要查找的头部名称（通常是"Set-Cookie"）
 * @param value 用于存储找到的头部值
 * @return 找到的头部结构体指针，如果未找到则返回NULL
 *
 * 此函数专门用于解析Set-Cookie头部，它可能包含多个值。
 * 它在给定的头部数组中查找Set-Cookie头部，
 * 并处理可能的多个Set-Cookie值。
 * 找到的头部值将存储在value参数中。
 */
ngx_table_elt_t *ngx_http_parse_set_cookie_lines(ngx_http_request_t *r,
    ngx_table_elt_t *headers, ngx_str_t *name, ngx_str_t *value);
/*
 * 从HTTP请求中获取指定参数的值
 *
 * @param r HTTP请求结构体指针
 * @param name 要查找的参数名
 * @param len 参数名的长度
 * @param value 用于存储找到的参数值的结构体指针
 * @return 成功找到参数返回NGX_OK，未找到返回NGX_DECLINED，出错返回NGX_ERROR
 *
 * 此函数用于从HTTP请求的查询字符串或请求体中提取指定名称的参数值。
 * 它会搜索GET请求的查询字符串和POST请求的表单数据。
 * 找到的参数值将存储在value参数指向的ngx_str_t结构中。
 */
ngx_int_t ngx_http_arg(ngx_http_request_t *r, u_char *name, size_t len,
    ngx_str_t *value);
/*
 * 分割URI和查询参数
 *
 * @param r HTTP请求结构体指针
 * @param uri 用于存储分割后的URI
 * @param args 用于存储分割后的查询参数
 *
 * 此函数用于将完整的请求URI分割成URI部分和查询参数部分。
 * 它会在URI中查找'?'字符，将其前面的部分作为URI，后面的部分作为查询参数。
 * 分割后的结果分别存储在uri和args参数中。
 */
void ngx_http_split_args(ngx_http_request_t *r, ngx_str_t *uri,
    ngx_str_t *args);
/*
 * 解析分块传输编码的HTTP请求体
 *
 * @param r HTTP请求结构体指针
 * @param b 包含请求体数据的缓冲区
 * @param ctx 分块解析的上下文结构体指针
 * @return NGX_OK 解析成功
 *         NGX_AGAIN 需要更多数据
 *         NGX_ERROR 解析错误
 *
 * 此函数用于解析使用分块传输编码(chunked transfer encoding)的HTTP请求体。
 * 它会逐块解析请求体，处理每个块的大小和数据。
 * 解析状态和进度保存在ctx结构体中，以支持多次调用。
 */
ngx_int_t ngx_http_parse_chunked(ngx_http_request_t *r, ngx_buf_t *b,
    ngx_http_chunked_t *ctx);


/*
 * 创建一个新的HTTP请求对象
 *
 * @param c 与请求相关联的连接对象指针
 * @return 成功时返回新创建的HTTP请求对象指针，失败时返回NULL
 *
 * 此函数用于为新到达的HTTP连接创建一个请求对象。
 * 它会分配内存并初始化请求结构体中的各个字段。
 * 创建的请求对象与给定的连接对象相关联。
 */
ngx_http_request_t *ngx_http_create_request(ngx_connection_t *c);
/*
 * 处理HTTP请求的URI
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 处理成功
 *         NGX_ERROR 处理失败
 *         NGX_DECLINED 需要进一步处理
 *
 * 此函数用于处理HTTP请求中的URI。
 * 它会对URI进行解析、验证和规范化，
 * 并根据需要进行重写或重定向等操作。
 * 处理过程中可能会涉及到URI的解码、
 * 路径合并、别名处理等多个步骤。
 */
ngx_int_t ngx_http_process_request_uri(ngx_http_request_t *r);
/*
 * 处理HTTP请求头
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 处理成功
 *         NGX_ERROR 处理失败
 *         NGX_AGAIN 需要更多数据
 *
 * 此函数用于处理HTTP请求的头部信息。
 * 它会解析和验证请求头中的各个字段，
 * 包括请求方法、HTTP版本、各种头部等。
 * 同时也会进行一些必要的安全检查和规范化处理。
 */
ngx_int_t ngx_http_process_request_header(ngx_http_request_t *r);
/*
 * 处理HTTP请求
 *
 * @param r HTTP请求结构体指针
 * @return 无返回值
 *
 * 此函数用于处理完整的HTTP请求。
 * 它在请求头部和URI都已经被解析后调用，
 * 负责执行请求的主要处理逻辑，包括:
 * - 设置请求的处理函数
 * - 调用各个阶段的处理器
 * - 生成响应
 * - 发送响应给客户端
 * 整个HTTP请求的生命周期主要由这个函数控制。
 */
void ngx_http_process_request(ngx_http_request_t *r);
/*
 * 更新HTTP请求的位置配置
 *
 * @param r HTTP请求结构体指针
 * @return 无返回值
 *
 * 此函数用于更新给定HTTP请求的位置配置。
 * 它可能会在请求处理过程中被调用，以确保使用最新的位置配置。
 * 更新可能涉及重新评估请求URI与位置配置的匹配，
 * 并相应地调整请求的处理逻辑。
 */
void ngx_http_update_location_config(ngx_http_request_t *r);
/*
 * 处理HTTP请求的主要函数
 *
 * @param r HTTP请求结构体指针
 * @return 无返回值
 *
 * 此函数是HTTP请求处理的核心函数。它负责:
 * - 初始化请求处理的各个阶段
 * - 调用相应的处理器来处理请求
 * - 生成HTTP响应
 * - 处理错误情况
 * - 最终完成请求的处理
 *
 * 在整个HTTP请求的生命周期中，这个函数起着关键作用，
 * 协调各个模块和阶段的处理流程。
 */
void ngx_http_handler(ngx_http_request_t *r);
/*
 * 运行已发布的HTTP请求
 *
 * @param c 连接结构体指针
 * @return 无返回值
 *
 * 此函数用于处理已发布（posted）的HTTP请求。
 * 在Nginx中，某些请求可能会被"发布"以便稍后处理，
 * 通常是为了优化性能或处理异步操作。
 * 这个函数会遍历并处理与给定连接相关的所有已发布请求，
 * 确保它们得到适当的处理和响应。
 */
void ngx_http_run_posted_requests(ngx_connection_t *c);
/*
 * 发布HTTP请求
 *
 * @param r HTTP请求结构体指针
 * @param pr 已发布的请求结构体指针
 * @return NGX_OK 成功发布请求
 *         NGX_ERROR 发布请求失败
 *
 * 此函数用于将HTTP请求添加到已发布请求队列中，以便稍后处理。
 * 这通常用于异步处理或延迟处理某些请求。
 */
ngx_int_t ngx_http_post_request(ngx_http_request_t *r,
    ngx_http_posted_request_t *pr);
/*
 * 设置HTTP请求的虚拟服务器
 *
 * @param r HTTP请求结构体指针
 * @param host 主机名字符串指针
 * @return NGX_OK 成功设置虚拟服务器
 *         NGX_ERROR 设置虚拟服务器失败
 *
 * 此函数用于根据给定的主机名为HTTP请求设置适当的虚拟服务器。
 * 它会根据主机名匹配配置中定义的虚拟服务器，并更新请求的相关配置。
 * 这个过程通常发生在请求处理的早期阶段，以确保后续的处理基于正确的服务器配置。
 */
ngx_int_t ngx_http_set_virtual_server(ngx_http_request_t *r,
    ngx_str_t *host);
/*
 * 验证HTTP主机名
 *
 * @param host 指向包含主机名的ngx_str_t结构的指针
 * @param pool 内存池指针，用于分配内存
 * @param alloc 是否需要分配新内存的标志
 * @return NGX_OK 主机名有效
 *         NGX_ERROR 主机名无效或处理过程中出错
 *
 * 此函数用于验证HTTP请求中的主机名。
 * 它检查主机名的格式是否正确，并可能进行规范化处理。
 * 如果alloc标志设置为真，函数可能会为规范化的主机名分配新的内存。
 */
ngx_int_t ngx_http_validate_host(ngx_str_t *host, ngx_pool_t *pool,
    ngx_uint_t alloc);
/*
 * 关闭HTTP请求
 *
 * @param r HTTP请求结构体指针
 * @param rc 返回码，表示关闭操作的结果
 *
 * 此函数用于关闭一个HTTP请求。它会执行必要的清理操作，
 * 释放与请求相关的资源，并可能触发一些回调函数。
 * 返回码rc用于指示关闭操作的结果或原因，可能影响后续的错误处理或日志记录。
 */
void ngx_http_close_request(ngx_http_request_t *r, ngx_int_t rc);
/*
 * 完成HTTP请求处理
 *
 * @param r HTTP请求结构体指针
 * @param rc 返回码，表示请求处理的结果
 *
 * 此函数用于完成HTTP请求的处理。它会执行以下操作：
 * 1. 根据返回码rc决定后续处理流程
 * 2. 可能会发送响应头和主体
 * 3. 处理请求相关的清理工作
 * 4. 可能会触发后续的请求（如子请求）
 * 5. 在适当的时候关闭请求
 * 
 * 这是请求处理生命周期中的关键函数，通常在所有处理完成后调用。
 */
void ngx_http_finalize_request(ngx_http_request_t *r, ngx_int_t rc);
/*
 * 释放HTTP请求资源
 *
 * @param r HTTP请求结构体指针
 * @param rc 返回码，表示释放操作的结果
 *
 * 此函数用于释放与HTTP请求相关的所有资源。它会执行以下操作：
 * 1. 释放请求体相关的内存
 * 2. 关闭相关的文件描述符
 * 3. 清理请求相关的内存池
 * 4. 处理任何未完成的异步操作
 * 5. 可能会触发一些清理回调函数
 * 
 * 这个函数通常在请求处理完全结束，不再需要任何相关资源时调用。
 * 返回码rc可能用于指示释放过程中的特殊情况或最终的处理结果。
 */
void ngx_http_free_request(ngx_http_request_t *r, ngx_int_t rc);

/*
 * HTTP空处理函数
 *
 * @param wev 写事件结构体指针
 *
 * 此函数是一个空的处理函数，用于HTTP模块中需要一个处理函数但实际上不需要执行任何操作的场景。
 * 它可能被用作占位符或默认处理函数，以满足某些接口要求。
 * 当调用此函数时，它不会执行任何实际操作。
 */
void ngx_http_empty_handler(ngx_event_t *wev);
/*
 * HTTP请求空处理函数
 *
 * @param r HTTP请求结构体指针
 *
 * 此函数是一个空的请求处理函数，用于HTTP模块中需要一个请求处理函数但实际上不需要执行任何操作的场景。
 * 它可能被用作占位符或默认的请求处理函数，以满足某些接口要求。
 * 当调用此函数时，它不会对传入的HTTP请求执行任何实际操作。
 */
void ngx_http_request_empty_handler(ngx_http_request_t *r);


/*
 * HTTP请求处理标志：最后一个数据块
 *
 * 此宏定义表示当前数据块是HTTP响应的最后一个数据块。
 * 当使用此标志时，通常意味着响应数据传输即将结束，
 * 可能会触发一些清理操作或者结束请求的处理流程。
 */
#define NGX_HTTP_LAST   1
/*
 * HTTP请求处理标志：刷新缓冲区
 *
 * 此宏定义表示需要立即刷新输出缓冲区。
 * 当使用此标志时，Nginx会尝试立即将当前缓冲区中的数据发送给客户端，
 * 而不是等待缓冲区填满或者响应结束。
 * 这对于需要实时性的应用场景（如流媒体或长轮询）非常有用。
 */
#define NGX_HTTP_FLUSH  2

/*
 * 发送特殊HTTP响应
 *
 * @param r HTTP请求结构体指针
 * @param flags 特殊标志，可以是NGX_HTTP_LAST或NGX_HTTP_FLUSH
 * @return NGX_OK 成功时返回
 *         NGX_ERROR 发生错误时返回
 *
 * 此函数用于发送特殊的HTTP响应，通常用于处理最后一个数据块或强制刷新缓冲区。
 * 当flags为NGX_HTTP_LAST时，表示这是最后一个数据块，可能会触发请求结束的处理。
 * 当flags为NGX_HTTP_FLUSH时，会立即刷新输出缓冲区，将数据发送给客户端。
 * 这个函数在需要精细控制HTTP响应发送过程时非常有用。
 */
ngx_int_t ngx_http_send_special(ngx_http_request_t *r, ngx_uint_t flags);


/*
 * 读取客户端请求体
 *
 * @param r HTTP请求结构体指针
 * @param post_handler 读取完成后的回调处理函数
 * @return NGX_OK 成功时返回
 *         NGX_ERROR 发生错误时返回
 *         NGX_AGAIN 需要继续读取时返回
 *
 * 此函数用于读取HTTP客户端请求的请求体。
 * 它会根据请求的Content-Length或Transfer-Encoding来决定如何读取请求体。
 * 读取完成后，会调用post_handler回调函数进行后续处理。
 * 这个函数通常用于处理POST、PUT等包含请求体的HTTP方法。
 */
ngx_int_t ngx_http_read_client_request_body(ngx_http_request_t *r,
    ngx_http_client_body_handler_pt post_handler);
/*
 * 读取未缓冲的请求体
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功时返回
 *         NGX_ERROR 发生错误时返回
 *         NGX_AGAIN 需要继续读取时返回
 *
 * 此函数用于读取未被缓冲的HTTP请求体。
 * 与ngx_http_read_client_request_body不同，这个函数直接从连接中读取数据，
 * 不会将整个请求体缓存在内存中。
 * 这对于处理大型请求体或需要流式处理的场景非常有用，可以减少内存使用。
 */
ngx_int_t ngx_http_read_unbuffered_request_body(ngx_http_request_t *r);

/*
 * 发送HTTP响应头
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功发送响应头时返回
 *         NGX_ERROR 发生错误时返回
 *
 * 此函数用于发送HTTP响应的头部信息。
 * 它会根据请求结构体中的信息构造并发送适当的HTTP响应头。
 * 这个函数通常在处理HTTP请求并准备发送响应体之前调用。
 * 它负责设置状态码、内容类型、长度等重要的头部字段。
 */
ngx_int_t ngx_http_send_header(ngx_http_request_t *r);
/*
 * 处理特殊HTTP响应
 *
 * @param r HTTP请求结构体指针
 * @param error 错误码
 * @return NGX_OK 成功处理特殊响应时返回
 *         NGX_ERROR 发生错误时返回
 *         NGX_DECLINED 拒绝处理时返回
 *
 * 此函数用于处理特殊的HTTP响应，通常是错误响应或重定向。
 * 它会根据给定的错误码生成适当的HTTP响应，包括状态码和响应体。
 * 这个函数在处理诸如404 Not Found、500 Internal Server Error等特殊情况时非常有用。
 */
ngx_int_t ngx_http_special_response_handler(ngx_http_request_t *r,
    ngx_int_t error);
/*
 * 完成HTTP请求的过滤处理
 *
 * @param r HTTP请求结构体指针
 * @param m 模块结构体指针
 * @param error 错误码
 * @return NGX_OK 成功完成过滤处理
 *         NGX_ERROR 处理过程中发生错误
 *
 * 此函数用于在过滤器链中完成HTTP请求的处理。
 * 它通常在过滤器遇到错误或需要提前结束请求处理时被调用。
 * 函数会根据给定的错误码设置适当的响应，并清理相关资源。
 */
ngx_int_t ngx_http_filter_finalize_request(ngx_http_request_t *r,
    ngx_module_t *m, ngx_int_t error);
/*
 * 清理HTTP请求头
 *
 * @param r HTTP请求结构体指针
 * @return 无返回值
 *
 * 此函数用于清理HTTP请求的头部信息。
 * 它会释放与请求头相关的资源，重置相关字段，
 * 并确保请求结构体处于一个干净的状态，以便可能的重用或进一步处理。
 * 通常在请求处理完成或需要重置请求状态时调用此函数。
 */
void ngx_http_clean_header(ngx_http_request_t *r);


/*
 * 丢弃HTTP请求体
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功丢弃请求体
 *         NGX_ERROR 处理过程中发生错误
 *
 * 此函数用于丢弃HTTP请求的请求体。
 * 在某些情况下，服务器可能不需要处理请求体（例如，对于HEAD请求或某些错误情况），
 * 这时可以调用此函数来丢弃请求体数据，释放相关资源。
 * 这有助于提高服务器效率，避免不必要的数据处理。
 */
ngx_int_t ngx_http_discard_request_body(ngx_http_request_t *r);
/*
 * 处理被丢弃的HTTP请求体
 *
 * @param r HTTP请求结构体指针
 * @return 无返回值
 *
 * 此函数用于处理已被丢弃的HTTP请求体。
 * 当请求体被标记为需要丢弃时（例如，在某些错误情况下），
 * 这个函数会被调用来执行实际的丢弃操作。
 * 它可能会读取并丢弃剩余的请求体数据，
 * 或者执行其他必要的清理操作，以确保请求处理的正确性。
 */
void ngx_http_discarded_request_body_handler(ngx_http_request_t *r);
/*
 * 阻塞HTTP请求的读取操作
 *
 * @param r HTTP请求结构体指针
 * @return 无返回值
 *
 * 此函数用于阻止对HTTP请求的进一步读取操作。
 * 在某些情况下，例如当请求处理已经完成或遇到错误时，
 * 可能需要停止读取更多的请求数据。
 * 调用此函数后，Nginx将不会再尝试从客户端读取更多数据，
 * 直到请求处理完全结束或重新启用读取操作。
 */
void ngx_http_block_reading(ngx_http_request_t *r);
/*
 * 测试HTTP请求的读取状态
 *
 * @param r HTTP请求结构体指针
 * @return 无返回值
 *
 * 此函数用于测试HTTP请求的读取状态。
 * 它可能会检查请求的读取缓冲区是否有可用数据，
 * 或者连接是否准备好进行读取操作。
 * 这个函数通常用于决定是否应该继续读取请求数据，
 * 或者是否应该暂停读取以等待更多数据到达。
 * 在处理keep-alive连接或者需要精确控制请求读取过程时特别有用。
 */
void ngx_http_test_reading(ngx_http_request_t *r);


/*
 * 处理HTTP类型配置的槽函数
 *
 * @param cf 配置结构体指针
 * @param cmd 命令结构体指针
 * @param conf 配置指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 *
 * 此函数用于处理Nginx配置文件中与HTTP MIME类型相关的配置指令。
 * 它可能会解析和设置MIME类型映射，更新类型哈希表，
 * 或执行其他与HTTP内容类型相关的配置任务。
 * 这个函数通常在Nginx的HTTP模块初始化过程中被调用，
 * 用于设置服务器如何处理不同类型的内容。
 */
char *ngx_http_types_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/*
 * 合并HTTP MIME类型配置
 *
 * @param cf 配置结构体指针
 * @param keys 指向类型键数组的指针的指针
 * @param types_hash 类型哈希表指针
 * @param prev_keys 指向前一级配置的类型键数组的指针的指针
 * @param prev_types_hash 前一级配置的类型哈希表指针
 * @param default_types 默认类型字符串数组指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 *
 * 此函数用于合并不同配置级别（如http、server、location）的MIME类型设置。
 * 它会处理新旧配置中的类型键和哈希表，并在必要时应用默认类型。
 * 这个过程确保了Nginx在不同配置层级中正确继承和覆盖MIME类型设置。
 */
char *ngx_http_merge_types(ngx_conf_t *cf, ngx_array_t **keys,
    ngx_hash_t *types_hash, ngx_array_t **prev_keys,
    ngx_hash_t *prev_types_hash, ngx_str_t *default_types);
/*
 * 设置默认的HTTP MIME类型
 *
 * @param cf 配置结构体指针
 * @param types 指向类型数组的指针的指针
 * @param default_type 默认类型字符串数组指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 此函数用于设置HTTP模块的默认MIME类型。
 * 它会将默认类型添加到给定的类型数组中。
 * 如果类型数组还未创建，函数会首先创建它。
 * 这个函数通常在初始化HTTP配置时被调用，
 * 以确保即使没有明确配置，也能有基本的MIME类型支持。
 */
ngx_int_t ngx_http_set_default_types(ngx_conf_t *cf, ngx_array_t **types,
    ngx_str_t *default_type);

#if (NGX_HTTP_DEGRADATION)
/*
 * 检查HTTP请求是否处于降级模式
 *
 * @param r HTTP请求结构体指针
 * @return 如果请求处于降级模式返回非零值，否则返回0
 *
 * 此函数用于判断给定的HTTP请求是否处于降级模式。
 * 在系统负载过高或资源不足时，Nginx可能会进入降级模式，
 * 此时某些非关键功能可能会被禁用或简化，以保证核心服务的可用性。
 * 这个函数通常在处理请求的各个阶段被调用，以决定是否应用降级策略。
 */
ngx_uint_t  ngx_http_degraded(ngx_http_request_t *);
#endif


#if (NGX_HTTP_V2 || NGX_HTTP_V3)
/*
 * HTTP Huffman解码函数
 *
 * @param state 解码状态指针
 * @param src 源数据指针
 * @param len 源数据长度
 * @param dst 目标数据指针的指针（输出参数）
 * @param last 标识是否为最后一块数据
 * @param log 日志对象指针
 * @return 成功返回NGX_OK，失败返回错误码
 *
 * 此函数用于解码HTTP/2和HTTP/3协议中使用的Huffman编码数据。
 * Huffman编码是一种用于压缩头部字段的方法，可以显著减少传输的数据量。
 * 该函数接收编码后的数据，并将其解码为原始形式。
 */
ngx_int_t ngx_http_huff_decode(u_char *state, u_char *src, size_t len,
    u_char **dst, ngx_uint_t last, ngx_log_t *log);
/*
 * HTTP Huffman编码函数
 *
 * @param src 源数据指针
 * @param len 源数据长度
 * @param dst 目标数据指针（输出参数）
 * @param lower 是否使用小写字母的标志
 * @return 编码后的数据长度
 *
 * 此函数用于对HTTP/2和HTTP/3协议中的数据进行Huffman编码。
 * Huffman编码是一种用于压缩头部字段的方法，可以显著减少传输的数据量。
 * 该函数接收原始数据，将其编码为Huffman格式，并返回编码后的数据长度。
 */
size_t ngx_http_huff_encode(u_char *src, size_t len, u_char *dst,
    ngx_uint_t lower);
#endif


/*
 * 声明HTTP模块
 *
 * @var ngx_http_module 全局HTTP模块变量
 *
 * 这个外部声明表示HTTP模块在其他源文件中定义。
 * HTTP模块是Nginx中处理HTTP请求的核心模块，
 * 负责初始化HTTP相关的配置和功能。
 */
extern ngx_module_t  ngx_http_module;

/*
 * HTTP默认MIME类型数组
 *
 * @var ngx_http_html_default_types 存储默认MIME类型的数组
 *
 * 这个外部声明定义了一个包含默认MIME类型的数组。
 * 在HTTP响应中，当无法确定特定文件的MIME类型时，
 * Nginx可能会使用这些默认类型。
 * 这有助于确保浏览器正确解释和显示服务器发送的内容。
 */
extern ngx_str_t  ngx_http_html_default_types[];


/*
 * HTTP顶层响应头过滤器
 *
 * @var ngx_http_top_header_filter 指向顶层响应头过滤器函数的指针
 *
 * 这个外部声明定义了HTTP响应头处理的顶层过滤器。
 * 在Nginx处理HTTP响应时，所有的响应头都会通过这个过滤器进行处理。
 * 模块可以通过修改这个指针来插入自己的响应头处理逻辑，
 * 从而实现对响应头的自定义修改或增强。
 */
extern ngx_http_output_header_filter_pt  ngx_http_top_header_filter;
/*
 * HTTP顶层响应体过滤器
 *
 * @var ngx_http_top_body_filter 指向顶层响应体过滤器函数的指针
 *
 * 这个外部声明定义了HTTP响应体处理的顶层过滤器。
 * 在Nginx处理HTTP响应时，所有的响应体内容都会通过这个过滤器进行处理。
 * 模块可以通过修改这个指针来插入自己的响应体处理逻辑，
 * 从而实现对响应内容的自定义修改、压缩或其他处理。
 */
extern ngx_http_output_body_filter_pt    ngx_http_top_body_filter;
/*
 * HTTP顶层请求体过滤器
 *
 * @var ngx_http_top_request_body_filter 指向顶层请求体过滤器函数的指针
 *
 * 这个外部声明定义了HTTP请求体处理的顶层过滤器。
 * 在Nginx处理HTTP请求时，所有的请求体内容都会通过这个过滤器进行处理。
 * 模块可以通过修改这个指针来插入自己的请求体处理逻辑，
 * 从而实现对请求内容的自定义修改、验证或其他处理。
 * 这对于实现请求体的预处理、安全检查或特殊格式解析等功能非常有用。
 */
extern ngx_http_request_body_filter_pt   ngx_http_top_request_body_filter;


#endif /* _NGX_HTTP_H_INCLUDED_ */
