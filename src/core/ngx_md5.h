
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MD5_H_INCLUDED_
#define _NGX_MD5_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief MD5哈希算法的上下文结构
 */
typedef struct {
    uint64_t  bytes;   /**< 已处理的字节数 */
    uint32_t  a, b, c, d;  /**< MD5算法的四个32位状态变量 */
    u_char    buffer[64];  /**< 64字节的数据缓冲区 */
} ngx_md5_t;


/**
 * @brief 初始化MD5上下文
 * @param ctx 指向ngx_md5_t结构的指针
 */
void ngx_md5_init(ngx_md5_t *ctx);

/**
 * @brief 更新MD5哈希计算
 * @param ctx 指向ngx_md5_t结构的指针
 * @param data 要计算哈希的数据
 * @param size 数据的大小
 */
void ngx_md5_update(ngx_md5_t *ctx, const void *data, size_t size);

/**
 * @brief 完成MD5哈希计算并输出结果
 * @param result 存储16字节MD5哈希结果的数组
 * @param ctx 指向ngx_md5_t结构的指针
 */
void ngx_md5_final(u_char result[16], ngx_md5_t *ctx);


#endif /* _NGX_MD5_H_INCLUDED_ */
