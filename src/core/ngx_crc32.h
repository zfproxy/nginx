
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CRC32_H_INCLUDED_
#define _NGX_CRC32_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* 短CRC32表的指针,用于快速计算CRC32校验和 */
extern uint32_t  *ngx_crc32_table_short;

/* 完整的256项CRC32表,用于标准CRC32计算 */
extern uint32_t   ngx_crc32_table256[];


/* 使用短表计算CRC32校验和的内联函数 */
static ngx_inline uint32_t
ngx_crc32_short(u_char *p, size_t len)
{
    u_char    c;
    uint32_t  crc;

    /* 初始化CRC值为0xffffffff */
    crc = 0xffffffff;

    while (len--) {
        c = *p++;
        /* 使用短表计算CRC32,每次处理4位 */
        crc = ngx_crc32_table_short[(crc ^ (c & 0xf)) & 0xf] ^ (crc >> 4);
        crc = ngx_crc32_table_short[(crc ^ (c >> 4)) & 0xf] ^ (crc >> 4);
    }

    /* 返回最终的CRC32值 */
    return crc ^ 0xffffffff;
}


/* 使用完整256项表计算CRC32校验和的内联函数 */
static ngx_inline uint32_t
ngx_crc32_long(u_char *p, size_t len)
{
    uint32_t  crc;

    /* 初始化CRC值为0xffffffff */
    crc = 0xffffffff;

    while (len--) {
        /* 使用256项表计算CRC32,每次处理8位 */
        crc = ngx_crc32_table256[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    }

    /* 返回最终的CRC32值 */
    return crc ^ 0xffffffff;
}


/* 初始化CRC32计算的宏定义 */
#define ngx_crc32_init(crc)                                                   \
    crc = 0xffffffff


/* 更新CRC32值的内联函数 */
static ngx_inline void
ngx_crc32_update(uint32_t *crc, u_char *p, size_t len)
{
    uint32_t  c;

    c = *crc;

    while (len--) {
        /* 使用256项表更新CRC32值 */
        c = ngx_crc32_table256[(c ^ *p++) & 0xff] ^ (c >> 8);
    }

    *crc = c;
}


/* 完成CRC32计算的宏定义 */
#define ngx_crc32_final(crc)                                                  \
    crc ^= 0xffffffff


/* CRC32表初始化函数声明 */
ngx_int_t ngx_crc32_table_init(void);


#endif /* _NGX_CRC32_H_INCLUDED_ */
