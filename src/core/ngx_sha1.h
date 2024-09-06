
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SHA1_H_INCLUDED_
#define _NGX_SHA1_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief SHA1哈希算法的上下文结构体
 */
typedef struct {
    uint64_t  bytes;    /**< 已处理的字节数 */
    uint32_t  a, b, c, d, e, f;  /**< SHA1算法的中间状态变量 */
    u_char    buffer[64];  /**< 64字节的数据缓冲区 */
} ngx_sha1_t;

/**
 * @brief 初始化SHA1上下文
 * @param ctx 指向SHA1上下文结构体的指针
 */
void ngx_sha1_init(ngx_sha1_t *ctx);

/**
 * @brief 更新SHA1哈希计算
 * @param ctx 指向SHA1上下文结构体的指针
 * @param data 要计算哈希的数据
 * @param size 数据的大小
 */
void ngx_sha1_update(ngx_sha1_t *ctx, const void *data, size_t size);

/**
 * @brief 完成SHA1哈希计算并输出结果
 * @param result 存储20字节SHA1哈希结果的数组
 * @param ctx 指向SHA1上下文结构体的指针
 */
void ngx_sha1_final(u_char result[20], ngx_sha1_t *ctx);


#endif /* _NGX_SHA1_H_INCLUDED_ */
