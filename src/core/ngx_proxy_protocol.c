
/*
 * Copyright (C) Roman Arutyunyan
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_proxy_protocol.c
 *
 * 本文件实现了PROXY协议的解析和处理功能。
 *
 * 支持的功能：
 * 1. 解析PROXY协议v1和v2版本
 * 2. 支持IPv4和IPv6地址
 * 3. 解析TLV (Type-Length-Value) 扩展字段
 * 4. 处理PROXY协议头部信息
 *
 * 使用注意点：
 * 1. 确保在配置中正确启用PROXY协议支持
 * 2. 注意处理可能的协议解析错误
 * 3. 对于TLV扩展字段，需要根据具体需求进行额外处理
 * 4. 在高并发环境下，需要注意PROXY协议解析对性能的影响
 */


#include <ngx_config.h>
#include <ngx_core.h>


// PROXY协议地址族定义
#define NGX_PROXY_PROTOCOL_AF_INET          1  // IPv4地址族
#define NGX_PROXY_PROTOCOL_AF_INET6         2  // IPv6地址族


// 从字节数组中解析16位无符号整数
#define ngx_proxy_protocol_parse_uint16(p)                                    \
    ( ((uint16_t) (p)[0] << 8)                                                \
    + (           (p)[1]) )

// 从字节数组中解析32位无符号整数
#define ngx_proxy_protocol_parse_uint32(p)                                    \
    ( ((uint32_t) (p)[0] << 24)                                               \
    + (           (p)[1] << 16)                                               \
    + (           (p)[2] << 8)                                                \
    + (           (p)[3]) )


// PROXY协议头部结构
typedef struct {
    u_char                                  signature[12];  // 协议签名
    u_char                                  version_command;  // 版本和命令
    u_char                                  family_transport;  // 地址族和传输协议
    u_char                                  len[2];  // 剩余头部长度
} ngx_proxy_protocol_header_t;


// IPv4地址结构
typedef struct {
    u_char                                  src_addr[4];  // 源IP地址
    u_char                                  dst_addr[4];  // 目标IP地址
    u_char                                  src_port[2];  // 源端口
    u_char                                  dst_port[2];  // 目标端口
} ngx_proxy_protocol_inet_addrs_t;


// IPv6地址结构
typedef struct {
    u_char                                  src_addr[16];  // 源IP地址
    u_char                                  dst_addr[16];  // 目标IP地址
    u_char                                  src_port[2];  // 源端口
    u_char                                  dst_port[2];  // 目标端口
} ngx_proxy_protocol_inet6_addrs_t;


// TLV (Type-Length-Value) 结构
typedef struct {
    u_char                                  type;  // TLV类型
    u_char                                  len[2];  // TLV长度
} ngx_proxy_protocol_tlv_t;


// SSL TLV结构
typedef struct {
    u_char                                  client;  // 客户端标志
    u_char                                  verify[4];  // 验证结果
} ngx_proxy_protocol_tlv_ssl_t;


// TLV条目结构
typedef struct {
    ngx_str_t                               name;  // TLV名称
    ngx_uint_t                              type;  // TLV类型
} ngx_proxy_protocol_tlv_entry_t;


// 从PROXY协议中读取地址信息
static u_char *ngx_proxy_protocol_read_addr(ngx_connection_t *c, u_char *p,
    u_char *last, ngx_str_t *addr);

// 从PROXY协议中读取端口信息
static u_char *ngx_proxy_protocol_read_port(u_char *p, u_char *last,
    in_port_t *port, u_char sep);

// 解析PROXY协议v2版本的数据
static u_char *ngx_proxy_protocol_v2_read(ngx_connection_t *c, u_char *buf,
    u_char *last);

// 在PROXY协议的TLV(Type-Length-Value)结构中查找指定类型的值
static ngx_int_t ngx_proxy_protocol_lookup_tlv(ngx_connection_t *c,
    ngx_str_t *tlvs, ngx_uint_t type, ngx_str_t *value);


// PROXY协议TLV条目数组
static ngx_proxy_protocol_tlv_entry_t  ngx_proxy_protocol_tlv_entries[] = {
    { ngx_string("alpn"),       0x01 },  // 应用层协议协商
    { ngx_string("authority"),  0x02 },  // 权威服务器名称
    { ngx_string("unique_id"),  0x05 },  // 唯一标识符
    { ngx_string("ssl"),        0x20 },  // SSL相关信息
    { ngx_string("netns"),      0x30 },  // 网络命名空间
    { ngx_null_string,          0x00 }   // 数组结束标志
};


// PROXY协议SSL TLV条目数组
static ngx_proxy_protocol_tlv_entry_t  ngx_proxy_protocol_tlv_ssl_entries[] = {
    { ngx_string("version"),    0x21 },  // SSL版本
    { ngx_string("cn"),         0x22 },  // 通用名称
    { ngx_string("cipher"),     0x23 },  // 加密套件
    { ngx_string("sig_alg"),    0x24 },  // 签名算法
    { ngx_string("key_alg"),    0x25 },  // 密钥算法
    { ngx_null_string,          0x00 }   // 数组结束标志
};


u_char *
ngx_proxy_protocol_read(ngx_connection_t *c, u_char *buf, u_char *last)
{
    size_t                 len;
    u_char                *p;
    ngx_proxy_protocol_t  *pp;

    // PROXY协议v2版本的签名
    static const u_char signature[] = "\r\n\r\n\0\r\nQUIT\n";

    p = buf;
    len = last - buf;

    // 检查是否为PROXY协议v2版本
    if (len >= sizeof(ngx_proxy_protocol_header_t)
        && ngx_memcmp(p, signature, sizeof(signature) - 1) == 0)
    {
        return ngx_proxy_protocol_v2_read(c, buf, last);
    }

    // 检查PROXY协议v1版本的开头
    if (len < 8 || ngx_strncmp(p, "PROXY ", 6) != 0) {
        goto invalid;
    }

    p += 6;
    len -= 6;

    // 处理UNKNOWN协议类型
    if (len >= 7 && ngx_strncmp(p, "UNKNOWN", 7) == 0) {
        ngx_log_debug0(NGX_LOG_DEBUG_CORE, c->log, 0,
                       "PROXY protocol unknown protocol");
        p += 7;
        goto skip;
    }

    // 检查协议类型是否为TCP4或TCP6
    if (len < 5 || ngx_strncmp(p, "TCP", 3) != 0
        || (p[3] != '4' && p[3] != '6') || p[4] != ' ')
    {
        goto invalid;
    }

    p += 5;

    // 分配内存用于存储PROXY协议信息
    pp = ngx_pcalloc(c->pool, sizeof(ngx_proxy_protocol_t));
    if (pp == NULL) {
        return NULL;
    }

    // 读取源地址
    p = ngx_proxy_protocol_read_addr(c, p, last, &pp->src_addr);
    if (p == NULL) {
        goto invalid;
    }

    // 读取目标地址
    p = ngx_proxy_protocol_read_addr(c, p, last, &pp->dst_addr);
    if (p == NULL) {
        goto invalid;
    }

    // 读取源端口
    p = ngx_proxy_protocol_read_port(p, last, &pp->src_port, ' ');
    if (p == NULL) {
        goto invalid;
    }

    // 读取目标端口
    p = ngx_proxy_protocol_read_port(p, last, &pp->dst_port, CR);
    if (p == NULL) {
        goto invalid;
    }

    // 检查是否到达缓冲区末尾
    if (p == last) {
        goto invalid;
    }

    // 检查行尾是否为LF
    if (*p++ != LF) {
        goto invalid;
    }

    // 记录调试信息
    ngx_log_debug4(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "PROXY protocol src: %V %d, dst: %V %d",
                   &pp->src_addr, pp->src_port, &pp->dst_addr, pp->dst_port);

    // 将PROXY协议信息与连接关联
    c->proxy_protocol = pp;

    return p;

skip:
    // 跳过剩余的PROXY协议头部
    for ( /* void */ ; p < last - 1; p++) {
        if (p[0] == CR && p[1] == LF) {
            return p + 2;
        }
    }

invalid:
    // 处理无效的PROXY协议头部
    for (p = buf; p < last; p++) {
        if (*p == CR || *p == LF) {
            break;
        }
    }

    // 记录错误信息
    ngx_log_error(NGX_LOG_ERR, c->log, 0,
                  "broken header: \"%*s\"", (size_t) (p - buf), buf);

    return NULL;
}


static u_char *
ngx_proxy_protocol_read_addr(ngx_connection_t *c, u_char *p, u_char *last,
    ngx_str_t *addr)
{
    size_t  len;
    u_char  ch, *pos;

    pos = p;

    // 循环读取地址字符串
    for ( ;; ) {
        // 检查是否到达缓冲区末尾
        if (p == last) {
            return NULL;
        }

        ch = *p++;

        // 遇到空格表示地址结束
        if (ch == ' ') {
            break;
        }

        // 验证字符是否为有效的IP地址字符
        if (ch != ':' && ch != '.'
            && (ch < 'a' || ch > 'f')
            && (ch < 'A' || ch > 'F')
            && (ch < '0' || ch > '9'))
        {
            return NULL;
        }
    }

    // 计算地址长度
    len = p - pos - 1;

    // 为地址分配内存
    addr->data = ngx_pnalloc(c->pool, len);
    if (addr->data == NULL) {
        return NULL;
    }

    // 复制地址字符串
    ngx_memcpy(addr->data, pos, len);
    addr->len = len;

    return p;
}


static u_char *
ngx_proxy_protocol_read_port(u_char *p, u_char *last, in_port_t *port,
    u_char sep)
{
    size_t      len;
    u_char     *pos;
    ngx_int_t   n;

    pos = p;

    // 循环读取端口号字符串
    for ( ;; ) {
        // 检查是否到达缓冲区末尾
        if (p == last) {
            return NULL;
        }

        // 遇到分隔符表示端口号结束
        if (*p++ == sep) {
            break;
        }
    }

    // 计算端口号字符串长度
    len = p - pos - 1;

    // 将端口号字符串转换为整数
    n = ngx_atoi(pos, len);
    // 验证端口号是否在有效范围内（0-65535）
    if (n < 0 || n > 65535) {
        return NULL;
    }

    // 将有效的端口号赋值给输出参数
    *port = (in_port_t) n;

    // 返回指向分隔符后的位置
    return p;
}


u_char *
ngx_proxy_protocol_write(ngx_connection_t *c, u_char *buf, u_char *last)
{
    ngx_uint_t  port, lport;

    // 检查缓冲区是否足够大
    if (last - buf < NGX_PROXY_PROTOCOL_V1_MAX_HEADER) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "too small buffer for PROXY protocol");
        return NULL;
    }

    // 获取本地套接字地址
    if (ngx_connection_local_sockaddr(c, NULL, 0) != NGX_OK) {
        return NULL;
    }

    // 根据套接字地址族选择适当的协议版本
    switch (c->sockaddr->sa_family) {

    case AF_INET:
        buf = ngx_cpymem(buf, "PROXY TCP4 ", sizeof("PROXY TCP4 ") - 1);
        break;

#if (NGX_HAVE_INET6)
    case AF_INET6:
        buf = ngx_cpymem(buf, "PROXY TCP6 ", sizeof("PROXY TCP6 ") - 1);
        break;
#endif

    default:
        // 对于未知的地址族，使用 UNKNOWN
        return ngx_cpymem(buf, "PROXY UNKNOWN" CRLF,
                          sizeof("PROXY UNKNOWN" CRLF) - 1);
    }

    // 写入源IP地址
    buf += ngx_sock_ntop(c->sockaddr, c->socklen, buf, last - buf, 0);

    *buf++ = ' ';

    // 写入目标IP地址
    buf += ngx_sock_ntop(c->local_sockaddr, c->local_socklen, buf, last - buf,
                         0);

    // 获取源端口和目标端口
    port = ngx_inet_get_port(c->sockaddr);
    lport = ngx_inet_get_port(c->local_sockaddr);

    // 写入源端口和目标端口，并添加CRLF
    return ngx_slprintf(buf, last, " %ui %ui" CRLF, port, lport);
}


static u_char *
ngx_proxy_protocol_v2_read(ngx_connection_t *c, u_char *buf, u_char *last)
{
    u_char                             *end;
    size_t                              len;
    socklen_t                           socklen;
    ngx_uint_t                          version, command, family, transport;
    ngx_sockaddr_t                      src_sockaddr, dst_sockaddr;
    ngx_proxy_protocol_t               *pp;
    ngx_proxy_protocol_header_t        *header;
    ngx_proxy_protocol_inet_addrs_t    *in;
#if (NGX_HAVE_INET6)
    ngx_proxy_protocol_inet6_addrs_t   *in6;
#endif

    // 将缓冲区开始部分解析为PROXY协议头部
    header = (ngx_proxy_protocol_header_t *) buf;

    // 移动缓冲区指针，跳过头部
    buf += sizeof(ngx_proxy_protocol_header_t);

    // 获取PROXY协议版本
    version = header->version_command >> 4;

    // 检查版本是否为2
    if (version != 2) {
        ngx_log_error(NGX_LOG_ERR, c->log, 0,
                      "unknown PROXY protocol version: %ui", version);
        return NULL;
    }

    // 解析头部中的长度字段
    len = ngx_proxy_protocol_parse_uint16(header->len);

    // 检查剩余缓冲区是否足够大
    if ((size_t) (last - buf) < len) {
        ngx_log_error(NGX_LOG_ERR, c->log, 0, "header is too large");
        return NULL;
    }

    // 计算有效数据的结束位置
    end = buf + len;

    // 获取命令类型
    command = header->version_command & 0x0f;

    // 只支持PROXY命令（值为1）
    if (command != 1) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, c->log, 0,
                       "PROXY protocol v2 unsupported command %ui", command);
        return end;
    }

    // 获取传输协议类型
    transport = header->family_transport & 0x0f;

    // 只支持STREAM传输协议（值为1）
    if (transport != 1) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, c->log, 0,
                       "PROXY protocol v2 unsupported transport %ui",
                       transport);
        return end;
    }

    // 分配内存用于存储PROXY协议信息
    pp = ngx_pcalloc(c->pool, sizeof(ngx_proxy_protocol_t));
    if (pp == NULL) {
        return NULL;
    }

    // 获取地址族
    family = header->family_transport >> 4;

    // 根据地址族处理不同的地址格式
    switch (family) {

    case NGX_PROXY_PROTOCOL_AF_INET:
        // 处理IPv4地址

        // 检查剩余数据是否足够
        if ((size_t) (end - buf) < sizeof(ngx_proxy_protocol_inet_addrs_t)) {
            return NULL;
        }

        in = (ngx_proxy_protocol_inet_addrs_t *) buf;

        // 设置源地址
        src_sockaddr.sockaddr_in.sin_family = AF_INET;
        src_sockaddr.sockaddr_in.sin_port = 0;
        ngx_memcpy(&src_sockaddr.sockaddr_in.sin_addr, in->src_addr, 4);

        // 设置目标地址
        dst_sockaddr.sockaddr_in.sin_family = AF_INET;
        dst_sockaddr.sockaddr_in.sin_port = 0;
        ngx_memcpy(&dst_sockaddr.sockaddr_in.sin_addr, in->dst_addr, 4);

        // 解析源端口和目标端口
        pp->src_port = ngx_proxy_protocol_parse_uint16(in->src_port);
        pp->dst_port = ngx_proxy_protocol_parse_uint16(in->dst_port);

        socklen = sizeof(struct sockaddr_in);

        buf += sizeof(ngx_proxy_protocol_inet_addrs_t);

        break;

#if (NGX_HAVE_INET6)

    case NGX_PROXY_PROTOCOL_AF_INET6:
        // 处理IPv6地址

        // 检查剩余数据是否足够
        if ((size_t) (end - buf) < sizeof(ngx_proxy_protocol_inet6_addrs_t)) {
            return NULL;
        }

        in6 = (ngx_proxy_protocol_inet6_addrs_t *) buf;

        // 设置源地址
        src_sockaddr.sockaddr_in6.sin6_family = AF_INET6;
        src_sockaddr.sockaddr_in6.sin6_port = 0;
        ngx_memcpy(&src_sockaddr.sockaddr_in6.sin6_addr, in6->src_addr, 16);

        // 设置目标地址
        dst_sockaddr.sockaddr_in6.sin6_family = AF_INET6;
        dst_sockaddr.sockaddr_in6.sin6_port = 0;
        ngx_memcpy(&dst_sockaddr.sockaddr_in6.sin6_addr, in6->dst_addr, 16);

        // 解析源端口和目标端口
        pp->src_port = ngx_proxy_protocol_parse_uint16(in6->src_port);
        pp->dst_port = ngx_proxy_protocol_parse_uint16(in6->dst_port);

        socklen = sizeof(struct sockaddr_in6);

        buf += sizeof(ngx_proxy_protocol_inet6_addrs_t);

        break;

#endif

    default:
        // 不支持的地址族
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, c->log, 0,
                       "PROXY protocol v2 unsupported address family %ui",
                       family);
        return end;
    }

    // 分配内存并格式化源地址字符串
    pp->src_addr.data = ngx_pnalloc(c->pool, NGX_SOCKADDR_STRLEN);
    if (pp->src_addr.data == NULL) {
        return NULL;
    }

    pp->src_addr.len = ngx_sock_ntop(&src_sockaddr.sockaddr, socklen,
                                     pp->src_addr.data, NGX_SOCKADDR_STRLEN, 0);

    // 分配内存并格式化目标地址字符串
    pp->dst_addr.data = ngx_pnalloc(c->pool, NGX_SOCKADDR_STRLEN);
    if (pp->dst_addr.data == NULL) {
        return NULL;
    }

    pp->dst_addr.len = ngx_sock_ntop(&dst_sockaddr.sockaddr, socklen,
                                     pp->dst_addr.data, NGX_SOCKADDR_STRLEN, 0);

    // 记录调试日志
    ngx_log_debug4(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "PROXY protocol v2 src: %V %d, dst: %V %d",
                   &pp->src_addr, pp->src_port, &pp->dst_addr, pp->dst_port);

    // 处理可能存在的TLV（Type-Length-Value）数据
    if (buf < end) {
        pp->tlvs.data = ngx_pnalloc(c->pool, end - buf);
        if (pp->tlvs.data == NULL) {
            return NULL;
        }

        ngx_memcpy(pp->tlvs.data, buf, end - buf);
        pp->tlvs.len = end - buf;
    }

    // 将PROXY协议信息与连接关联
    c->proxy_protocol = pp;

    return end;
}


ngx_int_t
ngx_proxy_protocol_get_tlv(ngx_connection_t *c, ngx_str_t *name,
    ngx_str_t *value)
{
    u_char                          *p;
    size_t                           n;
    uint32_t                         verify;
    ngx_str_t                        ssl, *tlvs;
    ngx_int_t                        rc, type;
    ngx_proxy_protocol_tlv_ssl_t    *tlv_ssl;
    ngx_proxy_protocol_tlv_entry_t  *te;

    // 检查连接是否有PROXY协议信息
    if (c->proxy_protocol == NULL) {
        return NGX_DECLINED;
    }

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "PROXY protocol v2 get tlv \"%V\"", name);

    // 初始化TLV条目和TLV数据
    te = ngx_proxy_protocol_tlv_entries;
    tlvs = &c->proxy_protocol->tlvs;

    p = name->data;
    n = name->len;

    // 处理SSL相关的TLV
    if (n >= 4 && p[0] == 's' && p[1] == 's' && p[2] == 'l' && p[3] == '_') {

        // 查找SSL TLV
        rc = ngx_proxy_protocol_lookup_tlv(c, tlvs, 0x20, &ssl);
        if (rc != NGX_OK) {
            return rc;
        }

        // 检查SSL TLV长度
        if (ssl.len < sizeof(ngx_proxy_protocol_tlv_ssl_t)) {
            return NGX_ERROR;
        }

        p += 4;
        n -= 4;

        // 处理SSL验证结果
        if (n == 6 && ngx_strncmp(p, "verify", 6) == 0) {

            tlv_ssl = (ngx_proxy_protocol_tlv_ssl_t *) ssl.data;
            verify = ngx_proxy_protocol_parse_uint32(tlv_ssl->verify);

            // 分配内存并格式化验证结果
            value->data = ngx_pnalloc(c->pool, NGX_INT32_LEN);
            if (value->data == NULL) {
                return NGX_ERROR;
            }

            value->len = ngx_sprintf(value->data, "%uD", verify)
                         - value->data;
            return NGX_OK;
        }

        // 更新SSL TLV数据指针和长度
        ssl.data += sizeof(ngx_proxy_protocol_tlv_ssl_t);
        ssl.len -= sizeof(ngx_proxy_protocol_tlv_ssl_t);

        // 切换到SSL特定的TLV条目
        te = ngx_proxy_protocol_tlv_ssl_entries;
        tlvs = &ssl;
    }

    // 处理十六进制格式的TLV类型
    if (n >= 2 && p[0] == '0' && p[1] == 'x') {

        // 将十六进制字符串转换为整数
        type = ngx_hextoi(p + 2, n - 2);
        if (type == NGX_ERROR) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0,
                          "invalid PROXY protocol TLV \"%V\"", name);
            return NGX_ERROR;
        }

        // 查找指定类型的TLV
        return ngx_proxy_protocol_lookup_tlv(c, tlvs, type, value);
    }

    // 遍历TLV条目，查找匹配的名称
    for ( /* void */ ; te->type; te++) {
        if (te->name.len == n && ngx_strncmp(te->name.data, p, n) == 0) {
            return ngx_proxy_protocol_lookup_tlv(c, tlvs, te->type, value);
        }
    }

    // 未找到匹配的TLV，记录错误日志
    ngx_log_error(NGX_LOG_ERR, c->log, 0,
                  "unknown PROXY protocol TLV \"%V\"", name);

    return NGX_DECLINED;
}


static ngx_int_t
ngx_proxy_protocol_lookup_tlv(ngx_connection_t *c, ngx_str_t *tlvs,
    ngx_uint_t type, ngx_str_t *value)
{
    u_char                    *p;
    size_t                     n, len;
    ngx_proxy_protocol_tlv_t  *tlv;

    // 记录调试日志，显示要查找的TLV类型
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "PROXY protocol v2 lookup tlv:%02xi", type);

    // 初始化指针和长度
    p = tlvs->data;
    n = tlvs->len;

    // 遍历所有TLV
    while (n) {
        // 检查剩余数据是否足够包含一个TLV头部
        if (n < sizeof(ngx_proxy_protocol_tlv_t)) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0, "broken PROXY protocol TLV");
            return NGX_ERROR;
        }

        // 解析TLV头部
        tlv = (ngx_proxy_protocol_tlv_t *) p;
        len = ngx_proxy_protocol_parse_uint16(tlv->len);

        // 移动指针和更新剩余长度
        p += sizeof(ngx_proxy_protocol_tlv_t);
        n -= sizeof(ngx_proxy_protocol_tlv_t);

        // 检查TLV值的长度是否合法
        if (n < len) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0, "broken PROXY protocol TLV");
            return NGX_ERROR;
        }

        // 如果找到匹配的TLV类型
        if (tlv->type == type) {
            // 设置返回值并返回成功
            value->data = p;
            value->len = len;
            return NGX_OK;
        }

        // 移动到下一个TLV
        p += len;
        n -= len;
    }

    // 未找到匹配的TLV，返回未找到
    return NGX_DECLINED;
}
