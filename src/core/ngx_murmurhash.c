
/*
 * Copyright (C) Austin Appleby
 */

/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Austin Appleby
 *
 * 本文件实现了MurmurHash2算法，这是一种快速的非加密哈希函数。
 *
 * 支持的功能:
 * 1. 计算32位哈希值
 * 2. 支持任意长度的输入数据
 * 3. 具有良好的分布性和碰撞抗性
 *
 * 使用注意点:
 * 1. 该函数不适用于加密目的，仅用于非安全性场景
 * 2. 输入数据的微小变化可能导致完全不同的哈希结果
 * 3. 对于大量数据或频繁调用，建议考虑性能影响
 * 4. 哈希结果依赖于系统字节序，跨平台使用时需注意
 */


#include <ngx_config.h>
#include <ngx_core.h>


uint32_t
ngx_murmur_hash2(u_char *data, size_t len)
{
    uint32_t  h, k;

    h = 0 ^ len;

    while (len >= 4) {
        k  = data[0];
        k |= data[1] << 8;
        k |= data[2] << 16;
        k |= data[3] << 24;

        k *= 0x5bd1e995;
        k ^= k >> 24;
        k *= 0x5bd1e995;

        h *= 0x5bd1e995;
        h ^= k;

        data += 4;
        len -= 4;
    }

    switch (len) {
    case 3:
        h ^= data[2] << 16;
        /* fall through */
    case 2:
        h ^= data[1] << 8;
        /* fall through */
    case 1:
        h ^= data[0];
        h *= 0x5bd1e995;
    }

    h ^= h >> 13;
    h *= 0x5bd1e995;
    h ^= h >> 15;

    return h;
}
