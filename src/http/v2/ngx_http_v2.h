/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 */


#ifndef _NGX_HTTP_V2_H_INCLUDED_
#define _NGX_HTTP_V2_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


// HTTP/2协议的ALPN标识符
#define NGX_HTTP_V2_ALPN_PROTO           "\x02h2"

// HTTP/2状态缓冲区大小
#define NGX_HTTP_V2_STATE_BUFFER_SIZE    16

// HTTP/2默认帧大小和最大帧大小
#define NGX_HTTP_V2_DEFAULT_FRAME_SIZE   (1 << 14)
#define NGX_HTTP_V2_MAX_FRAME_SIZE       ((1 << 24) - 1)

// HTTP/2整数编码的最大字节数和最大字段值
#define NGX_HTTP_V2_INT_OCTETS           4
#define NGX_HTTP_V2_MAX_FIELD                                                 \
    (127 + (1 << (NGX_HTTP_V2_INT_OCTETS - 1) * 7) - 1)

// HTTP/2帧头部大小
#define NGX_HTTP_V2_FRAME_HEADER_SIZE    9

// HTTP/2帧类型定义
#define NGX_HTTP_V2_DATA_FRAME           0x0
#define NGX_HTTP_V2_HEADERS_FRAME        0x1
#define NGX_HTTP_V2_PRIORITY_FRAME       0x2
#define NGX_HTTP_V2_RST_STREAM_FRAME     0x3
#define NGX_HTTP_V2_SETTINGS_FRAME       0x4
#define NGX_HTTP_V2_PUSH_PROMISE_FRAME   0x5
#define NGX_HTTP_V2_PING_FRAME           0x6
#define NGX_HTTP_V2_GOAWAY_FRAME         0x7
#define NGX_HTTP_V2_WINDOW_UPDATE_FRAME  0x8
#define NGX_HTTP_V2_CONTINUATION_FRAME   0x9

// HTTP/2帧标志定义
#define NGX_HTTP_V2_NO_FLAG              0x00
#define NGX_HTTP_V2_ACK_FLAG             0x01
#define NGX_HTTP_V2_END_STREAM_FLAG      0x01
#define NGX_HTTP_V2_END_HEADERS_FLAG     0x04
#define NGX_HTTP_V2_PADDED_FLAG          0x08
#define NGX_HTTP_V2_PRIORITY_FLAG        0x20

// HTTP/2流控窗口相关定义
#define NGX_HTTP_V2_MAX_WINDOW           ((1U << 31) - 1)
#define NGX_HTTP_V2_DEFAULT_WINDOW       65535

// HTTP/2默认权重值
#define NGX_HTTP_V2_DEFAULT_WEIGHT       16


// 前向声明
typedef struct ngx_http_v2_connection_s   ngx_http_v2_connection_t;
typedef struct ngx_http_v2_node_s         ngx_http_v2_node_t;
typedef struct ngx_http_v2_out_frame_s    ngx_http_v2_out_frame_t;


// HTTP/2帧处理函数类型定义
typedef u_char *(*ngx_http_v2_handler_pt) (ngx_http_v2_connection_t *h2c,
    u_char *pos, u_char *end);


// HTTP/2服务器配置结构体
typedef struct {
    ngx_flag_t                       enable;                // 是否启用HTTP/2
    size_t                           pool_size;             // 内存池大小
    ngx_uint_t                       concurrent_streams;    // 并发流数量
    size_t                           preread_size;          // 预读大小
    ngx_uint_t                       streams_index_mask;    // 流索引掩码
} ngx_http_v2_srv_conf_t;


// HTTP/2头部结构体
typedef struct {
    ngx_str_t                        name;                  // 头部名称
    ngx_str_t                        value;                 // 头部值
} ngx_http_v2_header_t;


// HTTP/2状态结构体
typedef struct {
    ngx_uint_t                       sid;                   // 流ID
    size_t                           length;                // 帧长度
    size_t                           padding;               // 填充长度
    unsigned                         flags:8;               // 帧标志

    unsigned                         incomplete:1;          // 是否不完整
    unsigned                         keep_pool:1;           // 是否保持内存池

    /* HPACK */
    unsigned                         parse_name:1;          // 是否解析名称
    unsigned                         parse_value:1;         // 是否解析值
    unsigned                         index:1;               // 是否索引
    ngx_http_v2_header_t             header;                // 头部
    size_t                           header_limit;          // 头部大小限制
    u_char                           field_state;           // 字段状态
    u_char                          *field_start;           // 字段起始位置
    u_char                          *field_end;             // 字段结束位置
    size_t                           field_rest;            // 剩余字段长度
    ngx_pool_t                      *pool;                  // 内存池

    ngx_http_v2_stream_t            *stream;                // 关联的流

    u_char                           buffer[NGX_HTTP_V2_STATE_BUFFER_SIZE];  // 状态缓冲区
    size_t                           buffer_used;           // 已使用的缓冲区大小
    ngx_http_v2_handler_pt           handler;               // 处理函数
} ngx_http_v2_state_t;



typedef struct {
    ngx_http_v2_header_t           **entries;

    ngx_uint_t                       added;
    ngx_uint_t                       deleted;
    ngx_uint_t                       reused;
    ngx_uint_t                       allocated;

    size_t                           size;
    size_t                           free;
    u_char                          *storage;
    u_char                          *pos;
} ngx_http_v2_hpack_t;


// HTTP/2连接结构体
struct ngx_http_v2_connection_s {
    ngx_connection_t                *connection;            // 底层TCP连接
    ngx_http_connection_t           *http_connection;       // HTTP连接

    off_t                            total_bytes;           // 总字节数
    off_t                            payload_bytes;         // 有效负载字节数

    ngx_uint_t                       processing;            // 正在处理的流数量
    ngx_uint_t                       frames;                // 已处理的帧数量
    ngx_uint_t                       idle;                  // 空闲时间
    ngx_uint_t                       new_streams;           // 新创建的流数量
    ngx_uint_t                       refused_streams;       // 被拒绝的流数量
    ngx_uint_t                       priority_limit;        // 优先级限制

    size_t                           send_window;           // 发送窗口大小
    size_t                           recv_window;           // 接收窗口大小
    size_t                           init_window;           // 初始窗口大小

    size_t                           frame_size;            // 帧大小

    ngx_queue_t                      waiting;               // 等待队列

    ngx_http_v2_state_t              state;                 // HTTP/2状态

    ngx_http_v2_hpack_t              hpack;                 // HPACK压缩上下文

    ngx_pool_t                      *pool;                  // 内存池

    ngx_http_v2_out_frame_t         *free_frames;           // 空闲帧链表
    ngx_connection_t                *free_fake_connections; // 空闲伪连接

    ngx_http_v2_node_t             **streams_index;         // 流索引

    ngx_http_v2_out_frame_t         *last_out;              // 最后一个输出帧

    ngx_queue_t                      dependencies;          // 依赖关系队列
    ngx_queue_t                      closed;                // 已关闭流队列

    ngx_uint_t                       closed_nodes;          // 已关闭节点数量
    ngx_uint_t                       last_sid;              // 最后使用的流ID

    time_t                           lingering_time;        // 延迟关闭时间

    unsigned                         settings_ack:1;        // 设置帧确认标志
    unsigned                         table_update:1;        // 表更新标志
    unsigned                         blocked:1;             // 阻塞标志
    unsigned                         goaway:1;              // GOAWAY标志
};


// HTTP/2流节点结构体
struct ngx_http_v2_node_s {
    ngx_uint_t                       id;                    // 节点ID
    ngx_http_v2_node_t              *index;                 // 索引
    ngx_http_v2_node_t              *parent;                // 父节点
    ngx_queue_t                      queue;                 // 队列
    ngx_queue_t                      children;              // 子节点队列
    ngx_queue_t                      reuse;                 // 重用队列
    ngx_uint_t                       rank;                  // 排名
    ngx_uint_t                       weight;                // 权重
    double                           rel_weight;            // 相对权重
    ngx_http_v2_stream_t            *stream;                // 关联的流
};


// HTTP/2流结构体
struct ngx_http_v2_stream_s {
    ngx_http_request_t              *request;               // 关联的HTTP请求
    ngx_http_v2_connection_t        *connection;            // 关联的HTTP/2连接
    ngx_http_v2_node_t              *node;                  // 关联的节点

    ngx_uint_t                       queued;                // 队列中的帧数

    /*
     * A change to SETTINGS_INITIAL_WINDOW_SIZE could cause the
     * send_window to become negative, hence it's signed.
     */
    ssize_t                          send_window;           // 发送窗口大小
    size_t                           recv_window;           // 接收窗口大小

    ngx_buf_t                       *preread;               // 预读缓冲区

    ngx_uint_t                       frames;                // 已处理的帧数

    ngx_http_v2_out_frame_t         *free_frames;           // 空闲帧链表
    ngx_chain_t                     *free_frame_headers;    // 空闲帧头部链表
    ngx_chain_t                     *free_bufs;             // 空闲缓冲区链表

    ngx_queue_t                      queue;                 // 队列

    ngx_array_t                     *cookies;               // Cookie数组

    ngx_pool_t                      *pool;                  // 内存池

    unsigned                         waiting:1;             // 等待标志
    unsigned                         blocked:1;             // 阻塞标志
    unsigned                         exhausted:1;           // 资源耗尽标志
    unsigned                         in_closed:1;           // 输入关闭标志
    unsigned                         out_closed:1;          // 输出关闭标志
    unsigned                         rst_sent:1;            // RST帧已发送标志
    unsigned                         no_flow_control:1;     // 无流控制标志
    unsigned                         skip_data:1;           // 跳过数据标志
};


// HTTP/2输出帧结构体
struct ngx_http_v2_out_frame_s {
    ngx_http_v2_out_frame_t         *next;                  // 下一个帧
    ngx_chain_t                     *first;                 // 第一个缓冲区链
    ngx_chain_t                     *last;                  // 最后一个缓冲区链
    ngx_int_t                      (*handler)(ngx_http_v2_connection_t *h2c,
                                        ngx_http_v2_out_frame_t *frame);  // 处理函数

    ngx_http_v2_stream_t            *stream;                // 关联的流
    size_t                           length;                // 帧长度

    unsigned                         blocked:1;             // 阻塞标志
    unsigned                         fin:1;                 // 结束标志
};


// 将帧加入队列的内联函数
static ngx_inline void
ngx_http_v2_queue_frame(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_out_frame_t *frame)
{
    ngx_http_v2_out_frame_t  **out;

    // 遍历输出帧队列
    for (out = &h2c->last_out; *out; out = &(*out)->next) {

        // 如果遇到阻塞帧或无关联流的帧，停止遍历
        if ((*out)->blocked || (*out)->stream == NULL) {
            break;
        }

        // 根据流的优先级和权重决定插入位置
        if ((*out)->stream->node->rank < frame->stream->node->rank
            || ((*out)->stream->node->rank == frame->stream->node->rank
                && (*out)->stream->node->rel_weight
                   >= frame->stream->node->rel_weight))
        {
            break;
        }
    }

    // 插入新帧
    frame->next = *out;
    *out = frame;
}


// 将阻塞帧加入队列的内联函数
static ngx_inline void
ngx_http_v2_queue_blocked_frame(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_out_frame_t *frame)
{
    ngx_http_v2_out_frame_t  **out;

    // 遍历输出帧队列，找到第一个阻塞帧或无关联流的帧
    for (out = &h2c->last_out; *out; out = &(*out)->next) {

        if ((*out)->blocked || (*out)->stream == NULL) {
            break;
        }
    }

    // 插入新帧
    frame->next = *out;
    *out = frame;
}


// 将有序帧加入队列的内联函数
static ngx_inline void
ngx_http_v2_queue_ordered_frame(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_out_frame_t *frame)
{
    // 直接将帧插入到队列头部
    frame->next = h2c->last_out;
    h2c->last_out = frame;
}


// 初始化HTTP/2连接
void ngx_http_v2_init(ngx_event_t *rev);

// 读取HTTP/2请求体
ngx_int_t ngx_http_v2_read_request_body(ngx_http_request_t *r);
// 读取未缓冲的HTTP/2请求体
ngx_int_t ngx_http_v2_read_unbuffered_request_body(ngx_http_request_t *r);

// 关闭HTTP/2流
void ngx_http_v2_close_stream(ngx_http_v2_stream_t *stream, ngx_int_t rc);

// 发送输出队列中的帧
ngx_int_t ngx_http_v2_send_output_queue(ngx_http_v2_connection_t *h2c);


// 获取静态表中的名称
ngx_str_t *ngx_http_v2_get_static_name(ngx_uint_t index);
// 获取静态表中的值
ngx_str_t *ngx_http_v2_get_static_value(ngx_uint_t index);

// 获取索引的头部
ngx_int_t ngx_http_v2_get_indexed_header(ngx_http_v2_connection_t *h2c,
    ngx_uint_t index, ngx_uint_t name_only);
// 添加头部
ngx_int_t ngx_http_v2_add_header(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_header_t *header);
// 设置动态表大小
ngx_int_t ngx_http_v2_table_size(ngx_http_v2_connection_t *h2c, size_t size);


// 定义一个位掩码，用于提取指定位数的最低位
#define ngx_http_v2_prefix(bits)  ((1 << (bits)) - 1)


#if (NGX_HAVE_NONALIGNED)

// 如果支持非对齐内存访问，直接使用ntohs和ntohl函数解析16位和32位整数
#define ngx_http_v2_parse_uint16(p)  ntohs(*(uint16_t *) (p))
#define ngx_http_v2_parse_uint32(p)  ntohl(*(uint32_t *) (p))

#else

// 如果不支持非对齐内存访问，手动解析16位和32位整数
#define ngx_http_v2_parse_uint16(p)  ((p)[0] << 8 | (p)[1])
#define ngx_http_v2_parse_uint32(p)                                           \
    ((uint32_t) (p)[0] << 24 | (p)[1] << 16 | (p)[2] << 8 | (p)[3])

#endif

// 解析HTTP/2帧的长度（24位）
#define ngx_http_v2_parse_length(p)  ((p) >> 8)
// 解析HTTP/2帧的类型（8位）
#define ngx_http_v2_parse_type(p)    ((p) & 0xff)
// 解析HTTP/2帧的流标识符（31位）
#define ngx_http_v2_parse_sid(p)     (ngx_http_v2_parse_uint32(p) & 0x7fffffff)
// 解析HTTP/2窗口更新帧的窗口大小增量（31位）
#define ngx_http_v2_parse_window(p)  (ngx_http_v2_parse_uint32(p) & 0x7fffffff)


// 写入16位对齐的无符号整数
#define ngx_http_v2_write_uint16_aligned(p, s)                                \
    (*(uint16_t *) (p) = htons((uint16_t) (s)), (p) + sizeof(uint16_t))
// 写入32位对齐的无符号整数
#define ngx_http_v2_write_uint32_aligned(p, s)                                \
    (*(uint32_t *) (p) = htonl((uint32_t) (s)), (p) + sizeof(uint32_t))

#if (NGX_HAVE_NONALIGNED)

// 如果支持非对齐内存访问，直接使用对齐的写入函数
#define ngx_http_v2_write_uint16  ngx_http_v2_write_uint16_aligned
#define ngx_http_v2_write_uint32  ngx_http_v2_write_uint32_aligned

#else

// 如果不支持非对齐内存访问，手动写入16位和32位整数
#define ngx_http_v2_write_uint16(p, s)                                        \
    ((p)[0] = (u_char) ((s) >> 8),                                            \
     (p)[1] = (u_char)  (s),                                                  \
     (p) + sizeof(uint16_t))

#define ngx_http_v2_write_uint32(p, s)                                        \
    ((p)[0] = (u_char) ((s) >> 24),                                           \
     (p)[1] = (u_char) ((s) >> 16),                                           \
     (p)[2] = (u_char) ((s) >> 8),                                            \
     (p)[3] = (u_char)  (s),                                                  \
     (p) + sizeof(uint32_t))

#endif

// 写入长度和类型到HTTP/2帧头
// p: 目标缓冲区指针
// l: 帧长度（24位）
// t: 帧类型（8位）
#define ngx_http_v2_write_len_and_type(p, l, t)                               \
    ngx_http_v2_write_uint32_aligned(p, (l) << 8 | (t))

// 写入流标识符（31位）
#define ngx_http_v2_write_sid  ngx_http_v2_write_uint32


// 计算索引表示的值（用于HPACK静态表和动态表）
#define ngx_http_v2_indexed(i)      (128 + (i))
#define ngx_http_v2_inc_indexed(i)  (64 + (i))

// 编码HTTP/2头部名称和值
#define ngx_http_v2_write_name(dst, src, len, tmp)                            \
    ngx_http_v2_string_encode(dst, src, len, tmp, 1)
#define ngx_http_v2_write_value(dst, src, len, tmp)                           \
    ngx_http_v2_string_encode(dst, src, len, tmp, 0)

// HTTP/2头部编码方式
#define NGX_HTTP_V2_ENCODE_RAW            0    // 原始编码，不进行Huffman压缩
#define NGX_HTTP_V2_ENCODE_HUFF           0x80 // Huffman编码，使用Huffman压缩

// HPACK静态表中常用头部字段的索引
// HPACK静态表中的索引值定义

// :authority 伪头部字段的索引
#define NGX_HTTP_V2_AUTHORITY_INDEX       1

// 请求方法相关的索引
#define NGX_HTTP_V2_METHOD_INDEX          2  // 通用方法索引
#define NGX_HTTP_V2_METHOD_GET_INDEX      2  // GET方法索引
#define NGX_HTTP_V2_METHOD_POST_INDEX     3  // POST方法索引

// 请求路径相关的索引
#define NGX_HTTP_V2_PATH_INDEX            4  // 通用路径索引
#define NGX_HTTP_V2_PATH_ROOT_INDEX       4  // 根路径"/"的索引

// 请求协议相关的索引
#define NGX_HTTP_V2_SCHEME_HTTP_INDEX     6  // HTTP协议索引
#define NGX_HTTP_V2_SCHEME_HTTPS_INDEX    7  // HTTPS协议索引

// 响应状态码相关的索引
#define NGX_HTTP_V2_STATUS_INDEX          8  // 通用状态码索引
#define NGX_HTTP_V2_STATUS_200_INDEX      8  // 200 OK
#define NGX_HTTP_V2_STATUS_204_INDEX      9  // 204 No Content
#define NGX_HTTP_V2_STATUS_206_INDEX      10 // 206 Partial Content
#define NGX_HTTP_V2_STATUS_304_INDEX      11 // 304 Not Modified
#define NGX_HTTP_V2_STATUS_400_INDEX      12 // 400 Bad Request
#define NGX_HTTP_V2_STATUS_404_INDEX      13 // 404 Not Found
#define NGX_HTTP_V2_STATUS_500_INDEX      14 // 500 Internal Server Error

// 其他常用HTTP头部字段的索引
#define NGX_HTTP_V2_CONTENT_LENGTH_INDEX  28 // Content-Length头部索引
#define NGX_HTTP_V2_CONTENT_TYPE_INDEX    31 // Content-Type头部索引
#define NGX_HTTP_V2_DATE_INDEX            33 // Date头部索引
#define NGX_HTTP_V2_LAST_MODIFIED_INDEX   44 // Last-Modified头部索引
#define NGX_HTTP_V2_LOCATION_INDEX        46 // Location头部索引
#define NGX_HTTP_V2_SERVER_INDEX          54 // Server头部索引
#define NGX_HTTP_V2_VARY_INDEX            59 // Vary头部索引

// HTTP/2连接前言（客户端发送的magic字符串）
#define NGX_HTTP_V2_PREFACE_START         "PRI * HTTP/2.0\r\n"
#define NGX_HTTP_V2_PREFACE_END           "\r\nSM\r\n\r\n"
#define NGX_HTTP_V2_PREFACE               NGX_HTTP_V2_PREFACE_START           \
                                          NGX_HTTP_V2_PREFACE_END


// 字符串编码函数声明
u_char *ngx_http_v2_string_encode(u_char *dst, u_char *src, size_t len,
    u_char *tmp, ngx_uint_t lower);


// 声明HTTP/2模块
extern ngx_module_t  ngx_http_v2_module;


#endif /* _NGX_HTTP_V2_H_INCLUDED_ */
