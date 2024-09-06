
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_upstream.c
 *
 * 该文件实现了Nginx的上游（upstream）处理功能。
 *
 * 支持的功能:
 * 1. 负载均衡
 * 2. 健康检查
 * 3. 故障转移
 * 4. 缓存
 * 5. SSL/TLS支持
 * 6. 超时控制
 * 7. 重试机制
 * 8. 动态服务器组
 * 9. HTTP/2上游支持
 * 10. 请求/响应缓冲
 *
 * 支持的指令:
 * - upstream: 定义上游服务器组
 *   语法: upstream name { ... }
 *   上下文: http
 *
 * - server: 在upstream块中定义服务器
 *   语法: server address [parameters];
 *   上下文: upstream
 *
 * - keepalive: 设置到上游服务器的空闲keepalive连接数
 *   语法: keepalive connections;
 *   上下文: upstream
 *
 * - proxy_pass: 设置代理服务器的协议和地址
 *   语法: proxy_pass URL;
 *   上下文: location, if in location, limit_except
 *
 * 支持的变量:
 * - $upstream_addr: 处理请求的上游服务器地址
 * - $upstream_response_time: 从上游服务器接收响应的时间
 * - $upstream_status: 从上游服务器接收的响应状态码
 * - $upstream_http_*: 任意的上游响应头字段
 *
 * 使用注意点:
 * 1. 合理配置负载均衡策略，避免单点故障
 * 2. 正确设置健康检查参数，及时发现故障节点
 * 3. 适当配置超时时间，避免长时间等待
 * 4. 谨慎使用缓存功能，确保缓存策略符合业务需求
 * 5. 在使用SSL/TLS时，注意证书的配置和更新
 * 6. 合理设置重试次数，避免对上游服务器造成过大压力
 * 7. 监控$upstream_*变量，及时发现异常情况
 * 8. 使用keepalive指令时，注意与上游服务器的兼容性
 * 9. 在配置动态服务器组时，确保DNS解析的可靠性
 * 10. 根据实际需求调整缓冲区大小，平衡内存使用和性能
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#if (NGX_HTTP_CACHE)
/*
 * 处理上游缓存的函数
 * 参数:
 *   r: HTTP请求结构体指针
 *   u: 上游结构体指针(在函数定义中未显示,但通常作为第二个参数)
 * 返回值:
 *   ngx_int_t类型,表示处理结果
 */
static ngx_int_t ngx_http_upstream_cache(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/*
 * 获取上游缓存的函数
 * 参数:
 *   r: HTTP请求结构体指针
 *   u: 上游结构体指针(在函数定义中未显示,但通常作为第二个参数)
 *   cache: 指向文件缓存结构体指针的指针
 * 返回值:
 *   ngx_int_t类型,表示获取缓存的结果
 */
static ngx_int_t ngx_http_upstream_cache_get(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_http_file_cache_t **cache);
/*
 * 发送上游缓存的函数
 * 参数:
 *   r: HTTP请求结构体指针
 *   u: 上游结构体指针(在函数定义中未显示,但通常作为第二个参数)
 * 返回值:
 *   ngx_int_t类型,表示发送缓存的结果
 * 功能:
 *   从缓存中读取响应并发送给客户端
 */
static ngx_int_t ngx_http_upstream_cache_send(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/*
 * 执行上游缓存的后台更新
 * 参数:
 *   r: HTTP请求结构体指针
 *   u: 上游结构体指针(在函数定义中未显示,但通常作为第二个参数)
 * 返回值:
 *   ngx_int_t类型,表示后台更新的结果
 * 功能:
 *   在后台异步更新缓存,不影响当前请求的响应
 */
static ngx_int_t ngx_http_upstream_cache_background_update(
    ngx_http_request_t *r, ngx_http_upstream_t *u);
/*
 * 检查上游缓存的范围请求
 * 参数:
 *   r: HTTP请求结构体指针
 *   u: 上游结构体指针(在函数定义中未显示,但通常作为第二个参数)
 * 返回值:
 *   ngx_int_t类型,表示检查结果
 * 功能:
 *   验证客户端的Range请求是否可以从缓存中满足
 */
static ngx_int_t ngx_http_upstream_cache_check_range(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/*
 * 获取上游缓存状态的函数
 * 参数:
 *   r: HTTP请求结构体指针
 *   v: 变量值结构体指针（在函数定义中未显示，但通常作为第二个参数）
 *   data: 用户定义的数据（在函数定义中未显示，但通常作为第三个参数）
 * 返回值:
 *   ngx_int_t类型，表示获取缓存状态的结果
 * 功能:
 *   获取并返回当前请求的上游缓存状态
 */
static ngx_int_t ngx_http_upstream_cache_status(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
/*
 * 获取上游缓存的Last-Modified头部值
 * 参数:
 *   r: HTTP请求结构体指针
 *   v: 变量值结构体指针（在函数定义中未显示，但通常作为第二个参数）
 *   data: 用户定义的数据（在函数定义中未显示，但通常作为第三个参数）
 * 返回值:
 *   ngx_int_t类型，表示获取Last-Modified值的结果
 * 功能:
 *   从上游缓存中获取并返回Last-Modified头部的值
 */
static ngx_int_t ngx_http_upstream_cache_last_modified(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
/*
 * 获取上游缓存的ETag头部值
 * 参数:
 *   r: HTTP请求结构体指针
 *   v: 变量值结构体指针（在函数定义中未显示，但通常作为第二个参数）
 *   data: 用户定义的数据（在函数定义中未显示，但通常作为第三个参数）
 * 返回值:
 *   ngx_int_t类型，表示获取ETag值的结果
 * 功能:
 *   从上游缓存中获取并返回ETag头部的值
 */
static ngx_int_t ngx_http_upstream_cache_etag(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
#endif

/**
 * @brief 初始化上游请求
 *
 * 该函数用于初始化与上游服务器的请求。它设置必要的参数和回调函数，
 * 为向上游服务器发送请求做准备。
 *
 * @param r 指向当前HTTP请求结构体的指针
 *
 * 主要功能包括:
 * 1. 设置上游请求的各种参数
 * 2. 初始化连接池
 * 3. 设置各种回调函数
 * 4. 处理DNS解析（如果需要）
 * 5. 开始与上游服务器的连接过程
 */
static void ngx_http_upstream_init_request(ngx_http_request_t *r);
/**
 * @brief 处理上游服务器域名解析的回调函数
 *
 * 该函数在域名解析完成后被调用，用于处理解析结果并继续上游请求的处理流程。
 *
 * @param ctx 指向域名解析上下文的指针，包含解析结果和相关信息
 *
 * 主要功能:
 * 1. 检查域名解析结果
 * 2. 处理解析失败的情况
 * 3. 更新上游服务器的地址信息
 * 4. 继续进行上游连接的建立
 */
static void ngx_http_upstream_resolve_handler(ngx_resolver_ctx_t *ctx);
/**
 * @brief 检查上游读取连接是否断开
 *
 * 该函数用于检查与上游服务器的读取连接是否已经断开。
 * 它通常在读取上游响应数据时被调用，以确保连接的完整性。
 *
 * @param r 指向当前HTTP请求结构体的指针
 *
 * 主要功能:
 * 1. 检测读取连接的状态
 * 2. 如果连接断开，进行相应的错误处理
 * 3. 可能会触发重试机制或错误响应
 */
static void ngx_http_upstream_rd_check_broken_connection(ngx_http_request_t *r);
/**
 * @brief 检查上游写入连接是否断开
 *
 * 该函数用于检查与上游服务器的写入连接是否已经断开。
 * 它通常在向上游服务器发送请求数据时被调用，以确保连接的完整性。
 *
 * @param r 指向当前HTTP请求结构体的指针
 *
 * 主要功能:
 * 1. 检测写入连接的状态
 * 2. 如果连接断开，进行相应的错误处理
 * 3. 可能会触发重试机制或错误响应
 */
static void ngx_http_upstream_wr_check_broken_connection(ngx_http_request_t *r);
/**
 * @brief 检查上游连接是否断开
 *
 * 该函数用于检查与上游服务器的连接是否已经断开。
 * 它可以用于检查读取和写入连接的状态。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param ev 指向触发此检查的事件结构体的指针
 *
 * 主要功能:
 * 1. 根据传入的事件类型判断是读取还是写入连接
 * 2. 检测连接的状态
 * 3. 如果连接断开，进行相应的错误处理
 * 4. 可能会触发重试机制或错误响应
 */
static void ngx_http_upstream_check_broken_connection(ngx_http_request_t *r,
    ngx_event_t *ev);
/**
 * @brief 建立与上游服务器的连接
 *
 * 该函数负责建立与上游服务器的连接。它是上游处理的关键步骤之一，
 * 在解析完上游服务器地址后被调用。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 *
 * 主要功能:
 * 1. 初始化连接参数
 * 2. 尝试建立TCP连接
 * 3. 处理连接成功或失败的情况
 * 4. 可能触发SSL握手（如果配置了SSL）
 * 5. 设置连接的读写处理函数
 */
static void ngx_http_upstream_connect(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 重新初始化上游连接
 *
 * 该函数用于在上游连接失败或需要重试时重新初始化上游连接。
 * 它会重置一些状态并准备重新建立连接。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 * @return NGX_OK 如果重新初始化成功，否则返回错误码
 *
 * 主要功能:
 * 1. 重置上游连接的各种状态
 * 2. 清理之前连接的残留数据
 * 3. 准备重新选择上游服务器
 * 4. 可能会触发新的连接尝试
 */
static ngx_int_t ngx_http_upstream_reinit(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 向上游服务器发送请求
 *
 * 该函数负责将HTTP请求发送到上游服务器。它是上游处理流程中的关键步骤，
 * 在与上游服务器建立连接后被调用。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 * @param do_write 指示是否需要执行写操作的标志
 *
 * 主要功能:
 * 1. 准备要发送到上游服务器的请求头和正文
 * 2. 处理请求发送的各个阶段（如头部发送、正文发送等）
 * 3. 根据do_write参数决定是否立即执行写操作
 * 4. 设置相应的回调函数以处理发送后的操作
 * 5. 处理可能出现的错误情况
 */
static void ngx_http_upstream_send_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_uint_t do_write);
/**
 * @brief 向上游服务器发送请求体
 *
 * 该函数负责将HTTP请求的请求体发送到上游服务器。它是上游处理流程中的一个重要步骤，
 * 通常在发送请求头之后被调用。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 * @param do_write 指示是否需要执行写操作的标志
 * @return NGX_OK 如果发送成功，否则返回错误码
 *
 * 主要功能:
 * 1. 准备要发送到上游服务器的请求体数据
 * 2. 处理请求体发送的各个阶段
 * 3. 根据do_write参数决定是否立即执行写操作
 * 4. 设置相应的回调函数以处理发送后的操作
 * 5. 处理可能出现的错误情况
 */
static ngx_int_t ngx_http_upstream_send_request_body(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_uint_t do_write);
/**
 * @brief 处理向上游服务器发送请求的回调函数
 *
 * 该函数是一个回调处理器，用于管理向上游服务器发送请求的过程。
 * 它在发送请求的不同阶段被调用，负责协调请求的发送和后续处理。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 *
 * 主要功能:
 * 1. 处理请求发送过程中的各种状态
 * 2. 协调请求头和请求体的发送
 * 3. 处理发送过程中可能出现的错误
 * 4. 在请求完全发送后，设置接收响应的处理器
 */
static void ngx_http_upstream_send_request_handler(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理从上游服务器读取响应的回调函数
 *
 * 该函数是一个回调处理器，用于管理从上游服务器读取响应的过程。
 * 它在接收响应的不同阶段被调用，负责协调响应的接收和后续处理。
 *
 * @param r 指向当前HTTP请求结构体的指针
 *
 * 主要功能:
 * 1. 处理响应接收过程中的各种状态
 * 2. 协调响应头和响应体的接收
 * 3. 处理接收过程中可能出现的错误
 * 4. 在响应完全接收后，触发相应的处理逻辑
 */
static void ngx_http_upstream_read_request_handler(ngx_http_request_t *r);
/**
 * @brief 处理上游服务器返回的响应头
 *
 * 该函数负责处理从上游服务器接收到的响应头。它是上游处理流程中的一个关键步骤，
 * 通常在接收到响应头后被调用。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 *
 * 主要功能:
 * 1. 解析上游服务器返回的响应头
 * 2. 处理特殊的响应状态码（如重定向、错误等）
 * 3. 设置相应的回调函数以处理后续的响应体
 * 4. 处理可能出现的错误情况
 * 5. 准备将处理后的响应头发送给客户端
 */
static void ngx_http_upstream_process_header(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 测试下一个上游服务器
 *
 * 该函数用于测试下一个可用的上游服务器。在当前上游服务器失败或不可用时被调用。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 *
 * @return ngx_int_t 返回测试结果，通常是NGX_OK表示成功找到下一个服务器，或者错误码
 */
static ngx_int_t ngx_http_upstream_test_next(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 拦截并处理上游服务器返回的错误
 *
 * 该函数用于拦截和处理从上游服务器返回的错误响应。它在上游处理过程中检测到错误时被调用。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 *
 * @return ngx_int_t 返回处理结果，可能是NGX_OK表示成功处理，或者其他错误码
 *
 * 主要功能:
 * 1. 检查上游服务器返回的错误状态码
 * 2. 根据配置决定是否拦截特定的错误
 * 3. 如果需要拦截，可能会触发错误页面的生成或其他错误处理逻辑
 * 4. 决定是否继续处理请求或终止请求
 */
static ngx_int_t ngx_http_upstream_intercept_errors(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 测试上游连接是否成功建立
 *
 * 该函数用于测试与上游服务器的连接是否已成功建立。
 * 通常在尝试建立连接后调用，以确认连接状态。
 *
 * @param c 指向当前连接结构体的指针
 *
 * @return ngx_int_t 返回连接测试结果
 *                   通常返回NGX_OK表示连接成功，
 *                   或者返回错误码表示连接失败
 */
static ngx_int_t ngx_http_upstream_test_connect(ngx_connection_t *c);
/**
 * @brief 处理上游服务器返回的响应头
 *
 * 该函数用于处理从上游服务器接收到的HTTP响应头。
 * 它负责解析、验证和处理这些头部信息，为后续的响应处理做准备。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 *
 * @return ngx_int_t 返回处理结果
 *                   通常返回NGX_OK表示成功处理，
 *                   或者返回错误码表示处理过程中遇到问题
 */
static ngx_int_t ngx_http_upstream_process_headers(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理上游服务器返回的尾部（trailers）
 *
 * 该函数用于处理从上游服务器接收到的HTTP响应尾部。
 * 尾部通常包含在分块传输编码（chunked transfer encoding）的最后一个块之后。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 *
 * @return ngx_int_t 返回处理结果
 *                   通常返回NGX_OK表示成功处理，
 *                   或者返回错误码表示处理过程中遇到问题
 */
static ngx_int_t ngx_http_upstream_process_trailers(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 发送上游服务器的响应给客户端
 *
 * 该函数负责将从上游服务器接收到的响应发送给客户端。
 * 它处理响应的头部和主体，并根据需要进行适当的处理和转发。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 */
static void ngx_http_upstream_send_response(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理上游连接升级
 *
 * 该函数用于处理HTTP连接升级的情况，例如从HTTP升级到WebSocket。
 * 它负责设置必要的回调函数和状态，以便在连接升级后正确处理通信。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 */
static void ngx_http_upstream_upgrade(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理已升级连接的下游读取操作
 *
 * 该函数用于处理已经升级的连接(如WebSocket)中从客户端(下游)读取数据的操作。
 * 它在连接升级后被调用，负责从客户端读取数据并可能将其转发到上游服务器。
 *
 * @param r 指向当前HTTP请求结构体的指针
 */
static void ngx_http_upstream_upgraded_read_downstream(ngx_http_request_t *r);
/**
 * @brief 处理已升级连接的下游写入操作
 *
 * 该函数用于处理已经升级的连接(如WebSocket)中向客户端(下游)写入数据的操作。
 * 它在连接升级后被调用，负责将数据写入到客户端，通常是将从上游服务器接收到的数据转发给客户端。
 *
 * @param r 指向当前HTTP请求结构体的指针
 */
static void ngx_http_upstream_upgraded_write_downstream(ngx_http_request_t *r);
/**
 * @brief 处理已升级连接的上游读取操作
 *
 * 该函数用于处理已经升级的连接(如WebSocket)中从上游服务器读取数据的操作。
 * 它在连接升级后被调用，负责从上游服务器读取数据并可能将其转发到客户端。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 */
static void ngx_http_upstream_upgraded_read_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理已升级连接的上游写入操作
 *
 * 该函数用于处理已经升级的连接(如WebSocket)中向上游服务器写入数据的操作。
 * 它在连接升级后被调用，负责将数据写入到上游服务器，通常是将从客户端接收到的数据转发给上游服务器。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 */
static void ngx_http_upstream_upgraded_write_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理已升级的上游连接
 *
 * 该函数用于处理已经升级的连接(如WebSocket)的数据传输。
 * 它负责协调上游和下游之间的数据流动，处理读写操作。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param from_upstream 指示数据是否来自上游服务器的标志
 * @param do_write 指示是否需要执行写操作的标志
 */
static void ngx_http_upstream_process_upgraded(ngx_http_request_t *r,
    ngx_uint_t from_upstream, ngx_uint_t do_write);
/**
 * @brief 处理非缓冲的下游数据
 *
 * 该函数用于处理非缓冲模式下从上游服务器接收到的数据，并将其直接发送给下游客户端。
 * 在非缓冲模式下，数据不会被完全存储在内存中，而是尽快地转发给客户端，
 * 这有助于减少内存使用并提高响应速度。
 *
 * @param r 指向当前HTTP请求结构体的指针
 */
static void
    ngx_http_upstream_process_non_buffered_downstream(ngx_http_request_t *r);
/**
 * @brief 处理非缓冲的上游数据
 *
 * 该函数用于处理非缓冲模式下从上游服务器接收到的数据。
 * 在非缓冲模式下，数据不会被完全存储在内存中，而是尽快地处理和转发。
 * 这有助于减少内存使用并提高处理大量数据时的效率。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游处理结构体的指针
 */
static void
    ngx_http_upstream_process_non_buffered_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理非缓冲请求
 *
 * 该函数用于处理非缓冲模式下的HTTP请求。
 * 在非缓冲模式下，数据不会被完全存储在内存中，而是尽快地处理和转发。
 * 这种模式有助于减少内存使用，并提高处理大量数据时的效率。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param do_write 指示是否需要执行写操作的标志
 */
static void
    ngx_http_upstream_process_non_buffered_request(ngx_http_request_t *r,
    ngx_uint_t do_write);
#if (NGX_THREADS)
/**
 * @brief 处理上游线程任务
 *
 * 该函数用于在线程池中处理上游相关的任务。
 * 它可能涉及文件操作或其他可能阻塞的I/O操作。
 *
 * @param task 指向线程任务结构的指针
 * @param file 指向文件结构的指针，用于文件操作
 * @return 返回处理结果的状态码
 */
static ngx_int_t ngx_http_upstream_thread_handler(ngx_thread_task_t *task,
    ngx_file_t *file);

/**
 * @brief 处理上游线程事件
 *
 * 该函数用于处理与上游线程相关的事件。
 * 它可能在线程任务完成后被调用，用于处理任务结果或触发后续操作。
 *
 * @param ev 指向事件结构的指针
 */
static void ngx_http_upstream_thread_event_handler(ngx_event_t *ev);
#endif
/**
 * @brief 上游输出过滤器
 *
 * 该函数用于处理从上游服务器接收到的响应数据。
 * 它可能会对数据进行转换、压缩或其他处理，然后将处理后的数据传递给下一个过滤器。
 *
 * @param data 指向用户定义数据的指针，通常包含请求或上游相关的上下文信息
 * @param chain 指向包含响应数据的链表的指针
 * @return 返回处理结果的状态码
 */
static ngx_int_t ngx_http_upstream_output_filter(void *data,
    ngx_chain_t *chain);
/**
 * @brief 处理下游请求
 *
 * 该函数用于处理来自客户端的下游请求。
 * 它可能涉及读取请求数据、解析请求头、处理请求体等操作。
 * 这是处理HTTP请求生命周期中的一个重要步骤。
 *
 * @param r 指向当前HTTP请求结构体的指针
 */
static void ngx_http_upstream_process_downstream(ngx_http_request_t *r);
/**
 * @brief 处理上游请求
 *
 * 该函数用于处理发送到上游服务器的请求。
 * 它可能涉及发送请求数据、处理上游服务器的响应、管理连接等操作。
 * 这是处理HTTP请求生命周期中与上游服务器交互的关键步骤。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游配置结构体的指针
 */
static void ngx_http_upstream_process_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理上游请求
 *
 * 该函数用于处理发送到上游服务器的请求。
 * 它负责管理整个上游请求的生命周期，包括发送请求、接收响应、处理响应数据等。
 * 这是上游模块中的核心函数之一，协调了与上游服务器的所有交互。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含了请求的所有相关信息
 * @param u 指向上游结构体的指针，包含了上游连接和处理的相关信息
 */
static void ngx_http_upstream_process_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理上游响应的存储
 *
 * 该函数用于将从上游服务器接收到的响应内容存储到本地。
 * 这通常用于实现缓存或其他需要持久化上游响应的场景。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含了请求的所有相关信息
 * @param u 指向上游结构体的指针，包含了上游连接和处理的相关信息
 */
static void ngx_http_upstream_store(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 上游请求的虚拟处理函数
 *
 * 这是一个虚拟的处理函数，通常用作占位符或默认处理程序。
 * 在某些情况下，当不需要实际的处理逻辑时，可能会使用此函数。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游结构体的指针
 */
static void ngx_http_upstream_dummy_handler(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
/**
 * @brief 处理上游请求的下一步操作
 *
 * 该函数用于处理上游请求的下一个步骤。
 * 它可能在当前上游服务器失败或需要尝试下一个上游服务器时被调用。
 * 函数可能涉及选择新的上游服务器、重试请求或处理错误情况。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向上游结构体的指针
 * @param ft_type 失败类型，指示了为什么需要进行下一步操作
 */
static void ngx_http_upstream_next(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_uint_t ft_type);
/**
 * @brief 清理上游连接资源
 *
 * 该函数用于清理与上游连接相关的资源。
 * 当上游请求完成或因错误终止时，此函数会被调用以确保所有相关资源被正确释放。
 * 清理操作可能包括关闭连接、释放内存、重置状态等。
 *
 * @param data 指向需要清理的数据的指针，通常是指向ngx_http_request_t结构的指针
 */
static void ngx_http_upstream_cleanup(void *data);
/**
 * @brief 完成上游请求的处理
 *
 * 该函数用于完成上游请求的处理过程。它通常在上游请求完成、出错或需要终止时被调用。
 * 函数负责清理资源、记录日志、发送响应给客户端等最终处理步骤。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含了请求的所有相关信息
 * @param u 指向上游结构体的指针，包含了上游连接和处理的相关信息
 * @param rc 请求的结果代码，用于指示请求的最终状态
 */
static void ngx_http_upstream_finalize_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_int_t rc);

/**
 * @brief 处理上游响应头的单个头部行
 *
 * 该函数用于处理从上游服务器接收到的单个HTTP头部行。
 * 它可能会解析头部内容、验证其有效性，并根据需要更新请求或上游结构中的相关信息。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应头的多个头部行
 *
 * 该函数用于处理从上游服务器接收到的多个相关的HTTP头部行。
 * 它可能会解析多个头部内容、验证其有效性，并根据需要更新请求或上游结构中的相关信息。
 * 这通常用于处理可能出现多次的头部，如Set-Cookie或Vary。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t
    ngx_http_upstream_process_multi_header_lines(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的Content-Length头部
 *
 * 该函数用于处理从上游服务器接收到的Content-Length头部。
 * 它可能会解析头部值、验证其有效性，并更新请求或上游结构中的相关信息。
 * 这对于正确处理响应体长度和后续的数据传输非常重要。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的Content-Length头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_content_length(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的Last-Modified头部
 *
 * 该函数用于处理从上游服务器接收到的Last-Modified头部。
 * 它可能会解析头部值、验证其有效性，并更新请求或上游结构中的相关信息。
 * 这对于实现缓存控制和条件请求（如If-Modified-Since）非常重要。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的Last-Modified头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_last_modified(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的Set-Cookie头部
 *
 * 该函数用于处理从上游服务器接收到的Set-Cookie头部。
 * 它可能会解析头部值、验证其有效性，并更新请求或上游结构中的相关信息。
 * 这对于正确处理和传递cookie信息非常重要，特别是在代理场景下。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的Set-Cookie头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_set_cookie(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的Cache-Control头部
 *
 * 该函数用于处理从上游服务器接收到的Cache-Control头部。
 * 它可能会解析头部值、验证其有效性，并更新请求或上游结构中的缓存控制相关信息。
 * 这对于实现正确的缓存行为和遵守上游服务器的缓存指令非常重要。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的Cache-Control头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t
    ngx_http_upstream_process_cache_control(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 忽略上游响应中的特定头部行
 *
 * 该函数用于处理需要被忽略的上游响应头部。
 * 在某些情况下，我们可能不希望将某些上游头部传递给客户端，
 * 或者我们可能需要特殊处理这些头部而不是简单地复制它们。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 通常返回NGX_OK，表示该头部已被成功忽略
 */
static ngx_int_t ngx_http_upstream_ignore_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的Expires头部
 *
 * 该函数用于处理从上游服务器接收到的Expires头部。
 * 它可能会解析头部值、验证其有效性，并更新请求或上游结构中的过期时间相关信息。
 * 这对于实现正确的缓存过期行为非常重要。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的Expires头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_expires(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的X-Accel-Expires头部
 *
 * 该函数用于处理从上游服务器接收到的X-Accel-Expires头部。
 * X-Accel-Expires是Nginx特有的头部，用于控制缓存的过期时间。
 * 函数可能会解析头部值、验证其有效性，并更新请求或上游结构中的加速缓存过期时间相关信息。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的X-Accel-Expires头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_accel_expires(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的X-Accel-Limit-Rate头部
 *
 * 该函数用于处理从上游服务器接收到的X-Accel-Limit-Rate头部。
 * X-Accel-Limit-Rate是Nginx特有的头部，用于动态设置响应的传输速率限制。
 * 函数可能会解析头部值、验证其有效性，并更新请求或上游结构中的速率限制相关信息。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的X-Accel-Limit-Rate头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_limit_rate(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的X-Accel-Buffering头部
 *
 * 该函数用于处理从上游服务器接收到的X-Accel-Buffering头部。
 * X-Accel-Buffering是Nginx特有的头部，用于控制是否对响应进行缓冲。
 * 函数可能会解析头部值、验证其有效性，并更新请求或上游结构中的缓冲设置。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的X-Accel-Buffering头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_buffering(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的字符集（Charset）头部
 *
 * 该函数用于处理从上游服务器接收到的字符集相关头部。
 * 它可能会解析Content-Type头部中的charset参数，或处理单独的Charset头部。
 * 函数的目的是确定响应内容的字符编码，并可能更新请求或上游结构中的相关信息。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的字符集相关头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_charset(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的Connection头部
 *
 * 该函数用于处理从上游服务器接收到的Connection头部。
 * Connection头部用于指定当前事务完成后是否关闭网络连接。
 * 函数可能会解析头部值，并相应地更新请求或上游结构中的连接状态信息。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的Connection头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_connection(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的Transfer-Encoding头部
 *
 * 该函数用于处理从上游服务器接收到的Transfer-Encoding头部。
 * Transfer-Encoding头部指定了消息体的编码方式，如chunked（分块传输编码）。
 * 函数可能会解析头部值，并相应地更新请求或上游结构中的传输编码信息。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的Transfer-Encoding头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t
    ngx_http_upstream_process_transfer_encoding(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 处理上游响应中的Vary头部
 *
 * 该函数用于处理从上游服务器接收到的Vary头部。
 * Vary头部用于指示哪些请求头字段会影响服务器的响应内容。
 * 这对于缓存和内容协商非常重要。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要处理的Vary头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_process_vary(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 复制上游响应中的单个头部行到下游请求
 *
 * 该函数用于将从上游服务器接收到的单个头部行复制到发送给客户端的下游响应中。
 * 它处理常见的HTTP头部，确保正确地传递信息。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要复制的头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_copy_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 复制上游响应中的多个头部行到下游请求
 *
 * 该函数用于将从上游服务器接收到的多个相同类型的头部行复制到发送给客户端的下游响应中。
 * 这通常用于处理可能出现多次的HTTP头部，如Set-Cookie。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要复制的头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t
    ngx_http_upstream_copy_multi_header_lines(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 复制上游响应中的Content-Type头部到下游请求
 *
 * 该函数用于将从上游服务器接收到的Content-Type头部复制到发送给客户端的下游响应中。
 * Content-Type头部指定了响应体的媒体类型，对客户端正确解析响应内容至关重要。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要复制的Content-Type头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_copy_content_type(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 复制上游响应中的Last-Modified头部到下游请求
 *
 * 该函数用于将从上游服务器接收到的Last-Modified头部复制到发送给客户端的下游响应中。
 * Last-Modified头部指示资源的最后修改时间，对于缓存验证和条件请求非常重要。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要复制的Last-Modified头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_copy_last_modified(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 重写上游响应中的Location头部
 *
 * 该函数用于处理和重写从上游服务器接收到的Location头部。
 * 在反向代理场景中，可能需要修改Location头部以确保客户端能够正确访问资源。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param h 指向要重写的Location头部行结构体的指针
 * @param offset 头部在结构中的偏移量
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
/**
 * @brief 重写上游响应中的Location头部
 *
 * 该函数用于处理和重写从上游服务器接收到的Location头部。
 * 在反向代理场景中，可能需要修改Location头部以确保客户端能够正确访问资源。
 * 重写可能涉及更改主机名、端口或路径等部分。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param h 指向要重写的Location头部行结构体的指针
 * @param offset 头部在结构中的偏移量，用于定位头部在内存中的位置
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_rewrite_location(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 重写上游响应中的Refresh头部
 *
 * 该函数用于处理和重写从上游服务器接收到的Refresh头部。
 * Refresh头部通常用于指示浏览器在一定时间后自动刷新页面或重定向到新的URL。
 * 在反向代理场景中，可能需要修改Refresh头部以确保客户端能够正确执行刷新或重定向操作。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 */
static ngx_int_t ngx_http_upstream_rewrite_refresh(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 重写上游响应中的Set-Cookie头部
 *
 * 该函数用于处理和重写从上游服务器接收到的Set-Cookie头部。
 * 在反向代理场景中，可能需要修改Set-Cookie头部中的域名或路径，
 * 以确保cookie能够正确地设置到客户端，并在后续请求中正确发送。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param h 指向要重写的Set-Cookie头部行结构体的指针
 * @param offset 头部在结构中的偏移量，用于定位头部在内存中的位置
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_rewrite_set_cookie(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
/**
 * @brief 复制上游响应中的Accept-Ranges头部
 *
 * 该函数用于处理从上游服务器接收到的Accept-Ranges头部。
 * Accept-Ranges头部指示服务器是否接受范围请求，这对于支持断点续传等功能很重要。
 * 在反向代理场景中，通常需要将此头部从上游服务器原样传递给客户端。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param h 指向要复制的Accept-Ranges头部行结构体的指针
 * @param offset 头部在结构中的偏移量，用于定位头部在内存中的位置
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_copy_allow_ranges(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);

/**
 * @brief 添加上游相关的变量
 *
 * 该函数用于向Nginx配置中添加与上游处理相关的变量。
 * 这些变量可能包括上游服务器地址、响应状态码、响应时间等信息，
 * 可以在配置文件中使用这些变量来获取上游处理的相关信息。
 *
 * @param cf 指向Nginx配置结构的指针，包含当前配置上下文
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
static ngx_int_t ngx_http_upstream_add_variables(ngx_conf_t *cf);
/**
 * @brief 处理上游地址变量
 *
 * 该函数用于处理与上游服务器地址相关的变量。
 * 它可能会返回当前请求所使用的上游服务器的IP地址或主机名。
 * 这个变量通常用于日志记录或调试目的，以了解请求被发送到了哪个上游服务器。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param v 指向变量值结构的指针，用于存储处理结果
 * @param data 附加数据，在此函数中可能未使用
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_addr_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
/**
 * @brief 处理上游状态变量
 *
 * 该函数用于处理与上游服务器响应状态相关的变量。
 * 它可能会返回上游服务器的HTTP响应状态码。
 * 这个变量通常用于日志记录、条件判断或调试目的，以了解上游服务器的响应情况。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param v 指向变量值结构的指针，用于存储处理结果
 * @param data 附加数据，在此函数中可能未使用
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_status_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
/**
 * @brief 处理上游响应时间变量
 *
 * 该函数用于处理与上游服务器响应时间相关的变量。
 * 它可能会返回从发送请求到接收到上游服务器响应所花费的时间。
 * 这个变量通常用于性能监控、日志记录或调试目的，以了解上游服务器的响应速度。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param v 指向变量值结构的指针，用于存储处理结果
 * @param data 附加数据，在此函数中可能未使用
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_response_time_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
/**
 * @brief 处理上游响应长度变量
 *
 * 该函数用于处理与上游服务器响应长度相关的变量。
 * 它可能会返回从上游服务器接收到的响应体的字节长度。
 * 这个变量通常用于日志记录、统计分析或调试目的，以了解上游服务器响应的大小。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param v 指向变量值结构的指针，用于存储处理结果
 * @param data 附加数据，在此函数中可能未使用
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_response_length_variable(
    ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
/**
 * @brief 处理上游响应头变量
 *
 * 该函数用于处理与上游服务器响应头相关的变量。
 * 它可能会返回指定的上游响应头的值。
 * 这个变量通常用于获取和处理上游服务器返回的特定响应头信息。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param v 指向变量值结构的指针，用于存储处理结果
 * @param data 附加数据，可能包含要处理的特定响应头的信息
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_header_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
/**
 * @brief 处理上游响应尾部变量
 *
 * 该函数用于处理与上游服务器响应尾部（trailer）相关的变量。
 * 它可能会返回指定的上游响应尾部字段的值。
 * 这个变量通常用于获取和处理上游服务器在响应主体之后发送的额外元数据。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param v 指向变量值结构的指针，用于存储处理结果
 * @param data 附加数据，可能包含要处理的特定响应尾部字段的信息
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_trailer_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
/**
 * @brief 处理上游Cookie变量
 *
 * 该函数用于处理与上游服务器返回的Cookie相关的变量。
 * 它可能会解析和提取上游响应中的Set-Cookie头，并将其值设置为变量。
 * 这个变量通常用于在反向代理场景中处理和传递Cookie信息。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param v 指向变量值结构的指针，用于存储处理结果
 * @param data 附加数据，可能包含要处理的特定Cookie的信息
 * @return 返回处理结果的状态码，通常是NGX_OK表示成功，或错误码表示失败
 */
static ngx_int_t ngx_http_upstream_cookie_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

/**
 * @brief 处理upstream配置指令
 *
 * 该函数用于解析和处理Nginx配置文件中的upstream指令。
 * 它负责创建和初始化upstream服务器组的配置结构。
 *
 * @param cf 指向nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param dummy 未使用的参数，通常为NULL
 * @return 成功时返回NGX_CONF_OK，失败时返回NGX_CONF_ERROR
 */
static char *ngx_http_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy);
/**
 * @brief 处理upstream server配置指令
 *
 * 该函数用于解析和处理Nginx配置文件中的upstream server指令。
 * 它负责配置单个上游服务器的参数，如权重、最大失败次数等。
 *
 * @param cf 指向nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向upstream配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
static char *ngx_http_upstream_server(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/**
 * @brief 设置上游连接的本地地址
 *
 * 该函数用于为上游连接设置本地（源）地址。这在需要指定特定的本地IP地址或端口
 * 来连接上游服务器时非常有用，例如在多宿主主机环境中或需要使用特定网络接口时。
 *
 * @param r 指向当前HTTP请求结构体的指针，包含请求的上下文信息
 * @param u 指向ngx_http_upstream_t结构的指针，包含上游连接的相关信息
 * @param local 指向ngx_http_upstream_local_t结构的指针，包含本地地址配置
 * @return 成功时返回NGX_OK，失败时返回错误码
 */
static ngx_int_t ngx_http_upstream_set_local(ngx_http_request_t *r,
  ngx_http_upstream_t *u, ngx_http_upstream_local_t *local);

/**
 * @brief 创建上游模块的主配置结构
 *
 * 该函数用于创建和初始化上游模块的主配置结构。
 * 在Nginx配置解析阶段被调用，为上游模块分配内存并设置初始值。
 *
 * @param cf 指向nginx配置结构的指针
 * @return 返回创建的配置结构指针，如果分配内存失败则返回NULL
 */
static void *ngx_http_upstream_create_main_conf(ngx_conf_t *cf);
/**
 * @brief 初始化上游模块的主配置
 *
 * 该函数用于初始化上游模块的主配置结构。
 * 在Nginx配置解析完成后被调用，用于对主配置进行最后的设置和检查。
 *
 * @param cf 指向nginx配置结构的指针
 * @param conf 指向上游模块主配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
static char *ngx_http_upstream_init_main_conf(ngx_conf_t *cf, void *conf);

#if (NGX_HTTP_SSL)
/**
 * @brief 初始化上游SSL连接
 *
 * 该函数用于初始化与上游服务器的SSL连接。
 * 它设置必要的SSL参数并启动SSL握手过程。
 *
 * @param r 指向当前HTTP请求结构体的指针
 */
static void ngx_http_upstream_ssl_init_connection(ngx_http_request_t *,
    ngx_http_upstream_t *u, ngx_connection_t *c);
/**
 * @brief SSL握手处理函数
 *
 * 该函数用于处理与上游服务器的SSL握手过程。
 * 在SSL握手完成或出现错误时被调用，用于处理握手结果并执行相应的操作。
 *
 * @param c 指向与上游服务器建立的连接结构体的指针
 */
static void ngx_http_upstream_ssl_handshake_handler(ngx_connection_t *c);
/**
 * @brief 处理上游SSL握手
 *
 * 该函数用于处理与上游服务器的SSL握手过程。
 * 它负责初始化SSL连接、执行握手操作，并在握手完成后处理结果。
 *
 * @param r 指向当前HTTP请求结构体的指针（未使用，可能用于后续扩展）
 * @param u 指向ngx_http_upstream_t结构的指针，包含上游连接的相关信息
 * @param c 指向与上游服务器建立的连接结构体的指针
 */
static void ngx_http_upstream_ssl_handshake(ngx_http_request_t *,
    ngx_http_upstream_t *u, ngx_connection_t *c);
/**
 * @brief 保存上游SSL会话
 *
 * 该函数用于保存与上游服务器建立的SSL会话信息。
 * 保存SSL会话可以在后续连接中重用，从而减少SSL握手的开销，提高性能。
 *
 * @param c 指向与上游服务器建立的连接结构体的指针
 */
static void ngx_http_upstream_ssl_save_session(ngx_connection_t *c);
/**
 * @brief 设置上游SSL连接的服务器名称
 *
 * 该函数用于设置与上游服务器建立SSL连接时使用的服务器名称（SNI）。
 * 这对于支持基于名称的虚拟主机的HTTPS连接非常重要。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向ngx_http_upstream_t结构的指针，包含上游连接的相关信息
 * @param c 指向与上游服务器建立的连接结构体的指针
 * @return 成功时返回NGX_OK，失败时返回错误代码
 */
static ngx_int_t ngx_http_upstream_ssl_name(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_connection_t *c);
/**
 * @brief 设置上游SSL连接的客户端证书
 *
 * 该函数用于为与上游服务器建立的SSL连接设置客户端证书。
 * 在需要双向SSL认证的场景中，这个函数很重要，它确保了客户端（即Nginx）
 * 能够向上游服务器提供必要的身份验证。
 *
 * @param r 指向当前HTTP请求结构体的指针
 * @param u 指向ngx_http_upstream_t结构的指针，包含上游连接的相关信息
 * @param c 指向与上游服务器建立的连接结构体的指针
 * @return 成功时返回NGX_OK，失败时返回错误代码
 */
static ngx_int_t ngx_http_upstream_ssl_certificate(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_connection_t *c);
#endif


/**
 * @brief 定义上游响应头处理数组
 *
 * 这个数组包含了一系列的结构体，每个结构体定义了如何处理特定的HTTP响应头。
 * 数组中的每个元素对应一个特定的HTTP头，并指定了处理该头的函数和相关参数。
 * 这个数组用于在Nginx处理从上游服务器接收到的响应时，正确地解析和处理各种HTTP头。
 */
static ngx_http_upstream_header_t  ngx_http_upstream_headers_in[] = {

    { ngx_string("Status"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, status),
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("Content-Type"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, content_type),
                 ngx_http_upstream_copy_content_type, 0, 1 },

    { ngx_string("Content-Length"),
                 ngx_http_upstream_process_content_length, 0,
                 ngx_http_upstream_ignore_header_line, 0, 0 },

    { ngx_string("Date"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, date),
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, date), 0 },

    { ngx_string("Last-Modified"),
                 ngx_http_upstream_process_last_modified, 0,
                 ngx_http_upstream_copy_last_modified, 0, 0 },

    { ngx_string("ETag"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, etag),
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, etag), 0 },

    { ngx_string("Server"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, server),
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, server), 0 },

    { ngx_string("WWW-Authenticate"),
                 ngx_http_upstream_process_multi_header_lines,
                 offsetof(ngx_http_upstream_headers_in_t, www_authenticate),
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("Location"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, location),
                 ngx_http_upstream_rewrite_location, 0, 0 },

    { ngx_string("Refresh"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, refresh),
                 ngx_http_upstream_rewrite_refresh, 0, 0 },

    { ngx_string("Set-Cookie"),
                 ngx_http_upstream_process_set_cookie,
                 offsetof(ngx_http_upstream_headers_in_t, set_cookie),
                 ngx_http_upstream_rewrite_set_cookie, 0, 1 },

    { ngx_string("Content-Disposition"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_copy_header_line, 0, 1 },

    { ngx_string("Cache-Control"),
                 ngx_http_upstream_process_cache_control, 0,
                 ngx_http_upstream_copy_multi_header_lines,
                 offsetof(ngx_http_headers_out_t, cache_control), 1 },

    { ngx_string("Expires"),
                 ngx_http_upstream_process_expires, 0,
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, expires), 1 },

    { ngx_string("Accept-Ranges"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_copy_allow_ranges,
                 offsetof(ngx_http_headers_out_t, accept_ranges), 1 },

    { ngx_string("Content-Range"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, content_range), 0 },

    { ngx_string("Connection"),
                 ngx_http_upstream_process_connection, 0,
                 ngx_http_upstream_ignore_header_line, 0, 0 },

    { ngx_string("Keep-Alive"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_ignore_header_line, 0, 0 },

    { ngx_string("Vary"),
                 ngx_http_upstream_process_vary, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("Link"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_copy_multi_header_lines,
                 offsetof(ngx_http_headers_out_t, link), 0 },

    { ngx_string("X-Accel-Expires"),
                 ngx_http_upstream_process_accel_expires, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Accel-Redirect"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, x_accel_redirect),
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Accel-Limit-Rate"),
                 ngx_http_upstream_process_limit_rate, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Accel-Buffering"),
                 ngx_http_upstream_process_buffering, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Accel-Charset"),
                 ngx_http_upstream_process_charset, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("Transfer-Encoding"),
                 ngx_http_upstream_process_transfer_encoding, 0,
                 ngx_http_upstream_ignore_header_line, 0, 0 },

    { ngx_string("Content-Encoding"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, content_encoding), 0 },

    { ngx_null_string, NULL, 0, NULL, 0, 0 }
};


/**
 * @brief 定义 ngx_http_upstream 模块的配置指令数组
 *
 * 这个数组包含了 ngx_http_upstream 模块支持的所有配置指令。
 * 每个指令都是一个 ngx_command_t 结构体，定义了指令的名称、类型、处理函数等信息。
 * 这些指令用于配置上游服务器组和相关的负载均衡策略。
 */
static ngx_command_t  ngx_http_upstream_commands[] = {

    { ngx_string("upstream"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE1,
      ngx_http_upstream,
      0,
      0,
      NULL },

    { ngx_string("server"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_upstream_server,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


/**
 * @brief 定义 ngx_http_upstream 模块的上下文结构
 *
 * 这个结构体定义了 ngx_http_upstream 模块的各种回调函数和配置处理函数。
 * 它是 Nginx 模块系统的一个重要组成部分，用于指定模块在不同阶段的行为。
 */
static ngx_http_module_t  ngx_http_upstream_module_ctx = {
    ngx_http_upstream_add_variables,       /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_upstream_create_main_conf,    /* create main configuration */
    ngx_http_upstream_init_main_conf,      /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


/**
 * @brief 定义 ngx_http_upstream 模块
 *
 * 这个结构体定义了 ngx_http_upstream 模块的基本信息和行为。
 * 它包含了模块的上下文、指令集、类型等重要信息，是 Nginx 模块系统的核心组成部分。
 * ngx_http_upstream 模块负责处理与上游服务器的交互，包括负载均衡、健康检查等功能。
 */
ngx_module_t  ngx_http_upstream_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_module_ctx,         /* module context */
    ngx_http_upstream_commands,            /* module directives */
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


/**
 * @brief 定义上游相关的变量数组
 *
 * 这个数组包含了一系列与上游服务器交互相关的变量定义。
 * 每个变量都可以在Nginx配置中使用，用于获取上游请求的各种信息，
 * 如上游服务器地址、响应状态、响应时间等。
 * 这些变量对于日志记录、条件判断和调试非常有用。
 */
static ngx_http_variable_t  ngx_http_upstream_vars[] = {

    /**
     * @brief 定义上游服务器地址变量
     * 
     * 这个变量用于获取上游服务器的地址信息。
     * 当请求被代理到上游服务器时，此变量将包含实际处理请求的服务器地址。
     */
    { ngx_string("upstream_addr"), NULL,
      ngx_http_upstream_addr_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    /**
     * @brief 定义上游服务器响应状态变量
     * 
     * 这个变量用于获取上游服务器返回的HTTP状态码。
     * 它反映了上游服务器处理请求的结果。
     */
    { ngx_string("upstream_status"), NULL,
      ngx_http_upstream_status_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    /**
     * @brief 定义上游服务器连接时间变量
     * 
     * 这个变量用于记录与上游服务器建立连接所需的时间。
     * 它可以帮助诊断网络连接问题。
     */
    { ngx_string("upstream_connect_time"), NULL,
      ngx_http_upstream_response_time_variable, 2,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    /**
     * @brief 定义上游服务器响应头部时间变量
     * 
     * 这个变量用于记录从建立连接到接收到上游服务器响应头部的时间。
     * 它可以帮助评估上游服务器的处理速度。
     */
    { ngx_string("upstream_header_time"), NULL,
      ngx_http_upstream_response_time_variable, 1,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    /**
     * @brief 定义上游服务器响应时间变量
     * 
     * 这个变量用于记录从建立连接到接收完整个响应的时间。
     * 它反映了整个上游请求-响应周期的耗时。
     */
    { ngx_string("upstream_response_time"), NULL,
      ngx_http_upstream_response_time_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    /**
     * @brief 定义上游服务器响应长度变量
     * 
     * 这个变量用于记录从上游服务器接收到的响应体的长度。
     */
    { ngx_string("upstream_response_length"), NULL,
      ngx_http_upstream_response_length_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    /**
     * @brief 定义从上游服务器接收的字节数变量
     * 
     * 这个变量用于记录从上游服务器接收到的总字节数，包括响应头和响应体。
     */
    { ngx_string("upstream_bytes_received"), NULL,
      ngx_http_upstream_response_length_variable, 1,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    /**
     * @brief 定义发送到上游服务器的字节数变量
     * 
     * 这个变量用于记录发送到上游服务器的总字节数，包括请求头和请求体。
     */
    { ngx_string("upstream_bytes_sent"), NULL,
      ngx_http_upstream_response_length_variable, 2,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

#if (NGX_HTTP_CACHE)

    /**
     * @brief 定义上游缓存状态变量
     * 
     * 这个变量用于显示当前请求的缓存状态，如MISS, HIT, EXPIRED等。
     * 只有在启用了HTTP缓存功能时才可用。
     */
    { ngx_string("upstream_cache_status"), NULL,
      ngx_http_upstream_cache_status, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    /**
     * @brief 定义上游缓存最后修改时间变量
     * 
     * 这个变量用于获取缓存内容的最后修改时间。
     * 只有在启用了HTTP缓存功能时才可用。
     */
    { ngx_string("upstream_cache_last_modified"), NULL,
      ngx_http_upstream_cache_last_modified, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    /**
     * @brief 定义上游缓存ETag变量
     * 
     * 这个变量用于获取缓存内容的ETag值。
     * 只有在启用了HTTP缓存功能时才可用。
     */
    { ngx_string("upstream_cache_etag"), NULL,
      ngx_http_upstream_cache_etag, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

#endif

    /**
     * @brief 定义上游HTTP头部变量
     * 
     * 这个变量用于访问上游服务器返回的任意HTTP头部。
     * 使用方式为 $upstream_http_头部名称，例如 $upstream_http_server。
     */
    { ngx_string("upstream_http_"), NULL, ngx_http_upstream_header_variable,
      0, NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_PREFIX, 0 },

    /**
     * @brief 定义上游HTTP trailer变量
     * 
     * 这个变量用于访问上游服务器返回的HTTP trailer。
     * 使用方式为 $upstream_trailer_名称。
     */
    { ngx_string("upstream_trailer_"), NULL, ngx_http_upstream_trailer_variable,
      0, NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_PREFIX, 0 },

    /**
     * @brief 定义上游Cookie变量
     * 
     * 这个变量用于访问上游服务器设置的Cookie。
     * 使用方式为 $upstream_cookie_名称。
     */
    { ngx_string("upstream_cookie_"), NULL, ngx_http_upstream_cookie_variable,
      0, NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_PREFIX, 0 },

    /**
     * @brief 结束标记
     * 
     * 这个空的变量定义用于标记数组的结束。
     */
      ngx_http_null_variable
};


/**
 * @brief 定义上游错误处理数组
 *
 * 这个数组定义了不同HTTP状态码对应的上游错误处理标志。
 * 每个元素包含一个HTTP状态码和相应的错误处理标志。
 * 当上游服务器返回这些状态码时，Nginx可以根据这个数组来决定如何处理这些错误。
 */
static ngx_http_upstream_next_t  ngx_http_upstream_next_errors[] = {
    { 500, NGX_HTTP_UPSTREAM_FT_HTTP_500 },
    { 502, NGX_HTTP_UPSTREAM_FT_HTTP_502 },
    { 503, NGX_HTTP_UPSTREAM_FT_HTTP_503 },
    { 504, NGX_HTTP_UPSTREAM_FT_HTTP_504 },
    { 403, NGX_HTTP_UPSTREAM_FT_HTTP_403 },
    { 404, NGX_HTTP_UPSTREAM_FT_HTTP_404 },
    { 429, NGX_HTTP_UPSTREAM_FT_HTTP_429 },
    { 0, 0 }
};


/**
 * @brief 定义上游缓存方法掩码
 *
 * 这个数组定义了可以被缓存的HTTP方法。
 * 每个元素包含一个HTTP方法的字符串表示和对应的方法标志。
 * 用于配置哪些HTTP方法的响应可以被缓存。
 */
ngx_conf_bitmask_t  ngx_http_upstream_cache_method_mask[] = {
    { ngx_string("GET"), NGX_HTTP_GET },    // GET方法
    { ngx_string("HEAD"), NGX_HTTP_HEAD },  // HEAD方法
    { ngx_string("POST"), NGX_HTTP_POST },  // POST方法
    { ngx_null_string, 0 }                  // 结束标记
};


/**
 * @brief 定义上游忽略头部掩码
 *
 * 这个数组定义了可以被忽略的HTTP头部。
 * 每个元素包含一个HTTP头部的字符串表示和对应的忽略标志。
 * 用于配置哪些从上游服务器接收的HTTP头部应该被忽略。
 */
ngx_conf_bitmask_t  ngx_http_upstream_ignore_headers_masks[] = {
    { ngx_string("X-Accel-Redirect"), NGX_HTTP_UPSTREAM_IGN_XA_REDIRECT },   // 忽略X-Accel-Redirect头
    { ngx_string("X-Accel-Expires"), NGX_HTTP_UPSTREAM_IGN_XA_EXPIRES },     // 忽略X-Accel-Expires头
    { ngx_string("X-Accel-Limit-Rate"), NGX_HTTP_UPSTREAM_IGN_XA_LIMIT_RATE }, // 忽略X-Accel-Limit-Rate头
    { ngx_string("X-Accel-Buffering"), NGX_HTTP_UPSTREAM_IGN_XA_BUFFERING }, // 忽略X-Accel-Buffering头
    { ngx_string("X-Accel-Charset"), NGX_HTTP_UPSTREAM_IGN_XA_CHARSET },     // 忽略X-Accel-Charset头
    { ngx_string("Expires"), NGX_HTTP_UPSTREAM_IGN_EXPIRES },                // 忽略Expires头
    { ngx_string("Cache-Control"), NGX_HTTP_UPSTREAM_IGN_CACHE_CONTROL },    // 忽略Cache-Control头
    { ngx_string("Set-Cookie"), NGX_HTTP_UPSTREAM_IGN_SET_COOKIE },          // 忽略Set-Cookie头
    { ngx_string("Vary"), NGX_HTTP_UPSTREAM_IGN_VARY },                      // 忽略Vary头
    { ngx_null_string, 0 }                                                   // 结束标记
};


/**
 * @brief 创建上游请求结构体
 *
 * 该函数用于创建和初始化上游请求结构体。
 *
 * @param r 指向当前HTTP请求的指针
 * @return NGX_OK 成功时返回，NGX_ERROR 失败时返回
 */
ngx_int_t
ngx_http_upstream_create(ngx_http_request_t *r)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 如果已存在上游结构体且有清理函数，先执行清理
    if (u && u->cleanup) {
        r->main->count++;
        ngx_http_upstream_cleanup(r);
    }

    // 分配新的上游结构体内存
    u = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_t));
    if (u == NULL) {
        return NGX_ERROR;
    }

    r->upstream = u;

    // 设置日志相关参数
    u->peer.log = r->connection->log;
    u->peer.log_error = NGX_ERROR_ERR;

#if (NGX_HTTP_CACHE)
    r->cache = NULL;
#endif

    // 初始化头部信息
    u->headers_in.content_length_n = -1;
    u->headers_in.last_modified_time = -1;

    return NGX_OK;
}


/**
 * @brief 初始化上游请求
 *
 * 该函数用于初始化上游请求的处理过程。
 *
 * @param r 指向当前HTTP请求的指针
 */
void
ngx_http_upstream_init(ngx_http_request_t *r)
{
    ngx_connection_t     *c;

    c = r->connection;

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http init upstream, client timer: %d", c->read->timer_set);

#if (NGX_HTTP_V2)
    // HTTP/2 特殊处理
    if (r->stream) {
        ngx_http_upstream_init_request(r);
        return;
    }
#endif

#if (NGX_HTTP_V3)
    // HTTP/3 特殊处理
    if (c->quic) {
        ngx_http_upstream_init_request(r);
        return;
    }
#endif

    // 移除读事件定时器
    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    // 对于边缘触发模式，确保写事件被激活
    if (ngx_event_flags & NGX_USE_CLEAR_EVENT) {

        if (!c->write->active) {
            if (ngx_add_event(c->write, NGX_WRITE_EVENT, NGX_CLEAR_EVENT)
                == NGX_ERROR)
            {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
        }
    }

    // 调用请求初始化函数
    ngx_http_upstream_init_request(r);
}


static void
ngx_http_upstream_init_request(ngx_http_request_t *r)
{
    ngx_str_t                      *host;
    ngx_uint_t                      i;
    ngx_resolver_ctx_t             *ctx, temp;
    ngx_http_cleanup_t             *cln;
    ngx_http_upstream_t            *u;
    ngx_http_core_loc_conf_t       *clcf;
    ngx_http_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_http_upstream_main_conf_t  *umcf;

    // 如果请求正在进行异步I/O操作，则直接返回
    if (r->aio) {
        return;
    }

    u = r->upstream;

#if (NGX_HTTP_CACHE)

    // 如果配置了缓存，处理缓存逻辑
    if (u->conf->cache) {
        ngx_int_t  rc;

        rc = ngx_http_upstream_cache(r, u);

        if (rc == NGX_BUSY) {
            r->write_event_handler = ngx_http_upstream_init_request;
            return;
        }

        r->write_event_handler = ngx_http_request_empty_handler;

        if (rc == NGX_ERROR) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        if (rc == NGX_OK) {
            rc = ngx_http_upstream_cache_send(r, u);

            if (rc == NGX_DONE) {
                return;
            }

            if (rc == NGX_HTTP_UPSTREAM_INVALID_HEADER) {
                rc = NGX_DECLINED;
                r->cached = 0;
                u->buffer.start = NULL;
                u->cache_status = NGX_HTTP_CACHE_MISS;
                u->request_sent = 1;
            }
        }

        if (rc != NGX_DECLINED) {
            ngx_http_finalize_request(r, rc);
            return;
        }
    }

#endif

    // 设置存储标志
    u->store = u->conf->store;

    // 如果不需要存储、不是post_action且不忽略客户端中断，则设置相关事件处理器
    if (!u->store && !r->post_action && !u->conf->ignore_client_abort) {

        if (r->connection->read->ready) {
            ngx_post_event(r->connection->read, &ngx_posted_events);

        } else {
            if (ngx_handle_read_event(r->connection->read, 0) != NGX_OK) {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
        }

        r->read_event_handler = ngx_http_upstream_rd_check_broken_connection;
        r->write_event_handler = ngx_http_upstream_wr_check_broken_connection;
    }

    // 如果存在请求体，设置请求缓冲区
    if (r->request_body) {
        u->request_bufs = r->request_body->bufs;
    }

    // 创建上游请求
    if (u->create_request(r) != NGX_OK) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    // 设置本地地址
    if (ngx_http_upstream_set_local(r, u, u->conf->local) != NGX_OK) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    // 设置socket keepalive
    if (u->conf->socket_keepalive) {
        u->peer.so_keepalive = 1;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 设置输出缓冲区参数
    u->output.alignment = clcf->directio_alignment;
    u->output.pool = r->pool;
    u->output.bufs.num = 1;
    u->output.bufs.size = clcf->client_body_buffer_size;

    // 设置输出过滤器
    if (u->output.output_filter == NULL) {
        u->output.output_filter = ngx_chain_writer;
        u->output.filter_ctx = &u->writer;
    }

    u->writer.pool = r->pool;

    // 创建或更新上游状态数组
    if (r->upstream_states == NULL) {

        r->upstream_states = ngx_array_create(r->pool, 1,
                                            sizeof(ngx_http_upstream_state_t));
        if (r->upstream_states == NULL) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

    } else {

        u->state = ngx_array_push(r->upstream_states);
        if (u->state == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        ngx_memzero(u->state, sizeof(ngx_http_upstream_state_t));
    }

    // 添加清理回调
    cln = ngx_http_cleanup_add(r, 0);
    if (cln == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    cln->handler = ngx_http_upstream_cleanup;
    cln->data = r;
    u->cleanup = &cln->handler;

    // 处理上游服务器配置
    if (u->resolved == NULL) {

        uscf = u->conf->upstream;

    } else {

#if (NGX_HTTP_SSL)
        u->ssl_name = u->resolved->host;
#endif

        host = &u->resolved->host;

        umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

        uscfp = umcf->upstreams.elts;

        // 查找匹配的上游服务器配置
        for (i = 0; i < umcf->upstreams.nelts; i++) {

            uscf = uscfp[i];

            if (uscf->host.len == host->len
                && ((uscf->port == 0 && u->resolved->no_port)
                     || uscf->port == u->resolved->port)
                && ngx_strncasecmp(uscf->host.data, host->data, host->len) == 0)
            {
                goto found;
            }
        }

        // 处理已解析的sockaddr
        if (u->resolved->sockaddr) {

            if (u->resolved->port == 0
                && u->resolved->sockaddr->sa_family != AF_UNIX)
            {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "no port in upstream \"%V\"", host);
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            if (ngx_http_upstream_create_round_robin_peer(r, u->resolved)
                != NGX_OK)
            {
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            ngx_http_upstream_connect(r, u);

            return;
        }

        // 检查端口配置
        if (u->resolved->port == 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "no port in upstream \"%V\"", host);
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        // 解析主机名
        temp.name = *host;

        ctx = ngx_resolve_start(clcf->resolver, &temp);
        if (ctx == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        if (ctx == NGX_NO_RESOLVER) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "no resolver defined to resolve %V", host);

            ngx_http_upstream_finalize_request(r, u, NGX_HTTP_BAD_GATEWAY);
            return;
        }

        ctx->name = *host;
        ctx->handler = ngx_http_upstream_resolve_handler;
        ctx->data = r;
        ctx->timeout = clcf->resolver_timeout;

        u->resolved->ctx = ctx;

        if (ngx_resolve_name(ctx) != NGX_OK) {
            u->resolved->ctx = NULL;
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        return;
    }

found:

    // 检查上游配置是否存在
    if (uscf == NULL) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "no upstream configuration");
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    u->upstream = uscf;

#if (NGX_HTTP_SSL)
    u->ssl_name = uscf->host;
#endif

    // 初始化peer
    if (uscf->peer.init(r, uscf) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    u->peer.start_time = ngx_current_msec;

    // 设置重试次数
    if (u->conf->next_upstream_tries
        && u->peer.tries > u->conf->next_upstream_tries)
    {
        u->peer.tries = u->conf->next_upstream_tries;
    }

    // 连接上游服务器
    ngx_http_upstream_connect(r, u);
}


#if (NGX_HTTP_CACHE)

/**
 * @brief 处理上游缓存
 *
 * @param r 请求结构体
 * @param u 上游结构体
 * @return ngx_int_t 处理结果
 */
static ngx_int_t
ngx_http_upstream_cache(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_int_t               rc;
    ngx_http_cache_t       *c;
    ngx_http_file_cache_t  *cache;

    c = r->cache;

    // 如果缓存未初始化
    if (c == NULL) {

        // 检查请求方法是否允许缓存
        if (!(r->method & u->conf->cache_methods)) {
            return NGX_DECLINED;
        }

        // 获取缓存配置
        rc = ngx_http_upstream_cache_get(r, u, &cache);

        if (rc != NGX_OK) {
            return rc;
        }

        // 对HEAD请求的特殊处理
        if (r->method == NGX_HTTP_HEAD && u->conf->cache_convert_head) {
            u->method = ngx_http_core_get_method;
        }

        // 初始化文件缓存
        if (ngx_http_file_cache_new(r) != NGX_OK) {
            return NGX_ERROR;
        }

        // 创建缓存键
        if (u->create_key(r) != NGX_OK) {
            return NGX_ERROR;
        }

        /* TODO: add keys */

        ngx_http_file_cache_create_key(r);

        // 检查缓冲区大小是否足够
        if (r->cache->header_start + 256 > u->conf->buffer_size) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "%V_buffer_size %uz is not enough for cache key, "
                          "it should be increased to at least %uz",
                          &u->conf->module, u->conf->buffer_size,
                          ngx_align(r->cache->header_start + 256, 1024));

            r->cache = NULL;
            return NGX_DECLINED;
        }

        u->cacheable = 1;

        c = r->cache;

        // 设置缓存参数
        c->body_start = u->conf->buffer_size;
        c->min_uses = u->conf->cache_min_uses;
        c->file_cache = cache;

        // 检查缓存绕过条件
        switch (ngx_http_test_predicates(r, u->conf->cache_bypass)) {

        case NGX_ERROR:
            return NGX_ERROR;

        case NGX_DECLINED:
            u->cache_status = NGX_HTTP_CACHE_BYPASS;
            return NGX_DECLINED;

        default: /* NGX_OK */
            break;
        }

        // 设置缓存锁参数
        c->lock = u->conf->cache_lock;
        c->lock_timeout = u->conf->cache_lock_timeout;
        c->lock_age = u->conf->cache_lock_age;

        u->cache_status = NGX_HTTP_CACHE_MISS;
    }

    // 打开文件缓存
    rc = ngx_http_file_cache_open(r);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream cache: %i", rc);

    // 处理不同的缓存状态
    switch (rc) {

    case NGX_HTTP_CACHE_STALE:

        // 处理过期缓存的更新
        if (((u->conf->cache_use_stale & NGX_HTTP_UPSTREAM_FT_UPDATING)
             || c->stale_updating) && !r->background
            && u->conf->cache_background_update)
        {
            if (ngx_http_upstream_cache_background_update(r, u) == NGX_OK) {
                r->cache->background = 1;
                u->cache_status = rc;
                rc = NGX_OK;

            } else {
                rc = NGX_ERROR;
            }
        }

        break;

    case NGX_HTTP_CACHE_UPDATING:

        // 处理正在更新的缓存
        if (((u->conf->cache_use_stale & NGX_HTTP_UPSTREAM_FT_UPDATING)
             || c->stale_updating) && !r->background)
        {
            u->cache_status = rc;
            rc = NGX_OK;

        } else {
            rc = NGX_HTTP_CACHE_STALE;
        }

        break;

    case NGX_OK:
        u->cache_status = NGX_HTTP_CACHE_HIT;
    }

    // 根据缓存状态进行最终处理
    switch (rc) {

    case NGX_OK:

        return NGX_OK;

    case NGX_HTTP_CACHE_STALE:

        // 重置缓存有效期
        c->valid_sec = 0;
        c->updating_sec = 0;
        c->error_sec = 0;

        u->buffer.start = NULL;
        u->cache_status = NGX_HTTP_CACHE_EXPIRED;

        break;

    case NGX_DECLINED:

        // 调整缓冲区
        if ((size_t) (u->buffer.end - u->buffer.start) < u->conf->buffer_size) {
            u->buffer.start = NULL;

        } else {
            u->buffer.pos = u->buffer.start + c->header_start;
            u->buffer.last = u->buffer.pos;
        }

        break;

    case NGX_HTTP_CACHE_SCARCE:

        u->cacheable = 0;

        break;

    case NGX_AGAIN:

        return NGX_BUSY;

    case NGX_ERROR:

        return NGX_ERROR;

    default:

        /* cached NGX_HTTP_BAD_GATEWAY, NGX_HTTP_GATEWAY_TIME_OUT, etc. */

        u->cache_status = NGX_HTTP_CACHE_HIT;

        return rc;
    }

    // 检查范围请求
    if (ngx_http_upstream_cache_check_range(r, u) == NGX_DECLINED) {
        u->cacheable = 0;
    }

    r->cached = 0;

    return NGX_DECLINED;
}


/**
 * @brief 获取上游缓存配置
 *
 * @param r 请求结构体
 * @param u 上游结构体
 * @param cache 文件缓存指针的指针
 * @return ngx_int_t 处理结果
 */
static ngx_int_t
ngx_http_upstream_cache_get(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_http_file_cache_t **cache)
{
    ngx_str_t               *name, val;
    ngx_uint_t               i;
    ngx_http_file_cache_t  **caches;

    // 如果已配置缓存区域，直接返回
    if (u->conf->cache_zone) {
        *cache = u->conf->cache_zone->data;
        return NGX_OK;
    }

    // 获取缓存值
    if (ngx_http_complex_value(r, u->conf->cache_value, &val) != NGX_OK) {
        return NGX_ERROR;
    }

    // 检查缓存是否被禁用
    if (val.len == 0
        || (val.len == 3 && ngx_strncmp(val.data, "off", 3) == 0))
    {
        return NGX_DECLINED;
    }

    caches = u->caches->elts;

    // 查找匹配的缓存配置
    for (i = 0; i < u->caches->nelts; i++) {
        name = &caches[i]->shm_zone->shm.name;

        if (name->len == val.len
            && ngx_strncmp(name->data, val.data, val.len) == 0)
        {
            *cache = caches[i];
            return NGX_OK;
        }
    }

    // 未找到匹配的缓存配置
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "cache \"%V\" not found", &val);

    return NGX_ERROR;
}


/**
 * @brief 发送上游缓存响应
 *
 * @param r 请求结构体
 * @param u 上游结构体
 * @return ngx_int_t 处理结果
 */
static ngx_int_t
ngx_http_upstream_cache_send(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_int_t          rc;
    ngx_http_cache_t  *c;

    r->cached = 1;
    c = r->cache;

    // 处理HTTP/0.9请求
    if (c->header_start == c->body_start) {
        r->http_version = NGX_HTTP_VERSION_9;
        return ngx_http_cache_send(r);
    }

    /* TODO: cache stack */

    // 设置缓冲区
    u->buffer = *c->buf;
    u->buffer.pos += c->header_start;

    // 初始化上游头部信息
    ngx_memzero(&u->headers_in, sizeof(ngx_http_upstream_headers_in_t));
    u->headers_in.content_length_n = -1;
    u->headers_in.last_modified_time = -1;

    // 初始化头部列表
    if (ngx_list_init(&u->headers_in.headers, r->pool, 8,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 初始化尾部列表
    if (ngx_list_init(&u->headers_in.trailers, r->pool, 2,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 处理头部
    rc = u->process_header(r);

    if (rc == NGX_OK) {

        if (ngx_http_upstream_process_headers(r, u) != NGX_OK) {
            return NGX_DONE;
        }

        return ngx_http_cache_send(r);
    }

    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (rc == NGX_AGAIN) {
        rc = NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    /* rc == NGX_HTTP_UPSTREAM_INVALID_HEADER */

    // 记录无效头部错误
    ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                  "cache file \"%s\" contains invalid header",
                  c->file.name.data);

    /* TODO: delete file */

    return rc;
}


/**
 * @brief 处理上游缓存的后台更新
 *
 * @param r 当前请求
 * @param u 上游结构体
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
static ngx_int_t
ngx_http_upstream_cache_background_update(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_http_request_t  *sr;

    // 如果是主请求，设置保留请求体标志
    if (r == r->main) {
        r->preserve_body = 1;
    }

    // 创建子请求进行后台更新
    if (ngx_http_subrequest(r, &r->uri, &r->args, &sr, NULL,
                            NGX_HTTP_SUBREQUEST_CLONE
                            |NGX_HTTP_SUBREQUEST_BACKGROUND)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 设置子请求为仅处理头部
    sr->header_only = 1;

    return NGX_OK;
}


/**
 * @brief 检查缓存的范围请求是否有效
 *
 * @param r 当前请求
 * @param u 上游结构体
 * @return NGX_OK 有效，NGX_DECLINED 无效，需要重新获取
 */
static ngx_int_t
ngx_http_upstream_cache_check_range(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    off_t             offset;
    u_char           *p, *start;
    ngx_table_elt_t  *h;

    h = r->headers_in.range;

    // 检查是否需要处理范围请求
    if (h == NULL
        || !u->cacheable
        || u->conf->cache_max_range_offset == NGX_MAX_OFF_T_VALUE)
    {
        return NGX_OK;
    }

    // 如果最大范围偏移设置为0，则拒绝所有范围请求
    if (u->conf->cache_max_range_offset == 0) {
        return NGX_DECLINED;
    }

    // 检查Range头部格式
    if (h->value.len < 7
        || ngx_strncasecmp(h->value.data, (u_char *) "bytes=", 6) != 0)
    {
        return NGX_OK;
    }

    p = h->value.data + 6;

    // 跳过空格
    while (*p == ' ') { p++; }

    // 如果是负偏移，拒绝请求
    if (*p == '-') {
        return NGX_DECLINED;
    }

    start = p;

    // 解析偏移值
    while (*p >= '0' && *p <= '9') { p++; }

    offset = ngx_atoof(start, p - start);

    // 检查偏移是否超过最大允许值
    if (offset >= u->conf->cache_max_range_offset) {
        return NGX_DECLINED;
    }

    return NGX_OK;
}

#endif


/**
 * @brief 处理域名解析完成后的回调函数
 *
 * @param ctx 解析器上下文
 */
static void
ngx_http_upstream_resolve_handler(ngx_resolver_ctx_t *ctx)
{
    ngx_uint_t                     run_posted;
    ngx_connection_t              *c;
    ngx_http_request_t            *r;
    ngx_http_upstream_t           *u;
    ngx_http_upstream_resolved_t  *ur;

    run_posted = ctx->async;

    r = ctx->data;
    c = r->connection;

    u = r->upstream;
    ur = u->resolved;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream resolve: \"%V?%V\"", &r->uri, &r->args);

    // 检查解析状态
    if (ctx->state) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "%V could not be resolved (%i: %s)",
                      &ctx->name, ctx->state,
                      ngx_resolver_strerror(ctx->state));

        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_BAD_GATEWAY);
        goto failed;
    }

    // 保存解析结果
    ur->naddrs = ctx->naddrs;
    ur->addrs = ctx->addrs;

#if (NGX_DEBUG)
    {
    u_char      text[NGX_SOCKADDR_STRLEN];
    ngx_str_t   addr;
    ngx_uint_t  i;

    addr.data = text;

    // 打印解析得到的地址
    for (i = 0; i < ctx->naddrs; i++) {
        addr.len = ngx_sock_ntop(ur->addrs[i].sockaddr, ur->addrs[i].socklen,
                                 text, NGX_SOCKADDR_STRLEN, 0);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "name was resolved to %V", &addr);
    }
    }
#endif

    // 创建轮询负载均衡器
    if (ngx_http_upstream_create_round_robin_peer(r, ur) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        goto failed;
    }

    // 完成域名解析
    ngx_resolve_name_done(ctx);
    ur->ctx = NULL;

    u->peer.start_time = ngx_current_msec;

    // 设置重试次数
    if (u->conf->next_upstream_tries
        && u->peer.tries > u->conf->next_upstream_tries)
    {
        u->peer.tries = u->conf->next_upstream_tries;
    }

    // 开始连接上游服务器
    ngx_http_upstream_connect(r, u);

failed:

    // 如果是异步操作，运行已发布的请求
    if (run_posted) {
        ngx_http_run_posted_requests(c);
    }
}


/**
 * @brief 处理上游事件的主要函数
 *
 * @param ev 触发的事件
 */
static void
ngx_http_upstream_handler(ngx_event_t *ev)
{
    ngx_connection_t     *c;
    ngx_http_request_t   *r;
    ngx_http_upstream_t  *u;

    // 获取连接、请求和上游对象
    c = ev->data;
    r = c->data;
    u = r->upstream;
    c = r->connection;

    // 设置日志请求上下文
    ngx_http_set_log_request(c->log, r);

    // 记录调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream request: \"%V?%V\"", &r->uri, &r->args);

    // 处理延迟和超时事件
    if (ev->delayed && ev->timedout) {
        ev->delayed = 0;
        ev->timedout = 0;
    }

    // 根据事件类型调用相应的处理函数
    if (ev->write) {
        u->write_event_handler(r, u);
    } else {
        u->read_event_handler(r, u);
    }

    // 运行已发布的请求
    ngx_http_run_posted_requests(c);
}

/**
 * @brief 检查读事件上的连接是否断开
 *
 * @param r HTTP请求
 */
static void
ngx_http_upstream_rd_check_broken_connection(ngx_http_request_t *r)
{
    ngx_http_upstream_check_broken_connection(r, r->connection->read);
}

/**
 * @brief 检查写事件上的连接是否断开
 *
 * @param r HTTP请求
 */
static void
ngx_http_upstream_wr_check_broken_connection(ngx_http_request_t *r)
{
    ngx_http_upstream_check_broken_connection(r, r->connection->write);
}


/**
 * @brief 检查上游连接是否断开
 *
 * @param r HTTP请求
 * @param ev 事件对象
 */
static void
ngx_http_upstream_check_broken_connection(ngx_http_request_t *r,
    ngx_event_t *ev)
{
    int                  n;
    char                 buf[1];
    ngx_err_t            err;
    ngx_int_t            event;
    ngx_connection_t     *c;
    ngx_http_upstream_t  *u;

    // 记录调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                   "http upstream check client, write event:%d, \"%V\"",
                   ev->write, &r->uri);

    c = r->connection;
    u = r->upstream;

    // 检查连接是否已经出错
    if (c->error) {
        // 如果使用水平触发事件，且事件处于活动状态，则删除事件
        if ((ngx_event_flags & NGX_USE_LEVEL_EVENT) && ev->active) {

            event = ev->write ? NGX_WRITE_EVENT : NGX_READ_EVENT;

            if (ngx_del_event(ev, event, 0) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
        }

        // 如果不可缓存，则结束请求
        if (!u->cacheable) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
        }

        return;
    }

#if (NGX_HTTP_V2)
    // HTTP/2 特殊处理
    if (r->stream) {
        return;
    }
#endif

#if (NGX_HTTP_V3)

    // HTTP/3 (QUIC) 特殊处理
    if (c->quic) {
        if (c->write->error) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
        }

        return;
    }

#endif

#if (NGX_HAVE_KQUEUE)

    // kqueue 事件处理
    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {

        if (!ev->pending_eof) {
            return;
        }

        ev->eof = 1;
        c->error = 1;

        if (ev->kq_errno) {
            ev->error = 1;
        }

        // 处理客户端提前关闭连接的情况
        if (!u->cacheable && u->peer.connection) {
            ngx_log_error(NGX_LOG_INFO, ev->log, ev->kq_errno,
                          "kevent() reported that client prematurely closed "
                          "connection, so upstream connection is closed too");
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
            return;
        }

        ngx_log_error(NGX_LOG_INFO, ev->log, ev->kq_errno,
                      "kevent() reported that client prematurely closed "
                      "connection");

        if (u->peer.connection == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
        }

        return;
    }

#endif

#if (NGX_HAVE_EPOLLRDHUP)

    // epoll 事件处理
    if ((ngx_event_flags & NGX_USE_EPOLL_EVENT) && ngx_use_epoll_rdhup) {
        socklen_t  len;

        if (!ev->pending_eof) {
            return;
        }

        ev->eof = 1;
        c->error = 1;

        err = 0;
        len = sizeof(ngx_err_t);

        // 获取套接字错误
        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
        {
            err = ngx_socket_errno;
        }

        if (err) {
            ev->error = 1;
        }

        // 处理客户端提前关闭连接的情况
        if (!u->cacheable && u->peer.connection) {
            ngx_log_error(NGX_LOG_INFO, ev->log, err,
                        "epoll_wait() reported that client prematurely closed "
                        "connection, so upstream connection is closed too");
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
            return;
        }

        ngx_log_error(NGX_LOG_INFO, ev->log, err,
                      "epoll_wait() reported that client prematurely closed "
                      "connection");

        if (u->peer.connection == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
        }

        return;
    }

#endif

    // 尝试接收数据以检查连接状态
    n = recv(c->fd, buf, 1, MSG_PEEK);

    err = ngx_socket_errno;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ev->log, err,
                   "http upstream recv(): %d", n);

    // 写事件且接收成功或遇到EAGAIN，则返回
    if (ev->write && (n >= 0 || err == NGX_EAGAIN)) {
        return;
    }

    // 如果使用水平触发事件，且事件处于活动状态，则删除事件
    if ((ngx_event_flags & NGX_USE_LEVEL_EVENT) && ev->active) {

        event = ev->write ? NGX_WRITE_EVENT : NGX_READ_EVENT;

        if (ngx_del_event(ev, event, 0) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    if (n > 0) {
        return;
    }

    if (n == -1) {
        if (err == NGX_EAGAIN) {
            return;
        }

        ev->error = 1;

    } else { /* n == 0 */
        err = 0;
    }

    // 标记连接已关闭或出错
    ev->eof = 1;
    c->error = 1;

    // 处理客户端提前关闭连接的情况
    if (!u->cacheable && u->peer.connection) {
        ngx_log_error(NGX_LOG_INFO, ev->log, err,
                      "client prematurely closed connection, "
                      "so upstream connection is closed too");
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_CLIENT_CLOSED_REQUEST);
        return;
    }

    ngx_log_error(NGX_LOG_INFO, ev->log, err,
                  "client prematurely closed connection");

    if (u->peer.connection == NULL) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_CLIENT_CLOSED_REQUEST);
    }
}


/**
 * @brief 连接上游服务器
 *
 * @param r HTTP请求
 * @param u 上游结构体
 */
static void
ngx_http_upstream_connect(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_int_t                  rc;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    // 设置日志动作
    r->connection->log->action = "connecting to upstream";

    // 更新响应时间
    if (u->state && u->state->response_time == (ngx_msec_t) -1) {
        u->state->response_time = ngx_current_msec - u->start_time;
    }

    // 为新的上游状态分配内存
    u->state = ngx_array_push(r->upstream_states);
    if (u->state == NULL) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    // 初始化新的上游状态
    ngx_memzero(u->state, sizeof(ngx_http_upstream_state_t));

    u->start_time = ngx_current_msec;

    u->state->response_time = (ngx_msec_t) -1;
    u->state->connect_time = (ngx_msec_t) -1;
    u->state->header_time = (ngx_msec_t) -1;

    // 连接到上游服务器
    rc = ngx_event_connect_peer(&u->peer);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream connect: %i", rc);

    // 处理连接错误
    if (rc == NGX_ERROR) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    u->state->peer = u->peer.name;

    // 处理没有可用上游服务器的情况
    if (rc == NGX_BUSY) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "no live upstreams");
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_NOLIVE);
        return;
    }

    // 处理连接被拒绝的情况
    if (rc == NGX_DECLINED) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    /* rc == NGX_OK || rc == NGX_AGAIN || rc == NGX_DONE */

    c = u->peer.connection;

    c->requests++;

    c->data = r;

    // 设置事件处理函数
    c->write->handler = ngx_http_upstream_handler;
    c->read->handler = ngx_http_upstream_handler;

    u->write_event_handler = ngx_http_upstream_send_request_handler;
    u->read_event_handler = ngx_http_upstream_process_header;

    // 配置sendfile
    c->sendfile &= r->connection->sendfile;
    u->output.sendfile = c->sendfile;

    // 配置TCP_NOPUSH
    if (r->connection->tcp_nopush == NGX_TCP_NOPUSH_DISABLED) {
        c->tcp_nopush = NGX_TCP_NOPUSH_DISABLED;
    }

    // 创建连接池
    if (c->pool == NULL) {
        c->pool = ngx_create_pool(128, r->connection->log);
        if (c->pool == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    // 设置日志
    c->log = r->connection->log;
    c->pool->log = c->log;
    c->read->log = c->log;
    c->write->log = c->log;

    // 初始化或重新初始化输出链和链写入器上下文
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    u->writer.out = NULL;
    u->writer.last = &u->writer.out;
    u->writer.connection = c;
    u->writer.limit = clcf->sendfile_max_chunk;

    // 如果请求已发送，重新初始化
    if (u->request_sent) {
        if (ngx_http_upstream_reinit(r, u) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    // 处理请求体
    if (r->request_body
        && r->request_body->buf
        && r->request_body->temp_file
        && r == r->main)
    {
        u->output.free = ngx_alloc_chain_link(r->pool);
        if (u->output.free == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        u->output.free->buf = r->request_body->buf;
        u->output.free->next = NULL;
        u->output.allocated = 1;

        r->request_body->buf->pos = r->request_body->buf->start;
        r->request_body->buf->last = r->request_body->buf->start;
        r->request_body->buf->tag = u->output.tag;
    }

    // 重置请求状态
    u->request_sent = 0;
    u->request_body_sent = 0;
    u->request_body_blocked = 0;

    // 如果连接未完成，设置超时
    if (rc == NGX_AGAIN) {
        ngx_add_timer(c->write, u->conf->connect_timeout);
        return;
    }

#if (NGX_HTTP_SSL)

    // 处理SSL连接
    if (u->ssl && c->ssl == NULL) {
        ngx_http_upstream_ssl_init_connection(r, u, c);
        return;
    }

#endif

    // 发送请求
    ngx_http_upstream_send_request(r, u, 1);
}


#if (NGX_HTTP_SSL)

static void
ngx_http_upstream_ssl_init_connection(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_connection_t *c)
{
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *clcf;

    // 测试连接是否成功
    if (ngx_http_upstream_test_connect(c) != NGX_OK) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    // 创建SSL连接
    if (ngx_ssl_create_connection(u->conf->ssl, c,
                                  NGX_SSL_BUFFER|NGX_SSL_CLIENT)
        != NGX_OK)
    {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    // 设置SSL服务器名称和验证
    if (u->conf->ssl_server_name || u->conf->ssl_verify) {
        if (ngx_http_upstream_ssl_name(r, u, c) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    // 设置SSL证书
    if (u->conf->ssl_certificate
        && u->conf->ssl_certificate->value.len
        && (u->conf->ssl_certificate->lengths
            || u->conf->ssl_certificate_key->lengths))
    {
        if (ngx_http_upstream_ssl_certificate(r, u, c) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    // 处理SSL会话重用
    if (u->conf->ssl_session_reuse) {
        c->ssl->save_session = ngx_http_upstream_ssl_save_session;

        if (u->peer.set_session(&u->peer, u->peer.data) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        // 处理TCP_NODELAY选项
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->tcp_nodelay && ngx_tcp_nodelay(c) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    // 设置日志动作
    r->connection->log->action = "SSL handshaking to upstream";

    // 执行SSL握手
    rc = ngx_ssl_handshake(c);

    if (rc == NGX_AGAIN) {
        // 如果握手未完成，设置超时并注册回调
        if (!c->write->timer_set) {
            ngx_add_timer(c->write, u->conf->connect_timeout);
        }

        c->ssl->handler = ngx_http_upstream_ssl_handshake_handler;
        return;
    }

    // 握手完成，处理结果
    ngx_http_upstream_ssl_handshake(r, u, c);
}


static void
ngx_http_upstream_ssl_handshake_handler(ngx_connection_t *c)
{
    ngx_http_request_t   *r;
    ngx_http_upstream_t  *u;

    r = c->data;

    u = r->upstream;
    c = r->connection;

    // 设置日志请求
    ngx_http_set_log_request(c->log, r);

    // 记录调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream ssl handshake: \"%V?%V\"",
                   &r->uri, &r->args);

    // 处理SSL握手结果
    ngx_http_upstream_ssl_handshake(r, u, u->peer.connection);

    // 运行已发布的请求
    ngx_http_run_posted_requests(c);
}


static void
ngx_http_upstream_ssl_handshake(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_connection_t *c)
{
    long  rc;

    if (c->ssl->handshaked) {
        // SSL握手已完成

        if (u->conf->ssl_verify) {
            // 验证SSL证书
            rc = SSL_get_verify_result(c->ssl->connection);

            if (rc != X509_V_OK) {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "upstream SSL certificate verify error: (%l:%s)",
                              rc, X509_verify_cert_error_string(rc));
                goto failed;
            }

            // 检查主机名
            if (ngx_ssl_check_host(c, &u->ssl_name) != NGX_OK) {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "upstream SSL certificate does not match \"%V\"",
                              &u->ssl_name);
                goto failed;
            }
        }

        // 设置sendfile标志
        if (!c->ssl->sendfile) {
            c->sendfile = 0;
            u->output.sendfile = 0;
        }

        // 设置读写处理函数
        c->write->handler = ngx_http_upstream_handler;
        c->read->handler = ngx_http_upstream_handler;

        // 发送请求
        ngx_http_upstream_send_request(r, u, 1);

        return;
    }

    // 检查是否超时
    if (c->write->timedout) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_TIMEOUT);
        return;
    }

failed:
    // 处理失败情况
    ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
}


static void
ngx_http_upstream_ssl_save_session(ngx_connection_t *c)
{
    ngx_http_request_t   *r;
    ngx_http_upstream_t  *u;

    // 如果连接处于空闲状态，直接返回
    if (c->idle) {
        return;
    }

    // 获取请求和上游对象
    r = c->data;
    u = r->upstream;
    c = r->connection;

    // 设置日志请求
    ngx_http_set_log_request(c->log, r);

    // 保存会话
    u->peer.save_session(&u->peer, u->peer.data);
}


static ngx_int_t
ngx_http_upstream_ssl_name(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_connection_t *c)
{
    u_char     *p, *last;
    ngx_str_t   name;

    // 获取SSL名称
    if (u->conf->ssl_name) {
        if (ngx_http_complex_value(r, u->conf->ssl_name, &name) != NGX_OK) {
            return NGX_ERROR;
        }
    } else {
        name = u->ssl_name;
    }

    // 如果名称为空，直接跳到done标签
    if (name.len == 0) {
        goto done;
    }

    // 处理可能包含端口的SSL名称

    p = name.data;
    last = name.data + name.len;

    // 处理IPv6地址
    if (*p == '[') {
        p = ngx_strlchr(p, last, ']');

        if (p == NULL) {
            p = name.data;
        }
    }

    // 查找并去除端口号
    p = ngx_strlchr(p, last, ':');

    if (p != NULL) {
        name.len = p - name.data;
    }

    // 如果不需要设置服务器名称，直接跳到done标签
    if (!u->conf->ssl_server_name) {
        goto done;
    }

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME

    // 根据RFC 6066，不允许使用字面IPv4和IPv6地址
    if (name.len == 0 || *name.data == '[') {
        goto done;
    }

    if (ngx_inet_addr(name.data, name.len) != INADDR_NONE) {
        goto done;
    }

    // 为SSL_set_tlsext_host_name()准备一个以null结尾的字符串

    p = ngx_pnalloc(r->pool, name.len + 1);
    if (p == NULL) {
        return NGX_ERROR;
    }

    (void) ngx_cpystrn(p, name.data, name.len + 1);

    name.data = p;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "upstream SSL server name: \"%s\"", name.data);

    // 设置SSL服务器名称
    if (SSL_set_tlsext_host_name(c->ssl->connection,
                                 (char *) name.data)
        == 0)
    {
        ngx_ssl_error(NGX_LOG_ERR, r->connection->log, 0,
                      "SSL_set_tlsext_host_name(\"%s\") failed", name.data);
        return NGX_ERROR;
    }

#endif

done:

    // 保存SSL名称
    u->ssl_name = name;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_ssl_certificate(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_connection_t *c)
{
    ngx_str_t  cert, key;

    // 获取SSL证书
    if (ngx_http_complex_value(r, u->conf->ssl_certificate, &cert)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream ssl cert: \"%s\"", cert.data);

    // 如果证书为空，直接返回
    if (*cert.data == '\0') {
        return NGX_OK;
    }

    // 获取SSL密钥
    if (ngx_http_complex_value(r, u->conf->ssl_certificate_key, &key)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream ssl key: \"%s\"", key.data);

    // 设置SSL连接的证书和密钥
    if (ngx_ssl_connection_certificate(c, r->pool, &cert, &key,
                                       u->conf->ssl_passwords)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}

#endif


static ngx_int_t
ngx_http_upstream_reinit(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    off_t         file_pos;
    ngx_chain_t  *cl;

    // 重新初始化请求
    if (u->reinit_request(r) != NGX_OK) {
        return NGX_ERROR;
    }

    // 重置一些标志位
    u->keepalive = 0;
    u->upgrade = 0;
    u->error = 0;

    // 清空并重新初始化上游响应头结构
    ngx_memzero(&u->headers_in, sizeof(ngx_http_upstream_headers_in_t));
    u->headers_in.content_length_n = -1;
    u->headers_in.last_modified_time = -1;

    // 初始化响应头列表
    if (ngx_list_init(&u->headers_in.headers, r->pool, 8,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 初始化响应尾部列表
    if (ngx_list_init(&u->headers_in.trailers, r->pool, 2,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    /* 重新初始化请求缓冲链 */

    file_pos = 0;

    for (cl = u->request_bufs; cl; cl = cl->next) {
        cl->buf->pos = cl->buf->start;

        /* 最多只有一个文件 */

        if (cl->buf->in_file) {
            cl->buf->file_pos = file_pos;
            file_pos = cl->buf->file_last;
        }
    }

    /* 重新初始化子请求的ngx_output_chain()上下文 */

    if (r->request_body && r->request_body->temp_file
        && r != r->main && u->output.buf)
    {
        u->output.free = ngx_alloc_chain_link(r->pool);
        if (u->output.free == NULL) {
            return NGX_ERROR;
        }

        u->output.free->buf = u->output.buf;
        u->output.free->next = NULL;

        u->output.buf->pos = u->output.buf->start;
        u->output.buf->last = u->output.buf->start;
    }

    // 重置输出缓冲区
    u->output.buf = NULL;
    u->output.in = NULL;
    u->output.busy = NULL;

    /* 重新初始化u->buffer */

    u->buffer.pos = u->buffer.start;

#if (NGX_HTTP_CACHE)

    if (r->cache) {
        u->buffer.pos += r->cache->header_start;
    }

#endif

    u->buffer.last = u->buffer.pos;

    return NGX_OK;
}


static void
ngx_http_upstream_send_request(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_uint_t do_write)
{
    ngx_int_t          rc;
    ngx_connection_t  *c;

    c = u->peer.connection;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream send request");

    // 记录连接时间
    if (u->state->connect_time == (ngx_msec_t) -1) {
        u->state->connect_time = ngx_current_msec - u->start_time;
    }

    // 测试连接是否成功
    if (!u->request_sent && ngx_http_upstream_test_connect(c) != NGX_OK) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    c->log->action = "sending request to upstream";

    // 发送请求体
    rc = ngx_http_upstream_send_request_body(r, u, do_write);

    if (rc == NGX_ERROR) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        ngx_http_upstream_finalize_request(r, u, rc);
        return;
    }

    if (rc == NGX_AGAIN) {
        // 处理写事件未就绪的情况
        if (!c->write->ready || u->request_body_blocked) {
            ngx_add_timer(c->write, u->conf->send_timeout);

        } else if (c->write->timer_set) {
            ngx_del_timer(c->write);
        }

        if (ngx_handle_write_event(c->write, u->conf->send_lowat) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        // 处理TCP NOPUSH
        if (c->write->ready && c->tcp_nopush == NGX_TCP_NOPUSH_SET) {
            if (ngx_tcp_push(c->fd) == -1) {
                ngx_log_error(NGX_LOG_CRIT, c->log, ngx_socket_errno,
                              ngx_tcp_push_n " failed");
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            c->tcp_nopush = NGX_TCP_NOPUSH_UNSET;
        }

        if (c->read->ready) {
            ngx_post_event(c->read, &ngx_posted_events);
        }

        return;
    }

    /* rc == NGX_OK */

    // 请求发送成功，清理定时器
    if (c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    // 处理TCP NOPUSH
    if (c->tcp_nopush == NGX_TCP_NOPUSH_SET) {
        if (ngx_tcp_push(c->fd) == -1) {
            ngx_log_error(NGX_LOG_CRIT, c->log, ngx_socket_errno,
                          ngx_tcp_push_n " failed");
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        c->tcp_nopush = NGX_TCP_NOPUSH_UNSET;
    }

    // 设置写事件处理函数
    if (!u->conf->preserve_output) {
        u->write_event_handler = ngx_http_upstream_dummy_handler;
    }

    if (ngx_handle_write_event(c->write, 0) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    // 处理请求体发送完成后的操作
    if (!u->request_body_sent) {
        u->request_body_sent = 1;

        if (u->header_sent) {
            return;
        }

        ngx_add_timer(c->read, u->conf->read_timeout);

        if (c->read->ready) {
            ngx_http_upstream_process_header(r, u);
            return;
        }
    }
}


static ngx_int_t
ngx_http_upstream_send_request_body(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_uint_t do_write)
{
    ngx_int_t                  rc;
    ngx_chain_t               *out, *cl, *ln;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream send request body");

    // 如果请求体不是无缓冲的
    if (!r->request_body_no_buffering) {

        // 处理缓冲的请求体

        if (!u->request_sent) {
            u->request_sent = 1;
            out = u->request_bufs;
        } else {
            out = NULL;
        }

        // 输出请求体链
        rc = ngx_output_chain(&u->output, out);

        if (rc == NGX_AGAIN) {
            u->request_body_blocked = 1;
        } else {
            u->request_body_blocked = 0;
        }

        return rc;
    }

    // 处理无缓冲的请求体
    if (!u->request_sent) {
        u->request_sent = 1;
        out = u->request_bufs;

        // 如果请求体有缓冲区，将其添加到输出链的末尾
        if (r->request_body->bufs) {
            for (cl = out; cl->next; cl = cl->next) { /* void */ }
            cl->next = r->request_body->bufs;
            r->request_body->bufs = NULL;
        }

        c = u->peer.connection;
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        // 设置TCP_NODELAY选项
        if (clcf->tcp_nodelay && ngx_tcp_nodelay(c) != NGX_OK) {
            return NGX_ERROR;
        }

        // 设置读事件处理函数
        r->read_event_handler = ngx_http_upstream_read_request_handler;

    } else {
        out = NULL;
    }

    for ( ;; ) {

        if (do_write) {
            // 输出请求体链
            rc = ngx_output_chain(&u->output, out);

            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            // 释放已发送的缓冲区
            while (out) {
                ln = out;
                out = out->next;
                ngx_free_chain(r->pool, ln);
            }

            if (rc == NGX_AGAIN) {
                u->request_body_blocked = 1;
            } else {
                u->request_body_blocked = 0;
            }

            if (rc == NGX_OK && !r->reading_body) {
                break;
            }
        }

        if (r->reading_body) {
            // 读取客户端请求体

            rc = ngx_http_read_unbuffered_request_body(r);

            if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                return rc;
            }

            out = r->request_body->bufs;
            r->request_body->bufs = NULL;
        }

        // 如果没有数据要发送，则停止

        if (out == NULL) {
            rc = NGX_AGAIN;
            break;
        }

        do_write = 1;
    }

    // 设置读事件处理函数
    if (!r->reading_body) {
        if (!u->store && !r->post_action && !u->conf->ignore_client_abort) {
            r->read_event_handler =
                                  ngx_http_upstream_rd_check_broken_connection;
        }
    }

    return rc;
}


static void
ngx_http_upstream_send_request_handler(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_connection_t  *c;

    c = u->peer.connection;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream send request handler");

    // 检查写超时
    if (c->write->timedout) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_TIMEOUT);
        return;
    }

#if (NGX_HTTP_SSL)

    // 初始化SSL连接
    if (u->ssl && c->ssl == NULL) {
        ngx_http_upstream_ssl_init_connection(r, u, c);
        return;
    }

#endif

    // 如果头部已发送且不需要保留输出
    if (u->header_sent && !u->conf->preserve_output) {
        u->write_event_handler = ngx_http_upstream_dummy_handler;

        (void) ngx_handle_write_event(c->write, 0);

        return;
    }

    // 发送请求
    ngx_http_upstream_send_request(r, u, 1);
}


static void
ngx_http_upstream_read_request_handler(ngx_http_request_t *r)
{
    ngx_connection_t     *c;
    ngx_http_upstream_t  *u;

    c = r->connection;
    u = r->upstream;

    // 记录调试日志
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream read request handler");

    // 检查读取超时
    if (c->read->timedout) {
        c->timedout = 1;
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    // 发送请求到上游服务器
    ngx_http_upstream_send_request(r, u, 0);
}


static void
ngx_http_upstream_process_header(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ssize_t            n;
    ngx_int_t          rc;
    ngx_connection_t  *c;

    c = u->peer.connection;

    // 记录调试日志
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process header");

    // 设置日志动作
    c->log->action = "reading response header from upstream";

    // 检查读取超时
    if (c->read->timedout) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_TIMEOUT);
        return;
    }

    // 检查连接是否成功建立
    if (!u->request_sent && ngx_http_upstream_test_connect(c) != NGX_OK) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    // 初始化缓冲区
    if (u->buffer.start == NULL) {
        u->buffer.start = ngx_palloc(r->pool, u->conf->buffer_size);
        if (u->buffer.start == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        u->buffer.pos = u->buffer.start;
        u->buffer.last = u->buffer.start;
        u->buffer.end = u->buffer.start + u->conf->buffer_size;
        u->buffer.temporary = 1;

        u->buffer.tag = u->output.tag;

        // 初始化头部列表
        if (ngx_list_init(&u->headers_in.headers, r->pool, 8,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        // 初始化尾部列表
        if (ngx_list_init(&u->headers_in.trailers, r->pool, 2,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

#if (NGX_HTTP_CACHE)

        // 处理缓存情况
        if (r->cache) {
            u->buffer.pos += r->cache->header_start;
            u->buffer.last = u->buffer.pos;
        }
#endif
    }

    for ( ;; ) {

        // 从上游服务器接收数据
        n = c->recv(c, u->buffer.last, u->buffer.end - u->buffer.last);

        if (n == NGX_AGAIN) {
#if 0
            ngx_add_timer(rev, u->read_timeout);
#endif

            // 处理读事件
            if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            return;
        }

        // 检查连接是否提前关闭
        if (n == 0) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0,
                          "upstream prematurely closed connection");
        }

        // 处理接收错误
        if (n == NGX_ERROR || n == 0) {
            ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
            return;
        }

        // 更新接收字节数
        u->state->bytes_received += n;

        u->buffer.last += n;

#if 0
        u->valid_header_in = 0;

        u->peer.cached = 0;
#endif

        // 处理接收到的头部
        rc = u->process_header(r);

        if (rc == NGX_AGAIN) {

            // 检查缓冲区是否已满
            if (u->buffer.last == u->buffer.end) {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "upstream sent too big header");

                ngx_http_upstream_next(r, u,
                                       NGX_HTTP_UPSTREAM_FT_INVALID_HEADER);
                return;
            }

            continue;
        }

        break;
    }

    // 处理无效头部
    if (rc == NGX_HTTP_UPSTREAM_INVALID_HEADER) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_INVALID_HEADER);
        return;
    }

    // 处理错误
    if (rc == NGX_ERROR) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    /* rc == NGX_OK */

    // 记录头部处理时间
    u->state->header_time = ngx_current_msec - u->start_time;

    // 处理特殊响应状态码
    if (u->headers_in.status_n >= NGX_HTTP_SPECIAL_RESPONSE) {

        if (ngx_http_upstream_test_next(r, u) == NGX_OK) {
            return;
        }

        if (ngx_http_upstream_intercept_errors(r, u) == NGX_OK) {
            return;
        }
    }

    // 处理响应头部
    if (ngx_http_upstream_process_headers(r, u) != NGX_OK) {
        return;
    }

    // 发送响应给客户端
    ngx_http_upstream_send_response(r, u);
}


static ngx_int_t
ngx_http_upstream_test_next(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_msec_t                 timeout;
    ngx_uint_t                 status, mask;
    ngx_http_upstream_next_t  *un;

    // 获取上游服务器返回的状态码
    status = u->headers_in.status_n;

    // 遍历预定义的错误状态码列表
    for (un = ngx_http_upstream_next_errors; un->status; un++) {

        // 如果当前状态码不匹配，继续下一个
        if (status != un->status) {
            continue;
        }

        // 获取配置的下一个上游服务器超时时间
        timeout = u->conf->next_upstream_timeout;

        // 根据请求方法和是否已发送请求确定掩码
        if (u->request_sent
            && (r->method & (NGX_HTTP_POST|NGX_HTTP_LOCK|NGX_HTTP_PATCH)))
        {
            mask = un->mask | NGX_HTTP_UPSTREAM_FT_NON_IDEMPOTENT;

        } else {
            mask = un->mask;
        }

        // 检查是否需要尝试下一个上游服务器
        if (u->peer.tries > 1
            && ((u->conf->next_upstream & mask) == mask)
            && !(u->request_sent && r->request_body_no_buffering)
            && !(timeout && ngx_current_msec - u->peer.start_time >= timeout))
        {
            ngx_http_upstream_next(r, u, un->mask);
            return NGX_OK;
        }

#if (NGX_HTTP_CACHE)

        // 处理缓存过期的情况
        if (u->cache_status == NGX_HTTP_CACHE_EXPIRED
            && (u->conf->cache_use_stale & un->mask))
        {
            ngx_int_t  rc;

            // 重新初始化请求
            rc = u->reinit_request(r);

            if (rc != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, rc);
                return NGX_OK;
            }

            // 将缓存状态设置为过期
            u->cache_status = NGX_HTTP_CACHE_STALE;
            rc = ngx_http_upstream_cache_send(r, u);

            if (rc == NGX_DONE) {
                return NGX_OK;
            }

            if (rc == NGX_HTTP_UPSTREAM_INVALID_HEADER) {
                rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            ngx_http_upstream_finalize_request(r, u, rc);
            return NGX_OK;
        }

#endif

        break;
    }

#if (NGX_HTTP_CACHE)

    // 处理304 Not Modified响应
    if (status == NGX_HTTP_NOT_MODIFIED
        && u->cache_status == NGX_HTTP_CACHE_EXPIRED
        && u->conf->cache_revalidate)
    {
        time_t     now, valid, updating, error;
        ngx_int_t  rc;

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http upstream not modified");

        now = ngx_time();

        // 保存当前缓存的有效期信息
        valid = r->cache->valid_sec;
        updating = r->cache->updating_sec;
        error = r->cache->error_sec;

        // 重新初始化请求
        rc = u->reinit_request(r);

        if (rc != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u, rc);
            return NGX_OK;
        }

        // 更新缓存状态并发送缓存
        u->cache_status = NGX_HTTP_CACHE_REVALIDATED;
        rc = ngx_http_upstream_cache_send(r, u);

        if (rc == NGX_DONE) {
            return NGX_OK;
        }

        if (rc == NGX_HTTP_UPSTREAM_INVALID_HEADER) {
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        // 更新缓存有效期
        if (valid == 0) {
            valid = r->cache->valid_sec;
            updating = r->cache->updating_sec;
            error = r->cache->error_sec;
        }

        if (valid == 0) {
            valid = ngx_http_file_cache_valid(u->conf->cache_valid,
                                              u->headers_in.status_n);
            if (valid) {
                valid = now + valid;
            }
        }

        if (valid) {
            r->cache->valid_sec = valid;
            r->cache->updating_sec = updating;
            r->cache->error_sec = error;

            r->cache->date = now;

            ngx_http_file_cache_update_header(r);
        }

        ngx_http_upstream_finalize_request(r, u, rc);
        return NGX_OK;
    }

#endif

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_upstream_intercept_errors(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_int_t                  status;
    ngx_uint_t                 i;
    ngx_table_elt_t           *h, *ho, **ph;
    ngx_http_err_page_t       *err_page;
    ngx_http_core_loc_conf_t  *clcf;

    // 获取上游响应状态码
    status = u->headers_in.status_n;

    // 如果状态码是404且配置了拦截404，则直接结束请求
    if (status == NGX_HTTP_NOT_FOUND && u->conf->intercept_404) {
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_NOT_FOUND);
        return NGX_OK;
    }

    // 如果没有配置拦截错误，则返回NGX_DECLINED
    if (!u->conf->intercept_errors) {
        return NGX_DECLINED;
    }

    // 获取核心模块的位置配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 如果没有配置错误页面，则返回NGX_DECLINED
    if (clcf->error_pages == NULL) {
        return NGX_DECLINED;
    }

    // 遍历错误页面配置
    err_page = clcf->error_pages->elts;
    for (i = 0; i < clcf->error_pages->nelts; i++) {

        // 如果找到匹配的状态码
        if (err_page[i].status == status) {

            // 处理401未授权的特殊情况
            if (status == NGX_HTTP_UNAUTHORIZED
                && u->headers_in.www_authenticate)
            {
                h = u->headers_in.www_authenticate;
                ph = &r->headers_out.www_authenticate;

                // 复制WWW-Authenticate头部
                while (h) {
                    ho = ngx_list_push(&r->headers_out.headers);

                    if (ho == NULL) {
                        ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                        return NGX_OK;
                    }

                    *ho = *h;
                    ho->next = NULL;

                    *ph = ho;
                    ph = &ho->next;

                    h = h->next;
                }
            }

#if (NGX_HTTP_CACHE)

            // 处理缓存相关逻辑
            if (r->cache) {

                // 检查是否可缓存
                if (u->headers_in.no_cache || u->headers_in.expired) {
                    u->cacheable = 0;
                }

                if (u->cacheable) {
                    time_t  valid;

                    valid = r->cache->valid_sec;

                    // 计算缓存有效期
                    if (valid == 0) {
                        valid = ngx_http_file_cache_valid(u->conf->cache_valid,
                                                          status);
                        if (valid) {
                            r->cache->valid_sec = ngx_time() + valid;
                        }
                    }

                    // 设置缓存错误状态
                    if (valid) {
                        r->cache->error = status;
                    }
                }

                // 释放临时文件
                ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
            }
#endif
            // 结束上游请求处理
            ngx_http_upstream_finalize_request(r, u, status);

            return NGX_OK;
        }
    }

    // 如果没有找到匹配的错误页面，返回NGX_DECLINED
    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_upstream_test_connect(ngx_connection_t *c)
{
    int        err;
    socklen_t  len;

#if (NGX_HAVE_KQUEUE)

    // 对于kqueue事件模型的特殊处理
    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT)  {
        if (c->write->pending_eof || c->read->pending_eof) {
            if (c->write->pending_eof) {
                err = c->write->kq_errno;

            } else {
                err = c->read->kq_errno;
            }

            c->log->action = "connecting to upstream";
            (void) ngx_connection_error(c, err,
                                    "kevent() reported that connect() failed");
            return NGX_ERROR;
        }

    } else
#endif
    {
        err = 0;
        len = sizeof(int);

        /*
         * BSDs和Linux返回0并在err中设置待处理错误
         * Solaris返回-1并设置errno
         */

        // 获取套接字错误状态
        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
        {
            err = ngx_socket_errno;
        }

        // 如果有错误，记录日志并返回NGX_ERROR
        if (err) {
            c->log->action = "connecting to upstream";
            (void) ngx_connection_error(c, err, "connect() failed");
            return NGX_ERROR;
        }
    }

    // 连接测试成功
    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_headers(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_str_t                       uri, args;
    ngx_uint_t                      i, flags;
    ngx_list_part_t                *part;
    ngx_table_elt_t                *h;
    ngx_http_upstream_header_t     *hh;
    ngx_http_upstream_main_conf_t  *umcf;

    // 获取upstream模块的主配置
    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

    // 检查是否可缓存
    if (u->headers_in.no_cache || u->headers_in.expired) {
        u->cacheable = 0;
    }

    // 处理X-Accel-Redirect头
    if (u->headers_in.x_accel_redirect
        && !(u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_REDIRECT))
    {
        ngx_http_upstream_finalize_request(r, u, NGX_DECLINED);

        // 遍历所有头部
        part = &u->headers_in.headers.part;
        h = part->elts;

        for (i = 0; /* void */; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                h = part->elts;
                i = 0;
            }

            if (h[i].hash == 0) {
                continue;
            }

            // 查找并处理特定头部
            hh = ngx_hash_find(&umcf->headers_in_hash, h[i].hash,
                               h[i].lowcase_key, h[i].key.len);

            if (hh && hh->redirect) {
                if (hh->copy_handler(r, &h[i], hh->conf) != NGX_OK) {
                    ngx_http_finalize_request(r,
                                              NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return NGX_DONE;
                }
            }
        }

        // 处理X-Accel-Redirect的值
        uri = u->headers_in.x_accel_redirect->value;

        if (uri.data[0] == '@') {
            ngx_http_named_location(r, &uri);

        } else {
            ngx_str_null(&args);
            flags = NGX_HTTP_LOG_UNSAFE;

            if (ngx_http_parse_unsafe_uri(r, &uri, &args, &flags) != NGX_OK) {
                ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
                return NGX_DONE;
            }

            // 修改请求方法为GET（如果不是HEAD）
            if (r->method != NGX_HTTP_HEAD) {
                r->method = NGX_HTTP_GET;
                r->method_name = ngx_http_core_get_method;
            }

            ngx_http_internal_redirect(r, &uri, &args);
        }

        ngx_http_finalize_request(r, NGX_DONE);
        return NGX_DONE;
    }

    // 处理其他头部
    part = &u->headers_in.headers.part;
    h = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if (h[i].hash == 0) {
            continue;
        }

        // 检查是否需要隐藏该头部
        if (ngx_hash_find(&u->conf->hide_headers_hash, h[i].hash,
                          h[i].lowcase_key, h[i].key.len))
        {
            continue;
        }

        // 查找并处理特定头部
        hh = ngx_hash_find(&umcf->headers_in_hash, h[i].hash,
                           h[i].lowcase_key, h[i].key.len);

        if (hh) {
            if (hh->copy_handler(r, &h[i], hh->conf) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_DONE;
            }

            continue;
        }

        // 复制其他头部
        if (ngx_http_upstream_copy_header_line(r, &h[i], 0) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_DONE;
        }
    }

    // 处理Server头
    if (r->headers_out.server && r->headers_out.server->value.data == NULL) {
        r->headers_out.server->hash = 0;
    }

    // 处理Date头
    if (r->headers_out.date && r->headers_out.date->value.data == NULL) {
        r->headers_out.date->hash = 0;
    }

    // 设置响应状态码和状态行
    r->headers_out.status = u->headers_in.status_n;
    r->headers_out.status_line = u->headers_in.status_line;

    // 设置Content-Length
    r->headers_out.content_length_n = u->headers_in.content_length_n;

    // 禁用Not Modified响应（如果不可缓存）
    r->disable_not_modified = !u->cacheable;

    // 处理范围请求
    if (u->conf->force_ranges) {
        r->allow_ranges = 1;
        r->single_range = 1;

#if (NGX_HTTP_CACHE)
        if (r->cached) {
            r->single_range = 0;
        }
#endif
    }

    // 设置响应长度为未知
    u->length = -1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_trailers(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_uint_t        i;
    ngx_list_part_t  *part;
    ngx_table_elt_t  *h, *ho;

    // 如果不需要传递尾部字段，直接返回成功
    if (!u->conf->pass_trailers) {
        return NGX_OK;
    }

    // 获取上游响应的尾部字段列表
    part = &u->headers_in.trailers.part;
    h = part->elts;

    // 遍历所有尾部字段
    for (i = 0; /* void */; i++) {

        // 如果当前部分的所有元素都已处理完，移动到下一个部分
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        // 检查当前尾部字段是否在隐藏头部列表中
        if (ngx_hash_find(&u->conf->hide_headers_hash, h[i].hash,
                          h[i].lowcase_key, h[i].key.len))
        {
            continue;  // 如果在隐藏列表中，跳过此字段
        }

        // 将尾部字段添加到输出尾部字段列表中
        ho = ngx_list_push(&r->headers_out.trailers);
        if (ho == NULL) {
            return NGX_ERROR;  // 内存分配失败
        }

        // 复制尾部字段
        *ho = h[i];
    }

    return NGX_OK;
}


static void
ngx_http_upstream_send_response(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ssize_t                    n;
    ngx_int_t                  rc;
    ngx_event_pipe_t          *p;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    // 发送响应头
    rc = ngx_http_send_header(r);

    // 如果发送头部失败或需要执行post_action，则结束请求
    if (rc == NGX_ERROR || rc > NGX_OK || r->post_action) {
        ngx_http_upstream_finalize_request(r, u, rc);
        return;
    }

    // 标记头部已发送
    u->header_sent = 1;

    // 如果是升级请求，则处理升级
    if (u->upgrade) {

#if (NGX_HTTP_CACHE)

        // 如果启用了缓存，释放临时文件
        if (r->cache) {
            ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
        }

#endif

        ngx_http_upstream_upgrade(r, u);
        return;
    }

    c = r->connection;

    // 如果只需要发送头部
    if (r->header_only) {

        // 如果不缓冲且不需要存储，则结束请求
        if (!u->buffering) {
            ngx_http_upstream_finalize_request(r, u, rc);
            return;
        }

        if (!u->cacheable && !u->store) {
            ngx_http_upstream_finalize_request(r, u, rc);
            return;
        }

        // 标记下游错误
        u->pipe->downstream_error = 1;
    }

    // 清理请求体临时文件
    if (r->request_body && r->request_body->temp_file
        && r == r->main && !r->preserve_body
        && !u->conf->preserve_output)
    {
        ngx_pool_run_cleanup_file(r->pool, r->request_body->temp_file->file.fd);
        r->request_body->temp_file->file.fd = NGX_INVALID_FILE;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 如果不缓冲
    if (!u->buffering) {

#if (NGX_HTTP_CACHE)

        // 如果启用了缓存，释放临时文件
        if (r->cache) {
            ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
        }

#endif

        // 设置非缓冲过滤器
        if (u->input_filter == NULL) {
            u->input_filter_init = ngx_http_upstream_non_buffered_filter_init;
            u->input_filter = ngx_http_upstream_non_buffered_filter;
            u->input_filter_ctx = r;
        }

        // 设置非缓冲处理函数
        u->read_event_handler = ngx_http_upstream_process_non_buffered_upstream;
        r->write_event_handler =
                             ngx_http_upstream_process_non_buffered_downstream;

        // 禁用限速
        r->limit_rate = 0;
        r->limit_rate_set = 1;

        // 初始化输入过滤器
        if (u->input_filter_init(u->input_filter_ctx) == NGX_ERROR) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }

        // 设置TCP_NODELAY
        if (clcf->tcp_nodelay && ngx_tcp_nodelay(c) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }

        n = u->buffer.last - u->buffer.pos;

        // 如果缓冲区有数据，处理数据
        if (n) {
            u->buffer.last = u->buffer.pos;

            u->state->response_length += n;

            if (u->input_filter(u->input_filter_ctx, n) == NGX_ERROR) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

            ngx_http_upstream_process_non_buffered_downstream(r);

        } else {
            // 如果缓冲区没有数据，重置缓冲区并刷新
            u->buffer.pos = u->buffer.start;
            u->buffer.last = u->buffer.start;

            if (ngx_http_send_special(r, NGX_HTTP_FLUSH) == NGX_ERROR) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

            ngx_http_upstream_process_non_buffered_upstream(r, u);
        }

        return;
    }

    // TODO: 预分配event_pipe缓冲区，检查"Content-Length"

#if (NGX_HTTP_CACHE)

    // 处理缓存相关逻辑
    if (r->cache && r->cache->file.fd != NGX_INVALID_FILE) {
        ngx_pool_run_cleanup_file(r->pool, r->cache->file.fd);
        r->cache->file.fd = NGX_INVALID_FILE;
    }

    switch (ngx_http_test_predicates(r, u->conf->no_cache)) {

    case NGX_ERROR:
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;

    case NGX_DECLINED:
        u->cacheable = 0;
        break;

    default: /* NGX_OK */

        if (u->cache_status == NGX_HTTP_CACHE_BYPASS) {

            // 如果之前绕过缓存，现在创建缓存
            if (ngx_http_file_cache_create(r) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }
        }

        break;
    }

    if (u->cacheable) {
        time_t  now, valid;

        now = ngx_time();

        valid = r->cache->valid_sec;

        if (valid == 0) {
            valid = ngx_http_file_cache_valid(u->conf->cache_valid,
                                              u->headers_in.status_n);
            if (valid) {
                r->cache->valid_sec = now + valid;
            }
        }

        if (valid) {
            r->cache->date = now;
            r->cache->body_start = (u_short) (u->buffer.pos - u->buffer.start);

            if (u->headers_in.status_n == NGX_HTTP_OK
                || u->headers_in.status_n == NGX_HTTP_PARTIAL_CONTENT)
            {
                r->cache->last_modified = u->headers_in.last_modified_time;

                if (u->headers_in.etag) {
                    r->cache->etag = u->headers_in.etag->value;

                } else {
                    ngx_str_null(&r->cache->etag);
                }

            } else {
                r->cache->last_modified = -1;
                ngx_str_null(&r->cache->etag);
            }

            if (ngx_http_file_cache_set_header(r, u->buffer.start) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

        } else {
            u->cacheable = 0;
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http cacheable: %d", u->cacheable);

    if (u->cacheable == 0 && r->cache) {
        ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
    }

    if (r->header_only && !u->cacheable && !u->store) {
        ngx_http_upstream_finalize_request(r, u, 0);
        return;
    }

#endif

    // 设置管道处理
    p = u->pipe;

    p->output_filter = ngx_http_upstream_output_filter;
    p->output_ctx = r;
    p->tag = u->output.tag;
    p->bufs = u->conf->bufs;
    p->busy_size = u->conf->busy_buffers_size;
    p->upstream = u->peer.connection;
    p->downstream = c;
    p->pool = r->pool;
    p->log = c->log;
    p->limit_rate = ngx_http_complex_value_size(r, u->conf->limit_rate, 0);
    p->start_sec = ngx_time();

    p->cacheable = u->cacheable || u->store;

    // 分配临时文件结构
    p->temp_file = ngx_pcalloc(r->pool, sizeof(ngx_temp_file_t));
    if (p->temp_file == NULL) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    p->temp_file->file.fd = NGX_INVALID_FILE;
    p->temp_file->file.log = c->log;
    p->temp_file->path = u->conf->temp_path;
    p->temp_file->pool = r->pool;

    if (p->cacheable) {
        p->temp_file->persistent = 1;

#if (NGX_HTTP_CACHE)
        if (r->cache && !r->cache->file_cache->use_temp_path) {
            p->temp_file->path = r->cache->file_cache->path;
            p->temp_file->file.name = r->cache->file.name;
        }
#endif

    } else {
        p->temp_file->log_level = NGX_LOG_WARN;
        p->temp_file->warn = "an upstream response is buffered "
                             "to a temporary file";
    }

    p->max_temp_file_size = u->conf->max_temp_file_size;
    p->temp_file_write_size = u->conf->temp_file_write_size;

#if (NGX_THREADS)
    // 设置线程处理
    if (clcf->aio == NGX_HTTP_AIO_THREADS && clcf->aio_write) {
        p->thread_handler = ngx_http_upstream_thread_handler;
        p->thread_ctx = r;
    }
#endif

    // 分配预读缓冲区
    p->preread_bufs = ngx_alloc_chain_link(r->pool);
    if (p->preread_bufs == NULL) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    p->preread_bufs->buf = &u->buffer;
    p->preread_bufs->next = NULL;
    u->buffer.recycled = 1;

    p->preread_size = u->buffer.last - u->buffer.pos;

    if (u->cacheable) {

        p->buf_to_file = ngx_calloc_buf(r->pool);
        if (p->buf_to_file == NULL) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }

        p->buf_to_file->start = u->buffer.start;
        p->buf_to_file->pos = u->buffer.start;
        p->buf_to_file->last = u->buffer.pos;
        p->buf_to_file->temporary = 1;
    }

    if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
        // IOCP事件可能会破坏影子缓冲区
        p->single_buf = 1;
    }

    // TODO: 如果使用ngx_create_chain_of_bufs()，则p->free_bufs = 0
    p->free_bufs = 1;

    /*
     * event_pipe会执行u->buffer.last += p->preread_size
     * 就像这些字节已经被读取一样
     */
    u->buffer.last = u->buffer.pos;

    if (u->conf->cyclic_temp_file) {

        /*
         * 如果使用循环临时文件，我们需要禁用sendfile()
         * 因为写入新数据可能会干扰使用相同内核文件页的sendfile()
         * （至少在FreeBSD上是这样）
         */

        p->cyclic_temp_file = 1;
        c->sendfile = 0;

    } else {
        p->cyclic_temp_file = 0;
    }

    p->read_timeout = u->conf->read_timeout;
    p->send_timeout = clcf->send_timeout;
    p->send_lowat = clcf->send_lowat;

    p->length = -1;

    if (u->input_filter_init
        && u->input_filter_init(p->input_ctx) != NGX_OK)
    {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    u->read_event_handler = ngx_http_upstream_process_upstream;
    r->write_event_handler = ngx_http_upstream_process_downstream;

    ngx_http_upstream_process_upstream(r, u);
}


static void
ngx_http_upstream_upgrade(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    /* TODO: 防止在未请求或不可能的情况下进行升级 */

    // 检查是否为主请求，子请求不允许升级连接
    if (r != r->main) {
        ngx_log_error(NGX_LOG_ERR, c->log, 0,
                      "connection upgrade in subrequest");
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    // 禁用keepalive
    r->keepalive = 0;
    c->log->action = "proxying upgraded connection";

    // 设置升级后的事件处理函数
    u->read_event_handler = ngx_http_upstream_upgraded_read_upstream;
    u->write_event_handler = ngx_http_upstream_upgraded_write_upstream;
    r->read_event_handler = ngx_http_upstream_upgraded_read_downstream;
    r->write_event_handler = ngx_http_upstream_upgraded_write_downstream;

    // 如果配置了TCP_NODELAY，则为客户端和上游连接启用它
    if (clcf->tcp_nodelay) {

        if (ngx_tcp_nodelay(c) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }

        if (ngx_tcp_nodelay(u->peer.connection) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }

    // 发送特殊的flush标志
    if (ngx_http_send_special(r, NGX_HTTP_FLUSH) == NGX_ERROR) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    // 检查上游连接是否已准备好读取或缓冲区中是否有数据
    if (u->peer.connection->read->ready
        || u->buffer.pos != u->buffer.last)
    {
        ngx_post_event(c->read, &ngx_posted_events);
        ngx_http_upstream_process_upgraded(r, 1, 1);
        return;
    }

    // 开始处理升级后的连接
    ngx_http_upstream_process_upgraded(r, 0, 1);
}


// 处理从下游（客户端）读取数据的事件
static void
ngx_http_upstream_upgraded_read_downstream(ngx_http_request_t *r)
{
    ngx_http_upstream_process_upgraded(r, 0, 0);
}


// 处理向下游（客户端）写入数据的事件
static void
ngx_http_upstream_upgraded_write_downstream(ngx_http_request_t *r)
{
    ngx_http_upstream_process_upgraded(r, 1, 1);
}


// 处理从上游读取数据的事件
static void
ngx_http_upstream_upgraded_read_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_http_upstream_process_upgraded(r, 1, 0);
}


// 处理向上游写入数据的事件
static void
ngx_http_upstream_upgraded_write_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_http_upstream_process_upgraded(r, 0, 1);
}


// 处理升级后的连接
static void
ngx_http_upstream_process_upgraded(ngx_http_request_t *r,
    ngx_uint_t from_upstream, ngx_uint_t do_write)
{
    size_t                     size;
    ssize_t                    n;
    ngx_buf_t                 *b;
    ngx_uint_t                 flags;
    ngx_connection_t          *c, *downstream, *upstream, *dst, *src;
    ngx_http_upstream_t       *u;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    u = r->upstream;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process upgraded, fu:%ui", from_upstream);

    downstream = c;
    upstream = u->peer.connection;

    // 检查下游连接是否超时
    if (downstream->write->timedout) {
        c->timedout = 1;
        ngx_connection_error(c, NGX_ETIMEDOUT, "client timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    // 检查上游连接是否超时
    if (upstream->read->timedout || upstream->write->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "upstream timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_GATEWAY_TIME_OUT);
        return;
    }

    // 根据数据流向设置源和目标连接以及缓冲区
    if (from_upstream) {
        src = upstream;
        dst = downstream;
        b = &u->buffer;

    } else {
        src = downstream;
        dst = upstream;
        b = &u->from_client;

        // 如果请求头中还有未处理的数据，优先处理
        if (r->header_in->last > r->header_in->pos) {
            b = r->header_in;
            b->end = b->last;
            do_write = 1;
        }

        // 如果缓冲区未初始化，则进行初始化
        if (b->start == NULL) {
            b->start = ngx_palloc(r->pool, u->conf->buffer_size);
            if (b->start == NULL) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

            b->pos = b->start;
            b->last = b->start;
            b->end = b->start + u->conf->buffer_size;
            b->temporary = 1;
            b->tag = u->output.tag;
        }
    }

    // 主循环：处理数据传输
    for ( ;; ) {

        // 写数据到目标连接
        if (do_write) {

            size = b->last - b->pos;

            if (size && dst->write->ready) {

                n = dst->send(dst, b->pos, size);

                if (n == NGX_ERROR) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                    return;
                }

                if (n > 0) {
                    b->pos += n;

                    // 如果缓冲区已空，重置指针
                    if (b->pos == b->last) {
                        b->pos = b->start;
                        b->last = b->start;
                    }
                }
            }
        }

        // 从源连接读取数据
        size = b->end - b->last;

        if (size && src->read->ready) {

            n = src->recv(src, b->last, size);

            if (n == NGX_AGAIN || n == 0) {
                break;
            }

            if (n > 0) {
                do_write = 1;
                b->last += n;

                // 更新接收到的字节数统计
                if (from_upstream) {
                    u->state->bytes_received += n;
                }

                continue;
            }

            if (n == NGX_ERROR) {
                src->read->eof = 1;
            }
        }

        break;
    }

    // 检查是否所有数据都已传输完毕
    if ((upstream->read->eof && u->buffer.pos == u->buffer.last)
        || (downstream->read->eof && u->from_client.pos == u->from_client.last)
        || (downstream->read->eof && upstream->read->eof))
    {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http upstream upgraded done");
        ngx_http_upstream_finalize_request(r, u, 0);
        return;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 处理上游连接的写事件
    if (ngx_handle_write_event(upstream->write, u->conf->send_lowat)
        != NGX_OK)
    {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    // 设置或删除上游写超时定时器
    if (upstream->write->active && !upstream->write->ready) {
        ngx_add_timer(upstream->write, u->conf->send_timeout);

    } else if (upstream->write->timer_set) {
        ngx_del_timer(upstream->write);
    }

    // 处理上游连接的读事件
    if (upstream->read->eof || upstream->read->error) {
        flags = NGX_CLOSE_EVENT;

    } else {
        flags = 0;
    }

    if (ngx_handle_read_event(upstream->read, flags) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    // 设置或删除上游读超时定时器
    if (upstream->read->active && !upstream->read->ready) {
        ngx_add_timer(upstream->read, u->conf->read_timeout);

    } else if (upstream->read->timer_set) {
        ngx_del_timer(upstream->read);
    }

    // 处理下游连接的写事件
    if (ngx_handle_write_event(downstream->write, clcf->send_lowat)
        != NGX_OK)
    {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    // 处理下游连接的读事件
    if (downstream->read->eof || downstream->read->error) {
        flags = NGX_CLOSE_EVENT;

    } else {
        flags = 0;
    }

    if (ngx_handle_read_event(downstream->read, flags) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    // 设置或删除下游写超时定时器
    if (downstream->write->active && !downstream->write->ready) {
        ngx_add_timer(downstream->write, clcf->send_timeout);

    } else if (downstream->write->timer_set) {
        ngx_del_timer(downstream->write);
    }
}


// 处理非缓冲的下游数据
static void
ngx_http_upstream_process_non_buffered_downstream(ngx_http_request_t *r)
{
    ngx_event_t          *wev;
    ngx_connection_t     *c;
    ngx_http_upstream_t  *u;

    c = r->connection;
    u = r->upstream;
    wev = c->write;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process non buffered downstream");

    c->log->action = "sending to client";

    // 检查下游连接是否超时
    if (wev->timedout) {
        c->timedout = 1;
        ngx_connection_error(c, NGX_ETIMEDOUT, "client timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    // 处理非缓冲的请求
    ngx_http_upstream_process_non_buffered_request(r, 1);
}


// 处理非缓冲的上游数据
static void
ngx_http_upstream_process_non_buffered_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_connection_t  *c;

    c = u->peer.connection;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process non buffered upstream");

    c->log->action = "reading upstream";

    // 检查上游连接是否超时
    if (c->read->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "upstream timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_GATEWAY_TIME_OUT);
        return;
    }

    // 处理非缓冲的请求
    ngx_http_upstream_process_non_buffered_request(r, 0);
}


// 处理非缓冲的请求
static void
ngx_http_upstream_process_non_buffered_request(ngx_http_request_t *r,
    ngx_uint_t do_write)
{
    size_t                     size;
    ssize_t                    n;
    ngx_buf_t                 *b;
    ngx_int_t                  rc;
    ngx_uint_t                 flags;
    ngx_connection_t          *downstream, *upstream;
    ngx_http_upstream_t       *u;
    ngx_http_core_loc_conf_t  *clcf;

    u = r->upstream;
    downstream = r->connection;
    upstream = u->peer.connection;

    b = &u->buffer;

    // 判断是否需要写入数据
    do_write = do_write || u->length == 0;

    for ( ;; ) {

        if (do_write) {

            // 如果有待发送的数据，进行输出过滤
            if (u->out_bufs || u->busy_bufs || downstream->buffered) {
                rc = ngx_http_output_filter(r, u->out_bufs);

                if (rc == NGX_ERROR) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                    return;
                }

                // 更新缓冲链
                ngx_chain_update_chains(r->pool, &u->free_bufs, &u->busy_bufs,
                                        &u->out_bufs, u->output.tag);
            }

            // 如果没有待处理的缓冲区
            if (u->busy_bufs == NULL) {

                // 检查是否已经接收完所有数据
                if (u->length == 0
                    || (upstream->read->eof && u->length == -1))
                {
                    ngx_http_upstream_finalize_request(r, u, 0);
                    return;
                }

                // 检查上游是否过早关闭连接
                if (upstream->read->eof) {
                    ngx_log_error(NGX_LOG_ERR, upstream->log, 0,
                                  "upstream prematurely closed connection");

                    ngx_http_upstream_finalize_request(r, u,
                                                       NGX_HTTP_BAD_GATEWAY);
                    return;
                }

                // 检查上游是否发生错误
                if (upstream->read->error || u->error) {
                    ngx_http_upstream_finalize_request(r, u,
                                                       NGX_HTTP_BAD_GATEWAY);
                    return;
                }

                // 重置缓冲区指针
                b->pos = b->start;
                b->last = b->start;
            }
        }

        // 计算可用缓冲区大小
        size = b->end - b->last;

        // 如果有可用空间且上游数据就绪，则接收数据
        if (size && upstream->read->ready) {

            n = upstream->recv(upstream, b->last, size);

            if (n == NGX_AGAIN) {
                break;
            }

            if (n > 0) {
                // 更新接收字节数统计
                u->state->bytes_received += n;
                u->state->response_length += n;

                // 调用输入过滤器
                if (u->input_filter(u->input_filter_ctx, n) == NGX_ERROR) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                    return;
                }
            }

            do_write = 1;

            continue;
        }

        break;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 处理下游写事件
    if (downstream->data == r) {
        if (ngx_handle_write_event(downstream->write, clcf->send_lowat)
            != NGX_OK)
        {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }

    // 设置或删除下游写超时定时器
    if (downstream->write->active && !downstream->write->ready) {
        ngx_add_timer(downstream->write, clcf->send_timeout);

    } else if (downstream->write->timer_set) {
        ngx_del_timer(downstream->write);
    }

    // 设置上游读事件标志
    if (upstream->read->eof || upstream->read->error) {
        flags = NGX_CLOSE_EVENT;

    } else {
        flags = 0;
    }

    // 处理上游读事件
    if (ngx_handle_read_event(upstream->read, flags) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    // 设置或删除上游读超时定时器
    if (upstream->read->active && !upstream->read->ready) {
        ngx_add_timer(upstream->read, u->conf->read_timeout);

    } else if (upstream->read->timer_set) {
        ngx_del_timer(upstream->read);
    }
}


// 初始化非缓冲过滤器
ngx_int_t
ngx_http_upstream_non_buffered_filter_init(void *data)
{
    return NGX_OK;
}


ngx_int_t
ngx_http_upstream_non_buffered_filter(void *data, ssize_t bytes)
{
    ngx_http_request_t  *r = data;

    ngx_buf_t            *b;
    ngx_chain_t          *cl, **ll;
    ngx_http_upstream_t  *u;

    // 获取上游结构体
    u = r->upstream;

    // 检查上游数据长度是否已经达到预期
    if (u->length == 0) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent more data than specified in "
                      "\"Content-Length\" header");
        return NGX_OK;
    }

    // 遍历输出缓冲链表，找到最后一个节点
    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next;
    }

    // 获取一个新的缓冲区
    cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    // 将新缓冲区添加到输出链表末尾
    *ll = cl;

    // 设置缓冲区标志
    cl->buf->flush = 1;
    cl->buf->memory = 1;

    // 获取上游缓冲区
    b = &u->buffer;

    // 设置新缓冲区的位置和长度
    cl->buf->pos = b->last;
    b->last += bytes;
    cl->buf->last = b->last;
    cl->buf->tag = u->output.tag;

    // 如果长度为-1，表示不限长度，直接返回
    if (u->length == -1) {
        return NGX_OK;
    }

    // 检查接收的数据是否超过预期长度
    if (bytes > u->length) {

        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent more data than specified in "
                      "\"Content-Length\" header");

        // 调整缓冲区长度，丢弃多余数据
        cl->buf->last = cl->buf->pos + u->length;
        u->length = 0;

        return NGX_OK;
    }

    // 更新剩余长度
    u->length -= bytes;

    return NGX_OK;
}


#if (NGX_THREADS)

// 处理上游线程任务的函数
static ngx_int_t
ngx_http_upstream_thread_handler(ngx_thread_task_t *task, ngx_file_t *file)
{
    ngx_str_t                  name;
    ngx_event_pipe_t          *p;
    ngx_connection_t          *c;
    ngx_thread_pool_t         *tp;
    ngx_http_request_t        *r;
    ngx_http_core_loc_conf_t  *clcf;

    r = file->thread_ctx;
    p = r->upstream->pipe;

    // 处理异步I/O情况
    if (r->aio) {
        /*
         * 如果另一个操作已经在运行，则容忍sendfile()调用；
         * 这可能发生在子请求、过滤器多次调用下一个主体过滤器，
         * 或在HTTP/2中由于主连接上的写事件而发生
         */

        c = r->connection;

#if (NGX_HTTP_V2)
        if (r->stream) {
            c = r->stream->connection->connection;
        }
#endif

        if (task == c->sendfile_task) {
            return NGX_OK;
        }
    }

    // 获取线程池配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    tp = clcf->thread_pool;

    // 如果没有配置线程池，尝试获取动态线程池
    if (tp == NULL) {
        if (ngx_http_complex_value(r, clcf->thread_pool_value, &name)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        tp = ngx_thread_pool_get((ngx_cycle_t *) ngx_cycle, &name);

        if (tp == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "thread pool \"%V\" not found", &name);
            return NGX_ERROR;
        }
    }

    // 设置任务事件处理器
    task->event.data = r;
    task->event.handler = ngx_http_upstream_thread_event_handler;

    // 将任务提交到线程池
    if (ngx_thread_task_post(tp, task) != NGX_OK) {
        return NGX_ERROR;
    }

    // 更新请求状态
    r->main->blocked++;
    r->aio = 1;
    p->aio = 1;

    // 设置任务超时定时器
    ngx_add_timer(&task->event, 60000);

    return NGX_OK;
}


// 处理上游线程事件的函数
static void
ngx_http_upstream_thread_event_handler(ngx_event_t *ev)
{
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    r = ev->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream thread: \"%V?%V\"", &r->uri, &r->args);

    // 处理超时情况
    if (ev->timedout) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "thread operation took too long");
        ev->timedout = 0;
        return;
    }

    // 如果定时器还在运行，删除它
    if (ev->timer_set) {
        ngx_del_timer(ev);
    }

    // 更新请求状态
    r->main->blocked--;
    r->aio = 0;

#if (NGX_HTTP_V2)

    if (r->stream) {
        /*
         * 对于HTTP/2，更新写事件以确保处理会到达主连接，
         * 以处理线程中的sendfile()
         */

        c->write->ready = 1;
        c->write->active = 0;
    }

#endif

    // 处理请求完成或终止的情况
    if (r->done || r->main->terminated) {
        /*
         * 如果子请求已经完成（这可能发生在处理程序用于线程中的sendfile()时），
         * 或者请求被终止，则触发连接事件处理程序
         */

        c->write->handler(c->write);

    } else {
        // 继续处理请求
        r->write_event_handler(r);
        ngx_http_run_posted_requests(c);
    }
}

#endif


// 上游输出过滤器函数
static ngx_int_t
ngx_http_upstream_output_filter(void *data, ngx_chain_t *chain)
{
    ngx_int_t            rc;
    ngx_event_pipe_t    *p;
    ngx_http_request_t  *r;

    r = data;
    p = r->upstream->pipe;

    // 调用HTTP输出过滤器
    rc = ngx_http_output_filter(r, chain);

    // 更新异步I/O标志
    p->aio = r->aio;

    return rc;
}


// 处理下游（客户端）的函数
static void
ngx_http_upstream_process_downstream(ngx_http_request_t *r)
{
    ngx_event_t          *wev;
    ngx_connection_t     *c;
    ngx_event_pipe_t     *p;
    ngx_http_upstream_t  *u;

    c = r->connection;
    u = r->upstream;
    p = u->pipe;
    wev = c->write;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process downstream");

    c->log->action = "sending to client";

#if (NGX_THREADS)
    // 更新异步I/O标志
    p->aio = r->aio;
#endif

    if (wev->timedout) {
        // 处理客户端超时
        p->downstream_error = 1;
        c->timedout = 1;
        ngx_connection_error(c, NGX_ETIMEDOUT, "client timed out");

    } else {

        if (wev->delayed) {
            // 处理延迟的写事件
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http downstream delayed");

            if (ngx_handle_write_event(wev, p->send_lowat) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            }

            return;
        }

        // 处理事件管道
        if (ngx_event_pipe(p, 1) == NGX_ABORT) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }

    // 继续处理请求
    ngx_http_upstream_process_request(r, u);
}


// 处理上游（后端服务器）的函数
static void
ngx_http_upstream_process_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_event_t       *rev;
    ngx_event_pipe_t  *p;
    ngx_connection_t  *c;

    c = u->peer.connection;
    p = u->pipe;
    rev = c->read;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process upstream");

    c->log->action = "reading upstream";

    if (rev->timedout) {
        // 处理上游超时
        p->upstream_error = 1;
        ngx_connection_error(c, NGX_ETIMEDOUT, "upstream timed out");

    } else {

        if (rev->delayed) {
            // 处理延迟的读事件
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http upstream delayed");

            if (ngx_handle_read_event(rev, 0) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            }

            return;
        }

        // 处理事件管道
        if (ngx_event_pipe(p, 0) == NGX_ABORT) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }

    // 继续处理请求
    ngx_http_upstream_process_request(r, u);
}


static void
ngx_http_upstream_process_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_temp_file_t   *tf;
    ngx_event_pipe_t  *p;

    p = u->pipe;

#if (NGX_THREADS)

    if (p->writing && !p->aio) {

        /*
         * 确保在有未完成的异步写入时调用 ngx_event_pipe()
         */

        if (ngx_event_pipe(p, 1) == NGX_ABORT) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }

    if (p->writing) {
        return;
    }

#endif

    if (u->peer.connection) {

        if (u->store) {

            if (p->upstream_eof || p->upstream_done) {

                tf = p->temp_file;

                // 检查是否满足存储条件
                if (u->headers_in.status_n == NGX_HTTP_OK
                    && (p->upstream_done || p->length == -1)
                    && (u->headers_in.content_length_n == -1
                        || u->headers_in.content_length_n == tf->offset))
                {
                    ngx_http_upstream_store(r, u);
                }
            }
        }

#if (NGX_HTTP_CACHE)

        if (u->cacheable) {

            if (p->upstream_done) {
                // 更新文件缓存
                ngx_http_file_cache_update(r, p->temp_file);

            } else if (p->upstream_eof) {

                tf = p->temp_file;

                // 检查是否满足缓存条件
                if (p->length == -1
                    && (u->headers_in.content_length_n == -1
                        || u->headers_in.content_length_n
                           == tf->offset - (off_t) r->cache->body_start))
                {
                    ngx_http_file_cache_update(r, tf);

                } else {
                    ngx_http_file_cache_free(r->cache, tf);
                }

            } else if (p->upstream_error) {
                // 发生错误时释放缓存
                ngx_http_file_cache_free(r->cache, p->temp_file);
            }
        }

#endif

        if (p->upstream_done || p->upstream_eof || p->upstream_error) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http upstream exit: %p", p->out);

            if (p->upstream_done
                || (p->upstream_eof && p->length == -1))
            {
                // 正常完成请求
                ngx_http_upstream_finalize_request(r, u, 0);
                return;
            }

            if (p->upstream_eof) {
                // 上游过早关闭连接
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream prematurely closed connection");
            }

            // 发生错误，返回 502 Bad Gateway
            ngx_http_upstream_finalize_request(r, u, NGX_HTTP_BAD_GATEWAY);
            return;
        }
    }

    if (p->downstream_error) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http upstream downstream error");

        // 如果不可缓存且不存储，并且上游连接存在，则结束请求
        if (!u->cacheable && !u->store && u->peer.connection) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        }
    }
}


static void
ngx_http_upstream_store(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    size_t                  root;
    time_t                  lm;
    ngx_str_t               path;
    ngx_temp_file_t        *tf;
    ngx_ext_rename_file_t   ext;

    tf = u->pipe->temp_file;

    // 如果临时文件的文件描述符无效
    if (tf->file.fd == NGX_INVALID_FILE) {

        // 为空的200响应创建文件

        // 分配内存给临时文件结构
        tf = ngx_pcalloc(r->pool, sizeof(ngx_temp_file_t));
        if (tf == NULL) {
            return;
        }

        // 初始化临时文件结构
        tf->file.fd = NGX_INVALID_FILE;
        tf->file.log = r->connection->log;
        tf->path = u->conf->temp_path;
        tf->pool = r->pool;
        tf->persistent = 1;

        // 创建临时文件
        if (ngx_create_temp_file(&tf->file, tf->path, tf->pool,
                                 tf->persistent, tf->clean, tf->access)
            != NGX_OK)
        {
            return;
        }

        u->pipe->temp_file = tf;
    }

    // 设置文件重命名的扩展选项
    ext.access = u->conf->store_access;
    ext.path_access = u->conf->store_access;
    ext.time = -1;
    ext.create_path = 1;
    ext.delete_file = 1;
    ext.log = r->connection->log;

    // 如果上游响应包含Last-Modified头
    if (u->headers_in.last_modified) {

        // 解析Last-Modified时间
        lm = ngx_parse_http_time(u->headers_in.last_modified->value.data,
                                 u->headers_in.last_modified->value.len);

        if (lm != NGX_ERROR) {
            ext.time = lm;
            ext.fd = tf->file.fd;
        }
    }

    // 如果没有配置存储路径长度
    if (u->conf->store_lengths == NULL) {

        // 将URI映射到文件系统路径
        if (ngx_http_map_uri_to_path(r, &path, &root, 0) == NULL) {
            return;
        }

    } else {
        // 使用配置的脚本生成存储路径
        if (ngx_http_script_run(r, &path, u->conf->store_lengths->elts, 0,
                                u->conf->store_values->elts)
            == NULL)
        {
            return;
        }
    }

    path.len--;

    // 记录日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "upstream stores \"%s\" to \"%s\"",
                   tf->file.name.data, path.data);

    // 重命名临时文件到最终存储路径
    (void) ngx_ext_rename_file(&tf->file.name, &path, &ext);

    // 标记存储完成
    u->store = 0;
}


static void
ngx_http_upstream_dummy_handler(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    // 记录调用了虚拟处理程序的日志
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream dummy handler");
}


static void
ngx_http_upstream_next(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_uint_t ft_type)
{
    ngx_msec_t  timeout;
    ngx_uint_t  status, state;

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http next upstream, %xi", ft_type);

    if (u->peer.sockaddr) {

        // 如果存在连接，更新已发送字节数
        if (u->peer.connection) {
            u->state->bytes_sent = u->peer.connection->sent;
        }

        // 根据错误类型设置peer状态
        if (ft_type == NGX_HTTP_UPSTREAM_FT_HTTP_403
            || ft_type == NGX_HTTP_UPSTREAM_FT_HTTP_404)
        {
            state = NGX_PEER_NEXT;

        } else {
            state = NGX_PEER_FAILED;
        }

        // 释放peer连接
        u->peer.free(&u->peer, u->peer.data, state);
        u->peer.sockaddr = NULL;
    }

    // 如果是超时错误，记录错误日志
    if (ft_type == NGX_HTTP_UPSTREAM_FT_TIMEOUT) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, NGX_ETIMEDOUT,
                      "upstream timed out");
    }

    // 如果是缓存的peer且发生错误，增加尝试次数
    if (u->peer.cached && ft_type == NGX_HTTP_UPSTREAM_FT_ERROR) {
        /* TODO: inform balancer instead */
        u->peer.tries++;
    }

    // 根据错误类型设置HTTP状态码
    switch (ft_type) {

    case NGX_HTTP_UPSTREAM_FT_TIMEOUT:
    case NGX_HTTP_UPSTREAM_FT_HTTP_504:
        status = NGX_HTTP_GATEWAY_TIME_OUT;
        break;

    case NGX_HTTP_UPSTREAM_FT_HTTP_500:
        status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        break;

    case NGX_HTTP_UPSTREAM_FT_HTTP_503:
        status = NGX_HTTP_SERVICE_UNAVAILABLE;
        break;

    case NGX_HTTP_UPSTREAM_FT_HTTP_403:
        status = NGX_HTTP_FORBIDDEN;
        break;

    case NGX_HTTP_UPSTREAM_FT_HTTP_404:
        status = NGX_HTTP_NOT_FOUND;
        break;

    case NGX_HTTP_UPSTREAM_FT_HTTP_429:
        status = NGX_HTTP_TOO_MANY_REQUESTS;
        break;

    /*
     * NGX_HTTP_UPSTREAM_FT_BUSY_LOCK and NGX_HTTP_UPSTREAM_FT_MAX_WAITING
     * never reach here
     */

    default:
        status = NGX_HTTP_BAD_GATEWAY;
    }

    // 如果客户端连接已关闭，结束请求
    if (r->connection->error) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_CLIENT_CLOSED_REQUEST);
        return;
    }

    // 设置upstream状态
    u->state->status = status;

    // 获取next_upstream超时时间
    timeout = u->conf->next_upstream_timeout;

    // 检查是否为非幂等请求
    if (u->request_sent
        && (r->method & (NGX_HTTP_POST|NGX_HTTP_LOCK|NGX_HTTP_PATCH)))
    {
        ft_type |= NGX_HTTP_UPSTREAM_FT_NON_IDEMPOTENT;
    }

    // 检查是否需要尝试下一个upstream
    if (u->peer.tries == 0
        || ((u->conf->next_upstream & ft_type) != ft_type)
        || (u->request_sent && r->request_body_no_buffering)
        || (timeout && ngx_current_msec - u->peer.start_time >= timeout))
    {
#if (NGX_HTTP_CACHE)

        // 处理缓存过期情况
        if (u->cache_status == NGX_HTTP_CACHE_EXPIRED
            && ((u->conf->cache_use_stale & ft_type) || r->cache->stale_error))
        {
            ngx_int_t  rc;

            rc = u->reinit_request(r);

            if (rc != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, rc);
                return;
            }

            u->cache_status = NGX_HTTP_CACHE_STALE;
            rc = ngx_http_upstream_cache_send(r, u);

            if (rc == NGX_DONE) {
                return;
            }

            if (rc == NGX_HTTP_UPSTREAM_INVALID_HEADER) {
                rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            ngx_http_upstream_finalize_request(r, u, rc);
            return;
        }
#endif

        // 结束请求
        ngx_http_upstream_finalize_request(r, u, status);
        return;
    }

    // 关闭当前upstream连接
    if (u->peer.connection) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "close http upstream connection: %d",
                       u->peer.connection->fd);
#if (NGX_HTTP_SSL)

        // 处理SSL连接
        if (u->peer.connection->ssl) {
            u->peer.connection->ssl->no_wait_shutdown = 1;
            u->peer.connection->ssl->no_send_shutdown = 1;

            (void) ngx_ssl_shutdown(u->peer.connection);
        }
#endif

        // 销毁连接池
        if (u->peer.connection->pool) {
            ngx_destroy_pool(u->peer.connection->pool);
        }

        // 关闭连接
        ngx_close_connection(u->peer.connection);
        u->peer.connection = NULL;
    }

    // 尝试连接下一个upstream
    ngx_http_upstream_connect(r, u);
}


static void
ngx_http_upstream_cleanup(void *data)
{
    ngx_http_request_t *r = data;

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "cleanup http upstream request: \"%V\"", &r->uri);

    // 结束upstream请求
    ngx_http_upstream_finalize_request(r, r->upstream, NGX_DONE);
}


static void
ngx_http_upstream_finalize_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_int_t rc)
{
    ngx_uint_t  flush;

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http upstream request: %i", rc);

    // 如果清理函数为空，表示请求已经完成，直接结束请求
    if (u->cleanup == NULL) {
        /* the request was already finalized */
        ngx_http_finalize_request(r, NGX_DONE);
        return;
    }

    // 清除清理函数
    *u->cleanup = NULL;
    u->cleanup = NULL;

    // 如果有解析的域名，释放相关资源
    if (u->resolved && u->resolved->ctx) {
        ngx_resolve_name_done(u->resolved->ctx);
        u->resolved->ctx = NULL;
    }

    // 更新状态信息
    if (u->state && u->state->response_time == (ngx_msec_t) -1) {
        u->state->response_time = ngx_current_msec - u->start_time;

        if (u->pipe && u->pipe->read_length) {
            u->state->bytes_received += u->pipe->read_length
                                        - u->pipe->preread_size;
            u->state->response_length = u->pipe->read_length;
        }

        if (u->peer.connection) {
            u->state->bytes_sent = u->peer.connection->sent;
        }
    }

    // 调用完成请求的回调函数
    u->finalize_request(r, rc);

    // 释放peer相关资源
    if (u->peer.free && u->peer.sockaddr) {
        u->peer.free(&u->peer, u->peer.data, 0);
        u->peer.sockaddr = NULL;
    }

    // 关闭upstream连接
    if (u->peer.connection) {

#if (NGX_HTTP_SSL)

        /* TODO: do not shutdown persistent connection */

        // 处理SSL连接
        if (u->peer.connection->ssl) {

            /*
             * We send the "close notify" shutdown alert to the upstream only
             * and do not wait its "close notify" shutdown alert.
             * It is acceptable according to the TLS standard.
             */

            u->peer.connection->ssl->no_wait_shutdown = 1;

            (void) ngx_ssl_shutdown(u->peer.connection);
        }
#endif

        // 记录关闭连接的日志
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "close http upstream connection: %d",
                       u->peer.connection->fd);

        // 销毁连接池
        if (u->peer.connection->pool) {
            ngx_destroy_pool(u->peer.connection->pool);
        }

        // 关闭连接
        ngx_close_connection(u->peer.connection);
    }

    u->peer.connection = NULL;

    // 清理pipe相关资源
    if (u->pipe) {
        u->pipe->upstream = NULL;
    }

    // 记录临时文件信息
    if (u->pipe && u->pipe->temp_file) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http upstream temp fd: %d",
                       u->pipe->temp_file->file.fd);
    }

    // 删除临时文件
    if (u->store && u->pipe && u->pipe->temp_file
        && u->pipe->temp_file->file.fd != NGX_INVALID_FILE)
    {
        if (ngx_delete_file(u->pipe->temp_file->file.name.data)
            == NGX_FILE_ERROR)
        {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                          ngx_delete_file_n " \"%s\" failed",
                          u->pipe->temp_file->file.name.data);
        }
    }

#if (NGX_HTTP_CACHE)

    // 处理缓存相关逻辑
    if (r->cache) {

        if (u->cacheable) {

            if (rc == NGX_HTTP_BAD_GATEWAY || rc == NGX_HTTP_GATEWAY_TIME_OUT) {
                time_t  valid;

                valid = ngx_http_file_cache_valid(u->conf->cache_valid, rc);

                if (valid) {
                    r->cache->valid_sec = ngx_time() + valid;
                    r->cache->error = rc;
                }
            }
        }

        ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
    }

#endif

    // 设置读事件处理函数
    r->read_event_handler = ngx_http_block_reading;

    // 如果返回NGX_DECLINED，直接返回
    if (rc == NGX_DECLINED) {
        return;
    }

    // 设置日志动作
    r->connection->log->action = "sending to client";

    // 处理特殊情况
    if (!u->header_sent
        || rc == NGX_HTTP_REQUEST_TIME_OUT
        || rc == NGX_HTTP_CLIENT_CLOSED_REQUEST)
    {
        ngx_http_finalize_request(r, rc);
        return;
    }

    flush = 0;

    // 处理特殊响应
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        rc = NGX_ERROR;
        flush = 1;
    }

    // 处理只有头部或下游错误的情况
    if (r->header_only
        || (u->pipe && u->pipe->downstream_error))
    {
        ngx_http_finalize_request(r, rc);
        return;
    }

    // 处理正常响应
    if (rc == 0) {

        // 处理trailers
        if (ngx_http_upstream_process_trailers(r, u) != NGX_OK) {
            ngx_http_finalize_request(r, NGX_ERROR);
            return;
        }

        // 发送最后的特殊响应
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);

    } else if (flush) {
        // 如果需要刷新，禁用keepalive并发送刷新响应
        r->keepalive = 0;
        rc = ngx_http_send_special(r, NGX_HTTP_FLUSH);
    }

    // 最终完成请求
    ngx_http_finalize_request(r, rc);
}


// 处理单个头部行
static ngx_int_t
ngx_http_upstream_process_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  **ph;

    // 计算头部字段在headers_in结构中的偏移量
    ph = (ngx_table_elt_t **) ((char *) &r->upstream->headers_in + offset);

    // 检查是否已存在相同的头部
    if (*ph) {
        // 如果存在，记录警告日志并忽略重复的头部
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent duplicate header line: \"%V: %V\", "
                      "previous value: \"%V: %V\", ignored",
                      &h->key, &h->value,
                      &(*ph)->key, &(*ph)->value);
        h->hash = 0;
        return NGX_OK;
    }

    // 设置新的头部
    *ph = h;
    h->next = NULL;

    return NGX_OK;
}


// 处理多个头部行
static ngx_int_t
ngx_http_upstream_process_multi_header_lines(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_table_elt_t  **ph;

    // 计算头部字段在headers_in结构中的偏移量
    ph = (ngx_table_elt_t **) ((char *) &r->upstream->headers_in + offset);

    // 找到链表的末尾
    while (*ph) { ph = &(*ph)->next; }

    // 添加新的头部到链表末尾
    *ph = h;
    h->next = NULL;

    return NGX_OK;
}


// 忽略头部行
static ngx_int_t
ngx_http_upstream_ignore_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    // 直接返回OK，不做任何处理
    return NGX_OK;
}


// 处理Content-Length头部
static ngx_int_t
ngx_http_upstream_process_content_length(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 检查是否已存在Content-Length头部
    if (u->headers_in.content_length) {
        // 如果存在，记录错误日志并返回无效头部错误
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent duplicate header line: \"%V: %V\", "
                      "previous value: \"%V: %V\"",
                      &h->key, &h->value,
                      &u->headers_in.content_length->key,
                      &u->headers_in.content_length->value);
        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    // 检查是否同时存在Transfer-Encoding头部
    if (u->headers_in.transfer_encoding) {
        // 如果同时存在，记录错误日志并返回无效头部错误
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent \"Content-Length\" and "
                      "\"Transfer-Encoding\" headers at the same time");
        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    // 设置Content-Length头部
    h->next = NULL;
    u->headers_in.content_length = h;
    // 将Content-Length的值转换为整数
    u->headers_in.content_length_n = ngx_atoof(h->value.data, h->value.len);

    // 检查Content-Length的值是否有效
    if (u->headers_in.content_length_n == NGX_ERROR) {
        // 如果无效，记录错误日志并返回无效头部错误
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent invalid \"Content-Length\" header: "
                      "\"%V: %V\"", &h->key, &h->value);
        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    return NGX_OK;
}


// 处理Last-Modified头部
static ngx_int_t
ngx_http_upstream_process_last_modified(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 检查是否已存在Last-Modified头部
    if (u->headers_in.last_modified) {
        // 如果存在，记录警告日志并忽略重复的头部
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent duplicate header line: \"%V: %V\", "
                      "previous value: \"%V: %V\", ignored",
                      &h->key, &h->value,
                      &u->headers_in.last_modified->key,
                      &u->headers_in.last_modified->value);
        h->hash = 0;
        return NGX_OK;
    }

    // 设置Last-Modified头部
    h->next = NULL;
    u->headers_in.last_modified = h;
    // 解析Last-Modified的时间值
    u->headers_in.last_modified_time = ngx_parse_http_time(h->value.data,
                                                           h->value.len);

    return NGX_OK;
}


// 处理Set-Cookie头部
static ngx_int_t
ngx_http_upstream_process_set_cookie(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t      **ph;
    ngx_http_upstream_t   *u;

    u = r->upstream;
    ph = &u->headers_in.set_cookie;

    // 找到Set-Cookie链表的末尾
    while (*ph) { ph = &(*ph)->next; }

    // 添加新的Set-Cookie头部到链表末尾
    *ph = h;
    h->next = NULL;

#if (NGX_HTTP_CACHE)
    // 如果不忽略Set-Cookie头部，则标记响应为不可缓存
    if (!(u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_SET_COOKIE)) {
        u->cacheable = 0;
    }
#endif

    return NGX_OK;
}


// 处理Cache-Control头部
static ngx_int_t
ngx_http_upstream_process_cache_control(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_table_elt_t      **ph;
    ngx_http_upstream_t   *u;

    u = r->upstream;
    ph = &u->headers_in.cache_control;

    // 找到Cache-Control链表的末尾
    while (*ph) { ph = &(*ph)->next; }

    // 添加新的Cache-Control头部到链表末尾
    *ph = h;
    h->next = NULL;

#if (NGX_HTTP_CACHE)
    {
    u_char     *p, *start, *last;
    ngx_int_t   n;

    // 如果配置忽略Cache-Control头部，直接返回
    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_CACHE_CONTROL) {
        return NGX_OK;
    }

    // 如果没有缓存对象，直接返回
    if (r->cache == NULL) {
        return NGX_OK;
    }

    start = h->value.data;
    last = start + h->value.len;

    // 如果已经设置了有效期且存在X-Accel-Expires头部，跳到扩展处理
    if (r->cache->valid_sec != 0 && u->headers_in.x_accel_expires != NULL) {
        goto extensions;
    }

    // 检查是否包含no-cache, no-store或private指令
    if (ngx_strlcasestrn(start, last, (u_char *) "no-cache", 8 - 1) != NULL
        || ngx_strlcasestrn(start, last, (u_char *) "no-store", 8 - 1) != NULL
        || ngx_strlcasestrn(start, last, (u_char *) "private", 7 - 1) != NULL)
    {
        u->headers_in.no_cache = 1;
        return NGX_OK;
    }

    // 查找s-maxage或max-age指令
    p = ngx_strlcasestrn(start, last, (u_char *) "s-maxage=", 9 - 1);
    offset = 9;

    if (p == NULL) {
        p = ngx_strlcasestrn(start, last, (u_char *) "max-age=", 8 - 1);
        offset = 8;
    }

    if (p) {
        n = 0;

        // 解析max-age值
        for (p += offset; p < last; p++) {
            if (*p == ',' || *p == ';' || *p == ' ') {
                break;
            }

            if (*p >= '0' && *p <= '9') {
                n = n * 10 + (*p - '0');
                continue;
            }

            u->cacheable = 0;
            return NGX_OK;
        }

        // 如果max-age为0，标记为不缓存
        if (n == 0) {
            u->headers_in.no_cache = 1;
            return NGX_OK;
        }

        // 设置缓存有效期
        r->cache->valid_sec = ngx_time() + n;
        u->headers_in.expired = 0;
    }

extensions:

    // 处理stale-while-revalidate指令
    p = ngx_strlcasestrn(start, last, (u_char *) "stale-while-revalidate=",
                         23 - 1);

    if (p) {
        n = 0;

        // 解析stale-while-revalidate值
        for (p += 23; p < last; p++) {
            if (*p == ',' || *p == ';' || *p == ' ') {
                break;
            }

            if (*p >= '0' && *p <= '9') {
                n = n * 10 + (*p - '0');
                continue;
            }

            u->cacheable = 0;
            return NGX_OK;
        }

        // 设置更新和错误时的过期时间
        r->cache->updating_sec = n;
        r->cache->error_sec = n;
    }

    // 处理stale-if-error指令
    p = ngx_strlcasestrn(start, last, (u_char *) "stale-if-error=", 15 - 1);

    if (p) {
        n = 0;

        // 解析stale-if-error值
        for (p += 15; p < last; p++) {
            if (*p == ',' || *p == ';' || *p == ' ') {
                break;
            }

            if (*p >= '0' && *p <= '9') {
                n = n * 10 + (*p - '0');
                continue;
            }

            u->cacheable = 0;
            return NGX_OK;
        }

        // 设置错误时的过期时间
        r->cache->error_sec = n;
    }
    }
#endif

    return NGX_OK;
}


// 处理Expires头部
static ngx_int_t
ngx_http_upstream_process_expires(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 检查是否已存在Expires头部
    if (u->headers_in.expires) {
        // 如果存在，记录警告日志并忽略重复的头部
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent duplicate header line: \"%V: %V\", "
                      "previous value: \"%V: %V\", ignored",
                      &h->key, &h->value,
                      &u->headers_in.expires->key,
                      &u->headers_in.expires->value);
        h->hash = 0;
        return NGX_OK;
    }

    // 设置Expires头部
    u->headers_in.expires = h;
    h->next = NULL;

#if (NGX_HTTP_CACHE)
    {
    time_t  expires;

    // 如果配置忽略Expires头部，直接返回
    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_EXPIRES) {
        return NGX_OK;
    }

    // 如果没有缓存或已设置有效期，直接返回
    if (r->cache == NULL) {
        return NGX_OK;
    }

    if (r->cache->valid_sec != 0) {
        return NGX_OK;
    }

    // 解析Expires时间
    expires = ngx_parse_http_time(h->value.data, h->value.len);

    // 如果解析失败或已过期，标记为过期
    if (expires == NGX_ERROR || expires < ngx_time()) {
        u->headers_in.expired = 1;
        return NGX_OK;
    }

    // 设置缓存有效期
    r->cache->valid_sec = expires;
    }
#endif

    return NGX_OK;
}


// 处理X-Accel-Expires头部
static ngx_int_t
ngx_http_upstream_process_accel_expires(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 检查是否已存在X-Accel-Expires头部
    if (u->headers_in.x_accel_expires) {
        // 如果存在，记录警告日志并忽略重复的头部
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent duplicate header line: \"%V: %V\", "
                      "previous value: \"%V: %V\", ignored",
                      &h->key, &h->value,
                      &u->headers_in.x_accel_expires->key,
                      &u->headers_in.x_accel_expires->value);
        h->hash = 0;
        return NGX_OK;
    }

    // 设置X-Accel-Expires头部
    u->headers_in.x_accel_expires = h;
    h->next = NULL;

#if (NGX_HTTP_CACHE)
    {
    u_char     *p;
    size_t      len;
    ngx_int_t   n;

    // 如果配置忽略X-Accel-Expires头部，直接返回
    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_EXPIRES) {
        return NGX_OK;
    }

    // 如果没有缓存，直接返回
    if (r->cache == NULL) {
        return NGX_OK;
    }

    len = h->value.len;
    p = h->value.data;

    // 处理相对时间
    if (p[0] != '@') {
        n = ngx_atoi(p, len);

        switch (n) {
        case 0:
            u->cacheable = 0;
            /* fall through */

        case NGX_ERROR:
            return NGX_OK;

        default:
            // 设置缓存有效期为当前时间加上相对时间
            r->cache->valid_sec = ngx_time() + n;
            u->headers_in.no_cache = 0;
            u->headers_in.expired = 0;
            return NGX_OK;
        }
    }

    // 处理绝对时间
    p++;
    len--;

    n = ngx_atoi(p, len);

    if (n != NGX_ERROR) {
        // 设置缓存有效期为绝对时间
        r->cache->valid_sec = n;
        u->headers_in.no_cache = 0;
        u->headers_in.expired = 0;
    }
    }
#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_limit_rate(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_int_t             n;
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 检查是否已经存在 X-Accel-Limit-Rate 头
    if (u->headers_in.x_accel_limit_rate) {
        // 如果存在，记录警告日志并忽略当前头
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent duplicate header line: \"%V: %V\", "
                      "previous value: \"%V: %V\", ignored",
                      &h->key, &h->value,
                      &u->headers_in.x_accel_limit_rate->key,
                      &u->headers_in.x_accel_limit_rate->value);
        h->hash = 0;
        return NGX_OK;
    }

    // 设置 X-Accel-Limit-Rate 头
    u->headers_in.x_accel_limit_rate = h;
    h->next = NULL;

    // 检查是否忽略 X-Accel-Limit-Rate 头
    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_LIMIT_RATE) {
        return NGX_OK;
    }

    // 解析限速值
    n = ngx_atoi(h->value.data, h->value.len);

    if (n != NGX_ERROR) {
        // 设置请求的限速值
        r->limit_rate = (size_t) n;
        r->limit_rate_set = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_buffering(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    u_char                c0, c1, c2;
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 检查是否忽略 X-Accel-Buffering 头
    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_BUFFERING) {
        return NGX_OK;
    }

    // 检查是否允许更改缓冲设置
    if (u->conf->change_buffering) {

        // 检查头值长度是否为2（可能是"no"）
        if (h->value.len == 2) {
            c0 = ngx_tolower(h->value.data[0]);
            c1 = ngx_tolower(h->value.data[1]);

            if (c0 == 'n' && c1 == 'o') {
                // 如果值为"no"，禁用缓冲
                u->buffering = 0;
            }

        // 检查头值长度是否为3（可能是"yes"）
        } else if (h->value.len == 3) {
            c0 = ngx_tolower(h->value.data[0]);
            c1 = ngx_tolower(h->value.data[1]);
            c2 = ngx_tolower(h->value.data[2]);

            if (c0 == 'y' && c1 == 'e' && c2 == 's') {
                // 如果值为"yes"，启用缓冲
                u->buffering = 1;
            }
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_charset(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 检查是否忽略 X-Accel-Charset 头
    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_CHARSET) {
        return NGX_OK;
    }

    // 设置响应头的字符集覆盖
    r->headers_out.override_charset = &h->value;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_connection(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t      **ph;
    ngx_http_upstream_t   *u;

    u = r->upstream;
    ph = &u->headers_in.connection;

    // 找到连接头链表的末尾
    while (*ph) { ph = &(*ph)->next; }

    // 将新的连接头添加到链表末尾
    *ph = h;
    h->next = NULL;

    // 检查连接头是否包含 "close"
    if (ngx_strlcasestrn(h->value.data, h->value.data + h->value.len,
                         (u_char *) "close", 5 - 1)
        != NULL)
    {
        // 如果包含 "close"，标记连接为关闭状态
        u->headers_in.connection_close = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_transfer_encoding(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    // 检查是否已存在 Transfer-Encoding 头
    if (u->headers_in.transfer_encoding) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent duplicate header line: \"%V: %V\", "
                      "previous value: \"%V: %V\"",
                      &h->key, &h->value,
                      &u->headers_in.transfer_encoding->key,
                      &u->headers_in.transfer_encoding->value);
        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    // 检查是否同时存在 Content-Length 头
    if (u->headers_in.content_length) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent \"Content-Length\" and "
                      "\"Transfer-Encoding\" headers at the same time");
        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    // 设置 Transfer-Encoding 头
    u->headers_in.transfer_encoding = h;
    h->next = NULL;

    // 检查 Transfer-Encoding 是否为 "chunked"
    if (h->value.len == 7
        && ngx_strncasecmp(h->value.data, (u_char *) "chunked", 7) == 0)
    {
        u->headers_in.chunked = 1;

    } else {
        // 如果不是 "chunked"，记录错误并返回无效头错误
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent unknown \"Transfer-Encoding\": \"%V\"",
                      &h->value);
        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_vary(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_table_elt_t      **ph;
    ngx_http_upstream_t   *u;

    u = r->upstream;
    ph = &u->headers_in.vary;

    // 找到 Vary 头链表的末尾
    while (*ph) { ph = &(*ph)->next; }

    // 将新的 Vary 头添加到链表末尾
    *ph = h;
    h->next = NULL;

#if (NGX_HTTP_CACHE)
    {
    u_char     *p;
    size_t      len;
    ngx_str_t   vary;

    // 如果配置了忽略 Vary 头，直接返回
    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_VARY) {
        return NGX_OK;
    }

    // 如果没有缓存或不可缓存，直接返回
    if (r->cache == NULL || !u->cacheable) {
        return NGX_OK;
    }

    // 如果 Vary 头的值为 "*"，将响应标记为不可缓存
    if (h->value.len == 1 && h->value.data[0] == '*') {
        u->cacheable = 0;
        return NGX_OK;
    }

    // 如果有多个 Vary 头
    if (u->headers_in.vary->next) {

        // 计算所有 Vary 头值的总长度
        len = 0;
        for (h = u->headers_in.vary; h; h = h->next) {
            len += h->value.len + 2;  // +2 用于 ", "
        }
        len -= 2;  // 最后一个不需要 ", "

        // 分配内存来存储合并后的 Vary 头值
        p = ngx_pnalloc(r->pool, len);
        if (p == NULL) {
            return NGX_ERROR;
        }

        vary.len = len;
        vary.data = p;

        // 合并所有 Vary 头值
        for (h = u->headers_in.vary; h; h = h->next) {
            p = ngx_copy(p, h->value.data, h->value.len);

            if (h->next == NULL) {
                break;
            }

            *p++ = ','; *p++ = ' ';
        }

    } else {
        // 如果只有一个 Vary 头，直接使用其值
        vary = h->value;
    }

    // 如果 Vary 头值太长，将响应标记为不可缓存
    if (vary.len > NGX_HTTP_CACHE_VARY_LEN) {
        u->cacheable = 0;
    }

    // 设置缓存的 Vary 值
    r->cache->vary = vary;
    }
#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_copy_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  *ho, **ph;

    // 在输出头部列表中添加新的头部
    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    // 复制头部信息
    *ho = *h;

    // 如果提供了偏移量，设置特定头部指针
    if (offset) {
        ph = (ngx_table_elt_t **) ((char *) &r->headers_out + offset);
        *ph = ho;
        ho->next = NULL;
    }

    return NGX_OK;
}


// 复制多个头部行
static ngx_int_t
ngx_http_upstream_copy_multi_header_lines(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_table_elt_t  *ho, **ph;

    // 在输出头部列表中添加新的头部
    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    // 复制头部信息
    *ho = *h;

    // 计算特定头部指针的偏移量
    ph = (ngx_table_elt_t **) ((char *) &r->headers_out + offset);

    // 找到链表的末尾
    while (*ph) { ph = &(*ph)->next; }

    // 将新头部添加到链表末尾
    *ph = ho;
    ho->next = NULL;

    return NGX_OK;
}


// 复制并处理Content-Type头部
static ngx_int_t
ngx_http_upstream_copy_content_type(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    u_char  *p, *last;

    // 设置Content-Type的长度和值
    r->headers_out.content_type_len = h->value.len;
    r->headers_out.content_type = h->value;
    r->headers_out.content_type_lowcase = NULL;

    // 遍历Content-Type值
    for (p = h->value.data; *p; p++) {

        // 查找分号，表示参数开始
        if (*p != ';') {
            continue;
        }

        last = p;

        // 跳过空格
        while (*++p == ' ') { /* void */ }

        // 如果到达字符串末尾，返回
        if (*p == '\0') {
            return NGX_OK;
        }

        // 检查是否为charset参数
        if (ngx_strncasecmp(p, (u_char *) "charset=", 8) != 0) {
            continue;
        }

        p += 8;

        // 设置Content-Type的实际长度（不包括charset）
        r->headers_out.content_type_len = last - h->value.data;

        // 处理引号
        if (*p == '"') {
            p++;
        }

        last = h->value.data + h->value.len;

        if (*(last - 1) == '"') {
            last--;
        }

        // 设置charset的长度和值
        r->headers_out.charset.len = last - p;
        r->headers_out.charset.data = p;

        return NGX_OK;
    }

    return NGX_OK;
}


// 复制Last-Modified头部
static ngx_int_t
ngx_http_upstream_copy_last_modified(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  *ho;

    // 向响应头列表中添加新的头部
    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    // 复制头部信息
    *ho = *h;
    ho->next = NULL;

    // 设置Last-Modified头部
    r->headers_out.last_modified = ho;
    // 设置Last-Modified时间
    r->headers_out.last_modified_time =
                                    r->upstream->headers_in.last_modified_time;

    return NGX_OK;
}


// 重写Location头部
static ngx_int_t
ngx_http_upstream_rewrite_location(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_int_t         rc;
    ngx_table_elt_t  *ho;

    // 向响应头列表中添加新的头部
    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    // 复制头部信息
    *ho = *h;
    ho->next = NULL;

    // 如果存在重写重定向的回调函数
    if (r->upstream->rewrite_redirect) {
        // 调用重写重定向回调函数
        rc = r->upstream->rewrite_redirect(r, ho, 0);

        if (rc == NGX_DECLINED) {
            return NGX_OK;
        }

        if (rc == NGX_OK) {
            // 设置Location头部
            r->headers_out.location = ho;

            // 记录调试日志
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "rewritten location: \"%V\"", &ho->value);
        }

        return rc;
    }

    // 如果Location值不是以'/'开头，设置Location头部
    if (ho->value.data[0] != '/') {
        r->headers_out.location = ho;
    }

    /*
     * 我们在这里不设置r->headers_out.location，以避免在ngx_http_header_filter()中处理相对重定向
     */

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_rewrite_refresh(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    u_char           *p;
    ngx_int_t         rc;
    ngx_table_elt_t  *ho;

    // 向响应头列表中添加新的头部
    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    // 复制头部信息
    *ho = *h;
    ho->next = NULL;

    // 如果存在重写重定向的回调函数
    if (r->upstream->rewrite_redirect) {

        // 查找"url="字符串
        p = ngx_strcasestrn(ho->value.data, "url=", 4 - 1);

        if (p) {
            // 调用重写重定向回调函数，传入url参数的位置
            rc = r->upstream->rewrite_redirect(r, ho, p + 4 - ho->value.data);

        } else {
            // 如果没有找到"url="，直接返回
            return NGX_OK;
        }

        if (rc == NGX_DECLINED) {
            return NGX_OK;
        }

        if (rc == NGX_OK) {
            // 设置Refresh头部
            r->headers_out.refresh = ho;

            // 记录调试日志
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "rewritten refresh: \"%V\"", &ho->value);
        }

        return rc;
    }

    // 如果没有重写重定向回调函数，直接设置Refresh头部
    r->headers_out.refresh = ho;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_rewrite_set_cookie(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_int_t         rc;
    ngx_table_elt_t  *ho;

    // 向响应头列表中添加新的头部
    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    // 复制头部信息
    *ho = *h;
    ho->next = NULL;

    // 如果存在重写cookie的回调函数
    if (r->upstream->rewrite_cookie) {
        // 调用重写cookie回调函数
        rc = r->upstream->rewrite_cookie(r, ho);

        if (rc == NGX_DECLINED) {
            return NGX_OK;
        }

#if (NGX_DEBUG)
        if (rc == NGX_OK) {
            // 记录调试日志
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "rewritten cookie: \"%V\"", &ho->value);
        }
#endif

        return rc;
    }

    return NGX_OK;
}


// 复制允许范围请求的头部信息
static ngx_int_t
ngx_http_upstream_copy_allow_ranges(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_table_elt_t  *ho;

    // 如果强制使用范围请求，直接返回OK
    if (r->upstream->conf->force_ranges) {
        return NGX_OK;
    }

#if (NGX_HTTP_CACHE)

    // 如果是缓存响应，允许范围请求
    if (r->cached) {
        r->allow_ranges = 1;
        return NGX_OK;
    }

    // 如果上游响应可缓存，允许范围请求并设置单一范围
    if (r->upstream->cacheable) {
        r->allow_ranges = 1;
        r->single_range = 1;
        return NGX_OK;
    }

#endif

    // 向响应头列表中添加新的头部
    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    // 复制头部信息
    *ho = *h;
    ho->next = NULL;

    // 设置响应的Accept-Ranges头部
    r->headers_out.accept_ranges = ho;

    return NGX_OK;
}


// 添加上游相关的变量
static ngx_int_t
ngx_http_upstream_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    // 遍历预定义的上游变量列表
    for (v = ngx_http_upstream_vars; v->name.len; v++) {
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


// 处理upstream地址变量
static ngx_int_t
ngx_http_upstream_addr_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                     *p;
    size_t                      len;
    ngx_uint_t                  i;
    ngx_http_upstream_state_t  *state;

    // 设置变量值的属性
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 如果没有upstream状态，将变量标记为未找到
    if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 计算所需的总长度
    len = 0;
    state = r->upstream_states->elts;

    for (i = 0; i < r->upstream_states->nelts; i++) {
        if (state[i].peer) {
            len += state[i].peer->len + 2;
        } else {
            len += 3;
        }
    }

    // 分配内存
    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    i = 0;

    // 构建地址字符串
    for ( ;; ) {
        if (state[i].peer) {
            p = ngx_cpymem(p, state[i].peer->data, state[i].peer->len);
        }

        if (++i == r->upstream_states->nelts) {
            break;
        }

        if (state[i].peer) {
            *p++ = ',';
            *p++ = ' ';
        } else {
            *p++ = ' ';
            *p++ = ':';
            *p++ = ' ';

            if (++i == r->upstream_states->nelts) {
                break;
            }

            continue;
        }
    }

    v->len = p - v->data;

    return NGX_OK;
}


// 处理upstream状态变量
static ngx_int_t
ngx_http_upstream_status_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                     *p;
    size_t                      len;
    ngx_uint_t                  i;
    ngx_http_upstream_state_t  *state;

    // 设置变量值的属性
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 如果没有upstream状态，将变量标记为未找到
    if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 计算所需的总长度
    len = r->upstream_states->nelts * (3 + 2);

    // 分配内存
    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    i = 0;
    state = r->upstream_states->elts;

    // 构建状态字符串
    for ( ;; ) {
        if (state[i].status) {
            p = ngx_sprintf(p, "%ui", state[i].status);
        } else {
            *p++ = '-';
        }

        if (++i == r->upstream_states->nelts) {
            break;
        }

        if (state[i].peer) {
            *p++ = ',';
            *p++ = ' ';
        } else {
            *p++ = ' ';
            *p++ = ':';
            *p++ = ' ';

            if (++i == r->upstream_states->nelts) {
                break;
            }

            continue;
        }
    }

    v->len = p - v->data;

    return NGX_OK;
}


// 处理upstream响应时间变量
static ngx_int_t
ngx_http_upstream_response_time_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                     *p;
    size_t                      len;
    ngx_uint_t                  i;
    ngx_msec_int_t              ms;
    ngx_http_upstream_state_t  *state;

    // 设置变量值的属性
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 如果没有upstream状态，将变量标记为未找到
    if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 计算所需的总长度
    len = r->upstream_states->nelts * (NGX_TIME_T_LEN + 4 + 2);

    // 分配内存
    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    i = 0;
    state = r->upstream_states->elts;

    // 构建响应时间字符串
    for ( ;; ) {

        // 根据data参数选择不同的时间类型
        if (data == 1) {
            ms = state[i].header_time;
        } else if (data == 2) {
            ms = state[i].connect_time;
        } else {
            ms = state[i].response_time;
        }

        if (ms != -1) {
            ms = ngx_max(ms, 0);
            p = ngx_sprintf(p, "%T.%03M", (time_t) ms / 1000, ms % 1000);
        } else {
            *p++ = '-';
        }

        if (++i == r->upstream_states->nelts) {
            break;
        }

        if (state[i].peer) {
            *p++ = ',';
            *p++ = ' ';
        } else {
            *p++ = ' ';
            *p++ = ':';
            *p++ = ' ';

            if (++i == r->upstream_states->nelts) {
                break;
            }

            continue;
        }
    }

    v->len = p - v->data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_response_length_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                     *p;
    size_t                      len;
    ngx_uint_t                  i;
    ngx_http_upstream_state_t  *state;

    // 设置变量的有效性标志
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    // 检查上游状态是否存在
    if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 计算所需的缓冲区长度
    len = r->upstream_states->nelts * (NGX_OFF_T_LEN + 2);

    // 分配内存
    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    i = 0;
    state = r->upstream_states->elts;

    // 遍历所有上游状态
    for ( ;; ) {

        // 根据data参数选择不同的长度类型
        if (data == 1) {
            p = ngx_sprintf(p, "%O", state[i].bytes_received);

        } else if (data == 2) {
            p = ngx_sprintf(p, "%O", state[i].bytes_sent);

        } else {
            p = ngx_sprintf(p, "%O", state[i].response_length);
        }

        if (++i == r->upstream_states->nelts) {
            break;
        }

        // 添加分隔符
        if (state[i].peer) {
            *p++ = ',';
            *p++ = ' ';

        } else {
            *p++ = ' ';
            *p++ = ':';
            *p++ = ' ';

            if (++i == r->upstream_states->nelts) {
                break;
            }

            continue;
        }
    }

    // 计算最终长度
    v->len = p - v->data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_header_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    // 检查上游是否存在
    if (r->upstream == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 查找未知的上游头部
    return ngx_http_variable_unknown_header(r, v, (ngx_str_t *) data,
                                         &r->upstream->headers_in.headers.part,
                                         sizeof("upstream_http_") - 1);
}


static ngx_int_t
ngx_http_upstream_trailer_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    // 检查上游是否存在
    if (r->upstream == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 查找未知的上游尾部
    return ngx_http_variable_unknown_header(r, v, (ngx_str_t *) data,
                                        &r->upstream->headers_in.trailers.part,
                                        sizeof("upstream_trailer_") - 1);
}


static ngx_int_t
ngx_http_upstream_cookie_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_str_t  *name = (ngx_str_t *) data;

    ngx_str_t   cookie, s;

    // 检查上游是否存在
    if (r->upstream == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 提取cookie名称
    s.len = name->len - (sizeof("upstream_cookie_") - 1);
    s.data = name->data + sizeof("upstream_cookie_") - 1;

    // 解析Set-Cookie头部
    if (ngx_http_parse_set_cookie_lines(r, r->upstream->headers_in.set_cookie,
                                        &s, &cookie)
        == NULL)
    {
        v->not_found = 1;
        return NGX_OK;
    }

    // 设置变量值
    v->len = cookie.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = cookie.data;

    return NGX_OK;
}


#if (NGX_HTTP_CACHE)

// 定义一个函数来处理upstream缓存状态变量
static ngx_int_t
ngx_http_upstream_cache_status(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_uint_t  n;

    // 检查upstream是否存在或缓存状态是否有效
    if (r->upstream == NULL || r->upstream->cache_status == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    // 获取缓存状态索引
    n = r->upstream->cache_status - 1;

    // 设置变量值
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->len = ngx_http_cache_status[n].len;
    v->data = ngx_http_cache_status[n].data;

    return NGX_OK;
}


// 定义一个函数来处理upstream缓存最后修改时间变量
static ngx_int_t
ngx_http_upstream_cache_last_modified(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    // 检查是否满足获取最后修改时间的条件
    if (r->upstream == NULL
        || !r->upstream->conf->cache_revalidate
        || r->upstream->cache_status != NGX_HTTP_CACHE_EXPIRED
        || r->cache->last_modified == -1)
    {
        v->not_found = 1;
        return NGX_OK;
    }

    // 分配内存用于存储时间字符串
    p = ngx_pnalloc(r->pool, sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1);
    if (p == NULL) {
        return NGX_ERROR;
    }

    // 格式化时间并设置变量值
    v->len = ngx_http_time(p, r->cache->last_modified) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


// 定义一个函数来处理upstream缓存ETag变量
static ngx_int_t
ngx_http_upstream_cache_etag(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    // 检查是否满足获取ETag的条件
    if (r->upstream == NULL
        || !r->upstream->conf->cache_revalidate
        || r->upstream->cache_status != NGX_HTTP_CACHE_EXPIRED
        || r->cache->etag.len == 0)
    {
        v->not_found = 1;
        return NGX_OK;
    }

    // 设置变量值
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->len = r->cache->etag.len;
    v->data = r->cache->etag.data;

    return NGX_OK;
}

#endif


// 定义一个函数来处理upstream配置指令
static char *
ngx_http_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    char                          *rv;
    void                          *mconf;
    ngx_str_t                     *value;
    ngx_url_t                      u;
    ngx_uint_t                     m;
    ngx_conf_t                     pcf;
    ngx_http_module_t             *module;
    ngx_http_conf_ctx_t           *ctx, *http_ctx;
    ngx_http_upstream_srv_conf_t  *uscf;

    ngx_memzero(&u, sizeof(ngx_url_t));

    // 获取upstream名称
    value = cf->args->elts;
    u.host = value[1];
    u.no_resolve = 1;
    u.no_port = 1;

    // 添加upstream配置
    uscf = ngx_http_upstream_add(cf, &u, NGX_HTTP_UPSTREAM_CREATE
                                         |NGX_HTTP_UPSTREAM_WEIGHT
                                         |NGX_HTTP_UPSTREAM_MAX_CONNS
                                         |NGX_HTTP_UPSTREAM_MAX_FAILS
                                         |NGX_HTTP_UPSTREAM_FAIL_TIMEOUT
                                         |NGX_HTTP_UPSTREAM_DOWN
                                         |NGX_HTTP_UPSTREAM_BACKUP);
    if (uscf == NULL) {
        return NGX_CONF_ERROR;
    }


    // 创建配置上下文
    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    http_ctx = cf->ctx;
    ctx->main_conf = http_ctx->main_conf;

    // 创建upstream的srv_conf

    ctx->srv_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->srv_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    ctx->srv_conf[ngx_http_upstream_module.ctx_index] = uscf;

    uscf->srv_conf = ctx->srv_conf;


    // 创建upstream的loc_conf

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    // 为每个HTTP模块创建配置
    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NGX_HTTP_MODULE) {
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

        if (module->create_loc_conf) {
            mconf = module->create_loc_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->loc_conf[cf->cycle->modules[m]->ctx_index] = mconf;
        }
    }

    // 创建servers数组
    uscf->servers = ngx_array_create(cf->pool, 4,
                                     sizeof(ngx_http_upstream_server_t));
    if (uscf->servers == NULL) {
        return NGX_CONF_ERROR;
    }


    // 解析upstream{}块内的配置

    pcf = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NGX_HTTP_UPS_CONF;

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
ngx_http_upstream_server(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_srv_conf_t  *uscf = conf;

    time_t                       fail_timeout;
    ngx_str_t                   *value, s;
    ngx_url_t                    u;
    ngx_int_t                    weight, max_conns, max_fails;
    ngx_uint_t                   i;
    ngx_http_upstream_server_t  *us;

    // 在upstream服务器数组中添加一个新的服务器
    us = ngx_array_push(uscf->servers);
    if (us == NULL) {
        return NGX_CONF_ERROR;
    }

    // 初始化新的服务器结构
    ngx_memzero(us, sizeof(ngx_http_upstream_server_t));

    value = cf->args->elts;

    // 设置默认值
    weight = 1;
    max_conns = 0;
    max_fails = 1;
    fail_timeout = 10;

    // 解析服务器配置参数
    for (i = 2; i < cf->args->nelts; i++) {

        // 解析权重参数
        if (ngx_strncmp(value[i].data, "weight=", 7) == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_WEIGHT)) {
                goto not_supported;
            }

            weight = ngx_atoi(&value[i].data[7], value[i].len - 7);

            if (weight == NGX_ERROR || weight == 0) {
                goto invalid;
            }

            continue;
        }

        // 解析最大连接数参数
        if (ngx_strncmp(value[i].data, "max_conns=", 10) == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_MAX_CONNS)) {
                goto not_supported;
            }

            max_conns = ngx_atoi(&value[i].data[10], value[i].len - 10);

            if (max_conns == NGX_ERROR) {
                goto invalid;
            }

            continue;
        }

        // 解析最大失败次数参数
        if (ngx_strncmp(value[i].data, "max_fails=", 10) == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_MAX_FAILS)) {
                goto not_supported;
            }

            max_fails = ngx_atoi(&value[i].data[10], value[i].len - 10);

            if (max_fails == NGX_ERROR) {
                goto invalid;
            }

            continue;
        }

        // 解析失败超时时间参数
        if (ngx_strncmp(value[i].data, "fail_timeout=", 13) == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_FAIL_TIMEOUT)) {
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

        // 解析备份服务器标志
        if (ngx_strcmp(value[i].data, "backup") == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_BACKUP)) {
                goto not_supported;
            }

            us->backup = 1;

            continue;
        }

        // 解析下线服务器标志
        if (ngx_strcmp(value[i].data, "down") == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_DOWN)) {
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
    u.default_port = 80;

    if (ngx_parse_url(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "%s in upstream \"%V\"", u.err, &u.url);
        }

        return NGX_CONF_ERROR;
    }

    // 设置服务器信息
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
ngx_http_upstream_srv_conf_t *
ngx_http_upstream_add(ngx_conf_t *cf, ngx_url_t *u, ngx_uint_t flags)
{
    ngx_uint_t                      i;
    ngx_http_upstream_server_t     *us;
    ngx_http_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_http_upstream_main_conf_t  *umcf;

    // 如果不是创建新的上游，则解析URL
    if (!(flags & NGX_HTTP_UPSTREAM_CREATE)) {
        if (ngx_parse_url(cf->pool, u) != NGX_OK) {
            if (u->err) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "%s in upstream \"%V\"", u->err, &u->url);
            }
            return NULL;
        }
    }

    // 获取主配置
    umcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);

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
        if ((flags & NGX_HTTP_UPSTREAM_CREATE)
             && (uscfp[i]->flags & NGX_HTTP_UPSTREAM_CREATE))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "duplicate upstream \"%V\"", &u->host);
            return NULL;
        }

        // 检查端口冲突
        if ((uscfp[i]->flags & NGX_HTTP_UPSTREAM_CREATE) && !u->no_port) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "upstream \"%V\" may not have port %d",
                               &u->host, u->port);
            return NULL;
        }

        if ((flags & NGX_HTTP_UPSTREAM_CREATE) && !uscfp[i]->no_port) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                          "upstream \"%V\" may not have port %d in %s:%ui",
                          &u->host, uscfp[i]->port,
                          uscfp[i]->file_name, uscfp[i]->line);
            return NULL;
        }

        // 检查端口是否匹配
        if (uscfp[i]->port && u->port
            && uscfp[i]->port != u->port)
        {
            continue;
        }

        // 如果是创建新的上游，更新标志
        if (flags & NGX_HTTP_UPSTREAM_CREATE) {
            uscfp[i]->flags = flags;
            uscfp[i]->port = 0;
        }

        return uscfp[i];
    }

    // 创建新的上游服务器配置
    uscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_srv_conf_t));
    if (uscf == NULL) {
        return NULL;
    }

    // 设置配置参数
    uscf->flags = flags;
    uscf->host = u->host;
    uscf->file_name = cf->conf_file->file.name.data;
    uscf->line = cf->conf_file->line;
    uscf->port = u->port;
    uscf->no_port = u->no_port;

    // 如果只有一个地址且有端口或是Unix域套接字
    if (u->naddrs == 1 && (u->port || u->family == AF_UNIX)) {
        // 创建服务器数组
        uscf->servers = ngx_array_create(cf->pool, 1,
                                         sizeof(ngx_http_upstream_server_t));
        if (uscf->servers == NULL) {
            return NULL;
        }

        // 添加服务器
        us = ngx_array_push(uscf->servers);
        if (us == NULL) {
            return NULL;
        }

        ngx_memzero(us, sizeof(ngx_http_upstream_server_t));

        us->addrs = u->addrs;
        us->naddrs = 1;
    }

    // 将新的上游配置添加到主配置中
    uscfp = ngx_array_push(&umcf->upstreams);
    if (uscfp == NULL) {
        return NULL;
    }

    *uscfp = uscf;

    return uscf;
}


// 设置上游绑定配置
char *
ngx_http_upstream_bind_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    char  *p = conf;

    ngx_int_t                           rc;
    ngx_str_t                          *value;
    ngx_http_complex_value_t            cv;
    ngx_http_upstream_local_t         **plocal, *local;
    ngx_http_compile_complex_value_t    ccv;

    plocal = (ngx_http_upstream_local_t **) (p + cmd->offset);

    // 检查是否重复设置
    if (*plocal != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 如果设置为"off"，则禁用绑定
    if (cf->args->nelts == 2 && ngx_strcmp(value[1].data, "off") == 0) {
        *plocal = NULL;
        return NGX_CONF_OK;
    }

    // 编译复杂值
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    // 分配本地配置结构
    local = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_local_t));
    if (local == NULL) {
        return NGX_CONF_ERROR;
    }

    *plocal = local;

    // 如果是复杂值，保存复杂值
    if (cv.lengths) {
        local->value = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
        if (local->value == NULL) {
            return NGX_CONF_ERROR;
        }

        *local->value = cv;

    } else {
        // 否则，解析地址
        local->addr = ngx_palloc(cf->pool, sizeof(ngx_addr_t));
        if (local->addr == NULL) {
            return NGX_CONF_ERROR;
        }

        rc = ngx_parse_addr_port(cf->pool, local->addr, value[1].data,
                                 value[1].len);

        switch (rc) {
        case NGX_OK:
            local->addr->name = value[1];
            break;

        case NGX_DECLINED:
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid address \"%V\"", &value[1]);
            /* fall through */

        default:
            return NGX_CONF_ERROR;
        }
    }

    // 处理额外参数（如透明代理）
    if (cf->args->nelts > 2) {
        if (ngx_strcmp(value[2].data, "transparent") == 0) {
#if (NGX_HAVE_TRANSPARENT_PROXY)
            ngx_core_conf_t  *ccf;

            ccf = (ngx_core_conf_t *) ngx_get_conf(cf->cycle->conf_ctx,
                                                   ngx_core_module);

            ccf->transparent = 1;
            local->transparent = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "transparent proxying is not supported "
                               "on this platform, ignored");
#endif
        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid parameter \"%V\"", &value[2]);
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_upstream_set_local(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_http_upstream_local_t *local)
{
    ngx_int_t    rc;
    ngx_str_t    val;
    ngx_addr_t  *addr;

    // 如果没有本地配置，直接返回
    if (local == NULL) {
        u->peer.local = NULL;
        return NGX_OK;
    }

#if (NGX_HAVE_TRANSPARENT_PROXY)
    // 设置透明代理标志
    u->peer.transparent = local->transparent;
#endif

    // 如果没有动态值，直接使用配置的地址
    if (local->value == NULL) {
        u->peer.local = local->addr;
        return NGX_OK;
    }

    // 解析动态值
    if (ngx_http_complex_value(r, local->value, &val) != NGX_OK) {
        return NGX_ERROR;
    }

    // 如果解析结果为空，直接返回
    if (val.len == 0) {
        return NGX_OK;
    }

    // 分配内存用于存储地址
    addr = ngx_palloc(r->pool, sizeof(ngx_addr_t));
    if (addr == NULL) {
        return NGX_ERROR;
    }

    // 解析地址和端口
    rc = ngx_parse_addr_port(r->pool, addr, val.data, val.len);
    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    // 如果解析失败，记录错误日志并返回
    if (rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "invalid local address \"%V\"", &val);
        return NGX_OK;
    }

    // 设置地址名称和本地地址
    addr->name = val;
    u->peer.local = addr;

    return NGX_OK;
}


char *
ngx_http_upstream_param_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    char  *p = conf;

    ngx_str_t                   *value;
    ngx_array_t                **a;
    ngx_http_upstream_param_t   *param;

    // 获取参数数组的指针
    a = (ngx_array_t **) (p + cmd->offset);

    // 如果数组还未创建，则创建一个新数组
    if (*a == NULL) {
        *a = ngx_array_create(cf->pool, 4, sizeof(ngx_http_upstream_param_t));
        if (*a == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    // 在数组中添加一个新的参数
    param = ngx_array_push(*a);
    if (param == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    // 设置参数的键和值
    param->key = value[1];
    param->value = value[2];
    param->skip_empty = 0;

    // 如果有第四个参数，检查是否为"if_not_empty"
    if (cf->args->nelts == 4) {
        if (ngx_strcmp(value[3].data, "if_not_empty") != 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid parameter \"%V\"", &value[3]);
            return NGX_CONF_ERROR;
        }

        param->skip_empty = 1;
    }

    return NGX_CONF_OK;
}


ngx_int_t
ngx_http_upstream_hide_headers_hash(ngx_conf_t *cf,
    ngx_http_upstream_conf_t *conf, ngx_http_upstream_conf_t *prev,
    ngx_str_t *default_hide_headers, ngx_hash_init_t *hash)
{
    ngx_str_t       *h;
    ngx_uint_t       i, j;
    ngx_array_t      hide_headers;
    ngx_hash_key_t  *hk;

    // 检查是否需要继承前一个配置
    if (conf->hide_headers == NGX_CONF_UNSET_PTR
        && conf->pass_headers == NGX_CONF_UNSET_PTR)
    {
        conf->hide_headers = prev->hide_headers;
        conf->pass_headers = prev->pass_headers;

        conf->hide_headers_hash = prev->hide_headers_hash;

        if (conf->hide_headers_hash.buckets) {
            return NGX_OK;
        }

    } else {
        // 如果未设置，则使用前一个配置的值
        if (conf->hide_headers == NGX_CONF_UNSET_PTR) {
            conf->hide_headers = prev->hide_headers;
        }

        if (conf->pass_headers == NGX_CONF_UNSET_PTR) {
            conf->pass_headers = prev->pass_headers;
        }
    }

    // 初始化临时数组用于存储需要隐藏的头部
    if (ngx_array_init(&hide_headers, cf->temp_pool, 4, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 添加默认需要隐藏的头部
    for (h = default_hide_headers; h->len; h++) {
        hk = ngx_array_push(&hide_headers);
        if (hk == NULL) {
            return NGX_ERROR;
        }

        hk->key = *h;
        hk->key_hash = ngx_hash_key_lc(h->data, h->len);
        hk->value = (void *) 1;
    }

    // 处理配置中指定需要隐藏的头部
    if (conf->hide_headers != NGX_CONF_UNSET_PTR) {

        h = conf->hide_headers->elts;

        for (i = 0; i < conf->hide_headers->nelts; i++) {

            hk = hide_headers.elts;

            for (j = 0; j < hide_headers.nelts; j++) {
                if (ngx_strcasecmp(h[i].data, hk[j].key.data) == 0) {
                    goto exist;
                }
            }

            hk = ngx_array_push(&hide_headers);
            if (hk == NULL) {
                return NGX_ERROR;
            }

            hk->key = h[i];
            hk->key_hash = ngx_hash_key_lc(h[i].data, h[i].len);
            hk->value = (void *) 1;

        exist:

            continue;
        }
    }

    // 处理配置中指定需要传递的头部
    if (conf->pass_headers != NGX_CONF_UNSET_PTR) {

        h = conf->pass_headers->elts;
        hk = hide_headers.elts;

        for (i = 0; i < conf->pass_headers->nelts; i++) {
            for (j = 0; j < hide_headers.nelts; j++) {

                if (hk[j].key.data == NULL) {
                    continue;
                }

                if (ngx_strcasecmp(h[i].data, hk[j].key.data) == 0) {
                    hk[j].key.data = NULL;
                    break;
                }
            }
        }
    }

    // 初始化哈希表
    hash->hash = &conf->hide_headers_hash;
    hash->key = ngx_hash_key_lc;
    hash->pool = cf->pool;
    hash->temp_pool = NULL;

    if (ngx_hash_init(hash, hide_headers.elts, hide_headers.nelts) != NGX_OK) {
        return NGX_ERROR;
    }

    /*
     * 特殊处理以保留"http"部分中的conf->hide_headers_hash，
     * 以便所有服务器都能继承它
     */

    if (prev->hide_headers_hash.buckets == NULL
        && conf->hide_headers == prev->hide_headers
        && conf->pass_headers == prev->pass_headers)
    {
        prev->hide_headers_hash = conf->hide_headers_hash;
    }

    return NGX_OK;
}


// 创建上游主配置结构
static void *
ngx_http_upstream_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_upstream_main_conf_t  *umcf;

    // 分配内存
    umcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_main_conf_t));
    if (umcf == NULL) {
        return NULL;
    }

    // 初始化上游服务器配置数组
    if (ngx_array_init(&umcf->upstreams, cf->pool, 4,
                       sizeof(ngx_http_upstream_srv_conf_t *))
        != NGX_OK)
    {
        return NULL;
    }

    return umcf;
}


// 初始化上游主配置
static char *
ngx_http_upstream_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_upstream_main_conf_t  *umcf = conf;

    ngx_uint_t                      i;
    ngx_array_t                     headers_in;
    ngx_hash_key_t                 *hk;
    ngx_hash_init_t                 hash;
    ngx_http_upstream_init_pt       init;
    ngx_http_upstream_header_t     *header;
    ngx_http_upstream_srv_conf_t  **uscfp;

    uscfp = umcf->upstreams.elts;

    // 初始化每个上游服务器配置
    for (i = 0; i < umcf->upstreams.nelts; i++) {

        // 选择初始化函数
        init = uscfp[i]->peer.init_upstream ? uscfp[i]->peer.init_upstream:
                                            ngx_http_upstream_init_round_robin;

        if (init(cf, uscfp[i]) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }


    /* 初始化upstream_headers_in_hash */

    // 创建临时数组用于存储头部信息
    if (ngx_array_init(&headers_in, cf->temp_pool, 32, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    // 将预定义的上游头部信息添加到数组中
    for (header = ngx_http_upstream_headers_in; header->name.len; header++) {
        hk = ngx_array_push(&headers_in);
        if (hk == NULL) {
            return NGX_CONF_ERROR;
        }

        hk->key = header->name;
        hk->key_hash = ngx_hash_key_lc(header->name.data, header->name.len);
        hk->value = header;
    }

    // 设置哈希表初始化参数
    hash.hash = &umcf->headers_in_hash;
    hash.key = ngx_hash_key_lc;
    hash.max_size = 512;
    hash.bucket_size = ngx_align(64, ngx_cacheline_size);
    hash.name = "upstream_headers_in_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    // 初始化哈希表
    if (ngx_hash_init(&hash, headers_in.elts, headers_in.nelts) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
