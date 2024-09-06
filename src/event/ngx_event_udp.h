
/*
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_UDP_H_INCLUDED_
#define _NGX_EVENT_UDP_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#if !(NGX_WIN32)

// 检查是否支持高级UDP功能
#if ((NGX_HAVE_MSGHDR_MSG_CONTROL)                                            \
     && (NGX_HAVE_IP_SENDSRCADDR || NGX_HAVE_IP_RECVDSTADDR                   \
         || NGX_HAVE_IP_PKTINFO                                               \
         || (NGX_HAVE_INET6 && NGX_HAVE_IPV6_RECVPKTINFO)))
#define NGX_HAVE_ADDRINFO_CMSG  1

#endif


/**
 * @brief UDP连接结构体
 *
 * 这个结构体用于表示一个UDP连接的相关信息。
 * 它包含了连接的各种属性，如红黑树节点、连接对象、缓冲区和键值等。
 */
struct ngx_udp_connection_s {
    ngx_rbtree_node_t   node;       // 红黑树节点，用于快速查找
    ngx_connection_t   *connection; // 指向对应的连接对象
    ngx_buf_t          *buffer;     // 用于存储数据的缓冲区
    ngx_str_t           key;        // 连接的唯一标识符
};


#if (NGX_HAVE_ADDRINFO_CMSG)

// 用于存储不同类型的地址信息的联合体
typedef union {
#if (NGX_HAVE_IP_SENDSRCADDR || NGX_HAVE_IP_RECVDSTADDR)
    struct in_addr        addr;     // IPv4地址
#endif

#if (NGX_HAVE_IP_PKTINFO)
    struct in_pktinfo     pkt;      // IPv4数据包信息
#endif

#if (NGX_HAVE_INET6 && NGX_HAVE_IPV6_RECVPKTINFO)
    struct in6_pktinfo    pkt6;     // IPv6数据包信息
#endif
} ngx_addrinfo_t;

// 设置源地址控制消息
size_t ngx_set_srcaddr_cmsg(struct cmsghdr *cmsg,
    struct sockaddr *local_sockaddr);

// 获取源地址控制消息
ngx_int_t ngx_get_srcaddr_cmsg(struct cmsghdr *cmsg,
    struct sockaddr *local_sockaddr);

#endif

// 接收UDP消息的事件处理函数
void ngx_event_recvmsg(ngx_event_t *ev);

// 发送UDP消息
ssize_t ngx_sendmsg(ngx_connection_t *c, struct msghdr *msg, int flags);

// UDP连接红黑树插入值的函数
void ngx_udp_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
#endif

// 删除UDP连接
void ngx_delete_udp_connection(void *data);


#endif /* _NGX_EVENT_UDP_H_INCLUDED_ */
