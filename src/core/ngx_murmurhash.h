
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MURMURHASH_H_INCLUDED_
#define _NGX_MURMURHASH_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 计算MurmurHash2哈希值
 * 
 * @param data 要计算哈希的数据
 * @param len 数据长度
 * @return uint32_t 返回32位哈希值
 */
uint32_t ngx_murmur_hash2(u_char *data, size_t len);


#endif /* _NGX_MURMURHASH_H_INCLUDED_ */
