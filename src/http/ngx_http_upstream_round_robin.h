
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_UPSTREAM_ROUND_ROBIN_H_INCLUDED_
#define _NGX_HTTP_UPSTREAM_ROUND_ROBIN_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief 定义上游服务器轮询负载均衡的单个服务器结构体类型别名
 *
 * 这个类型定义为 ngx_http_upstream_rr_peer_s 结构体创建了一个别名 ngx_http_upstream_rr_peer_t。
 * 该结构体用于表示在轮询（Round Robin）负载均衡算法中的单个上游服务器。
 *
 * 使用这个类型别名可以简化代码，使其更易读和维护。它在整个 Nginx 代码库中被广泛使用，
 * 特别是在处理上游服务器连接和负载均衡相关的功能时。
 */
typedef struct ngx_http_upstream_rr_peer_s   ngx_http_upstream_rr_peer_t;

/* 
 * ngx_http_upstream_rr_peer_s 结构体定义了一个上游服务器的属性和状态信息
 * 用于实现负载均衡中的轮询（Round Robin）算法
 */
struct ngx_http_upstream_rr_peer_s {
    struct sockaddr                *sockaddr;
    socklen_t                       socklen;
    ngx_str_t                       name;
    ngx_str_t                       server;

    ngx_int_t                       current_weight;
    ngx_int_t                       effective_weight;
    ngx_int_t                       weight;

    ngx_uint_t                      conns;
    ngx_uint_t                      max_conns;

    ngx_uint_t                      fails;
    time_t                          accessed;
    time_t                          checked;

    ngx_uint_t                      max_fails;
    time_t                          fail_timeout;
    ngx_msec_t                      slow_start;
    ngx_msec_t                      start_time;

    ngx_uint_t                      down;

#if (NGX_HTTP_SSL || NGX_COMPAT)
    void                           *ssl_session;
    int                             ssl_session_len;
#endif

#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_atomic_t                    lock;
#endif

    ngx_http_upstream_rr_peer_t    *next;

    NGX_COMPAT_BEGIN(32)
    NGX_COMPAT_END
};


/**
 * @brief 定义 ngx_http_upstream_rr_peers_t 类型别名
 *
 * 这个类型别名将 struct ngx_http_upstream_rr_peers_s 结构体定义为 ngx_http_upstream_rr_peers_t。
 * 它用于表示一组上游服务器的集合，主要用于实现负载均衡中的轮询（Round Robin）算法。
 *
 * 使用这个类型别名可以简化代码，提高可读性和可维护性。在 Nginx 的上游模块和负载均衡相关的代码中广泛使用。
 */
typedef struct ngx_http_upstream_rr_peers_s  ngx_http_upstream_rr_peers_t;

/*
 * ngx_http_upstream_rr_peers_s 结构体定义了一组上游服务器的集合
 * 用于实现负载均衡中的轮询（Round Robin）算法
 * 包含了服务器组的整体信息和配置
 */
struct ngx_http_upstream_rr_peers_s {
    ngx_uint_t                      number;

#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_slab_pool_t                *shpool;
    ngx_atomic_t                    rwlock;
    ngx_http_upstream_rr_peers_t   *zone_next;
#endif

    ngx_uint_t                      total_weight;
    ngx_uint_t                      tries;

    unsigned                        single:1;
    unsigned                        weighted:1;

    ngx_str_t                      *name;

    ngx_http_upstream_rr_peers_t   *next;

    ngx_http_upstream_rr_peer_t    *peer;
};


#if (NGX_HTTP_UPSTREAM_ZONE)

/**
 * @brief 为上游服务器组获取读锁的宏定义
 *
 * 这个宏用于在访问共享内存中的上游服务器组数据时获取读锁。
 * 只有在启用了 NGX_HTTP_UPSTREAM_ZONE 选项时才会实际执行加锁操作。
 *
 * @param peers 指向 ngx_http_upstream_rr_peers_t 结构的指针
 *
 * 使用方法:
 * ngx_http_upstream_rr_peers_rlock(peers);
 * // 执行需要读锁保护的操作
 * ngx_http_upstream_rr_peers_unlock(peers);
 */
#define ngx_http_upstream_rr_peers_rlock(peers)                               \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_rlock(&peers->rwlock);                                     \
    }

/**
 * @brief 为上游服务器组获取写锁的宏定义
 *
 * 这个宏用于在修改共享内存中的上游服务器组数据时获取写锁。
 * 只有在启用了 NGX_HTTP_UPSTREAM_ZONE 选项时才会实际执行加锁操作。
 *
 * @param peers 指向 ngx_http_upstream_rr_peers_t 结构的指针
 *
 * 使用方法:
 * ngx_http_upstream_rr_peers_wlock(peers);
 * // 执行需要写锁保护的操作
 * ngx_http_upstream_rr_peers_unlock(peers);
 */
#define ngx_http_upstream_rr_peers_wlock(peers)                               \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_wlock(&peers->rwlock);                                     \
    }

/**
 * @brief 为上游服务器组解锁的宏定义
 *
 * 这个宏用于在访问或修改完共享内存中的上游服务器组数据后释放锁。
 * 只有在启用了 NGX_HTTP_UPSTREAM_ZONE 选项时才会实际执行解锁操作。
 *
 * @param peers 指向 ngx_http_upstream_rr_peers_t 结构的指针
 *
 * 使用方法:
 * ngx_http_upstream_rr_peers_rlock(peers); // 或 ngx_http_upstream_rr_peers_wlock(peers);
 * // 执行需要锁保护的操作
 * ngx_http_upstream_rr_peers_unlock(peers);
 */
#define ngx_http_upstream_rr_peers_unlock(peers)                              \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_unlock(&peers->rwlock);                                    \
    }


/**
 * @brief 为单个上游服务器获取写锁的宏定义
 *
 * 这个宏用于在修改共享内存中的单个上游服务器数据时获取写锁。
 * 只有在启用了 NGX_HTTP_UPSTREAM_ZONE 选项时才会实际执行加锁操作。
 *
 * @param peers 指向 ngx_http_upstream_rr_peers_t 结构的指针
 * @param peer 指向要加锁的 ngx_http_upstream_rr_peer_t 结构的指针
 *
 * 使用方法:
 * ngx_http_upstream_rr_peer_lock(peers, peer);
 * // 执行需要写锁保护的操作
 * ngx_http_upstream_rr_peer_unlock(peers, peer);
 */
#define ngx_http_upstream_rr_peer_lock(peers, peer)                           \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_wlock(&peer->lock);                                        \
    }

/**
 * @brief 为单个上游服务器解锁的宏定义
 *
 * 这个宏用于在修改完共享内存中的单个上游服务器数据后释放写锁。
 * 只有在启用了 NGX_HTTP_UPSTREAM_ZONE 选项时才会实际执行解锁操作。
 *
 * @param peers 指向 ngx_http_upstream_rr_peers_t 结构的指针
 * @param peer 指向要解锁的 ngx_http_upstream_rr_peer_t 结构的指针
 *
 * 使用方法:
 * ngx_http_upstream_rr_peer_lock(peers, peer);
 * // 执行需要写锁保护的操作
 * ngx_http_upstream_rr_peer_unlock(peers, peer);
 */
#define ngx_http_upstream_rr_peer_unlock(peers, peer)                         \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_unlock(&peer->lock);                                       \
    }

#else

#define ngx_http_upstream_rr_peers_rlock(peers)
#define ngx_http_upstream_rr_peers_wlock(peers)
#define ngx_http_upstream_rr_peers_unlock(peers)
#define ngx_http_upstream_rr_peer_lock(peers, peer)
#define ngx_http_upstream_rr_peer_unlock(peers, peer)

#endif


/**
 * @brief 定义用于轮询负载均衡的数据结构
 *
 * 这个结构体包含了实现轮询负载均衡算法所需的各种数据，
 * 包括配置信息、服务器列表、当前选中的服务器等。
 */
typedef struct {
    ngx_uint_t                      config;
    ngx_http_upstream_rr_peers_t   *peers;
    ngx_http_upstream_rr_peer_t    *current;
    uintptr_t                      *tried;
    uintptr_t                       data;
} ngx_http_upstream_rr_peer_data_t;


/**
 * @brief 初始化轮询负载均衡算法
 *
 * 此函数用于初始化轮询（Round Robin）负载均衡算法的配置和数据结构。
 * 它在Nginx配置阶段被调用，为上游服务器组设置轮询策略。
 *
 * @param cf Nginx配置上下文
 * @param us 指向上游服务器配置结构的指针
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_http_upstream_init_round_robin(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us);
/**
 * @brief 初始化单个轮询负载均衡对等点
 *
 * 此函数用于为特定的HTTP请求初始化一个轮询负载均衡的对等点。
 * 它在处理请求时被调用，为当前请求设置轮询策略的具体参数。
 *
 * @param r 指向当前HTTP请求结构的指针
 * @param us 指向上游服务器配置结构的指针
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_http_upstream_init_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
/**
 * @brief 为解析的上游服务器创建轮询负载均衡对等点
 *
 * 此函数用于为已解析的上游服务器创建轮询负载均衡的对等点。
 * 它通常在动态解析上游服务器地址后被调用，为这些动态解析的服务器设置轮询策略。
 *
 * @param r 指向当前HTTP请求结构的指针
 * @param ur 指向已解析的上游服务器信息结构的指针
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_http_upstream_create_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_resolved_t *ur);
/**
 * @brief 获取轮询负载均衡的下一个对等点
 *
 * 此函数用于从轮询负载均衡池中选择下一个可用的上游服务器。
 * 它实现了轮询算法的核心逻辑，确保请求均匀分布到所有可用的上游服务器。
 *
 * @param pc 指向对等连接结构的指针，用于存储选中的服务器信息
 * @param data 指向轮询负载均衡相关数据的指针，通常是 ngx_http_upstream_rr_peer_data_t 类型
 * @return 成功选择服务器时返回 NGX_OK，失败时返回错误码
 */
ngx_int_t ngx_http_upstream_get_round_robin_peer(ngx_peer_connection_t *pc,
    void *data);
/**
 * @brief 释放轮询负载均衡的对等点连接
 *
 * 此函数用于释放一个轮询负载均衡的对等点连接。当一个上游连接完成或失败时调用。
 * 它负责清理连接资源，并可能根据连接状态更新负载均衡策略。
 *
 * @param pc 指向对等连接结构的指针，包含了要释放的连接信息
 */
void ngx_http_upstream_free_round_robin_peer(ngx_peer_connection_t *pc,
    void *data, ngx_uint_t state);

#if (NGX_HTTP_SSL)
/**
 * @brief 为轮询负载均衡的对等点设置SSL会话
 *
 * 此函数用于为轮询负载均衡的对等点设置SSL会话信息。
 * 在启用SSL的上游连接中，它负责重用或建立新的SSL会话。
 *
 * @param pc 指向对等连接结构的指针，包含连接信息
 * @param data 指向轮询负载均衡相关数据的指针
 * @return 成功设置SSL会话时返回NGX_OK，失败时返回错误码
 */
ngx_int_t
    ngx_http_upstream_set_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data);
/**
 * @brief 保存轮询负载均衡对等点的SSL会话
 *
 * 此函数用于保存轮询负载均衡对等点的SSL会话信息。
 * 在启用SSL的上游连接中，它负责在连接结束时保存SSL会话，以便后续连接可能重用。
 *
 * @param pc 指向对等连接结构的指针，包含要保存的SSL会话信息
 */
void ngx_http_upstream_save_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data);
#endif


#endif /* _NGX_HTTP_UPSTREAM_ROUND_ROBIN_H_INCLUDED_ */
