
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_USER_H_INCLUDED_
#define _NGX_USER_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* 定义Nginx使用的用户ID类型，基于系统的uid_t */
typedef uid_t  ngx_uid_t;

/* 定义Nginx使用的组ID类型，基于系统的gid_t */
typedef gid_t  ngx_gid_t;


/* 
 * 使用系统的crypt函数进行密码加密
 * @param pool 内存池
 * @param key 待加密的密钥
 * @param salt 加密用的盐值
 * @param encrypted 输出参数，存储加密后的结果
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_libc_crypt(ngx_pool_t *pool, u_char *key, u_char *salt,
    u_char **encrypted);


#endif /* _NGX_USER_H_INCLUDED_ */
