
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_UPSTREAM_H_INCLUDED_
#define _NGX_HTTP_UPSTREAM_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>
#include <ngx_event_pipe.h>
#include <ngx_http.h>


/* 定义上游服务器故障类型：错误
   This macro defines the upstream server failure type: ERROR
   值为0x00000002，表示发生了一般性错误 */
#define NGX_HTTP_UPSTREAM_FT_ERROR           0x00000002
/* 定义上游服务器故障类型：超时
   This macro defines the upstream server failure type: TIMEOUT
   值为0x00000004，表示连接或读取上游服务器响应时发生超时 */
#define NGX_HTTP_UPSTREAM_FT_TIMEOUT         0x00000004
/* 定义上游服务器故障类型：无效的响应头
   This macro defines the upstream server failure type: INVALID HEADER
   值为0x00000008，表示上游服务器返回了无效的响应头 */
#define NGX_HTTP_UPSTREAM_FT_INVALID_HEADER  0x00000008
/* 定义上游服务器故障类型：HTTP 500 内部服务器错误
   This macro defines the upstream server failure type: HTTP 500 Internal Server Error
   值为0x00000010，表示上游服务器返回了500状态码 */
#define NGX_HTTP_UPSTREAM_FT_HTTP_500        0x00000010
/* 定义上游服务器故障类型：HTTP 502 Bad Gateway
   This macro defines the upstream server failure type: HTTP 502 Bad Gateway
   值为0x00000020，表示上游服务器返回了502状态码，通常意味着网关或代理服务器从上游服务器收到了无效的响应 */
#define NGX_HTTP_UPSTREAM_FT_HTTP_502        0x00000020
/* 定义上游服务器故障类型：HTTP 503 Service Unavailable
   This macro defines the upstream server failure type: HTTP 503 Service Unavailable
   值为0x00000040，表示上游服务器返回了503状态码，通常意味着服务器暂时无法处理请求（可能由于过载或维护） */
#define NGX_HTTP_UPSTREAM_FT_HTTP_503        0x00000040
/* 定义上游服务器故障类型：HTTP 504 Gateway Timeout
   This macro defines the upstream server failure type: HTTP 504 Gateway Timeout
   值为0x00000080，表示上游服务器返回了504状态码，通常意味着网关或代理服务器在等待上游服务器响应时超时 */
#define NGX_HTTP_UPSTREAM_FT_HTTP_504        0x00000080
/* 定义上游服务器故障类型：HTTP 403 Forbidden
   This macro defines the upstream server failure type: HTTP 403 Forbidden
   值为0x00000100，表示上游服务器返回了403状态码，通常意味着客户端没有访问权限 */
#define NGX_HTTP_UPSTREAM_FT_HTTP_403        0x00000100
/* 定义上游服务器故障类型：HTTP 404 Not Found
   This macro defines the upstream server failure type: HTTP 404 Not Found
   值为0x00000200，表示上游服务器返回了404状态码，通常意味着请求的资源不存在 */
#define NGX_HTTP_UPSTREAM_FT_HTTP_404        0x00000200
/* 定义上游服务器故障类型：HTTP 429 Too Many Requests
   This macro defines the upstream server failure type: HTTP 429 Too Many Requests
   值为0x00000400，表示上游服务器返回了429状态码，通常意味着客户端在给定的时间内发送了太多请求，触发了速率限制 */
#define NGX_HTTP_UPSTREAM_FT_HTTP_429        0x00000400
/* 定义上游服务器故障类型：正在更新
   This macro defines the upstream server failure type: UPDATING
   值为0x00000800，表示上游服务器正在更新或维护中，暂时无法处理请求 */
#define NGX_HTTP_UPSTREAM_FT_UPDATING        0x00000800
/* 定义上游服务器故障类型：忙锁
 * This macro defines the upstream server failure type: BUSY LOCK
 * 值为0x00001000，表示上游服务器因为忙锁（busy lock）而无法处理请求
 * 这通常发生在服务器正在处理其他请求或执行关键操作，暂时无法接受新的连接时
 * 使用此标志可以帮助Nginx识别此类特定的故障情况，并相应地调整负载均衡或故障转移策略
 */
#define NGX_HTTP_UPSTREAM_FT_BUSY_LOCK       0x00001000
/* 定义上游服务器故障类型：最大等待连接数
 * This macro defines the upstream server failure type: MAX WAITING CONNECTIONS
 * 值为0x00002000，表示上游服务器达到了最大等待连接数
 * 这通常发生在服务器已经接受了最大数量的等待处理的连接时
 * 使用此标志可以帮助Nginx识别服务器负载过高的情况，并采取相应的负载均衡或故障转移措施
 */
#define NGX_HTTP_UPSTREAM_FT_MAX_WAITING     0x00002000
/* 定义上游服务器故障类型：非幂等请求
 * This macro defines the upstream server failure type: NON-IDEMPOTENT REQUEST
 * 值为0x00004000，表示对非幂等请求的处理失败
 * 非幂等请求是指多次执行可能会产生不同结果的请求，如POST或PUT
 * 使用此标志可以帮助Nginx在处理非幂等请求失败时采取特殊的故障处理策略
 * 例如，可能不会自动重试这类请求，以避免潜在的数据重复或不一致问题
 */
#define NGX_HTTP_UPSTREAM_FT_NON_IDEMPOTENT  0x00004000
/* 定义上游服务器故障类型：非活动服务器
 * This macro defines the upstream server failure type: NON-LIVE SERVER
 * 值为0x40000000，表示上游服务器被标记为非活动状态
 * 这通常发生在服务器被认为不可用或已从负载均衡池中移除时
 * 使用此标志可以帮助Nginx识别并跳过那些当前不应该接收请求的服务器
 * 这有助于提高整体系统的可靠性和性能，避免将请求发送到可能无法正常处理的服务器
 */
#define NGX_HTTP_UPSTREAM_FT_NOLIVE          0x40000000
/* 定义上游服务器故障类型：关闭故障转移
 * This macro defines the upstream server failure type: FAULT TOLERANCE OFF
 * 值为0x80000000（二进制最高位为1），表示禁用故障转移功能
 * 当设置此标志时，即使检测到上游服务器故障，Nginx也不会尝试故障转移到其他服务器
 * 这可能用于特殊情况下，管理员希望即使在故障情况下也坚持使用特定的上游服务器
 * 使用此标志时需谨慎，因为它可能影响系统的可用性和可靠性
 */
#define NGX_HTTP_UPSTREAM_FT_OFF             0x80000000

/* 定义上游服务器故障类型：HTTP状态码组合
 * This macro defines a combination of upstream server failure types related to HTTP status codes
 * 它包括以下HTTP状态码:
 * - 500 Internal Server Error
 * - 502 Bad Gateway
 * - 503 Service Unavailable
 * - 504 Gateway Timeout
 * - 403 Forbidden
 * - 404 Not Found
 * - 429 Too Many Requests
 * 当上游服务器返回这些状态码时，Nginx会将其视为故障，并可能触发故障转移机制
 */
#define NGX_HTTP_UPSTREAM_FT_STATUS          (NGX_HTTP_UPSTREAM_FT_HTTP_500  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_502  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_503  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_504  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_403  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_404  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_429)

/* 定义上游服务器无效响应头的最大数量
 * This macro defines the maximum number of invalid headers allowed from an upstream server
 * 值为40，表示当接收到的无效响应头数量达到或超过40个时，Nginx将认为上游服务器响应无效
 * 这有助于防止处理格式错误或恶意构造的响应头，提高了系统的安全性和稳定性
 * 当达到这个限制时，Nginx可能会中断与上游服务器的连接，并可能触发错误处理或重试机制
 */
#define NGX_HTTP_UPSTREAM_INVALID_HEADER     40


/* 定义上游服务器忽略X-Accel-Redirect头部的标志
 * This macro defines a flag to ignore the X-Accel-Redirect header from upstream servers
 * 值为0x00000002，表示当设置此标志时，Nginx将忽略上游服务器返回的X-Accel-Redirect头部
 * X-Accel-Redirect通常用于内部重定向，忽略它可以防止上游服务器触发意外的重定向
 * 这个标志可以用于控制Nginx对特定头部的处理行为，增加了配置的灵活性
 */
#define NGX_HTTP_UPSTREAM_IGN_XA_REDIRECT    0x00000002
/* 定义上游服务器忽略X-Accel-Expires头部的标志
 * This macro defines a flag to ignore the X-Accel-Expires header from upstream servers
 * 值为0x00000004，表示当设置此标志时，Nginx将忽略上游服务器返回的X-Accel-Expires头部
 * X-Accel-Expires通常用于控制缓存过期时间，忽略它可以防止上游服务器影响Nginx的缓存行为
 * 这个标志提供了更精细的缓存控制，允许Nginx管理员覆盖上游服务器的缓存设置
 */
#define NGX_HTTP_UPSTREAM_IGN_XA_EXPIRES     0x00000004
/* 定义上游服务器忽略Expires头部的标志
 * This macro defines a flag to ignore the Expires header from upstream servers
 * 值为0x00000008，表示当设置此标志时，Nginx将忽略上游服务器返回的Expires头部
 * Expires头部用于指定资源的过期时间，忽略它可以让Nginx完全控制缓存过期策略
 * 这个标志可以用于覆盖上游服务器的缓存控制，使Nginx能够独立管理缓存生命周期
 */
#define NGX_HTTP_UPSTREAM_IGN_EXPIRES        0x00000008
/* 定义上游服务器忽略Cache-Control头部的标志
 * This macro defines a flag to ignore the Cache-Control header from upstream servers
 * 值为0x00000010，表示当设置此标志时，Nginx将忽略上游服务器返回的Cache-Control头部
 * Cache-Control头部用于控制缓存行为，忽略它可以让Nginx完全控制缓存策略
 * 这个标志允许Nginx管理员覆盖上游服务器的缓存控制指令，提供更灵活的缓存管理
 */
#define NGX_HTTP_UPSTREAM_IGN_CACHE_CONTROL  0x00000010
/* 定义上游服务器忽略Set-Cookie头部的标志
 * This macro defines a flag to ignore the Set-Cookie header from upstream servers
 * 值为0x00000020，表示当设置此标志时，Nginx将忽略上游服务器返回的Set-Cookie头部
 * Set-Cookie头部用于设置客户端cookie，忽略它可以防止上游服务器影响客户端的cookie状态
 * 这个标志可以用于增强安全性，或在某些场景下控制cookie的设置行为
 */
#define NGX_HTTP_UPSTREAM_IGN_SET_COOKIE     0x00000020
/* 定义上游服务器忽略X-Accel-Limit-Rate头部的标志
 * This macro defines a flag to ignore the X-Accel-Limit-Rate header from upstream servers
 * 值为0x00000040，表示当设置此标志时，Nginx将忽略上游服务器返回的X-Accel-Limit-Rate头部
 * X-Accel-Limit-Rate头部通常用于控制响应传输速率，忽略它可以让Nginx完全控制传输速率
 * 这个标志允许Nginx管理员覆盖上游服务器的速率限制设置，提供更灵活的带宽管理
 */
#define NGX_HTTP_UPSTREAM_IGN_XA_LIMIT_RATE  0x00000040
/* 定义上游服务器忽略X-Accel-Buffering头部的标志
 * This macro defines a flag to ignore the X-Accel-Buffering header from upstream servers
 * 值为0x00000080，表示当设置此标志时，Nginx将忽略上游服务器返回的X-Accel-Buffering头部
 * X-Accel-Buffering头部用于控制Nginx是否缓冲上游响应，忽略它可以让Nginx使用自己的缓冲策略
 * 这个标志允许Nginx管理员覆盖上游服务器的缓冲设置，提供更灵活的内存使用和响应处理控制
 */
#define NGX_HTTP_UPSTREAM_IGN_XA_BUFFERING   0x00000080
/* 定义上游服务器忽略X-Accel-Charset头部的标志
 * This macro defines a flag to ignore the X-Accel-Charset header from upstream servers
 * 值为0x00000100，表示当设置此标志时，Nginx将忽略上游服务器返回的X-Accel-Charset头部
 * X-Accel-Charset头部通常用于指定响应的字符编码，忽略它可以让Nginx使用自己的字符编码设置
 * 这个标志允许Nginx管理员覆盖上游服务器的字符编码设置，提供更一致的编码处理
 */
#define NGX_HTTP_UPSTREAM_IGN_XA_CHARSET     0x00000100
/* 定义上游服务器忽略Vary头部的标志
 * This macro defines a flag to ignore the Vary header from upstream servers
 * 值为0x00000200，表示当设置此标志时，Nginx将忽略上游服务器返回的Vary头部
 * Vary头部用于指示缓存如何处理不同的请求头，忽略它可以简化缓存策略
 * 这个标志允许Nginx管理员覆盖上游服务器的Vary设置，提供更一致的缓存行为
 */
#define NGX_HTTP_UPSTREAM_IGN_VARY           0x00000200


/**
 * @brief 定义上游服务器状态结构体
 *
 * 这个结构体用于存储和跟踪上游服务器的各种状态信息，
 * 包括响应状态码、响应时间、连接时间等重要指标。
 * 这些信息对于监控上游服务器性能、进行负载均衡决策
 * 以及故障排除都非常有用。
 */
typedef struct {
    ngx_uint_t                       status;          /* 上游服务器响应的HTTP状态码 */
    ngx_msec_t                       response_time;   /* 从发送请求到接收到完整响应的总时间（毫秒） */
    ngx_msec_t                       connect_time;    /* 与上游服务器建立连接所需的时间（毫秒） */
    ngx_msec_t                       header_time;     /* 接收上游服务器响应头所需的时间（毫秒） */
    ngx_msec_t                       queue_time;      /* 请求在队列中等待处理的时间（毫秒） */
    off_t                            response_length; /* 上游服务器响应体的长度（字节） */
    off_t                            bytes_received;  /* 从上游服务器接收到的总字节数 */
    off_t                            bytes_sent;      /* 发送到上游服务器的总字节数 */

    ngx_str_t                       *peer;            /* 指向当前使用的上游服务器地址的指针 */
} ngx_http_upstream_state_t;


/**
 * @brief 定义上游主配置结构体
 *
 * 这个结构体用于存储Nginx上游模块的主要配置信息。
 * 它包含了处理上游服务器响应头的哈希表和上游服务器配置数组。
 * 这些信息对于管理和配置上游服务器连接至关重要。
 */
typedef struct {
    ngx_hash_t                       headers_in_hash;  /* 用于存储和快速查找上游响应头的哈希表 */
    ngx_array_t                      upstreams;        /* 存储上游服务器配置的动态数组 */
                                             /* ngx_http_upstream_srv_conf_t */
} ngx_http_upstream_main_conf_t;

/**
 * @brief 定义上游服务器配置结构体类型别名
 *
 * 这个类型定义创建了一个别名 ngx_http_upstream_srv_conf_t，
 * 它指向 ngx_http_upstream_srv_conf_s 结构体。
 * 这种前向声明允许在其他地方使用 ngx_http_upstream_srv_conf_t 类型，
 * 而无需完全定义结构体的内容。
 * 这在处理复杂的数据结构和循环依赖时非常有用。
 */
typedef struct ngx_http_upstream_srv_conf_s  ngx_http_upstream_srv_conf_t;

/**
 * @brief 定义上游初始化函数指针类型
 *
 * @param cf Nginx配置结构指针
 * @param us 上游服务器配置结构指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 这个函数指针类型用于定义上游模块的初始化函数。
 * 该函数在Nginx配置阶段被调用，用于初始化上游服务器的配置。
 * 它负责设置上游服务器的各种参数和行为。
 */
typedef ngx_int_t (*ngx_http_upstream_init_pt)(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us);
/**
 * @brief 定义上游初始化对等连接函数指针类型
 *
 * @param r 指向HTTP请求结构体的指针
 * @param us 指向上游服务器配置结构体的指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 这个函数指针类型用于定义初始化上游对等连接的函数。
 * 该函数在每个HTTP请求处理过程中被调用，用于为特定请求初始化与上游服务器的连接。
 * 它负责设置连接参数、选择合适的上游服务器，并准备与上游服务器的通信。
 */
typedef ngx_int_t (*ngx_http_upstream_init_peer_pt)(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);


/**
 * @brief 定义上游对等连接结构体
 *
 * 这个结构体用于存储上游对等连接的相关信息和函数指针。
 * 它包含了初始化上游服务器和对等连接的函数指针，以及一个通用的数据指针。
 * 这个结构体在管理上游连接时起着关键作用，提供了灵活的配置和初始化机制。
 */
typedef struct {
    ngx_http_upstream_init_pt        init_upstream;  /* 初始化上游服务器的函数指针 */
    ngx_http_upstream_init_peer_pt   init;           /* 初始化对等连接的函数指针 */
    void                            *data;           /* 通用数据指针，用于存储额外的配置信息 */
} ngx_http_upstream_peer_t;


/**
 * @brief 定义上游服务器结构体
 *
 * 这个结构体用于存储单个上游服务器的配置信息。
 * 它包含了服务器的地址、权重、连接限制等重要参数，
 * 这些参数用于Nginx的负载均衡和故障转移功能。
 */
typedef struct {
    ngx_str_t                        name;           /* 上游服务器的名称 */
    ngx_addr_t                      *addrs;          /* 上游服务器的地址列表 */
    ngx_uint_t                       naddrs;         /* 地址列表中的地址数量 */
    ngx_uint_t                       weight;         /* 服务器的权重，用于负载均衡 */
    ngx_uint_t                       max_conns;      /* 最大并发连接数 */
    ngx_uint_t                       max_fails;      /* 允许的最大失败次数 */
    time_t                           fail_timeout;   /* 失败超时时间 */
    ngx_msec_t                       slow_start;     /* 慢启动时间，用于逐步增加流量 */
    ngx_uint_t                       down;           /* 服务器是否下线的标志 */

    unsigned                         backup:1;       /* 是否为备用服务器的标志 */

    NGX_COMPAT_BEGIN(6)
    NGX_COMPAT_END
} ngx_http_upstream_server_t;


/**
 * @brief 定义上游服务器创建标志
 *
 * 这个宏定义了一个标志位，用于指示需要创建一个新的上游服务器配置。
 * 当设置这个标志时，Nginx会创建一个新的上游服务器配置结构，而不是使用现有的配置。
 * 这通常用于动态添加或修改上游服务器配置的场景。
 *
 * 值: 0x0001 (二进制: 0000 0000 0000 0001)
 */
#define NGX_HTTP_UPSTREAM_CREATE        0x0001
/**
 * @brief 定义上游服务器权重标志
 *
 * 这个宏定义了一个标志位，用于指示上游服务器配置中包含权重设置。
 * 当设置这个标志时，Nginx会考虑服务器的权重进行负载均衡。
 * 权重影响了请求分发到该服务器的概率，权重越高，被选中的概率越大。
 *
 * 值: 0x0002 (二进制: 0000 0000 0000 0010)
 */
#define NGX_HTTP_UPSTREAM_WEIGHT        0x0002
/**
 * @brief 定义上游服务器最大失败次数标志
 *
 * 这个宏定义了一个标志位，用于指示上游服务器配置中包含最大失败次数设置。
 * 当设置这个标志时，Nginx会跟踪服务器的失败次数，并在达到最大失败次数时
 * 暂时将该服务器标记为不可用。这有助于实现简单的故障转移机制。
 *
 * 值: 0x0004 (二进制: 0000 0000 0000 0100)
 */
#define NGX_HTTP_UPSTREAM_MAX_FAILS     0x0004
/**
 * @brief 定义上游服务器失败超时标志
 *
 * 这个宏定义了一个标志位，用于指示上游服务器配置中包含失败超时设置。
 * 当设置这个标志时，Nginx会在服务器失败后等待指定的超时时间，然后再尝试重新连接。
 * 这有助于防止持续向不可用的服务器发送请求，同时允许服务器在一定时间后恢复。
 *
 * 值: 0x0008 (二进制: 0000 0000 0000 1000)
 */
#define NGX_HTTP_UPSTREAM_FAIL_TIMEOUT  0x0008
/**
 * @brief 定义上游服务器下线标志
 *
 * 这个宏定义了一个标志位，用于指示上游服务器当前处于下线状态。
 * 当设置这个标志时，Nginx会将该服务器标记为不可用，不会向其发送请求。
 * 这通常用于手动或自动将服务器从负载均衡池中临时移除，例如在服务器维护期间。
 *
 * 值: 0x0010 (二进制: 0000 0000 0001 0000)
 */
#define NGX_HTTP_UPSTREAM_DOWN          0x0010
/**
 * @brief 定义上游服务器备份标志
 *
 * 这个宏定义了一个标志位，用于指示上游服务器是一个备份服务器。
 * 当设置这个标志时，Nginx会将该服务器视为备份服务器，只有在主服务器不可用时才会使用。
 * 这有助于实现高可用性和故障转移机制，确保服务的连续性。
 *
 * 值: 0x0020 (二进制: 0000 0000 0010 0000)
 */
#define NGX_HTTP_UPSTREAM_BACKUP        0x0020
/**
 * @brief 定义上游服务器最大连接数标志
 *
 * 这个宏定义了一个标志位，用于指示上游服务器配置中包含最大连接数设置。
 * 当设置这个标志时，Nginx会限制与该上游服务器的并发连接数量。
 * 这有助于防止单个上游服务器过载，并实现更均衡的负载分配。
 *
 * 值: 0x0100 (二进制: 0000 0001 0000 0000)
 */
#define NGX_HTTP_UPSTREAM_MAX_CONNS     0x0100


/**
 * @brief 定义上游服务器配置结构体
 *
 * 这个结构体包含了上游服务器的配置信息，用于管理和控制与上游服务器的连接和通信。
 * 它是Nginx处理上游服务器时的核心数据结构之一。
 */
struct ngx_http_upstream_srv_conf_s {
    ngx_http_upstream_peer_t         peer;     /* 上游服务器的对等连接信息 */
    void                           **srv_conf; /* 服务器配置数组指针 */

    ngx_array_t                     *servers;  /* ngx_http_upstream_server_t 类型的服务器数组 */

    ngx_uint_t                       flags;    /* 配置标志位 */
    ngx_str_t                        host;     /* 主机名 */
    u_char                          *file_name;/* 配置文件名 */
    ngx_uint_t                       line;     /* 配置在文件中的行号 */
    in_port_t                        port;     /* 端口号 */
    ngx_uint_t                       no_port;  /* unsigned no_port:1 表示是否没有指定端口 */

#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_shm_zone_t                  *shm_zone; /* 共享内存区域，用于存储运行时状态 */
#endif
};


/**
 * @brief 定义上游本地配置结构体
 *
 * 这个结构体用于存储上游服务器的本地配置信息，包括地址和复杂值。
 * 它在处理上游连接时提供必要的本地设置。
 */
typedef struct {
    ngx_addr_t                      *addr;     /* 本地地址配置 */
    ngx_http_complex_value_t        *value;    /* 复杂值配置，用于动态生成本地地址 */
#if (NGX_HAVE_TRANSPARENT_PROXY)
    ngx_uint_t                       transparent; /* 透明代理标志，unsigned transparent:1; */
#endif
} ngx_http_upstream_local_t;


/**
 * @brief 定义上游配置结构体
 *
 * 这个结构体用于存储和管理上游服务器的配置信息。
 * 它包含了与上游服务器通信所需的各种参数和设置，
 * 如超时时间、缓冲区大小、错误处理策略等。
 * 这个结构体在处理HTTP请求转发到上游服务器时起着关键作用。
 */
typedef struct {
    ngx_http_upstream_srv_conf_t    *upstream;  /* 指向上游服务器配置的指针 */

    ngx_msec_t                       connect_timeout;  /* 连接超时时间 */
    ngx_msec_t                       send_timeout;     /* 发送超时时间 */
    ngx_msec_t                       read_timeout;     /* 读取超时时间 */
    ngx_msec_t                       next_upstream_timeout;  /* 切换到下一个上游服务器的超时时间 */

    size_t                           send_lowat;       /* 发送缓冲区低水位标记 */
    size_t                           buffer_size;      /* 缓冲区大小 */
    ngx_http_complex_value_t        *limit_rate;       /* 限制传输速率的复杂值 */

    size_t                           busy_buffers_size;  /* 忙碌缓冲区大小 */
    size_t                           max_temp_file_size;  /* 临时文件最大大小 */
    size_t                           temp_file_write_size;  /* 临时文件写入大小 */

    size_t                           busy_buffers_size_conf;  /* 配置的忙碌缓冲区大小 */
    size_t                           max_temp_file_size_conf;  /* 配置的临时文件最大大小 */
    size_t                           temp_file_write_size_conf;  /* 配置的临时文件写入大小 */

    ngx_bufs_t                       bufs;  /* 缓冲区配置 */

    ngx_uint_t                       ignore_headers;  /* 忽略的头部标志 */
    ngx_uint_t                       next_upstream;   /* 切换到下一个上游服务器的条件 */
    ngx_uint_t                       store_access;    /* 存储访问权限 */
    ngx_uint_t                       next_upstream_tries;  /* 尝试切换上游服务器的次数 */
    ngx_flag_t                       buffering;       /* 是否启用缓冲 */
    ngx_flag_t                       request_buffering;  /* 是否启用请求缓冲 */
    ngx_flag_t                       pass_request_headers;  /* 是否传递请求头 */
    ngx_flag_t                       pass_request_body;  /* 是否传递请求体 */

    ngx_flag_t                       ignore_client_abort;  /* 是否忽略客户端中断 */
    ngx_flag_t                       intercept_errors;     /* 是否拦截错误 */
    ngx_flag_t                       cyclic_temp_file;     /* 是否使用循环临时文件 */
    ngx_flag_t                       force_ranges;         /* 是否强制使用范围请求 */

    ngx_path_t                      *temp_path;  /* 临时文件路径 */

    ngx_hash_t                       hide_headers_hash;  /* 隐藏头部的哈希表 */
    ngx_array_t                     *hide_headers;       /* 需要隐藏的头部数组 */
    ngx_array_t                     *pass_headers;       /* 需要传递的头部数组 */

    ngx_http_upstream_local_t       *local;  /* 本地配置 */
    ngx_flag_t                       socket_keepalive;  /* 是否启用套接字保活 */

#if (NGX_HTTP_CACHE)
    ngx_shm_zone_t                  *cache_zone;  /* 缓存共享内存区域 */
    ngx_http_complex_value_t        *cache_value;  /* 缓存键的复杂值 */

    ngx_uint_t                       cache_min_uses;  /* 缓存最小使用次数 */
    ngx_uint_t                       cache_use_stale;  /* 使用过期缓存的条件 */
    ngx_uint_t                       cache_methods;    /* 可缓存的HTTP方法 */

    off_t                            cache_max_range_offset;  /* 可缓存的最大范围偏移 */

    ngx_flag_t                       cache_lock;  /* 是否启用缓存锁 */
    ngx_msec_t                       cache_lock_timeout;  /* 缓存锁超时时间 */
    ngx_msec_t                       cache_lock_age;      /* 缓存锁年龄 */

    ngx_flag_t                       cache_revalidate;  /* 是否重新验证缓存 */
    ngx_flag_t                       cache_convert_head;  /* 是否将HEAD请求转换为GET以进行缓存 */
    ngx_flag_t                       cache_background_update;  /* 是否在后台更新缓存 */

    ngx_array_t                     *cache_valid;   /* 缓存有效期配置数组 */
    ngx_array_t                     *cache_bypass;  /* 缓存绕过条件数组 */
    ngx_array_t                     *cache_purge;   /* 缓存清除条件数组 */
    ngx_array_t                     *no_cache;      /* 不缓存条件数组 */
#endif

    ngx_array_t                     *store_lengths;  /* 存储长度数组 */
    ngx_array_t                     *store_values;   /* 存储值数组 */

#if (NGX_HTTP_CACHE)
    signed                           cache:2;  /* 缓存标志 */
#endif
    signed                           store:2;  /* 存储标志 */
    unsigned                         intercept_404:1;  /* 是否拦截404错误 */
    unsigned                         change_buffering:1;  /* 是否改变缓冲设置 */
    unsigned                         pass_trailers:1;  /* 是否传递尾部字段 */
    unsigned                         preserve_output:1;  /* 是否保留输出 */

#if (NGX_HTTP_SSL || NGX_COMPAT)
    ngx_ssl_t                       *ssl;  /* SSL配置 */
    ngx_flag_t                       ssl_session_reuse;  /* 是否重用SSL会话 */

    ngx_http_complex_value_t        *ssl_name;  /* SSL服务器名称 */
    ngx_flag_t                       ssl_server_name;  /* 是否发送SSL服务器名称 */
    ngx_flag_t                       ssl_verify;  /* 是否验证SSL证书 */

    ngx_http_complex_value_t        *ssl_certificate;  /* SSL证书 */
    ngx_http_complex_value_t        *ssl_certificate_key;  /* SSL证书密钥 */
    ngx_array_t                     *ssl_passwords;  /* SSL密码数组 */
#endif

    ngx_str_t                        module;  /* 模块名称 */

    NGX_COMPAT_BEGIN(2)
    NGX_COMPAT_END
} ngx_http_upstream_conf_t;


/*
 * 定义 ngx_http_upstream_header_t 结构体
 *
 * 这个结构体用于描述上游服务器响应头的处理信息
 * 包含了响应头的名称、处理函数、偏移量等重要信息
 * 在处理上游服务器响应时起关键作用
 */
typedef struct {
    ngx_str_t                        name;           /* 响应头的名称 */
    ngx_http_header_handler_pt       handler;        /* 处理该响应头的函数指针 */
    ngx_uint_t                       offset;         /* 在结构体中的偏移量 */
    ngx_http_header_handler_pt       copy_handler;   /* 复制该响应头的函数指针 */
    ngx_uint_t                       conf;           /* 配置标志 */
    ngx_uint_t                       redirect;       /* 重定向标志，原为位字段 unsigned redirect:1; */
} ngx_http_upstream_header_t;


/*
 * 定义 ngx_http_upstream_headers_in_t 结构体
 *
 * 这个结构体用于存储和处理从上游服务器接收到的HTTP响应头信息
 * 包含了各种常见的HTTP头字段，以及一些特定于Nginx的扩展头字段
 * 在处理上游响应时，这个结构体起着关键作用，用于解析和存储头信息
 */
typedef struct {
    ngx_list_t                       headers;        /* 存储所有响应头的列表 */
    ngx_list_t                       trailers;       /* 存储响应尾部字段的列表 */

    ngx_uint_t                       status_n;       /* HTTP状态码（数字形式） */
    ngx_str_t                        status_line;    /* 完整的状态行 */

    ngx_table_elt_t                 *status;         /* 状态头 */
    ngx_table_elt_t                 *date;           /* 日期头 */
    ngx_table_elt_t                 *server;         /* 服务器头 */
    ngx_table_elt_t                 *connection;     /* 连接头 */

    ngx_table_elt_t                 *expires;        /* 过期时间头 */
    ngx_table_elt_t                 *etag;           /* ETag头 */
    ngx_table_elt_t                 *x_accel_expires;    /* Nginx加速过期头 */
    ngx_table_elt_t                 *x_accel_redirect;   /* Nginx加速重定向头 */
    ngx_table_elt_t                 *x_accel_limit_rate; /* Nginx加速限速头 */

    ngx_table_elt_t                 *content_type;   /* 内容类型头 */
    ngx_table_elt_t                 *content_length; /* 内容长度头 */

    ngx_table_elt_t                 *last_modified;  /* 最后修改时间头 */
    ngx_table_elt_t                 *location;       /* 重定向位置头 */
    ngx_table_elt_t                 *refresh;        /* 刷新头 */
    ngx_table_elt_t                 *www_authenticate;   /* WWW认证头 */
    ngx_table_elt_t                 *transfer_encoding;  /* 传输编码头 */
    ngx_table_elt_t                 *vary;           /* Vary头 */

    ngx_table_elt_t                 *cache_control;  /* 缓存控制头 */
    ngx_table_elt_t                 *set_cookie;     /* Set-Cookie头 */

    off_t                            content_length_n;   /* 内容长度（数字形式） */
    time_t                           last_modified_time; /* 最后修改时间（时间戳） */

    unsigned                         connection_close:1; /* 连接是否关闭 */
    unsigned                         chunked:1;          /* 是否使用分块传输编码 */
    unsigned                         no_cache:1;         /* 是否禁止缓存 */
    unsigned                         expired:1;          /* 是否已过期 */
} ngx_http_upstream_headers_in_t;


/*
 * 定义 ngx_http_upstream_resolved_t 结构体
 *
 * 这个结构体用于存储和管理已解析的上游服务器信息
 * 包含了服务器的主机名、端口、IP地址等关键信息
 * 在进行上游连接时，这个结构体提供了必要的连接细节
 */
typedef struct {
    ngx_str_t                        host;
    in_port_t                        port;
    ngx_uint_t                       no_port; /* unsigned no_port:1 */

    ngx_uint_t                       naddrs;
    ngx_resolver_addr_t             *addrs;

    struct sockaddr                 *sockaddr;
    socklen_t                        socklen;
    ngx_str_t                        name;

    ngx_resolver_ctx_t              *ctx;
} ngx_http_upstream_resolved_t;


/**
 * @brief 定义上游处理函数指针类型
 *
 * @param r 指向 HTTP 请求结构体的指针
 * @param u 指向上游结构体的指针
 *
 * 这个函数指针类型用于定义处理上游连接的回调函数。
 * 它在处理上游服务器的读写事件时被调用，用于实现具体的业务逻辑。
 */
typedef void (*ngx_http_upstream_handler_pt)(ngx_http_request_t *r,
    ngx_http_upstream_t *u);


/**
 * @brief 定义上游结构体
 *
 * 这个结构体包含了处理上游连接所需的所有信息和状态。
 * 它是Nginx处理上游请求的核心数据结构，管理着与上游服务器的交互过程。
 */
struct ngx_http_upstream_s {
    ngx_http_upstream_handler_pt     read_event_handler;    /* 读事件处理函数 */
    ngx_http_upstream_handler_pt     write_event_handler;   /* 写事件处理函数 */

    ngx_peer_connection_t            peer;                  /* 对等连接结构 */

    ngx_event_pipe_t                *pipe;                  /* 事件管道 */

    ngx_chain_t                     *request_bufs;          /* 请求缓冲链 */

    ngx_output_chain_ctx_t           output;                /* 输出链上下文 */
    ngx_chain_writer_ctx_t           writer;                /* 链写入器上下文 */

    ngx_http_upstream_conf_t        *conf;                  /* 上游配置 */
    ngx_http_upstream_srv_conf_t    *upstream;              /* 上游服务器配置 */
#if (NGX_HTTP_CACHE)
    ngx_array_t                     *caches;                /* 缓存数组 */
#endif

    ngx_http_upstream_headers_in_t   headers_in;            /* 输入头部 */

    ngx_http_upstream_resolved_t    *resolved;              /* 已解析的上游信息 */

    ngx_buf_t                        from_client;           /* 来自客户端的缓冲区 */

    ngx_buf_t                        buffer;                /* 通用缓冲区 */
    off_t                            length;                /* 长度 */

    ngx_chain_t                     *out_bufs;              /* 输出缓冲链 */
    ngx_chain_t                     *busy_bufs;             /* 忙碌缓冲链 */
    ngx_chain_t                     *free_bufs;             /* 空闲缓冲链 */

    ngx_int_t                      (*input_filter_init)(void *data);  /* 输入过滤器初始化函数 */
    ngx_int_t                      (*input_filter)(void *data, ssize_t bytes);  /* 输入过滤器函数 */
    void                            *input_filter_ctx;      /* 输入过滤器上下文 */

#if (NGX_HTTP_CACHE)
    ngx_int_t                      (*create_key)(ngx_http_request_t *r);  /* 创建缓存键函数 */
#endif
    ngx_int_t                      (*create_request)(ngx_http_request_t *r);  /* 创建请求函数 */
    ngx_int_t                      (*reinit_request)(ngx_http_request_t *r);  /* 重新初始化请求函数 */
    ngx_int_t                      (*process_header)(ngx_http_request_t *r);  /* 处理头部函数 */
    void                           (*abort_request)(ngx_http_request_t *r);   /* 中止请求函数 */
    void                           (*finalize_request)(ngx_http_request_t *r,
                                         ngx_int_t rc);     /* 完成请求函数 */
    ngx_int_t                      (*rewrite_redirect)(ngx_http_request_t *r,
                                         ngx_table_elt_t *h, size_t prefix);  /* 重写重定向函数 */
    ngx_int_t                      (*rewrite_cookie)(ngx_http_request_t *r,
                                         ngx_table_elt_t *h);  /* 重写cookie函数 */

    ngx_msec_t                       start_time;            /* 开始时间 */

    ngx_http_upstream_state_t       *state;                 /* 上游状态 */

    ngx_str_t                        method;                /* 方法 */
    ngx_str_t                        schema;                /* 模式 */
    ngx_str_t                        uri;                   /* URI */

#if (NGX_HTTP_SSL || NGX_COMPAT)
    ngx_str_t                        ssl_name;              /* SSL名称 */
#endif

    ngx_http_cleanup_pt             *cleanup;               /* 清理函数 */

    unsigned                         store:1;               /* 存储标志 */
    unsigned                         cacheable:1;           /* 可缓存标志 */
    unsigned                         accel:1;               /* 加速标志 */
    unsigned                         ssl:1;                 /* SSL标志 */
#if (NGX_HTTP_CACHE)
    unsigned                         cache_status:3;        /* 缓存状态 */
#endif

    unsigned                         buffering:1;           /* 缓冲标志 */
    unsigned                         keepalive:1;           /* 保持连接标志 */
    unsigned                         upgrade:1;             /* 升级标志 */
    unsigned                         error:1;               /* 错误标志 */

    unsigned                         request_sent:1;        /* 请求已发送标志 */
    unsigned                         request_body_sent:1;   /* 请求体已发送标志 */
    unsigned                         request_body_blocked:1;/* 请求体阻塞标志 */
    unsigned                         header_sent:1;         /* 头部已发送标志 */
};


/**
 * @brief 定义上游服务器的下一个处理步骤结构体
 *
 * 这个结构体用于定义上游服务器处理请求时的下一个步骤。
 * 它包含了状态码和掩码，用于决定在不同情况下如何继续处理请求。
 * 这个结构体在实现复杂的上游服务器逻辑和错误处理时非常有用。
 */
typedef struct {
    ngx_uint_t                      status;  /* 状态码，表示当前处理的结果或状态 */
    ngx_uint_t                      mask;    /* 掩码，用于与状态码进行位操作，确定下一步操作 */
} ngx_http_upstream_next_t;


/**
 * @brief 定义上游参数结构体
 *
 * 这个结构体用于存储上游服务器的参数信息。
 * 它包含了键值对和一个标志，用于在处理上游请求时传递和控制参数。
 */
typedef struct {
    ngx_str_t   key;        /* 参数的键 */
    ngx_str_t   value;      /* 参数的值 */
    ngx_uint_t  skip_empty; /* 是否跳过空值，1表示跳过，0表示不跳过 */
} ngx_http_upstream_param_t;


/**
 * @brief 创建上游请求
 *
 * 此函数用于为给定的HTTP请求创建一个上游请求结构。
 * 上游请求通常用于Nginx作为反向代理时，将客户端的请求转发到上游服务器。
 *
 * @param r 指向ngx_http_request_t结构的指针，表示当前的HTTP请求
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR或其他错误码
 *
 * 该函数主要完成以下工作:
 * 1. 分配并初始化上游请求结构
 * 2. 设置上游请求的各种参数和回调函数
 * 3. 将上游请求与当前HTTP请求关联
 *
 * 注意: 调用此函数后，通常需要进一步配置上游请求，然后调用ngx_http_upstream_init()来启动上游处理。
 */
ngx_int_t ngx_http_upstream_create(ngx_http_request_t *r);
/**
 * @brief 初始化上游请求
 *
 * 此函数用于初始化给定HTTP请求的上游处理。
 * 它在创建上游请求结构后被调用，用于启动实际的上游处理过程。
 *
 * @param r 指向ngx_http_request_t结构的指针，表示当前的HTTP请求
 *
 * 该函数主要完成以下工作:
 * 1. 检查并设置上游请求的各种参数
 * 2. 选择合适的上游服务器
 * 3. 启动与上游服务器的连接过程
 * 4. 设置相应的回调函数以处理上游响应
 *
 * 注意: 此函数通常在ngx_http_upstream_create()之后调用，
 * 以开始实际的上游请求处理。
 */
void ngx_http_upstream_init(ngx_http_request_t *r);
/**
 * @brief 初始化非缓冲过滤器
 *
 * 此函数用于初始化上游请求的非缓冲过滤器。
 * 非缓冲过滤器通常用于直接处理和转发上游响应，而不进行完整的缓冲。
 *
 * @param data 指向过滤器相关数据的指针，通常是请求上下文
 * @return 成功时返回NGX_OK，失败时返回错误码
 *
 * 主要功能:
 * 1. 设置非缓冲过滤器的初始状态
 * 2. 分配必要的资源（如果需要）
 * 3. 准备过滤器以处理上游响应数据
 *
 * 注意:
 * - 此函数通常在开始处理上游响应之前调用
 * - 它为后续的非缓冲过滤操作做准备
 * - 返回值表示初始化是否成功，影响后续处理流程
 */
ngx_int_t ngx_http_upstream_non_buffered_filter_init(void *data);
/**
 * @brief 非缓冲过滤器函数
 *
 * 此函数用于处理上游响应的非缓冲过滤。
 * 它在接收到上游数据时被调用，用于直接处理和转发数据，而不进行完整的缓冲。
 *
 * @param data 指向过滤器相关数据的指针，通常是请求上下文
 * @param bytes 接收到的数据字节数
 * @return 成功时返回NGX_OK，失败时返回错误码
 *
 * 主要功能:
 * 1. 处理接收到的上游数据
 * 2. 根据需要修改或转换数据
 * 3. 将处理后的数据转发给下游
 *
 * 注意:
 * - 此函数在每次接收到上游数据时被调用
 * - bytes参数指示本次接收到的数据量
 * - 返回值表示过滤操作是否成功，影响后续处理流程
 */
ngx_int_t ngx_http_upstream_non_buffered_filter(void *data, ssize_t bytes);
/**
 * @brief 添加上游服务器配置
 *
 * @param cf Nginx配置结构指针
 * @param u 上游URL结构指针
 * @param flags 配置标志
 * @return 返回新创建的上游服务器配置结构指针，失败时返回NULL
 *
 * 此函数用于向Nginx配置中添加新的上游服务器配置。
 * 它创建并初始化一个新的ngx_http_upstream_srv_conf_t结构，
 * 根据提供的URL和标志设置相应的参数。
 * 这个函数在配置上游服务器群组时非常重要，为负载均衡和反向代理功能提供基础。
 */
ngx_http_upstream_srv_conf_t *ngx_http_upstream_add(ngx_conf_t *cf,
    ngx_url_t *u, ngx_uint_t flags);
/**
 * @brief 设置上游绑定配置的槽函数
 *
 * @param cf Nginx配置结构指针
 * @param cmd 当前处理的命令结构指针
 * @param conf 配置结构指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 *
 * 此函数用于处理上游服务器绑定配置的设置。
 * 它解析配置指令中的参数，并将绑定信息设置到相应的配置结构中。
 * 这对于指定上游连接应该使用的本地IP地址和端口非常有用。
 */
char *ngx_http_upstream_bind_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/**
 * @brief 设置上游参数配置的槽函数
 *
 * @param cf Nginx配置结构指针
 * @param cmd 当前处理的命令结构指针
 * @param conf 配置结构指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 *
 * 此函数用于处理上游服务器参数配置的设置。
 * 它解析配置指令中的参数，并将这些参数设置到相应的配置结构中。
 * 这对于自定义上游服务器的行为和特性非常有用，如设置连接超时、缓冲区大小等。
 */
char *ngx_http_upstream_param_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/**
 * @brief 创建隐藏头部的哈希表
 *
 * @param cf Nginx配置结构指针
 * @param conf 当前上游配置结构指针
 * @param prev 先前的上游配置结构指针
 * @param default_hide_headers 默认需要隐藏的头部数组
 * @param hash 哈希表初始化结构指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 此函数用于创建一个哈希表，用于存储需要在向客户端传递响应时隐藏的HTTP头部。
 * 它合并了配置文件中指定的隐藏头部和默认隐藏头部，并将它们存入哈希表中。
 * 这个哈希表后续会被用于快速检查某个头部是否需要被隐藏。
 */
ngx_int_t ngx_http_upstream_hide_headers_hash(ngx_conf_t *cf,
    ngx_http_upstream_conf_t *conf, ngx_http_upstream_conf_t *prev,
    ngx_str_t *default_hide_headers, ngx_hash_init_t *hash);


/**
 * @brief 获取特定模块的上游服务器配置
 *
 * @param uscf 上游服务器配置结构指针
 * @param module 需要获取配置的模块
 * @return 返回指定模块的上游服务器配置
 *
 * 这个宏用于从上游服务器配置结构中获取特定模块的配置。
 * 它通过模块的上下文索引来访问正确的配置项。
 * 这在处理特定模块的上游配置时非常有用。
 */
#define ngx_http_conf_upstream_srv_conf(uscf, module)                         \
    uscf->srv_conf[module.ctx_index]


/**
 * @brief 声明 HTTP 上游模块
 *
 * 这是对 ngx_http_upstream_module 的外部声明。
 * ngx_http_upstream_module 是 Nginx 中处理上游（upstream）服务器的核心模块。
 * 它负责管理和配置上游服务器组，处理负载均衡，以及与上游服务器的通信等功能。
 * 通过这个模块，Nginx 可以作为反向代理服务器，将请求转发给后端的上游服务器。
 */
extern ngx_module_t        ngx_http_upstream_module;
/**
 * @brief HTTP上游缓存方法掩码
 *
 * 这是一个外部声明的位掩码数组，用于定义哪些HTTP方法可以使用上游缓存。
 * 通过这个掩码，Nginx可以控制对不同HTTP方法的缓存行为。
 * 例如，可以配置只对GET和HEAD请求进行缓存，而对POST请求不缓存。
 * 这个掩码在配置上游缓存策略时非常有用，可以精细地控制缓存行为。
 */
extern ngx_conf_bitmask_t  ngx_http_upstream_cache_method_mask[];
/**
 * @brief HTTP上游忽略头部掩码
 *
 * 这是一个外部声明的位掩码数组，用于定义哪些HTTP头部应该被上游模块忽略。
 * 通过这个掩码，Nginx可以控制对不同HTTP头部的处理行为。
 * 例如，可以配置忽略某些特定的头部，不将其传递给客户端或不用于缓存决策。
 * 这个掩码在配置上游处理策略时非常有用，可以精细地控制对各种头部的处理方式。
 */
extern ngx_conf_bitmask_t  ngx_http_upstream_ignore_headers_masks[];


#endif /* _NGX_HTTP_UPSTREAM_H_INCLUDED_ */
