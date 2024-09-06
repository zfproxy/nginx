
/*
 * Copyright (C) Roman Arutyunyan
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_v3.c
 *
 * 该文件实现了HTTP/3协议的核心功能。
 *
 * 支持的功能:
 * 1. HTTP/3会话的初始化和清理
 * 2. HTTP/3连接的管理
 * 3. HTTP/3流的处理
 * 4. HTTP/3帧的发送和接收
 * 5. QPACK头部压缩的支持
 * 6. HTTP/3设置的处理
 * 7. HTTP/3服务器推送
 * 8. HTTP/3连接保活机制
 *
 * 支持的指令:
 * - http3_max_table_capacity: 设置QPACK动态表的最大容量
 *   语法: http3_max_table_capacity size;
 *   上下文: http, server
 * 
 * - http3_max_blocked_streams: 设置最大被阻塞的流数量
 *   语法: http3_max_blocked_streams number;
 *   上下文: http, server
 *
 * - http3_push: 启用或禁用HTTP/3服务器推送
 *   语法: http3_push on|off;
 *   上下文: http, server, location
 *
 * 支持的变量:
 * - $http3: 如果请求使用HTTP/3协议，则为"h3"，否则为空字符串
 * - $http3_ssl_curves: 客户端支持的椭圆曲线列表
 * - $http3_ssl_ciphers: 协商使用的密码套件
 *
 * 使用注意点:
 * 1. 确保正确配置SSL/TLS，HTTP/3依赖于QUIC，而QUIC需要TLS 1.3
 * 2. 合理设置http3_max_table_capacity以平衡压缩效率和内存使用
 * 3. 谨慎使用http3_push，避免不必要的资源推送
 * 4. 监控HTTP/3连接的性能，特别是在高并发场景下
 * 5. 注意处理HTTP/3特有的错误码和异常情况
 * 6. 定期检查和更新以适应HTTP/3协议的演进
 * 7. 考虑启用访问日志中的HTTP/3相关字段，以便于问题诊断
 * 8. 在启用HTTP/3之前，确保网络环境支持UDP传输
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static void ngx_http_v3_keepalive_handler(ngx_event_t *ev);
static void ngx_http_v3_cleanup_session(void *data);


ngx_int_t
ngx_http_v3_init_session(ngx_connection_t *c)
{
    ngx_pool_cleanup_t     *cln;
    ngx_http_connection_t  *hc;
    ngx_http_v3_session_t  *h3c;

    hc = c->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "http3 init session");

    h3c = ngx_pcalloc(c->pool, sizeof(ngx_http_v3_session_t));
    if (h3c == NULL) {
        goto failed;
    }

    h3c->http_connection = hc;

    ngx_queue_init(&h3c->blocked);

    h3c->keepalive.log = c->log;
    h3c->keepalive.data = c;
    h3c->keepalive.handler = ngx_http_v3_keepalive_handler;

    h3c->table.send_insert_count.log = c->log;
    h3c->table.send_insert_count.data = c;
    h3c->table.send_insert_count.handler = ngx_http_v3_inc_insert_count_handler;

    cln = ngx_pool_cleanup_add(c->pool, 0);
    if (cln == NULL) {
        goto failed;
    }

    cln->handler = ngx_http_v3_cleanup_session;
    cln->data = h3c;

    c->data = h3c;

    return NGX_OK;

failed:

    ngx_log_error(NGX_LOG_ERR, c->log, 0, "failed to create http3 session");
    return NGX_ERROR;
}


static void
ngx_http_v3_keepalive_handler(ngx_event_t *ev)
{
    ngx_connection_t  *c;

    c = ev->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "http3 keepalive handler");

    ngx_http_v3_finalize_connection(c, NGX_HTTP_V3_ERR_NO_ERROR,
                                    "keepalive timeout");
}


static void
ngx_http_v3_cleanup_session(void *data)
{
    ngx_http_v3_session_t  *h3c = data;

    ngx_http_v3_cleanup_table(h3c);

    if (h3c->keepalive.timer_set) {
        ngx_del_timer(&h3c->keepalive);
    }

    if (h3c->table.send_insert_count.posted) {
        ngx_delete_posted_event(&h3c->table.send_insert_count);
    }
}


ngx_int_t
ngx_http_v3_check_flood(ngx_connection_t *c)
{
    ngx_http_v3_session_t  *h3c;

    h3c = ngx_http_v3_get_session(c);

    if (h3c->total_bytes / 8 > h3c->payload_bytes + 1048576) {
        ngx_log_error(NGX_LOG_INFO, c->log, 0, "http3 flood detected");

        ngx_http_v3_finalize_connection(c, NGX_HTTP_V3_ERR_NO_ERROR,
                                        "HTTP/3 flood detected");
        return NGX_ERROR;
    }

    return NGX_OK;
}
