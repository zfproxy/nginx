
/*
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SYSLOG_H_INCLUDED_
#define _NGX_SYSLOG_H_INCLUDED_


/**
 * @brief Syslog对等体结构体，用于存储Syslog相关配置和状态
 */
typedef struct {
    ngx_uint_t         facility;    /**< Syslog设施 */
    ngx_uint_t         severity;    /**< Syslog严重程度 */
    ngx_str_t          tag;         /**< Syslog标签 */

    ngx_str_t         *hostname;    /**< 主机名 */

    ngx_addr_t         server;      /**< Syslog服务器地址 */
    ngx_connection_t   conn;        /**< 与Syslog服务器的连接 */

    ngx_log_t          log;         /**< 日志对象 */
    ngx_log_t         *logp;        /**< 日志对象指针 */

    unsigned           busy:1;      /**< 标记是否忙碌 */
    unsigned           nohostname:1; /**< 标记是否不使用主机名 */
} ngx_syslog_peer_t;


/**
 * @brief 处理Syslog配置
 * @param cf 配置对象
 * @param peer Syslog对等体
 * @return 成功返回NULL，失败返回错误信息
 */
char *ngx_syslog_process_conf(ngx_conf_t *cf, ngx_syslog_peer_t *peer);

/**
 * @brief 添加Syslog头部
 * @param peer Syslog对等体
 * @param buf 缓冲区
 * @return 添加头部后的缓冲区位置
 */
u_char *ngx_syslog_add_header(ngx_syslog_peer_t *peer, u_char *buf);

/**
 * @brief Syslog写入函数
 * @param log 日志对象
 * @param level 日志级别
 * @param buf 日志内容缓冲区
 * @param len 日志内容长度
 */
void ngx_syslog_writer(ngx_log_t *log, ngx_uint_t level, u_char *buf,
    size_t len);

/**
 * @brief 发送Syslog消息
 * @param peer Syslog对等体
 * @param buf 消息缓冲区
 * @param len 消息长度
 * @return 发送的字节数，失败返回-1
 */
ssize_t ngx_syslog_send(ngx_syslog_peer_t *peer, u_char *buf, size_t len);


#endif /* _NGX_SYSLOG_H_INCLUDED_ */
