
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_OPENSSL_H_INCLUDED_
#define _NGX_EVENT_OPENSSL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

#define OPENSSL_SUPPRESS_DEPRECATED

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/dh.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif
#include <openssl/evp.h>
#if (NGX_QUIC)
#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/hkdf.h>
#include <openssl/chacha.h>
#else
#include <openssl/kdf.h>
#endif
#endif
#include <openssl/hmac.h>
#ifndef OPENSSL_NO_OCSP
#include <openssl/ocsp.h>
#endif
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#define NGX_SSL_NAME     "OpenSSL"


#if (defined LIBRESSL_VERSION_NUMBER && OPENSSL_VERSION_NUMBER == 0x20000000L)
#undef OPENSSL_VERSION_NUMBER
#if (LIBRESSL_VERSION_NUMBER >= 0x3050000fL)
#define OPENSSL_VERSION_NUMBER  0x1010000fL
#else
#define OPENSSL_VERSION_NUMBER  0x1000107fL
#endif
#endif


#if (OPENSSL_VERSION_NUMBER >= 0x10100001L)

#define ngx_ssl_version()       OpenSSL_version(OPENSSL_VERSION)

#else

#define ngx_ssl_version()       SSLeay_version(SSLEAY_VERSION)

#endif


#define ngx_ssl_session_t       SSL_SESSION
#define ngx_ssl_conn_t          SSL


#if (OPENSSL_VERSION_NUMBER < 0x10002000L)
#define SSL_is_server(s)        (s)->server
#endif


#if (OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined SSL_get_peer_certificate)
#define SSL_get_peer_certificate(s)  SSL_get1_peer_certificate(s)
#endif


#if (OPENSSL_VERSION_NUMBER < 0x30000000L && !defined ERR_peek_error_data)
#define ERR_peek_error_data(d, f)    ERR_peek_error_line_data(NULL, NULL, d, f)
#endif


typedef struct ngx_ssl_ocsp_s  ngx_ssl_ocsp_t;


/**
 * @brief SSL结构体
 *
 * 这个结构体用于表示一个SSL的相关信息。
 * 它包含了SSL上下文、日志、缓冲区大小等。
 */
struct ngx_ssl_s {
    // SSL上下文
    SSL_CTX                    *ctx;
    // 日志
    ngx_log_t                  *log;
    // 缓冲区大小
    size_t                      buffer_size;
};


/**
 * @brief SSL连接结构体
 *
 * 这个结构体用于表示一个SSL连接的相关信息。
 * 它包含了SSL连接对象、会话上下文、连接处理函数等。
 */
struct ngx_ssl_connection_s {
    // SSL连接对象
    ngx_ssl_conn_t             *connection;
    // SSL会话上下文
    SSL_CTX                    *session_ctx;

    // 最后一个处理的事件
    ngx_int_t                   last;
    // 缓冲区
    ngx_buf_t                  *buf;
    // 缓冲区大小
    size_t                      buffer_size;

    // 连接处理函数
    ngx_connection_handler_pt   handler;

    // SSL会话
    ngx_ssl_session_t          *session;
    // 保存SSL会话的处理函数
    ngx_connection_handler_pt   save_session;

    // 保存的读事件处理函数
    ngx_event_handler_pt        saved_read_handler;
    // 保存的写事件处理函数
    ngx_event_handler_pt        saved_write_handler;

    // OCSP响应
    ngx_ssl_ocsp_t             *ocsp;

    // 早期数据缓冲区
    u_char                      early_buf;

    // 标志位
    // SSL握手完成标志
    unsigned                    handshaked:1;
    // SSL握手被拒绝标志
    unsigned                    handshake_rejected:1;
    // SSL重协商标志
    unsigned                    renegotiation:1;
    // 使用缓冲区标志
    unsigned                    buffer:1;
    // 使用sendfile标志
    unsigned                    sendfile:1;
    // 无需等待关闭标志
    unsigned                    no_wait_shutdown:1;
    // 不发送关闭标志
    unsigned                    no_send_shutdown:1;
    // 关闭不释放标志
    unsigned                    shutdown_without_free:1;
    // 设置握手缓冲区标志
    unsigned                    handshake_buffer_set:1;
    // 设置会话超时标志
    unsigned                    session_timeout_set:1;
    // 尝试早期数据标志
    unsigned                    try_early_data:1;
    // 在早期数据阶段标志
    unsigned                    in_early:1;
    // 在OCSP阶段标志
    unsigned                    in_ocsp:1;
    // 早期预读标志
    unsigned                    early_preread:1;
    // 写阻塞标志
    unsigned                    write_blocked:1;
};


/**
 * @brief 定义SSL会话缓存禁用标志
 *
 * 这个宏定义表示SSL会话缓存被禁用。
 * 当配置使用这个值时，表示不使用任何形式的SSL会话缓存。
 */
#define NGX_SSL_NO_SCACHE            -2
/**
 * @brief 定义SSL会话缓存无缓存标志
 *
 * 这个宏定义表示SSL会话缓存为无缓存模式。
 * 当配置使用这个值时，表示不使用任何形式的SSL会话缓存，
 * 并且明确指定为无缓存模式，与NGX_SSL_NO_SCACHE略有不同。
 */
#define NGX_SSL_NONE_SCACHE          -3
/**
 * @brief 定义SSL会话缓存不使用内置缓存标志
 *
 * 这个宏定义表示SSL会话缓存不使用内置的缓存机制。
 * 当配置使用这个值时，表示禁用Nginx内置的SSL会话缓存功能，
 * 可能会使用外部的缓存机制或完全不使用会话缓存。
 */
#define NGX_SSL_NO_BUILTIN_SCACHE    -4
/**
 * @brief 定义SSL会话缓存默认内置缓存标志
 *
 * 这个宏定义表示使用SSL会话的默认内置缓存机制。
 * 当配置使用这个值时，表示使用Nginx默认提供的内置SSL会话缓存功能。
 * 这通常是最常用的配置选项，提供了良好的性能和易用性。
 */
#define NGX_SSL_DFLT_BUILTIN_SCACHE  -5


/**
 * @brief 定义SSL会话的最大大小
 *
 * 这个宏定义了SSL会话的最大允许大小，单位为字节。
 * 它限制了单个SSL会话可以占用的最大内存空间，
 * 有助于防止潜在的内存耗尽攻击，并确保资源的合理分配。
 * 当前设置为4096字节（4KB），这通常足以容纳大多数SSL会话信息。
 */
#define NGX_SSL_MAX_SESSION_SIZE  4096

typedef struct ngx_ssl_sess_id_s  ngx_ssl_sess_id_t;

/**
 * @brief 定义SSL会话标识结构体
 *
 * 这个结构体用于存储SSL会话的标识信息。
 */
struct ngx_ssl_sess_id_s {
    /**
     * @brief 红黑树节点
     *
     * 用于在红黑树中存储SSL会话标识。
     */
    ngx_rbtree_node_t           node;
    /**
     * @brief 长度
     *
     * SSL会话标识的长度。
     */
    size_t                      len;
    /**
     * @brief 队列
     *
     * 用于存储SSL会话标识的队列。
     */
    ngx_queue_t                 queue;
    /**
     * @brief 过期时间
     *
     * SSL会话标识的过期时间。
     */
    time_t                      expire;
    /**
     * @brief 标识
     *
     * SSL会话的标识，长度为32字节。
     */
    u_char                      id[32];
    /**
     * @brief 会话数据
     *
     * 根据指针大小不同，存储SSL会话数据的方式也不同。
     * 如果指针大小为8字节（即64位系统），则使用指针存储会话数据。
     * 否则，使用数组存储会话数据。
     */
#if (NGX_PTR_SIZE == 8)
    u_char                     *session;
#else
    u_char                      session[1];
#endif
};


/**
 * @brief 定义SSL会话票据密钥结构体
 *
 * 这个结构体用于存储SSL会话票据的密钥信息。
 */
typedef struct {
    /**
     * @brief 名称
     *
     * SSL会话票据的名称，长度为16字节。
     */
    u_char                      name[16];
    /**
     * @brief HMAC密钥
     *
     * 用于HMAC加密的密钥，长度为32字节。
     */
    u_char                      hmac_key[32];
    /**
     * @brief AES密钥
     *
     * 用于AES加密的密钥，长度为32字节。
     */
    u_char                      aes_key[32];
    /**
     * @brief 过期时间
     *
     * SSL会话票据的过期时间。
     */
    time_t                      expire;
    /**
     * @brief 大小
     *
     * SSL会话票据的大小，8位无符号整数。
     */
    unsigned                    size:8;
    /**
     * @brief 共享标志
     *
     * 指示SSL会话票据是否共享，1位无符号整数。
     */
    unsigned                    shared:1;
} ngx_ssl_ticket_key_t;


/**
 * @brief 定义SSL会话缓存结构体
 *
 * 这个结构体用于存储SSL会话缓存的信息。
 */
typedef struct {
    /**
     * @brief 会话红黑树
     *
     * 用于存储SSL会话的红黑树。
     */
    ngx_rbtree_t                session_rbtree;
    /**
     * @brief 哨兵节点
     *
     * SSL会话红黑树的哨兵节点。
     */
    ngx_rbtree_node_t           sentinel;
    /**
     * @brief 过期队列
     *
     * 用于存储即将过期的SSL会话的队列。
     */
    ngx_queue_t                 expire_queue;
    /**
     * @brief 会话票据密钥
     *
     * 存储SSL会话票据密钥的数组，长度为3。
     */
    ngx_ssl_ticket_key_t        ticket_keys[3];
    /**
     * @brief 失败时间
     *
     * SSL会话缓存的失败时间。
     */
    time_t                      fail_time;
} ngx_ssl_session_cache_t;


#define NGX_SSL_SSLv2    0x0002
#define NGX_SSL_SSLv3    0x0004
#define NGX_SSL_TLSv1    0x0008
#define NGX_SSL_TLSv1_1  0x0010
#define NGX_SSL_TLSv1_2  0x0020
#define NGX_SSL_TLSv1_3  0x0040


// 定义SSL缓冲区标志
#define NGX_SSL_BUFFER   1
// 定义SSL客户端标志
#define NGX_SSL_CLIENT   2

// 定义SSL缓冲区大小为16384字节
#define NGX_SSL_BUFSIZE  16384


/**
 * @brief 初始化SSL
 *
 * 初始化SSL环境，准备进行SSL连接。
 *
 * @param log 日志对象
 * @return 初始化结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_init(ngx_log_t *log);
/**
 * @brief 创建SSL对象
 *
 * 根据给定的协议和数据创建一个SSL对象。
 *
 * @param ssl SSL对象指针
 * @param protocols 支持的SSL/TLS协议
 * @param data 初始化数据
 * @return 创建结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_create(ngx_ssl_t *ssl, ngx_uint_t protocols, void *data);

/**
 * @brief 配置SSL证书
 *
 * 从配置文件中读取证书、密钥和密码，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param certs 证书数组
 * @param keys 密钥数组
 * @param passwords 密码数组
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_certificates(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_array_t *certs, ngx_array_t *keys, ngx_array_t *passwords);
/**
 * @brief 配置SSL证书（单个）
 *
 * 从配置文件中读取单个证书、密钥和密码，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param cert 证书
 * @param key 密钥
 * @param passwords 密码数组
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_certificate(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_str_t *cert, ngx_str_t *key, ngx_array_t *passwords);
/**
 * @brief 配置连接的SSL证书
 *
 * 根据连接和池，配置SSL证书。
 *
 * @param c 连接对象
 * @param pool 连接池
 * @param cert 证书
 * @param key 密钥
 * @param passwords 密码数组
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_connection_certificate(ngx_connection_t *c, ngx_pool_t *pool,
    ngx_str_t *cert, ngx_str_t *key, ngx_array_t *passwords);

/**
 * @brief 配置SSL密码
 *
 * 从配置文件中读取密码，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param ciphers 密码字符串
 * @param prefer_server_ciphers 是否优先使用服务器密码
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_ciphers(ngx_conf_t *cf, ngx_ssl_t *ssl, ngx_str_t *ciphers,
    ngx_uint_t prefer_server_ciphers);
/**
 * @brief 配置SSL客户端证书
 *
 * 从配置文件中读取客户端证书和深度，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param cert 客户端证书
 * @param depth 证书深度
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_client_certificate(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_str_t *cert, ngx_int_t depth);
/**
 * @brief 配置SSL受信任证书
 *
 * 从配置文件中读取受信任证书和深度，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param cert 受信任证书
 * @param depth 证书深度
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_trusted_certificate(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_str_t *cert, ngx_int_t depth);
/**
 * @brief 配置SSL证书吊销列表
 *
 * 从配置文件中读取证书吊销列表，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param crl 证书吊销列表
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_crl(ngx_conf_t *cf, ngx_ssl_t *ssl, ngx_str_t *crl);
/**
 * @brief 配置SSL证书状态协议
 *
 * 从配置文件中读取证书状态协议文件和响应者，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param file 证书状态协议文件
 * @param responder 响应者
 * @param verify 是否验证
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_stapling(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_str_t *file, ngx_str_t *responder, ngx_uint_t verify);
/**
 * @brief 配置SSL证书状态协议解析器
 *
 * 从配置文件中读取证书状态协议解析器和超时时间，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param resolver 解析器
 * @param resolver_timeout 超时时间
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_stapling_resolver(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_resolver_t *resolver, ngx_msec_t resolver_timeout);
/**
 * @brief 配置SSL在线证书状态协议
 *
 * 从配置文件中读取在线证书状态协议响应者、深度和共享内存区域，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param responder 响应者
 * @param depth 深度
 * @param shm_zone 共享内存区域
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_ocsp(ngx_conf_t *cf, ngx_ssl_t *ssl, ngx_str_t *responder,
    ngx_uint_t depth, ngx_shm_zone_t *shm_zone);
/**
 * @brief 配置SSL在线证书状态协议解析器
 *
 * 从配置文件中读取在线证书状态协议解析器和超时时间，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param resolver 解析器
 * @param resolver_timeout 超时时间
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_ocsp_resolver(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_resolver_t *resolver, ngx_msec_t resolver_timeout);

/**
 * @brief 验证SSL在线证书状态
 *
 * 验证连接的SSL在线证书状态。
 *
 * @param c 连接对象
 * @return 验证结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_ocsp_validate(ngx_connection_t *c);
/**
 * @brief 获取SSL在线证书状态
 *
 * 获取连接的SSL在线证书状态字符串。
 *
 * @param c 连接对象
 * @param s 状态字符串
 * @return 获取结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_ocsp_get_status(ngx_connection_t *c, const char **s);
/**
 * @brief 清理SSL在线证书状态
 *
 * 清理连接的SSL在线证书状态。
 *
 * @param c 连接对象
 */
void ngx_ssl_ocsp_cleanup(ngx_connection_t *c);
/**
 * @brief 初始化SSL在线证书状态缓存
 *
 * 初始化SSL在线证书状态缓存区域。
 *
 * @param shm_zone 共享内存区域
 * @param data 初始化数据
 * @return 初始化结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_ocsp_cache_init(ngx_shm_zone_t *shm_zone, void *data);

/**
 * @brief 读取SSL密码文件
 *
 * 从文件中读取SSL密码，返回密码数组。
 *
 * @param cf 配置上下文
 * @param file 密码文件
 * @return 密码数组
 */
ngx_array_t *ngx_ssl_read_password_file(ngx_conf_t *cf, ngx_str_t *file);
/**
 * @brief 保留SSL密码
 *
 * 保留SSL密码数组。
 *
 * @param cf 配置上下文
 * @param passwords 密码数组
 * @return 保留后的密码数组
 */
ngx_array_t *ngx_ssl_preserve_passwords(ngx_conf_t *cf,
    ngx_array_t *passwords);
/**
 * @brief 配置SSLDiffie-Hellman参数
 *
 * 从文件中读取Diffie-Hellman参数，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param file 参数文件
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_dhparam(ngx_conf_t *cf, ngx_ssl_t *ssl, ngx_str_t *file);
/**
 * @brief 配置SSL椭圆曲线Diffie-Hellman参数
 *
 * 从配置文件中读取椭圆曲线Diffie-Hellman参数，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param name 参数名称
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_ecdh_curve(ngx_conf_t *cf, ngx_ssl_t *ssl, ngx_str_t *name);
/**
 * @brief 配置SSL早期数据
 *
 * 配置SSL早期数据功能。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param enable 是否启用
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_early_data(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_uint_t enable);
/**
 * @brief 配置SSL命令
 *
 * 从配置文件中读取SSL命令，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param commands 命令数组
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_conf_commands(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_array_t *commands);

/**
 * @brief 配置SSL客户端会话缓存
 *
 * 配置SSL客户端会话缓存功能。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param enable 是否启用
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_client_session_cache(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_uint_t enable);
/**
 * @brief 配置SSL会话缓存
 *
 * 配置SSL会话缓存。
 *
 * @param ssl SSL对象指针
 * @param sess_ctx 会话上下文
 * @param certificates 证书数组
 * @param builtin_session_cache 内置会话缓存大小
 * @param shm_zone 共享内存区域
 * @param timeout 超时时间
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_session_cache(ngx_ssl_t *ssl, ngx_str_t *sess_ctx,
    ngx_array_t *certificates, ssize_t builtin_session_cache,
    ngx_shm_zone_t *shm_zone, time_t timeout);
/**
 * @brief 配置SSL会话票据密钥
 *
 * 从配置文件中读取SSL会话票据密钥的路径，配置SSL对象。
 *
 * @param cf 配置上下文
 * @param ssl SSL对象指针
 * @param paths 密钥路径数组
 * @return 配置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_session_ticket_keys(ngx_conf_t *cf, ngx_ssl_t *ssl,
    ngx_array_t *paths);

/**
 * @brief 初始化SSL会话缓存
 *
 * 初始化SSL会话缓存，准备用于存储SSL会话。
 *
 * @param shm_zone 共享内存区域
 * @param data 初始化数据
 * @return 初始化结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_session_cache_init(ngx_shm_zone_t *shm_zone, void *data);

/**
 * @brief 创建SSL连接
 *
 * 根据给定的SSL对象和连接对象，创建一个SSL连接。
 *
 * @param ssl SSL对象指针
 * @param c 连接对象
 * @param flags 连接标志
 * @return 创建结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_create_connection(ngx_ssl_t *ssl, ngx_connection_t *c,
    ngx_uint_t flags);

/**
 * @brief 移除缓存的SSL会话
 *
 * 从SSL上下文中移除指定的SSL会话。
 *
 * @param ssl SSL上下文
 * @param sess 要移除的SSL会话
 */
void ngx_ssl_remove_cached_session(SSL_CTX *ssl, ngx_ssl_session_t *sess);

/**
 * @brief 设置SSL会话
 *
 * 将给定的SSL会话设置到连接对象中。
 *
 * @param c 连接对象
 * @param session 要设置的SSL会话
 * @return 设置结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_ssl_set_session(ngx_connection_t *c, ngx_ssl_session_t *session);

/**
 * @brief 获取SSL会话
 *
 * 从连接对象中获取当前的SSL会话。
 *
 * @param c 连接对象
 * @return SSL会话对象，失败返回NULL
 */
ngx_ssl_session_t *ngx_ssl_get_session(ngx_connection_t *c);

/**
 * @brief 获取SSL会话（不增加引用计数）
 *
 * 从连接对象中获取当前的SSL会话，不增加会话的引用计数。
 *
 * @param c 连接对象
 * @return SSL会话对象，失败返回NULL
 */
ngx_ssl_session_t *ngx_ssl_get0_session(ngx_connection_t *c);
/**
 * @brief 释放SSL会话
 *
 * 释放给定的SSL会话对象。
 */
#define ngx_ssl_free_session        SSL_SESSION_free

/**
 * @brief 获取SSL连接
 *
 * 从SSL连接对象中获取关联的连接对象。
 *
 * @param ssl_conn SSL连接对象
 * @return 连接对象
 */
#define ngx_ssl_get_connection(ssl_conn)                                      \
    SSL_get_ex_data(ssl_conn, ngx_ssl_connection_index)

/**
 * @brief 获取SSL服务器配置
 *
 * 从SSL上下文中获取关联的服务器配置对象。
 *
 * @param ssl_ctx SSL上下文
 * @return 服务器配置对象
 */
#define ngx_ssl_get_server_conf(ssl_ctx)                                      \
    SSL_CTX_get_ex_data(ssl_ctx, ngx_ssl_server_conf_index)

/**
 * @brief 检查SSL验证错误是否为可选的
 *
 * 检查给定的SSL验证错误代码是否为可选的错误类型。
 *
 * @param n 错误代码
 * @return 如果错误代码为可选的错误类型，则返回1，否则返回0
 */
#define ngx_ssl_verify_error_optional(n)                                      \
    (n == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT                              \
     || n == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN                             \
     || n == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY                     \
     || n == X509_V_ERR_CERT_UNTRUSTED                                        \
     || n == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE)

/**
 * @brief 检查SSL主机
 *
 * 检查给定的SSL连接对象是否与给定的主机名匹配。
 *
 * @param c 连接对象
 * @param name 主机名
 * @return 如果SSL连接对象与主机名匹配，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_check_host(ngx_connection_t *c, ngx_str_t *name);


/**
 * @brief 获取SSL协议
 *
 * 从SSL连接对象中获取当前的SSL协议。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL协议，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_protocol(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL密码名称
 *
 * 从SSL连接对象中获取当前的SSL密码名称。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL密码名称，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_cipher_name(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL密码
 *
 * 从SSL连接对象中获取当前的SSL密码。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL密码，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_ciphers(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL曲线
 *
 * 从SSL连接对象中获取当前的SSL曲线。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL曲线，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_curve(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL曲线列表
 *
 * 从SSL连接对象中获取当前的SSL曲线列表。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL曲线列表，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_curves(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL会话ID
 *
 * 从SSL连接对象中获取当前的SSL会话ID。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL会话ID，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_session_id(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL会话重用状态
 *
 * 从SSL连接对象中获取当前的SSL会话重用状态。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL会话重用状态，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_session_reused(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL早期数据状态
 *
 * 从SSL连接对象中获取当前的SSL早期数据状态。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL早期数据状态，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_early_data(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL服务器名称
 *
 * 从SSL连接对象中获取当前的SSL服务器名称。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL服务器名称，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_server_name(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL ALPN协议
 *
 * 从SSL连接对象中获取当前的SSL ALPN协议。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL ALPN协议，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_alpn_protocol(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL原始证书
 *
 * 从SSL连接对象中获取当前的SSL原始证书。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL原始证书，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_raw_certificate(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL证书
 *
 * 从SSL连接对象中获取当前的SSL证书。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL证书，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_certificate(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL转义证书
 *
 * 从SSL连接对象中获取当前的SSL转义证书。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL转义证书，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_escaped_certificate(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL主题DN
 *
 * 从SSL连接对象中获取当前的SSL主题DN。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL主题DN，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_subject_dn(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL颁发者DN
 *
 * 从SSL连接对象中获取当前的SSL颁发者DN。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL颁发者DN，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_issuer_dn(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL主题DN（旧版）
 *
 * 从SSL连接对象中获取当前的SSL主题DN（旧版）。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL主题DN（旧版），则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_subject_dn_legacy(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL颁发者DN（旧版）
 *
 * 从SSL连接对象中获取当前的SSL颁发者DN（旧版）。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL颁发者DN（旧版），则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_issuer_dn_legacy(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL序列号
 *
 * 从SSL连接对象中获取当前的SSL序列号。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL序列号，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_serial_number(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL指纹
 *
 * 从SSL连接对象中获取当前的SSL指纹。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL指纹，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_fingerprint(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL客户端验证状态
 *
 * 从SSL连接对象中获取当前的SSL客户端验证状态。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL客户端验证状态，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_client_verify(ngx_connection_t *c, ngx_pool_t *pool, 
    ngx_str_t *s);

/**
 * @brief 获取SSL客户端验证开始时间
 *
 * 从SSL连接对象中获取当前的SSL客户端验证开始时间。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL客户端验证开始时间，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_client_v_start(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL客户端验证结束时间
 *
 * 从SSL连接对象中获取当前的SSL客户端验证结束时间。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL客户端验证结束时间，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_client_v_end(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);

/**
 * @brief 获取SSL客户端验证剩余时间
 *
 * 从SSL连接对象中获取当前的SSL客户端验证剩余时间。
 *
 * @param c 连接对象
 * @param pool 内存池
 * @param s 字符串
 * @return 如果成功获取SSL客户端验证剩余时间，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_get_client_v_remain(ngx_connection_t *c, ngx_pool_t *pool, 
ngx_str_t *s);


/**
 * @brief 执行SSL握手
 *
 * 对给定的连接对象执行SSL握手操作。
 *
 * @param c 连接对象
 * @return 如果握手成功，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_handshake(ngx_connection_t *c);
#if (NGX_DEBUG)
/**
 * @brief 记录SSL握手日志
 *
 * 对给定的连接对象记录SSL握手过程的日志信息。
 *
 * @param c 连接对象
 */
void ngx_ssl_handshake_log(ngx_connection_t *c);
#endif


/**
 * @brief SSL接收数据
 *
 * 从SSL连接中接收数据，并存储到指定的缓冲区中。
 *
 * @param c 连接对象
 * @param buf 接收数据的缓冲区
 * @param size 缓冲区的大小
 * @return 接收到的数据大小
 */
ssize_t ngx_ssl_recv(ngx_connection_t *c, u_char *buf, size_t size);

/**
 * @brief SSL发送数据
 *
 * 通过SSL连接发送数据。
 *
 * @param c 连接对象
 * @param data 要发送的数据
 * @param size 数据的大小
 * @return 发送的数据大小
 */
ssize_t ngx_ssl_write(ngx_connection_t *c, u_char *data, size_t size);

/**
 * @brief SSL接收链式数据
 *
 * 从SSL连接中接收链式数据，并存储到指定的链式结构中。
 *
 * @param c 连接对象
 * @param cl 链式结构
 * @param limit 接收数据的上限
 * @return 接收到的数据大小
 */
ssize_t ngx_ssl_recv_chain(ngx_connection_t *c, ngx_chain_t *cl, off_t limit);

/**
 * @brief SSL发送链式数据
 *
 * 通过SSL连接发送链式数据。
 *
 * @param c 连接对象
 * @param in 要发送的链式数据
 * @param limit 发送数据的上限
 * @return 发送的链式数据
 */
ngx_chain_t *ngx_ssl_send_chain(ngx_connection_t *c, ngx_chain_t *in, 
    off_t limit);

/**
 * @brief 释放SSL缓冲区
 *
 * 释放SSL连接对象中的缓冲区。
 *
 * @param c 连接对象
 */
void ngx_ssl_free_buffer(ngx_connection_t *c);

/**
 * @brief SSL连接关闭
 *
 * 关闭SSL连接。
 *
 * @param c 连接对象
 * @return 如果关闭成功，则返回1，否则返回0
 */
ngx_int_t ngx_ssl_shutdown(ngx_connection_t *c);

/**
 * @brief SSL错误处理
 *
 * 处理SSL连接中的错误，并记录错误日志。
 *
 * @param level 错误级别
 * @param log 日志对象
 * @param err 错误代码
 * @param fmt 错误信息格式字符串
 * @param ... 错误信息参数
 */
void ngx_cdecl ngx_ssl_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err, 
char *fmt, ...);

/**
 * @brief 清理SSL上下文
 *
 * 清理SSL上下文中的数据。
 *
 * @param data 要清理的数据
 */
void ngx_ssl_cleanup_ctx(void *data);


// SSL连接索引
extern int  ngx_ssl_connection_index;
// SSL服务器配置索引
extern int  ngx_ssl_server_conf_index;
// SSL会话缓存索引
extern int  ngx_ssl_session_cache_index;
// SSL会话票据密钥索引
extern int  ngx_ssl_ticket_keys_index;
// SSLOCSP索引
extern int  ngx_ssl_ocsp_index;
// SSL证书索引
extern int  ngx_ssl_certificate_index;
// 下一个SSL证书索引
extern int  ngx_ssl_next_certificate_index;
// SSL证书名称索引
extern int  ngx_ssl_certificate_name_index;
// SSL证书固定索引
extern int  ngx_ssl_stapling_index;


#endif /* _NGX_EVENT_OPENSSL_H_INCLUDED_ */
