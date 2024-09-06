
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * 本文件定义了CRC (循环冗余校验) 相关的函数和宏。
 * CRC是一种常用于检测数据传输或存储错误的技术。
 */

#ifndef _NGX_CRC_H_INCLUDED_
#define _NGX_CRC_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* 32位CRC16校验和计算 */

static ngx_inline uint32_t
ngx_crc(u_char *data, size_t len)
{
    uint32_t  sum;

    /* 初始化校验和为0 */
    for (sum = 0; len; len--) {

        /*
         * gcc 2.95.2 x86和icc 7.1.006将此操作编译为单个"rol"指令，
         * msvc 6.0sp2将其编译为四个指令。
         * 这个操作实现了循环左移1位
         */
        sum = sum >> 1 | sum << 31;

        /* 将当前字节加到校验和中 */
        sum += *data++;
    }

    /* 返回计算得到的CRC校验和 */
    return sum;
}


#endif /* _NGX_CRC_H_INCLUDED_ */
