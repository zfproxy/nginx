
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HASH_H_INCLUDED_
#define _NGX_HASH_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 哈希表元素结构体
 */
typedef struct {
    void             *value;    /**< 元素值指针 */
    u_short           len;      /**< 元素名称长度 */
    u_char            name[1];  /**< 元素名称（柔性数组成员） */
} ngx_hash_elt_t;

/**
 * @brief 哈希表结构体
 */
typedef struct {
    ngx_hash_elt_t  **buckets;  /**< 哈希桶数组 */
    ngx_uint_t        size;     /**< 哈希表大小 */
} ngx_hash_t;

/**
 * @brief 通配符哈希表结构体
 */
typedef struct {
    ngx_hash_t        hash;     /**< 基础哈希表 */
    void             *value;    /**< 通配符值 */
} ngx_hash_wildcard_t;

/**
 * @brief 哈希键结构体
 */
typedef struct {
    ngx_str_t         key;      /**< 键字符串 */
    ngx_uint_t        key_hash; /**< 键的哈希值 */
    void             *value;    /**< 键对应的值 */
} ngx_hash_key_t;

/**
 * @brief 哈希函数类型定义
 */
typedef ngx_uint_t (*ngx_hash_key_pt) (u_char *data, size_t len);

/**
 * @brief 组合哈希表结构体
 */
typedef struct {
    ngx_hash_t            hash;     /**< 基础哈希表 */
    ngx_hash_wildcard_t  *wc_head;  /**< 前缀通配符哈希表 */
    ngx_hash_wildcard_t  *wc_tail;  /**< 后缀通配符哈希表 */
} ngx_hash_combined_t;

/**
 * @brief 哈希表初始化结构体
 */
typedef struct {
    ngx_hash_t       *hash;         /**< 要初始化的哈希表指针 */
    ngx_hash_key_pt   key;          /**< 哈希函数 */

    ngx_uint_t        max_size;     /**< 最大桶数 */
    ngx_uint_t        bucket_size;  /**< 每个桶的大小 */

    char             *name;         /**< 哈希表名称 */
    ngx_pool_t       *pool;         /**< 内存池 */
    ngx_pool_t       *temp_pool;    /**< 临时内存池 */
} ngx_hash_init_t;

/* 哈希表类型定义 */
#define NGX_HASH_SMALL            1
#define NGX_HASH_LARGE            2

/* 大型哈希表的默认大小 */
#define NGX_HASH_LARGE_ASIZE      16384
#define NGX_HASH_LARGE_HSIZE      10007

/* 键类型标志 */
#define NGX_HASH_WILDCARD_KEY     1
#define NGX_HASH_READONLY_KEY     2

/**
 * @brief 哈希键数组结构体
 */
typedef struct {
    ngx_uint_t        hsize;               /**< 哈希表大小 */

    ngx_pool_t       *pool;                /**< 内存池 */
    ngx_pool_t       *temp_pool;           /**< 临时内存池 */

    ngx_array_t       keys;                /**< 键数组 */
    ngx_array_t      *keys_hash;           /**< 键哈希数组 */

    ngx_array_t       dns_wc_head;         /**< DNS前缀通配符数组 */
    ngx_array_t      *dns_wc_head_hash;    /**< DNS前缀通配符哈希数组 */

    ngx_array_t       dns_wc_tail;         /**< DNS后缀通配符数组 */
    ngx_array_t      *dns_wc_tail_hash;    /**< DNS后缀通配符哈希数组 */
} ngx_hash_keys_arrays_t;

/**
 * @brief 表元素结构体
 */
typedef struct ngx_table_elt_s  ngx_table_elt_t;

struct ngx_table_elt_s {
    ngx_uint_t        hash;         /**< 哈希值 */
    ngx_str_t         key;          /**< 键 */
    ngx_str_t         value;        /**< 值 */
    u_char           *lowcase_key;  /**< 小写键 */
    ngx_table_elt_t  *next;         /**< 下一个元素 */
};

/* 函数声明 */

/**
 * @brief 在哈希表中查找元素
 */
void *ngx_hash_find(ngx_hash_t *hash, ngx_uint_t key, u_char *name, size_t len);

/**
 * @brief 在前缀通配符哈希表中查找元素
 */
void *ngx_hash_find_wc_head(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);

/**
 * @brief 在后缀通配符哈希表中查找元素
 */
void *ngx_hash_find_wc_tail(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);

/**
 * @brief 在组合哈希表中查找元素
 */
void *ngx_hash_find_combined(ngx_hash_combined_t *hash, ngx_uint_t key,
    u_char *name, size_t len);

/**
 * @brief 初始化哈希表
 */
ngx_int_t ngx_hash_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names,
    ngx_uint_t nelts);

/**
 * @brief 初始化通配符哈希表
 */
ngx_int_t ngx_hash_wildcard_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names,
    ngx_uint_t nelts);

/**
 * @brief 计算哈希值的宏
 */
#define ngx_hash(key, c)   ((ngx_uint_t) key * 31 + c)

/**
 * @brief 计算字符串的哈希值
 */
ngx_uint_t ngx_hash_key(u_char *data, size_t len);

/**
 * @brief 计算小写字符串的哈希值
 */
ngx_uint_t ngx_hash_key_lc(u_char *data, size_t len);

/**
 * @brief 将字符串转换为小写并计算哈希值
 */
ngx_uint_t ngx_hash_strlow(u_char *dst, u_char *src, size_t n);

/**
 * @brief 初始化哈希键数组
 */
ngx_int_t ngx_hash_keys_array_init(ngx_hash_keys_arrays_t *ha, ngx_uint_t type);

/**
 * @brief 向哈希键数组中添加键
 */
ngx_int_t ngx_hash_add_key(ngx_hash_keys_arrays_t *ha, ngx_str_t *key,
    void *value, ngx_uint_t flags);


#endif /* _NGX_HASH_H_INCLUDED_ */
