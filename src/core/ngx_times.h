
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_TIMES_H_INCLUDED_
#define _NGX_TIMES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 时间结构体，用于存储时间信息
 */
typedef struct {
    time_t      sec;     /**< 秒数 */
    ngx_uint_t  msec;    /**< 毫秒数 */
    ngx_int_t   gmtoff;  /**< GMT偏移量 */
} ngx_time_t;


/**
 * @brief 初始化时间系统
 */
void ngx_time_init(void);

/**
 * @brief 更新当前时间
 */
void ngx_time_update(void);

/**
 * @brief 信号安全的时间更新
 */
void ngx_time_sigsafe_update(void);

/**
 * @brief 生成HTTP格式的时间字符串
 * @param buf 输出缓冲区
 * @param t 时间戳
 * @return 生成的时间字符串
 */
u_char *ngx_http_time(u_char *buf, time_t t);

/**
 * @brief 生成HTTP cookie格式的时间字符串
 * @param buf 输出缓冲区
 * @param t 时间戳
 * @return 生成的时间字符串
 */
u_char *ngx_http_cookie_time(u_char *buf, time_t t);

/**
 * @brief 将时间戳转换为GMT时间结构
 * @param t 时间戳
 * @param tp 输出的时间结构指针
 */
void ngx_gmtime(time_t t, ngx_tm_t *tp);

/**
 * @brief 计算下一个时间点
 * @param when 目标时间
 * @return 下一个时间点的时间戳
 */
time_t ngx_next_time(time_t when);

/**
 * @brief 定义ngx_next_time_n为"mktime()"字符串
 */
#define ngx_next_time_n      "mktime()"


/**
 * @brief 缓存的当前时间指针
 */
extern volatile ngx_time_t  *ngx_cached_time;

/**
 * @brief 获取当前时间的秒数
 */
#define ngx_time()           ngx_cached_time->sec

/**
 * @brief 获取当前时间的完整时间结构
 */
#define ngx_timeofday()      (ngx_time_t *) ngx_cached_time

/**
 * @brief 缓存的错误日志时间字符串
 */
extern volatile ngx_str_t    ngx_cached_err_log_time;

/**
 * @brief 缓存的HTTP时间字符串
 */
extern volatile ngx_str_t    ngx_cached_http_time;

/**
 * @brief 缓存的HTTP日志时间字符串
 */
extern volatile ngx_str_t    ngx_cached_http_log_time;

/**
 * @brief 缓存的HTTP日志ISO8601格式时间字符串
 */
extern volatile ngx_str_t    ngx_cached_http_log_iso8601;

/**
 * @brief 缓存的系统日志时间字符串
 */
extern volatile ngx_str_t    ngx_cached_syslog_time;

/**
 * 自某个未指定的过去时间点以来经过的毫秒数，
 * 截断为ngx_msec_t类型，用于事件定时器
 */
extern volatile ngx_msec_t  ngx_current_msec;


#endif /* _NGX_TIMES_H_INCLUDED_ */
