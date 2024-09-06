
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CRYPT_H_INCLUDED_
#define _NGX_CRYPT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 对给定的密钥进行加密
 *
 * @param pool 内存池，用于分配加密结果所需的内存
 * @param key 待加密的密钥
 * @param salt 用于加密的盐值
 * @param encrypted 指向加密结果的指针的指针，函数执行后将包含加密后的字符串
 * @return ngx_int_t 返回NGX_OK表示加密成功，NGX_ERROR表示失败
 */
ngx_int_t ngx_crypt(ngx_pool_t *pool, u_char *key, u_char *salt,
    u_char **encrypted);


#endif /* _NGX_CRYPT_H_INCLUDED_ */
