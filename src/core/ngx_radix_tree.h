
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_RADIX_TREE_H_INCLUDED_
#define _NGX_RADIX_TREE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* 定义基数树中表示无值的常量 */
#define NGX_RADIX_NO_VALUE   (uintptr_t) -1

/* 定义基数树节点结构体类型 */
typedef struct ngx_radix_node_s  ngx_radix_node_t;

/* 基数树节点结构体定义 */
struct ngx_radix_node_s {
    ngx_radix_node_t  *right;  /* 右子节点指针 */
    ngx_radix_node_t  *left;   /* 左子节点指针 */
    ngx_radix_node_t  *parent; /* 父节点指针 */
    uintptr_t          value;  /* 节点值 */
};

/* 基数树结构体定义 */
typedef struct {
    ngx_radix_node_t  *root;   /* 根节点指针 */
    ngx_pool_t        *pool;   /* 内存池指针 */
    ngx_radix_node_t  *free;   /* 空闲节点链表 */
    char              *start;  /* 预分配内存起始地址 */
    size_t             size;   /* 预分配内存大小 */
} ngx_radix_tree_t;

/* 创建基数树 */
ngx_radix_tree_t *ngx_radix_tree_create(ngx_pool_t *pool,
    ngx_int_t preallocate);

/* 32位基数树插入操作 */
ngx_int_t ngx_radix32tree_insert(ngx_radix_tree_t *tree,
    uint32_t key, uint32_t mask, uintptr_t value);
/* 32位基数树删除操作 */
ngx_int_t ngx_radix32tree_delete(ngx_radix_tree_t *tree,
    uint32_t key, uint32_t mask);
/* 32位基数树查找操作 */
uintptr_t ngx_radix32tree_find(ngx_radix_tree_t *tree, uint32_t key);

#if (NGX_HAVE_INET6)
/* 128位基数树插入操作（用于IPv6） */
ngx_int_t ngx_radix128tree_insert(ngx_radix_tree_t *tree,
    u_char *key, u_char *mask, uintptr_t value);
/* 128位基数树删除操作（用于IPv6） */
ngx_int_t ngx_radix128tree_delete(ngx_radix_tree_t *tree,
    u_char *key, u_char *mask);
/* 128位基数树查找操作（用于IPv6） */
uintptr_t ngx_radix128tree_find(ngx_radix_tree_t *tree, u_char *key);
#endif


#endif /* _NGX_RADIX_TREE_H_INCLUDED_ */
