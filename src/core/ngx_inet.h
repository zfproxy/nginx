
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_INET_H_INCLUDED_
#define _NGX_INET_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief IPv4地址字符串的最大长度（不包含结束符）
 */
#define NGX_INET_ADDRSTRLEN   (sizeof("255.255.255.255") - 1)

/**
 * @brief IPv6地址字符串的最大长度（不包含结束符）
 */
#define NGX_INET6_ADDRSTRLEN                                                 \
    (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)

/**
 * @brief Unix域套接字地址字符串的最大长度
 */
#define NGX_UNIX_ADDRSTRLEN                                                  \
    (sizeof("unix:") - 1 +                                                   \
     sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

/**
 * @brief 根据系统支持的套接字类型，定义通用的套接字地址字符串最大长度
 */
#if (NGX_HAVE_UNIX_DOMAIN)
#define NGX_SOCKADDR_STRLEN   NGX_UNIX_ADDRSTRLEN
#elif (NGX_HAVE_INET6)
#define NGX_SOCKADDR_STRLEN   (NGX_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1)
#else
#define NGX_SOCKADDR_STRLEN   (NGX_INET_ADDRSTRLEN + sizeof(":65535") - 1)
#endif

/* 兼容性定义 */
#define NGX_SOCKADDRLEN       sizeof(ngx_sockaddr_t)

/**
 * @brief 通用套接字地址结构体
 */
typedef union {
    struct sockaddr           sockaddr;    /**< 通用套接字地址 */
    struct sockaddr_in        sockaddr_in; /**< IPv4套接字地址 */
#if (NGX_HAVE_INET6)
    struct sockaddr_in6       sockaddr_in6; /**< IPv6套接字地址 */
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
    struct sockaddr_un        sockaddr_un; /**< Unix域套接字地址 */
#endif
} ngx_sockaddr_t;

/**
 * @brief IPv4 CIDR结构体
 */
typedef struct {
    in_addr_t                 addr; /**< IPv4地址 */
    in_addr_t                 mask; /**< 子网掩码 */
} ngx_in_cidr_t;

#if (NGX_HAVE_INET6)

/**
 * @brief IPv6 CIDR结构体
 */
typedef struct {
    struct in6_addr           addr; /**< IPv6地址 */
    struct in6_addr           mask; /**< 子网掩码 */
} ngx_in6_cidr_t;

#endif

/**
 * @brief 通用CIDR结构体
 */
typedef struct {
    ngx_uint_t                family; /**< 地址族 */
    union {
        ngx_in_cidr_t         in;     /**< IPv4 CIDR */
#if (NGX_HAVE_INET6)
        ngx_in6_cidr_t        in6;    /**< IPv6 CIDR */
#endif
    } u;
} ngx_cidr_t;

/**
 * @brief 地址结构体
 */
typedef struct {
    struct sockaddr          *sockaddr; /**< 套接字地址 */
    socklen_t                 socklen;  /**< 地址长度 */
    ngx_str_t                 name;     /**< 地址名称 */
} ngx_addr_t;

/**
 * @brief URL解析结构体
 */
typedef struct {
    ngx_str_t                 url;          /**< 完整URL */
    ngx_str_t                 host;         /**< 主机名 */
    ngx_str_t                 port_text;    /**< 端口号文本 */
    ngx_str_t                 uri;          /**< URI */

    in_port_t                 port;         /**< 端口号 */
    in_port_t                 default_port; /**< 默认端口号 */
    in_port_t                 last_port;    /**< 最后使用的端口号 */
    int                       family;       /**< 地址族 */

    unsigned                  listen:1;     /**< 是否为监听地址 */
    unsigned                  uri_part:1;   /**< URL是否包含URI部分 */
    unsigned                  no_resolve:1; /**< 是否不解析主机名 */

    unsigned                  no_port:1;    /**< 是否没有指定端口 */
    unsigned                  wildcard:1;   /**< 是否为通配符地址 */

    socklen_t                 socklen;      /**< 套接字地址长度 */
    ngx_sockaddr_t            sockaddr;     /**< 套接字地址 */

    ngx_addr_t               *addrs;        /**< 解析后的地址列表 */
    ngx_uint_t                naddrs;       /**< 地址列表中的地址数量 */

    char                     *err;          /**< 错误信息 */
} ngx_url_t;

/* 函数声明 */

/**
 * @brief 将IPv4地址文本转换为网络字节序的32位整数
 */
in_addr_t ngx_inet_addr(u_char *text, size_t len);

#if (NGX_HAVE_INET6)
/**
 * @brief 将IPv6地址文本转换为网络字节序的128位地址
 */
ngx_int_t ngx_inet6_addr(u_char *p, size_t len, u_char *addr);

/**
 * @brief 将网络字节序的IPv6地址转换为文本表示
 */
size_t ngx_inet6_ntop(u_char *p, u_char *text, size_t len);
#endif

/**
 * @brief 将套接字地址转换为文本表示
 */
size_t ngx_sock_ntop(struct sockaddr *sa, socklen_t socklen, u_char *text,
    size_t len, ngx_uint_t port);

/**
 * @brief 将IP地址转换为文本表示
 */
size_t ngx_inet_ntop(int family, void *addr, u_char *text, size_t len);

/**
 * @brief 将文本表示的CIDR转换为ngx_cidr_t结构
 */
ngx_int_t ngx_ptocidr(ngx_str_t *text, ngx_cidr_t *cidr);

/**
 * @brief 检查给定的套接字地址是否匹配CIDR列表中的任何一个
 */
ngx_int_t ngx_cidr_match(struct sockaddr *sa, ngx_array_t *cidrs);

/**
 * @brief 解析文本形式的地址
 */
ngx_int_t ngx_parse_addr(ngx_pool_t *pool, ngx_addr_t *addr, u_char *text,
    size_t len);

/**
 * @brief 解析文本形式的地址和端口
 */
ngx_int_t ngx_parse_addr_port(ngx_pool_t *pool, ngx_addr_t *addr,
    u_char *text, size_t len);

/**
 * @brief 解析URL
 */
ngx_int_t ngx_parse_url(ngx_pool_t *pool, ngx_url_t *u);

/**
 * @brief 解析主机名并获取IP地址
 */
ngx_int_t ngx_inet_resolve_host(ngx_pool_t *pool, ngx_url_t *u);

/**
 * @brief 比较两个套接字地址是否相等
 */
ngx_int_t ngx_cmp_sockaddr(struct sockaddr *sa1, socklen_t slen1,
    struct sockaddr *sa2, socklen_t slen2, ngx_uint_t cmp_port);

/**
 * @brief 获取套接字地址中的端口号
 */
in_port_t ngx_inet_get_port(struct sockaddr *sa);

/**
 * @brief 设置套接字地址中的端口号
 */
void ngx_inet_set_port(struct sockaddr *sa, in_port_t port);

/**
 * @brief 检查套接字地址是否为通配符地址
 */
ngx_uint_t ngx_inet_wildcard(struct sockaddr *sa);


#endif /* _NGX_INET_H_INCLUDED_ */
