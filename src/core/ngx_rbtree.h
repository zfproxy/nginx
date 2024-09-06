
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_RBTREE_H_INCLUDED_
#define _NGX_RBTREE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 红黑树键类型定义
 */
typedef ngx_uint_t  ngx_rbtree_key_t;

/**
 * @brief 红黑树整数键类型定义
 */
typedef ngx_int_t   ngx_rbtree_key_int_t;


/**
 * @brief 红黑树节点结构体前向声明
 */
typedef struct ngx_rbtree_node_s  ngx_rbtree_node_t;

/**
 * @brief 红黑树节点结构体定义
 */
struct ngx_rbtree_node_s {
    ngx_rbtree_key_t       key;    /**< 节点键值 */
    ngx_rbtree_node_t     *left;   /**< 左子节点指针 */
    ngx_rbtree_node_t     *right;  /**< 右子节点指针 */
    ngx_rbtree_node_t     *parent; /**< 父节点指针 */
    u_char                 color;  /**< 节点颜色 */
    u_char                 data;   /**< 节点数据 */
};


/**
 * @brief 红黑树结构体前向声明
 */
typedef struct ngx_rbtree_s  ngx_rbtree_t;

/**
 * @brief 红黑树插入函数指针类型定义
 */
typedef void (*ngx_rbtree_insert_pt) (ngx_rbtree_node_t *root,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);

/**
 * @brief 红黑树结构体定义
 */
struct ngx_rbtree_s {
    ngx_rbtree_node_t     *root;     /**< 根节点指针 */
    ngx_rbtree_node_t     *sentinel; /**< 哨兵节点指针 */
    ngx_rbtree_insert_pt   insert;   /**< 插入函数指针 */
};


/**
 * @brief 初始化红黑树
 * @param tree 红黑树指针
 * @param s 哨兵节点指针
 * @param i 插入函数指针
 */
#define ngx_rbtree_init(tree, s, i)                                           \
    ngx_rbtree_sentinel_init(s);                                              \
    (tree)->root = s;                                                         \
    (tree)->sentinel = s;                                                     \
    (tree)->insert = i

/**
 * @brief 获取包含红黑树节点的结构体指针
 * @param node 红黑树节点指针
 * @param type 包含红黑树节点的结构体类型
 * @param link 红黑树节点在结构体中的成员名
 * @return 包含红黑树节点的结构体指针
 */
#define ngx_rbtree_data(node, type, link)                                     \
    (type *) ((u_char *) (node) - offsetof(type, link))

/**
 * @brief 向红黑树中插入节点
 * @param tree 红黑树指针
 * @param node 要插入的节点指针
 */
void ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);

/**
 * @brief 从红黑树中删除节点
 * @param tree 红黑树指针
 * @param node 要删除的节点指针
 */
void ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);

/**
 * @brief 插入节点的默认比较函数
 * @param root 根节点指针
 * @param node 要插入的节点指针
 * @param sentinel 哨兵节点指针
 */
void ngx_rbtree_insert_value(ngx_rbtree_node_t *root, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel);

/**
 * @brief 插入定时器节点的比较函数
 * @param root 根节点指针
 * @param node 要插入的节点指针
 * @param sentinel 哨兵节点指针
 */
void ngx_rbtree_insert_timer_value(ngx_rbtree_node_t *root,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);

/**
 * @brief 获取红黑树中指定节点的后继节点
 * @param tree 红黑树指针
 * @param node 当前节点指针
 * @return 后继节点指针
 */
ngx_rbtree_node_t *ngx_rbtree_next(ngx_rbtree_t *tree,
    ngx_rbtree_node_t *node);

/**
 * @brief 将节点设置为红色
 * @param node 要设置的节点指针
 */
#define ngx_rbt_red(node)               ((node)->color = 1)

/**
 * @brief 将节点设置为黑色
 * @param node 要设置的节点指针
 */
#define ngx_rbt_black(node)             ((node)->color = 0)

/**
 * @brief 判断节点是否为红色
 * @param node 要判断的节点指针
 * @return 如果是红色返回非零值，否则返回0
 */
#define ngx_rbt_is_red(node)            ((node)->color)

/**
 * @brief 判断节点是否为黑色
 * @param node 要判断的节点指针
 * @return 如果是黑色返回非零值，否则返回0
 */
#define ngx_rbt_is_black(node)          (!ngx_rbt_is_red(node))

/**
 * @brief 复制节点颜色
 * @param n1 目标节点指针
 * @param n2 源节点指针
 */
#define ngx_rbt_copy_color(n1, n2)      (n1->color = n2->color)

/* 哨兵节点必须是黑色的 */

/**
 * @brief 初始化哨兵节点
 * @param node 哨兵节点指针
 */
#define ngx_rbtree_sentinel_init(node)  ngx_rbt_black(node)


static ngx_inline ngx_rbtree_node_t *
ngx_rbtree_min(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    while (node->left != sentinel) {
        node = node->left;
    }

    return node;
}


#endif /* _NGX_RBTREE_H_INCLUDED_ */
