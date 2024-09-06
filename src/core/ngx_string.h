
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_STRING_H_INCLUDED_
#define _NGX_STRING_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 定义字符串结构体
 *
 * 这个结构体用于表示Nginx中的字符串。
 * 它包含了字符串的长度和指向实际数据的指针。
 */
typedef struct {
    size_t      len;    /**< 字符串的长度 */
    u_char     *data;   /**< 指向字符串数据的指针 */
} ngx_str_t;


/**
 * @brief 定义键值对结构体
 *
 * 这个结构体用于表示Nginx中的键值对。
 * 它包含了一个键和一个值，都是ngx_str_t类型。
 */
typedef struct {
    ngx_str_t   key;    /**< 键 */
    ngx_str_t   value;  /**< 值 */
} ngx_keyval_t;


/**
 * @brief 定义变量值结构体
 *
 * 这个结构体用于表示Nginx中的变量值。
 * 它包含了变量值的长度、各种标志位和指向实际数据的指针。
 */
typedef struct {
    unsigned    len:28;         /**< 变量值的长度，使用28位存储 */

    unsigned    valid:1;        /**< 标志位：变量值是否有效 */
    unsigned    no_cacheable:1; /**< 标志位：变量值是否不可缓存 */
    unsigned    not_found:1;    /**< 标志位：变量是否未找到 */
    unsigned    escape:1;       /**< 标志位：变量值是否需要转义 */

    u_char     *data;           /**< 指向变量值数据的指针 */
} ngx_variable_value_t;


/**
 * @brief 创建一个ngx_str_t类型的字符串常量
 *
 * 这个宏用于快速创建一个ngx_str_t类型的字符串常量。
 * 它自动计算字符串的长度（不包括结尾的空字符），
 * 并将字符串指针转换为u_char*类型。
 *
 * @param str 要转换的C风格字符串常量
 * @return 返回一个初始化好的ngx_str_t结构体
 */
#define ngx_string(str)     { sizeof(str) - 1, (u_char *) str }
/**
 * @brief 定义一个空的ngx_str_t字符串常量
 *
 * 这个宏用于快速创建一个表示空字符串的ngx_str_t类型常量。
 * 它将长度设置为0，数据指针设置为NULL。
 * 
 * 使用示例:
 * ngx_str_t empty_str = ngx_null_string;
 */
#define ngx_null_string     { 0, NULL }
/**
 * @brief 设置ngx_str_t结构体的值
 *
 * 这个宏用于快速设置ngx_str_t结构体的len和data成员。
 * 它自动计算text的长度（不包括结尾的空字符），
 * 并将text指针转换为u_char*类型赋值给data成员。
 *
 * @param str 要设置的ngx_str_t结构体指针
 * @param text 要设置的C风格字符串常量
 */
#define ngx_str_set(str, text)                                               \
    (str)->len = sizeof(text) - 1; (str)->data = (u_char *) text
/**
 * @brief 将ngx_str_t结构体设置为空字符串
 *
 * 这个宏用于快速将一个ngx_str_t结构体重置为表示空字符串的状态。
 * 它将长度设置为0，数据指针设置为NULL。
 *
 * @param str 要设置的ngx_str_t结构体指针
 */
#define ngx_str_null(str)   (str)->len = 0; (str)->data = NULL


/**
 * @brief 将字符转换为小写
 *
 * 这个宏用于将单个字符转换为小写。
 * 它检查字符是否为大写字母（A-Z），如果是，则通过按位或运算将其转换为小写。
 * 对于非大写字母，保持原样不变。
 *
 * 原理：
 * - 大写字母的ASCII码范围是65-90
 * - 小写字母的ASCII码范围是97-122
 * - 大写字母与其对应的小写字母ASCII码相差32（0x20）
 * - 通过按位或0x20，可以将大写字母转换为小写
 *
 * @param c 要转换的字符
 * @return 返回转换后的字符（如果是大写字母则返回对应的小写字母，否则返回原字符）
 */
#define ngx_tolower(c)      (u_char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
/**
 * @brief 将字符转换为大写
 *
 * 这个宏用于将单个字符转换为大写。
 * 它检查字符是否为小写字母（a-z），如果是，则通过按位与运算将其转换为大写。
 * 对于非小写字母，保持原样不变。
 *
 * 原理：
 * - 小写字母的ASCII码范围是97-122
 * - 大写字母的ASCII码范围是65-90
 * - 小写字母与其对应的大写字母ASCII码相差32（0x20）
 * - 通过按位与~0x20（即11011111），可以将小写字母转换为大写
 *
 * @param c 要转换的字符
 * @return 返回转换后的字符（如果是小写字母则返回对应的大写字母，否则返回原字符）
 */
#define ngx_toupper(c)      (u_char) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)

/**
 * @brief 将源字符串转换为小写并复制到目标字符串
 *
 * 该函数将源字符串中的字符转换为小写，并将结果复制到目标字符串中。
 * 转换过程最多处理n个字符。
 *
 * @param dst 目标字符串的指针，用于存储转换后的小写字符串
 * @param src 源字符串的指针，包含要转换的原始字符串
 * @param n 要处理的最大字符数
 */
void ngx_strlow(u_char *dst, u_char *src, size_t n);


/**
 * @brief 比较两个字符串的前n个字符
 *
 * 这个宏定义封装了标准C库函数strncmp，用于比较两个字符串的前n个字符。
 * 它将输入的u_char指针强制转换为const char*，以符合strncmp的参数要求。
 *
 * @param s1 第一个要比较的字符串
 * @param s2 第二个要比较的字符串
 * @param n 要比较的字符数
 * @return 返回一个整数：
 *         - 如果s1和s2的前n个字符相同，返回0
 *         - 如果s1小于s2，返回小于0的值
 *         - 如果s1大于s2，返回大于0的值
 */
#define ngx_strncmp(s1, s2, n)  strncmp((const char *) s1, (const char *) s2, n)


/* msvc and icc7 compile strcmp() to inline loop */
/**
 * @brief 比较两个字符串
 *
 * 这个宏定义封装了标准C库函数strcmp，用于比较两个字符串。
 * 它将输入的u_char指针强制转换为const char*，以符合strcmp的参数要求。
 *
 * @param s1 第一个要比较的字符串
 * @param s2 第二个要比较的字符串
 * @return 返回一个整数：
 *         - 如果s1和s2相同，返回0
 *         - 如果s1小于s2，返回小于0的值
 *         - 如果s1大于s2，返回大于0的值
 */
#define ngx_strcmp(s1, s2)  strcmp((const char *) s1, (const char *) s2)


/**
 * @brief 在字符串中查找子串
 *
 * 这个宏定义封装了标准C库函数strstr，用于在字符串中查找子串。
 * 它将输入的u_char指针强制转换为const char*，以符合strstr的参数要求。
 *
 * @param s1 要搜索的主字符串
 * @param s2 要查找的子串
 * @return 如果找到子串，返回指向子串第一次出现位置的指针；如果未找到，返回NULL
 */
#define ngx_strstr(s1, s2)  strstr((const char *) s1, (const char *) s2)
/**
 * @brief 计算字符串长度
 *
 * 这个宏定义封装了标准C库函数strlen，用于计算字符串的长度。
 * 它将输入的u_char指针强制转换为const char*，以符合strlen的参数要求。
 *
 * @param s 要计算长度的字符串
 * @return 返回字符串的长度（不包括结尾的空字符）
 */
#define ngx_strlen(s)       strlen((const char *) s)

/**
 * @brief 计算字符串的长度，但不超过指定的最大长度
 *
 * 这个函数用于计算字符串的长度，但最多只检查n个字符。
 * 它在遇到空字符或达到最大长度n时停止计数。
 *
 * @param p 要计算长度的字符串
 * @param n 要检查的最大字符数
 * @return 返回字符串的长度或n（取较小值）
 */
size_t ngx_strnlen(u_char *p, size_t n);

/**
 * @brief 在字符串中查找字符
 *
 * 这个宏定义封装了标准C库函数strchr，用于在字符串中查找指定字符。
 * 它将输入的u_char指针强制转换为const char*，以符合strchr的参数要求。
 *
 * @param s1 要搜索的字符串
 * @param c 要查找的字符
 * @return 如果找到字符，返回指向该字符的指针；如果未找到，返回NULL
 */
#define ngx_strchr(s1, c)   strchr((const char *) s1, (int) c)

/**
 * @brief 在指定范围内查找字符
 *
 * 这个内联函数在给定的字符范围内查找指定的字符。
 * 它从起始位置p开始，直到结束位置last（不包括last），查找字符c。
 *
 * @param p 开始查找的位置
 * @param last 结束查找的位置（不包括此位置）
 * @param c 要查找的字符
 * @return 如果找到字符，返回指向该字符的指针；如果未找到，返回NULL
 */
static ngx_inline u_char *
ngx_strlchr(u_char *p, u_char *last, u_char c)
{
    while (p < last) {

        if (*p == c) {
            return p;
        }

        p++;
    }

    return NULL;
}


/*
 * msvc and icc7 compile memset() to the inline "rep stos"
 * while ZeroMemory() and bzero() are the calls.
 * icc7 may also inline several mov's of a zeroed register for small blocks.
 */
/**
 * @brief 将指定内存区域设置为零
 *
 * 这个宏定义使用memset函数将指定的内存区域设置为零。
 * 它通常用于初始化或清空内存缓冲区。
 *
 * @param buf 要设置为零的内存区域的起始地址
 * @param n 要设置为零的字节数
 */
#define ngx_memzero(buf, n)       (void) memset(buf, 0, n)
/**
 * @brief 将指定内存区域设置为特定值
 *
 * 这个宏定义使用memset函数将指定的内存区域设置为给定的值。
 * 它通常用于初始化或重置内存缓冲区。
 *
 * @param buf 要设置的内存区域的起始地址
 * @param c 要设置的值（会被转换为unsigned char）
 * @param n 要设置的字节数
 */
#define ngx_memset(buf, c, n)     (void) memset(buf, c, n)

/**
 * @brief 安全地将指定内存区域设置为零
 *
 * 这个函数用于安全地将指定的内存区域设置为零。
 * 与普通的memset不同，该函数设计用于清除敏感数据，
 * 并且尽量避免被编译器优化掉，以确保内存被真正清零。
 *
 * @param buf 要清零的内存区域的起始地址
 * @param n 要清零的字节数
 */
void ngx_explicit_memzero(void *buf, size_t n);


#if (NGX_MEMCPY_LIMIT)

/**
 * @brief 自定义内存复制函数
 *
 * 这个函数是memcpy的自定义实现，用于在特定情况下（如MEMCPY_LIMIT被定义时）替代标准的memcpy函数。
 * 它将源内存区域的内容复制到目标内存区域。
 *
 * @param dst 目标内存区域的指针
 * @param src 源内存区域的指针
 * @param n 要复制的字节数
 * @return 返回目标内存区域的指针
 */
void *ngx_memcpy(void *dst, const void *src, size_t n);
/**
 * @brief 复制内存并返回目标内存末尾的指针
 *
 * 这个宏定义用于复制内存并返回目标内存区域末尾的指针。
 * 它首先使用ngx_memcpy函数复制内存，然后将返回的指针
 * 加上复制的字节数，从而指向目标内存区域的末尾。
 *
 * @param dst 目标内存区域的指针
 * @param src 源内存区域的指针
 * @param n 要复制的字节数
 * @return 返回指向目标内存区域末尾的指针
 */
#define ngx_cpymem(dst, src, n)   (((u_char *) ngx_memcpy(dst, src, n)) + (n))

#else

/*
 * gcc3, msvc, and icc7 compile memcpy() to the inline "rep movs".
 * gcc3 compiles memcpy(d, s, 4) to the inline "mov"es.
 * icc8 compile memcpy(d, s, 4) to the inline "mov"es or XMM moves.
 */
/**
 * @brief 内存复制宏定义
 *
 * 这个宏定义用于将源内存区域的内容复制到目标内存区域。
 * 它是对标准C库函数memcpy的简单封装，添加了(void)类型转换以抑制可能的编译器警告。
 *
 * @param dst 目标内存区域的指针
 * @param src 源内存区域的指针
 * @param n 要复制的字节数
 */
#define ngx_memcpy(dst, src, n)   (void) memcpy(dst, src, n)
/**
 * @brief 复制内存并返回目标内存末尾的指针
 *
 * 这个宏定义用于复制内存并返回目标内存区域末尾的指针。
 * 它首先使用memcpy函数复制内存，然后将返回的指针
 * 加上复制的字节数，从而指向目标内存区域的末尾。
 *
 * @param dst 目标内存区域的指针
 * @param src 源内存区域的指针
 * @param n 要复制的字节数
 * @return 返回指向目标内存区域末尾的指针
 */
#define ngx_cpymem(dst, src, n)   (((u_char *) memcpy(dst, src, n)) + (n))

#endif


#if ( __INTEL_COMPILER >= 800 )

/*
 * the simple inline cycle copies the variable length strings up to 16
 * bytes faster than icc8 autodetecting _intel_fast_memcpy()
 */

static ngx_inline u_char *
ngx_copy(u_char *dst, u_char *src, size_t len)
{
    if (len < 17) {

        while (len) {
            *dst++ = *src++;
            len--;
        }

        return dst;

    } else {
        return ngx_cpymem(dst, src, len);
    }
}

#else

/**
 * @brief 定义ngx_copy为ngx_cpymem的别名
 *
 * 这个宏定义将ngx_copy定义为ngx_cpymem的别名。
 * 在不需要特殊优化的情况下，直接使用ngx_cpymem函数来实现内存复制操作。
 * 这样可以简化代码，并保持一致性，同时允许在需要时轻松替换为更优化的实现。
 */
#define ngx_copy                  ngx_cpymem

#endif


#define ngx_memmove(dst, src, n)  (void) memmove(dst, src, n)
#define ngx_movemem(dst, src, n)  (((u_char *) memmove(dst, src, n)) + (n))


/* msvc and icc7 compile memcmp() to the inline loop */
#define ngx_memcmp(s1, s2, n)     memcmp(s1, s2, n)


/**
 * @brief 复制字符串，最多复制n个字符
 * @param dst 目标字符串
 * @param src 源字符串
 * @param n 最大复制字符数
 * @return 复制结束后的目标字符串位置
 */
u_char *ngx_cpystrn(u_char *dst, u_char *src, size_t n);

/**
 * @brief 在内存池中复制字符串
 * @param pool 内存池
 * @param src 源字符串
 * @return 复制后的新字符串
 */
u_char *ngx_pstrdup(ngx_pool_t *pool, ngx_str_t *src);

/**
 * @brief 格式化字符串并写入缓冲区
 * @param buf 目标缓冲区
 * @param fmt 格式化字符串
 * @param ... 可变参数列表
 * @return 写入后的缓冲区位置
 */
u_char * ngx_cdecl ngx_sprintf(u_char *buf, const char *fmt, ...);

/**
 * @brief 格式化字符串并写入缓冲区，限制最大长度
 * @param buf 目标缓冲区
 * @param max 最大长度
 * @param fmt 格式化字符串
 * @param ... 可变参数列表
 * @return 写入后的缓冲区位置
 */
u_char * ngx_cdecl ngx_snprintf(u_char *buf, size_t max, const char *fmt, ...);

/**
 * @brief 格式化字符串并写入缓冲区，指定结束位置
 * @param buf 目标缓冲区
 * @param last 缓冲区结束位置
 * @param fmt 格式化字符串
 * @param ... 可变参数列表
 * @return 写入后的缓冲区位置
 */
u_char * ngx_cdecl ngx_slprintf(u_char *buf, u_char *last, const char *fmt, 
...);

/**
 * @brief 使用va_list格式化字符串并写入缓冲区
 * @param buf 目标缓冲区
 * @param last 缓冲区结束位置
 * @param fmt 格式化字符串
 * @param args 参数列表
 * @return 写入后的缓冲区位置
 */
u_char *ngx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args);

/**
 * @brief 使用va_list格式化字符串并写入缓冲区，限制最大长度
 */
#define ngx_vsnprintf(buf, max, fmt, args)                                   \
    ngx_vslprintf(buf, buf + (max), fmt, args)

/**
 * @brief 不区分大小写的字符串比较
 * @param s1 第一个字符串
 * @param s2 第二个字符串
 * @return 比较结果
 */
ngx_int_t ngx_strcasecmp(u_char *s1, u_char *s2);

/**
 * @brief 不区分大小写的字符串比较，限制比较长度
 * @param s1 第一个字符串
 * @param s2 第二个字符串
 * @param n 比较的最大长度
 * @return 比较结果
 */
ngx_int_t ngx_strncasecmp(u_char *s1, u_char *s2, size_t n);

/**
 * @brief 在字符串中查找子串，限制查找长度
 * @param s1 被查找的字符串
 * @param s2 要查找的子串
 * @param n 查找的最大长度
 * @return 找到子串的位置，未找到返回NULL
 */
u_char *ngx_strnstr(u_char *s1, char *s2, size_t n);

/**
 * @brief 在字符串中查找子串，限制查找长度
 * @param s1 被查找的字符串
 * @param s2 要查找的子串
 * @param n 查找的最大长度
 * @return 找到子串的位置，未找到返回NULL
 */
u_char *ngx_strstrn(u_char *s1, char *s2, size_t n);

/**
 * @brief 不区分大小写地在字符串中查找子串，限制查找长度
 * @param s1 被查找的字符串
 * @param s2 要查找的子串
 * @param n 查找的最大长度
 * @return 找到子串的位置，未找到返回NULL
 */
u_char *ngx_strcasestrn(u_char *s1, char *s2, size_t n);

/**
 * @brief 不区分大小写地在字符串中查找子串，指定结束位置
 * @param s1 被查找的字符串
 * @param last 字符串结束位置
 * @param s2 要查找的子串
 * @param n 查找的最大长度
 * @return 找到子串的位置，未找到返回NULL
 */
u_char *ngx_strlcasestrn(u_char *s1, u_char *last, u_char *s2, size_t n);

/**
 * @brief 从右向左比较字符串，限制比较长度
 * @param s1 第一个字符串
 * @param s2 第二个字符串
 * @param n 比较的最大长度
 * @return 比较结果
 */
ngx_int_t ngx_rstrncmp(u_char *s1, u_char *s2, size_t n);

/**
 * @brief 从右向左不区分大小写地比较字符串，限制比较长度
 * @param s1 第一个字符串
 * @param s2 第二个字符串
 * @param n 比较的最大长度
 * @return 比较结果
 */
ngx_int_t ngx_rstrncasecmp(u_char *s1, u_char *s2, size_t n);

/**
 * @brief 比较两个内存块
 * @param s1 第一个内存块
 * @param s2 第二个内存块
 * @param n1 第一个内存块的长度
 * @param n2 第二个内存块的长度
 * @return 比较结果
 */
ngx_int_t ngx_memn2cmp(u_char *s1, u_char *s2, size_t n1, size_t n2);

/**
 * @brief 比较两个DNS名称
 * @param s1 第一个DNS名称
 * @param s2 第二个DNS名称
 * @return 比较结果
 */
ngx_int_t ngx_dns_strcmp(u_char *s1, u_char *s2);

/**
 * @brief 比较两个文件名
 * @param s1 第一个文件名
 * @param s2 第二个文件名
 * @param n 比较的最大长度
 * @return 比较结果
 */
ngx_int_t ngx_filename_cmp(u_char *s1, u_char *s2, size_t n);

/**
 * @brief 将字符串转换为整数
 * @param line 要转换的字符串
 * @param n 字符串的长度
 * @return 转换后的整数
 */
ngx_int_t ngx_atoi(u_char *line, size_t n);

/**
 * @brief 将字符串转换为带小数点的浮点数
 * @param line 要转换的字符串
 * @param n 字符串的长度
 * @param point 小数点位置
 * @return 转换后的浮点数
 */
ngx_int_t ngx_atofp(u_char *line, size_t n, size_t point);

/**
 * @brief 将字符串转换为ssize_t类型的整数
 * @param line 要转换的字符串
 * @param n 字符串的长度
 * @return 转换后的ssize_t整数
 */
ssize_t ngx_atosz(u_char *line, size_t n);

/**
 * @brief 将字符串转换为off_t类型的整数
 * @param line 要转换的字符串
 * @param n 字符串的长度
 * @return 转换后的off_t整数
 */
off_t ngx_atoof(u_char *line, size_t n);

/**
 * @brief 将字符串转换为time_t类型的时间值
 * @param line 要转换的字符串
 * @param n 字符串的长度
 * @return 转换后的time_t时间值
 */
time_t ngx_atotm(u_char *line, size_t n);

/**
 * @brief 将十六进制字符串转换为整数
 * @param line 要转换的十六进制字符串
 * @param n 字符串的长度
 * @return 转换后的整数
 */
ngx_int_t ngx_hextoi(u_char *line, size_t n);

/**
 * @brief 将二进制数据转换为十六进制字符串
 * @param dst 目标缓冲区
 * @param src 源二进制数据
 * @param len 源数据长度
 * @return 转换后的十六进制字符串
 */
u_char *ngx_hex_dump(u_char *dst, u_char *src, size_t len);


/**
 * @brief 计算Base64编码后的长度
 * @param len 原始数据长度
 * @return Base64编码后的长度
 */
#define ngx_base64_encoded_length(len)  (((len + 2) / 3) * 4)

/**
 * @brief 计算Base64解码后的长度
 * @param len Base64编码的数据长度
 * @return 解码后的原始数据长度
 */
#define ngx_base64_decoded_length(len)  (((len + 3) / 4) * 3)

/**
 * @brief 执行标准Base64编码
 * @param dst 目标字符串
 * @param src 源字符串
 */
void ngx_encode_base64(ngx_str_t *dst, ngx_str_t *src);

/**
 * @brief 执行URL安全的Base64编码
 * @param dst 目标字符串
 * @param src 源字符串
 */
void ngx_encode_base64url(ngx_str_t *dst, ngx_str_t *src);

/**
 * @brief 执行标准Base64解码
 * @param dst 目标字符串
 * @param src 源字符串
 * @return 解码成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_decode_base64(ngx_str_t *dst, ngx_str_t *src);

/**
 * @brief 执行URL安全的Base64解码
 * @param dst 目标字符串
 * @param src 源字符串
 * @return 解码成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_decode_base64url(ngx_str_t *dst, ngx_str_t *src);

/**
 * @brief 解码UTF-8字符
 * @param p 指向UTF-8字符串的指针的指针
 * @param n 字符串的最大长度
 * @return 解码后的Unicode码点
 */
uint32_t ngx_utf8_decode(u_char **p, size_t n);

/**
 * @brief 计算UTF-8字符串中的字符数
 * @param p UTF-8字符串
 * @param n 字符串的最大长度
 * @return UTF-8字符串中的字符数
 */
size_t ngx_utf8_length(u_char *p, size_t n);

/**
 * @brief 复制UTF-8字符串，确保不超过指定的字节数和字符数
 * @param dst 目标缓冲区
 * @param src 源字符串
 * @param n 最大字节数
 * @param len 最大字符数
 * @return 复制操作结束后的目标缓冲区位置
 */
u_char *ngx_utf8_cpystrn(u_char *dst, u_char *src, size_t n, size_t len);


/**
 * @brief 定义不同的转义类型
 */
#define NGX_ESCAPE_URI            0  /**< URI转义 */
#define NGX_ESCAPE_ARGS           1  /**< 参数转义 */
#define NGX_ESCAPE_URI_COMPONENT  2  /**< URI组件转义 */
#define NGX_ESCAPE_HTML           3  /**< HTML转义 */
#define NGX_ESCAPE_REFRESH        4  /**< 刷新转义 */
#define NGX_ESCAPE_MEMCACHED      5  /**< Memcached转义 */
#define NGX_ESCAPE_MAIL_AUTH      6  /**< 邮件认证转义 */

/**
 * @brief 定义不同的反转义类型
 */
#define NGX_UNESCAPE_URI       1  /**< URI反转义 */
#define NGX_UNESCAPE_REDIRECT  2  /**< 重定向反转义 */

/**
 * @brief 对URI进行转义
 *
 * @param dst 目标缓冲区
 * @param src 源字符串
 * @param size 源字符串的长度
 * @param type 转义类型
 * @return 转义后的字符数
 */
uintptr_t ngx_escape_uri(u_char *dst, u_char *src, size_t size,
    ngx_uint_t type);

/**
 * @brief 对URI进行反转义
 *
 * @param dst 目标缓冲区的指针
 * @param src 源字符串的指针
 * @param size 源字符串的长度
 * @param type 反转义类型
 */
void ngx_unescape_uri(u_char **dst, u_char **src, size_t size, ngx_uint_t type);

/**
 * @brief 对HTML特殊字符进行转义
 *
 * @param dst 目标缓冲区
 * @param src 源字符串
 * @param size 源字符串的长度
 * @return 转义后的字符数
 */
uintptr_t ngx_escape_html(u_char *dst, u_char *src, size_t size);

/**
 * @brief 对JSON特殊字符进行转义
 *
 * @param dst 目标缓冲区
 * @param src 源字符串
 * @param size 源字符串的长度
 * @return 转义后的字符数
 */
uintptr_t ngx_escape_json(u_char *dst, u_char *src, size_t size);


/**
 * @brief 定义字符串节点结构体
 *
 * 这个结构体用于在红黑树中存储字符串。
 * 它包含一个红黑树节点和一个Nginx字符串。
 */
typedef struct {
    ngx_rbtree_node_t         node;  /**< 红黑树节点 */
    ngx_str_t                 str;   /**< Nginx字符串 */
} ngx_str_node_t;


/**
 * @brief 在红黑树中插入字符串节点的函数
 *
 * @param temp 临时节点，用于比较和定位插入位置
 */
void ngx_str_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
/**
 * @brief 在红黑树中查找字符串节点
 *
 * 该函数用于在给定的红黑树中查找与指定名称匹配的字符串节点。
 *
 * @param rbtree 要搜索的红黑树
 * @param name 要查找的字符串名称
 * @param hash 字符串的哈希值（用于快速比较）
 * @return 如果找到匹配的节点，返回指向该节点的指针；否则返回NULL
 */
ngx_str_node_t *ngx_str_rbtree_lookup(ngx_rbtree_t *rbtree, ngx_str_t *name,
    uint32_t hash);


/**
 * @brief 对数组进行排序
 *
 * 这个函数用于对给定的数组进行排序。它是一个通用的排序函数，可以处理任意类型的数据。
 *
 * @param base 指向要排序的数组的指针
 * @param n 数组中元素的数量
 * @param size 每个元素的大小（以字节为单位）
 */
void ngx_sort(void *base, size_t n, size_t size,
    ngx_int_t (*cmp)(const void *, const void *));
/**
 * @brief 定义快速排序函数别名
 *
 * 这个宏定义将标准C库中的qsort函数重命名为ngx_qsort。
 * 这样做可以在Nginx代码中使用ngx_qsort来调用快速排序算法，
 * 保持了命名的一致性，同时利用了标准库的高效实现。
 *
 * 使用示例:
 * ngx_qsort(array, n, sizeof(element), compare_function);
 *
 * @param array 要排序的数组
 * @param n 数组中元素的数量
 * @param size 每个元素的大小（以字节为单位）
 * @param compar 比较函数指针
 */
#define ngx_qsort             qsort


/**
 * @brief 辅助宏，用于将参数转换为字符串字面量
 *
 * 这个宏使用了C预处理器的字符串化操作符 (#)，
 * 将传入的参数 n 转换为字符串字面量。
 *
 * @param n 要转换为字符串的参数
 * @return 参数 n 的字符串字面量表示
 */
#define ngx_value_helper(n)   #n
/**
 * @brief 将参数转换为字符串字面量
 *
 * 这个宏利用了ngx_value_helper宏来将传入的参数n转换为字符串字面量。
 * 它是一个两步转换过程的第二步，提供了一个更简洁的接口来使用字符串化功能。
 *
 * 使用示例:
 * #define VERSION 1.0
 * char *version_str = ngx_value(VERSION); // 结果为 "1.0"
 *
 * @param n 要转换为字符串的参数
 * @return 参数n的字符串字面量表示
 */
#define ngx_value(n)          ngx_value_helper(n)


#endif /* _NGX_STRING_H_INCLUDED_ */
