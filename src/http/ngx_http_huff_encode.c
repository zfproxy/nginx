
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 * Copyright (C) 2015 Vlad Krasnov
 */


/*
 * ngx_http_huff_encode.c
 *
 * 该文件实现了HTTP/2中使用的Huffman编码功能。
 *
 * 支持的功能:
 * 1. 将ASCII字符编码为Huffman编码
 * 2. 支持完整的256字符ASCII集的Huffman编码
 * 3. 提供高效的查表编码方法
 * 4. 支持位级别的编码操作
 * 5. 可以处理任意长度的输入字符串
 * 6. 提供编码后的长度计算功能
 *
 * 使用注意点:
 * 1. 确保输入的字符串是有效的ASCII字符
 * 2. 注意处理编码过程中可能出现的内存分配失败情况
 * 3. 在高并发环境下使用时，需考虑线程安全性
 * 4. 编码大量数据时，注意监控CPU使用率，因为编码过程可能较为密集
 * 5. 定期检查和更新Huffman编码表，以适应HTTP/2协议的可能变化
 * 6. 在错误处理中，应该有合适的日志记录机制，以便于问题诊断
 * 7. 考虑使用缓存机制来优化频繁编码的场景
 * 8. 在嵌入式或资源受限的环境中使用时，需注意内存使用情况
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/*
 * 定义一个结构体用于存储Huffman编码
 * 这个结构体将在ngx_http_huff_encode_table中使用
 * 用于表示每个ASCII字符的Huffman编码
 */
typedef struct {
    uint32_t  code;
    uint32_t  len;
} ngx_http_huff_encode_code_t;


/*
 * 定义Huffman编码表
 * 这个表包含256个条目，对应ASCII字符集中的每个字符
 * 每个条目包含两个部分:
 * 1. code: 32位无符号整数，表示Huffman编码
 * 2. len: 32位无符号整数，表示编码的位长度
 * 这个表用于HTTP/2头部压缩中的Huffman编码
 */
static ngx_http_huff_encode_code_t  ngx_http_huff_encode_table[256] =
{
    {0x00001ff8, 13}, {0x007fffd8, 23}, {0x0fffffe2, 28}, {0x0fffffe3, 28},
    {0x0fffffe4, 28}, {0x0fffffe5, 28}, {0x0fffffe6, 28}, {0x0fffffe7, 28},
    {0x0fffffe8, 28}, {0x00ffffea, 24}, {0x3ffffffc, 30}, {0x0fffffe9, 28},
    {0x0fffffea, 28}, {0x3ffffffd, 30}, {0x0fffffeb, 28}, {0x0fffffec, 28},
    {0x0fffffed, 28}, {0x0fffffee, 28}, {0x0fffffef, 28}, {0x0ffffff0, 28},
    {0x0ffffff1, 28}, {0x0ffffff2, 28}, {0x3ffffffe, 30}, {0x0ffffff3, 28},
    {0x0ffffff4, 28}, {0x0ffffff5, 28}, {0x0ffffff6, 28}, {0x0ffffff7, 28},
    {0x0ffffff8, 28}, {0x0ffffff9, 28}, {0x0ffffffa, 28}, {0x0ffffffb, 28},
    {0x00000014,  6}, {0x000003f8, 10}, {0x000003f9, 10}, {0x00000ffa, 12},
    {0x00001ff9, 13}, {0x00000015,  6}, {0x000000f8,  8}, {0x000007fa, 11},
    {0x000003fa, 10}, {0x000003fb, 10}, {0x000000f9,  8}, {0x000007fb, 11},
    {0x000000fa,  8}, {0x00000016,  6}, {0x00000017,  6}, {0x00000018,  6},
    {0x00000000,  5}, {0x00000001,  5}, {0x00000002,  5}, {0x00000019,  6},
    {0x0000001a,  6}, {0x0000001b,  6}, {0x0000001c,  6}, {0x0000001d,  6},
    {0x0000001e,  6}, {0x0000001f,  6}, {0x0000005c,  7}, {0x000000fb,  8},
    {0x00007ffc, 15}, {0x00000020,  6}, {0x00000ffb, 12}, {0x000003fc, 10},
    {0x00001ffa, 13}, {0x00000021,  6}, {0x0000005d,  7}, {0x0000005e,  7},
    {0x0000005f,  7}, {0x00000060,  7}, {0x00000061,  7}, {0x00000062,  7},
    {0x00000063,  7}, {0x00000064,  7}, {0x00000065,  7}, {0x00000066,  7},
    {0x00000067,  7}, {0x00000068,  7}, {0x00000069,  7}, {0x0000006a,  7},
    {0x0000006b,  7}, {0x0000006c,  7}, {0x0000006d,  7}, {0x0000006e,  7},
    {0x0000006f,  7}, {0x00000070,  7}, {0x00000071,  7}, {0x00000072,  7},
    {0x000000fc,  8}, {0x00000073,  7}, {0x000000fd,  8}, {0x00001ffb, 13},
    {0x0007fff0, 19}, {0x00001ffc, 13}, {0x00003ffc, 14}, {0x00000022,  6},
    {0x00007ffd, 15}, {0x00000003,  5}, {0x00000023,  6}, {0x00000004,  5},
    {0x00000024,  6}, {0x00000005,  5}, {0x00000025,  6}, {0x00000026,  6},
    {0x00000027,  6}, {0x00000006,  5}, {0x00000074,  7}, {0x00000075,  7},
    {0x00000028,  6}, {0x00000029,  6}, {0x0000002a,  6}, {0x00000007,  5},
    {0x0000002b,  6}, {0x00000076,  7}, {0x0000002c,  6}, {0x00000008,  5},
    {0x00000009,  5}, {0x0000002d,  6}, {0x00000077,  7}, {0x00000078,  7},
    {0x00000079,  7}, {0x0000007a,  7}, {0x0000007b,  7}, {0x00007ffe, 15},
    {0x000007fc, 11}, {0x00003ffd, 14}, {0x00001ffd, 13}, {0x0ffffffc, 28},
    {0x000fffe6, 20}, {0x003fffd2, 22}, {0x000fffe7, 20}, {0x000fffe8, 20},
    {0x003fffd3, 22}, {0x003fffd4, 22}, {0x003fffd5, 22}, {0x007fffd9, 23},
    {0x003fffd6, 22}, {0x007fffda, 23}, {0x007fffdb, 23}, {0x007fffdc, 23},
    {0x007fffdd, 23}, {0x007fffde, 23}, {0x00ffffeb, 24}, {0x007fffdf, 23},
    {0x00ffffec, 24}, {0x00ffffed, 24}, {0x003fffd7, 22}, {0x007fffe0, 23},
    {0x00ffffee, 24}, {0x007fffe1, 23}, {0x007fffe2, 23}, {0x007fffe3, 23},
    {0x007fffe4, 23}, {0x001fffdc, 21}, {0x003fffd8, 22}, {0x007fffe5, 23},
    {0x003fffd9, 22}, {0x007fffe6, 23}, {0x007fffe7, 23}, {0x00ffffef, 24},
    {0x003fffda, 22}, {0x001fffdd, 21}, {0x000fffe9, 20}, {0x003fffdb, 22},
    {0x003fffdc, 22}, {0x007fffe8, 23}, {0x007fffe9, 23}, {0x001fffde, 21},
    {0x007fffea, 23}, {0x003fffdd, 22}, {0x003fffde, 22}, {0x00fffff0, 24},
    {0x001fffdf, 21}, {0x003fffdf, 22}, {0x007fffeb, 23}, {0x007fffec, 23},
    {0x001fffe0, 21}, {0x001fffe1, 21}, {0x003fffe0, 22}, {0x001fffe2, 21},
    {0x007fffed, 23}, {0x003fffe1, 22}, {0x007fffee, 23}, {0x007fffef, 23},
    {0x000fffea, 20}, {0x003fffe2, 22}, {0x003fffe3, 22}, {0x003fffe4, 22},
    {0x007ffff0, 23}, {0x003fffe5, 22}, {0x003fffe6, 22}, {0x007ffff1, 23},
    {0x03ffffe0, 26}, {0x03ffffe1, 26}, {0x000fffeb, 20}, {0x0007fff1, 19},
    {0x003fffe7, 22}, {0x007ffff2, 23}, {0x003fffe8, 22}, {0x01ffffec, 25},
    {0x03ffffe2, 26}, {0x03ffffe3, 26}, {0x03ffffe4, 26}, {0x07ffffde, 27},
    {0x07ffffdf, 27}, {0x03ffffe5, 26}, {0x00fffff1, 24}, {0x01ffffed, 25},
    {0x0007fff2, 19}, {0x001fffe3, 21}, {0x03ffffe6, 26}, {0x07ffffe0, 27},
    {0x07ffffe1, 27}, {0x03ffffe7, 26}, {0x07ffffe2, 27}, {0x00fffff2, 24},
    {0x001fffe4, 21}, {0x001fffe5, 21}, {0x03ffffe8, 26}, {0x03ffffe9, 26},
    {0x0ffffffd, 28}, {0x07ffffe3, 27}, {0x07ffffe4, 27}, {0x07ffffe5, 27},
    {0x000fffec, 20}, {0x00fffff3, 24}, {0x000fffed, 20}, {0x001fffe6, 21},
    {0x003fffe9, 22}, {0x001fffe7, 21}, {0x001fffe8, 21}, {0x007ffff3, 23},
    {0x003fffea, 22}, {0x003fffeb, 22}, {0x01ffffee, 25}, {0x01ffffef, 25},
    {0x00fffff4, 24}, {0x00fffff5, 24}, {0x03ffffea, 26}, {0x007ffff4, 23},
    {0x03ffffeb, 26}, {0x07ffffe6, 27}, {0x03ffffec, 26}, {0x03ffffed, 26},
    {0x07ffffe7, 27}, {0x07ffffe8, 27}, {0x07ffffe9, 27}, {0x07ffffea, 27},
    {0x07ffffeb, 27}, {0x0ffffffe, 28}, {0x07ffffec, 27}, {0x07ffffed, 27},
    {0x07ffffee, 27}, {0x07ffffef, 27}, {0x07fffff0, 27}, {0x03ffffee, 26}
};


/* same as above, but embeds lowercase transformation */
/* 
 * 定义一个静态数组 ngx_http_huff_encode_table_lc，用于存储 HTTP/2 Huffman 编码表
 * 这个表与上面的表相同，但嵌入了小写转换
 * 数组大小为 256，对应 ASCII 字符集的所有可能值
 * 每个元素都是 ngx_http_huff_encode_code_t 类型，包含 Huffman 编码和其位长
 */
static ngx_http_huff_encode_code_t  ngx_http_huff_encode_table_lc[256] =
{
    {0x00001ff8, 13}, {0x007fffd8, 23}, {0x0fffffe2, 28}, {0x0fffffe3, 28},
    {0x0fffffe4, 28}, {0x0fffffe5, 28}, {0x0fffffe6, 28}, {0x0fffffe7, 28},
    {0x0fffffe8, 28}, {0x00ffffea, 24}, {0x3ffffffc, 30}, {0x0fffffe9, 28},
    {0x0fffffea, 28}, {0x3ffffffd, 30}, {0x0fffffeb, 28}, {0x0fffffec, 28},
    {0x0fffffed, 28}, {0x0fffffee, 28}, {0x0fffffef, 28}, {0x0ffffff0, 28},
    {0x0ffffff1, 28}, {0x0ffffff2, 28}, {0x3ffffffe, 30}, {0x0ffffff3, 28},
    {0x0ffffff4, 28}, {0x0ffffff5, 28}, {0x0ffffff6, 28}, {0x0ffffff7, 28},
    {0x0ffffff8, 28}, {0x0ffffff9, 28}, {0x0ffffffa, 28}, {0x0ffffffb, 28},
    {0x00000014,  6}, {0x000003f8, 10}, {0x000003f9, 10}, {0x00000ffa, 12},
    {0x00001ff9, 13}, {0x00000015,  6}, {0x000000f8,  8}, {0x000007fa, 11},
    {0x000003fa, 10}, {0x000003fb, 10}, {0x000000f9,  8}, {0x000007fb, 11},
    {0x000000fa,  8}, {0x00000016,  6}, {0x00000017,  6}, {0x00000018,  6},
    {0x00000000,  5}, {0x00000001,  5}, {0x00000002,  5}, {0x00000019,  6},
    {0x0000001a,  6}, {0x0000001b,  6}, {0x0000001c,  6}, {0x0000001d,  6},
    {0x0000001e,  6}, {0x0000001f,  6}, {0x0000005c,  7}, {0x000000fb,  8},
    {0x00007ffc, 15}, {0x00000020,  6}, {0x00000ffb, 12}, {0x000003fc, 10},
    {0x00001ffa, 13}, {0x00000003,  5}, {0x00000023,  6}, {0x00000004,  5},
    {0x00000024,  6}, {0x00000005,  5}, {0x00000025,  6}, {0x00000026,  6},
    {0x00000027,  6}, {0x00000006,  5}, {0x00000074,  7}, {0x00000075,  7},
    {0x00000028,  6}, {0x00000029,  6}, {0x0000002a,  6}, {0x00000007,  5},
    {0x0000002b,  6}, {0x00000076,  7}, {0x0000002c,  6}, {0x00000008,  5},
    {0x00000009,  5}, {0x0000002d,  6}, {0x00000077,  7}, {0x00000078,  7},
    {0x00000079,  7}, {0x0000007a,  7}, {0x0000007b,  7}, {0x00001ffb, 13},
    {0x0007fff0, 19}, {0x00001ffc, 13}, {0x00003ffc, 14}, {0x00000022,  6},
    {0x00007ffd, 15}, {0x00000003,  5}, {0x00000023,  6}, {0x00000004,  5},
    {0x00000024,  6}, {0x00000005,  5}, {0x00000025,  6}, {0x00000026,  6},
    {0x00000027,  6}, {0x00000006,  5}, {0x00000074,  7}, {0x00000075,  7},
    {0x00000028,  6}, {0x00000029,  6}, {0x0000002a,  6}, {0x00000007,  5},
    {0x0000002b,  6}, {0x00000076,  7}, {0x0000002c,  6}, {0x00000008,  5},
    {0x00000009,  5}, {0x0000002d,  6}, {0x00000077,  7}, {0x00000078,  7},
    {0x00000079,  7}, {0x0000007a,  7}, {0x0000007b,  7}, {0x00007ffe, 15},
    {0x000007fc, 11}, {0x00003ffd, 14}, {0x00001ffd, 13}, {0x0ffffffc, 28},
    {0x000fffe6, 20}, {0x003fffd2, 22}, {0x000fffe7, 20}, {0x000fffe8, 20},
    {0x003fffd3, 22}, {0x003fffd4, 22}, {0x003fffd5, 22}, {0x007fffd9, 23},
    {0x003fffd6, 22}, {0x007fffda, 23}, {0x007fffdb, 23}, {0x007fffdc, 23},
    {0x007fffdd, 23}, {0x007fffde, 23}, {0x00ffffeb, 24}, {0x007fffdf, 23},
    {0x00ffffec, 24}, {0x00ffffed, 24}, {0x003fffd7, 22}, {0x007fffe0, 23},
    {0x00ffffee, 24}, {0x007fffe1, 23}, {0x007fffe2, 23}, {0x007fffe3, 23},
    {0x007fffe4, 23}, {0x001fffdc, 21}, {0x003fffd8, 22}, {0x007fffe5, 23},
    {0x003fffd9, 22}, {0x007fffe6, 23}, {0x007fffe7, 23}, {0x00ffffef, 24},
    {0x003fffda, 22}, {0x001fffdd, 21}, {0x000fffe9, 20}, {0x003fffdb, 22},
    {0x003fffdc, 22}, {0x007fffe8, 23}, {0x007fffe9, 23}, {0x001fffde, 21},
    {0x007fffea, 23}, {0x003fffdd, 22}, {0x003fffde, 22}, {0x00fffff0, 24},
    {0x001fffdf, 21}, {0x003fffdf, 22}, {0x007fffeb, 23}, {0x007fffec, 23},
    {0x001fffe0, 21}, {0x001fffe1, 21}, {0x003fffe0, 22}, {0x001fffe2, 21},
    {0x007fffed, 23}, {0x003fffe1, 22}, {0x007fffee, 23}, {0x007fffef, 23},
    {0x000fffea, 20}, {0x003fffe2, 22}, {0x003fffe3, 22}, {0x003fffe4, 22},
    {0x007ffff0, 23}, {0x003fffe5, 22}, {0x003fffe6, 22}, {0x007ffff1, 23},
    {0x03ffffe0, 26}, {0x03ffffe1, 26}, {0x000fffeb, 20}, {0x0007fff1, 19},
    {0x003fffe7, 22}, {0x007ffff2, 23}, {0x003fffe8, 22}, {0x01ffffec, 25},
    {0x03ffffe2, 26}, {0x03ffffe3, 26}, {0x03ffffe4, 26}, {0x07ffffde, 27},
    {0x07ffffdf, 27}, {0x03ffffe5, 26}, {0x00fffff1, 24}, {0x01ffffed, 25},
    {0x0007fff2, 19}, {0x001fffe3, 21}, {0x03ffffe6, 26}, {0x07ffffe0, 27},
    {0x07ffffe1, 27}, {0x03ffffe7, 26}, {0x07ffffe2, 27}, {0x00fffff2, 24},
    {0x001fffe4, 21}, {0x001fffe5, 21}, {0x03ffffe8, 26}, {0x03ffffe9, 26},
    {0x0ffffffd, 28}, {0x07ffffe3, 27}, {0x07ffffe4, 27}, {0x07ffffe5, 27},
    {0x000fffec, 20}, {0x00fffff3, 24}, {0x000fffed, 20}, {0x001fffe6, 21},
    {0x003fffe9, 22}, {0x001fffe7, 21}, {0x001fffe8, 21}, {0x007ffff3, 23},
    {0x003fffea, 22}, {0x003fffeb, 22}, {0x01ffffee, 25}, {0x01ffffef, 25},
    {0x00fffff4, 24}, {0x00fffff5, 24}, {0x03ffffea, 26}, {0x007ffff4, 23},
    {0x03ffffeb, 26}, {0x07ffffe6, 27}, {0x03ffffec, 26}, {0x03ffffed, 26},
    {0x07ffffe7, 27}, {0x07ffffe8, 27}, {0x07ffffe9, 27}, {0x07ffffea, 27},
    {0x07ffffeb, 27}, {0x0ffffffe, 28}, {0x07ffffec, 27}, {0x07ffffed, 27},
    {0x07ffffee, 27}, {0x07ffffef, 27}, {0x07fffff0, 27}, {0x03ffffee, 26}
};


#if (NGX_PTR_SIZE == 8)

#if (NGX_HAVE_LITTLE_ENDIAN)

#if (NGX_HAVE_GCC_BSWAP64)
/* 定义一个宏函数，用于将Huffman编码的缓冲区写入目标地址 */
/* dst: 目标地址指针 */
/* buf: 包含Huffman编码的缓冲区 */
#define ngx_http_huff_encode_buf(dst, buf)                                    \
    (*(uint64_t *) (dst) = __builtin_bswap64(buf))
#else
/* 定义一个宏函数，用于将Huffman编码的缓冲区写入目标地址 */
/* dst: 目标地址指针 */
/* buf: 包含Huffman编码的缓冲区 */
#define ngx_http_huff_encode_buf(dst, buf)                                    \
    ((dst)[0] = (u_char) ((buf) >> 56),                                       \
     (dst)[1] = (u_char) ((buf) >> 48),                                       \
     (dst)[2] = (u_char) ((buf) >> 40),                                       \
     (dst)[3] = (u_char) ((buf) >> 32),                                       \
     (dst)[4] = (u_char) ((buf) >> 24),                                       \
     (dst)[5] = (u_char) ((buf) >> 16),                                       \
     (dst)[6] = (u_char) ((buf) >> 8),                                        \
     (dst)[7] = (u_char)  (buf))
#endif

#else /* !NGX_HAVE_LITTLE_ENDIAN */
/* 定义一个宏函数，用于将Huffman编码的缓冲区写入目标地址 */
/* dst: 目标地址指针 */
/* buf: 包含Huffman编码的缓冲区 */
/* 在大端字节序系统上，直接将buf写入dst */
#define ngx_http_huff_encode_buf(dst, buf)                                    \
    (*(uint64_t *) (dst) = (buf))
#endif

#else /* NGX_PTR_SIZE == 4 */

/* 定义一个宏函数，用于将Huffman编码的缓冲区写入目标地址 */
/* dst: 目标地址指针 */
/* buf: 包含Huffman编码的缓冲区 */
/* 在32位系统上，使用htonl函数将buf转换为网络字节序后写入dst */
#define ngx_http_huff_encode_buf(dst, buf)                                    \
    (*(uint32_t *) (dst) = htonl(buf))

#endif


size_t
ngx_http_huff_encode(u_char *src, size_t len, u_char *dst, ngx_uint_t lower)
{
    u_char                       *end;
    size_t                        hlen;
    ngx_uint_t                    buf, pending, code;
    ngx_http_huff_encode_code_t  *table, *next;

    table = lower ? ngx_http_huff_encode_table_lc
                  : ngx_http_huff_encode_table;
    hlen = 0;
    buf = 0;
    pending = 0;

    end = src + len;

    while (src != end) {
        next = &table[*src++];

        code = next->code;
        pending += next->len;

        /* accumulate bits */
        if (pending < sizeof(buf) * 8) {
            buf |= code << (sizeof(buf) * 8 - pending);
            continue;
        }

        if (hlen + sizeof(buf) >= len) {
            return 0;
        }

        pending -= sizeof(buf) * 8;

        buf |= code >> pending;

        ngx_http_huff_encode_buf(&dst[hlen], buf);

        hlen += sizeof(buf);

        buf = pending ? code << (sizeof(buf) * 8 - pending) : 0;
    }

    if (pending == 0) {
        return hlen;
    }

    buf |= (ngx_uint_t) -1 >> pending;

    pending = ngx_align(pending, 8);

    if (hlen + pending / 8 >= len) {
        return 0;
    }

    buf >>= sizeof(buf) * 8 - pending;

    do {
        pending -= 8;
        dst[hlen++] = (u_char) (buf >> pending);
    } while (pending);

    return hlen;
}
