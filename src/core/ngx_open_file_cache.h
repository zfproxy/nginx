
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef _NGX_OPEN_FILE_CACHE_H_INCLUDED_
#define _NGX_OPEN_FILE_CACHE_H_INCLUDED_


#define NGX_OPEN_FILE_DIRECTIO_OFF  NGX_MAX_OFF_T_VALUE


/**
 * @brief 直接I/O关闭的标志值
 *
 * 这个宏定义表示直接I/O功能被关闭。
 * 它使用了NGX_MAX_OFF_T_VALUE，这通常是off_t类型能表示的最大值。
 * 当directio成员被设置为这个值时，表示不使用直接I/O。
 */
typedef struct {
    ngx_fd_t                 fd;              // 文件描述符
    ngx_file_uniq_t          uniq;            // 文件的唯一标识符
    time_t                   mtime;           // 文件的最后修改时间
    off_t                    size;            // 文件大小
    off_t                    fs_size;         // 文件系统中的文件大小
    off_t                    directio;        // 直接I/O的偏移量
    size_t                   read_ahead;      // 预读大小

    ngx_err_t                err;             // 错误码
    char                    *failed;          // 失败原因的描述

    time_t                   valid;           // 缓存有效期

    ngx_uint_t               min_uses;        // 最小使用次数

#if (NGX_HAVE_OPENAT)
    size_t                   disable_symlinks_from;  // 禁用符号链接的起始位置
    unsigned                 disable_symlinks:2;     // 禁用符号链接的标志
#endif

    unsigned                 test_dir:1;      // 是否测试目录
    unsigned                 test_only:1;     // 是否仅测试
    unsigned                 log:1;           // 是否记录日志
    unsigned                 errors:1;        // 是否有错误
    unsigned                 events:1;        // 是否有事件

    unsigned                 is_dir:1;        // 是否是目录
    unsigned                 is_file:1;       // 是否是文件
    unsigned                 is_link:1;       // 是否是符号链接
    unsigned                 is_exec:1;       // 是否是可执行文件
    unsigned                 is_directio:1;   // 是否使用直接I/O
} ngx_open_file_info_t;


typedef struct ngx_cached_open_file_s  ngx_cached_open_file_t;  // 缓存的打开文件结构体前向声明

/**
 * @brief 缓存的打开文件结构体
 *
 * 该结构体用于存储缓存中的打开文件的详细信息。
 * 它包含了文件的各种属性、状态和相关的数据结构节点。
 */
struct ngx_cached_open_file_s {
    ngx_rbtree_node_t        node;            // 红黑树节点
    ngx_queue_t              queue;           // 队列节点

    u_char                  *name;            // 文件名
    time_t                   created;         // 创建时间
    time_t                   accessed;        // 最后访问时间

    ngx_fd_t                 fd;              // 文件描述符
    ngx_file_uniq_t          uniq;            // 文件唯一标识符
    time_t                   mtime;           // 最后修改时间
    off_t                    size;            // 文件大小
    ngx_err_t                err;             // 错误码

    uint32_t                 uses;            // 使用次数

#if (NGX_HAVE_OPENAT)
    size_t                   disable_symlinks_from;  // 禁用符号链接的起始位置
    unsigned                 disable_symlinks:2;     // 禁用符号链接的标志
#endif

    unsigned                 count:24;        // 引用计数
    unsigned                 close:1;         // 是否关闭
    unsigned                 use_event:1;     // 是否使用事件

    unsigned                 is_dir:1;        // 是否是目录
    unsigned                 is_file:1;       // 是否是文件
    unsigned                 is_link:1;       // 是否是符号链接
    unsigned                 is_exec:1;       // 是否是可执行文件
    unsigned                 is_directio:1;   // 是否使用直接I/O

    ngx_event_t             *event;           // 关联的事件
};

/**
 * @brief 缓存的打开文件结构体
 *
 * 该结构体用于存储缓存中的打开文件的相关信息。
 */
typedef struct {
    ngx_rbtree_t             rbtree;          // 红黑树
    ngx_rbtree_node_t        sentinel;        // 哨兵节点
    ngx_queue_t              expire_queue;    // 过期队列

    ngx_uint_t               current;         // 当前缓存文件数量
    ngx_uint_t               max;             // 最大缓存文件数量
    time_t                   inactive;        // 非活动文件的过期时间
} ngx_open_file_cache_t;


typedef struct {
    ngx_open_file_cache_t   *cache;           // 文件缓存
    ngx_cached_open_file_t  *file;            // 缓存的文件
    ngx_uint_t               min_uses;        // 最小使用次数
    ngx_log_t               *log;             // 日志对象
} ngx_open_file_cache_cleanup_t;


typedef struct {

    /* ngx_connection_t stub to allow use c->fd as event ident */
    void                    *data;            // 用户数据
    ngx_event_t             *read;            // 读事件
    ngx_event_t             *write;           // 写事件
    ngx_fd_t                 fd;              // 文件描述符

    ngx_cached_open_file_t  *file;            // 缓存的文件
    ngx_open_file_cache_t   *cache;           // 文件缓存
} ngx_open_file_cache_event_t;


/**
 * @brief 初始化开放文件缓存
 *
 * @param pool 内存池
 * @param max 缓存中最大文件数
 * @param inactive 非活动文件的过期时间
 * @return ngx_open_file_cache_t* 返回初始化的开放文件缓存结构指针
 */
ngx_open_file_cache_t *ngx_open_file_cache_init(ngx_pool_t *pool,
    ngx_uint_t max, time_t inactive);

/**
 * @brief 打开缓存的文件
 *
 * @param cache 开放文件缓存
 * @param name 文件名
 * @param of 打开文件的信息结构
 * @param pool 内存池
 * @return ngx_int_t 返回操作结果，成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_open_cached_file(ngx_open_file_cache_t *cache, ngx_str_t *name,
    ngx_open_file_info_t *of, ngx_pool_t *pool);


#endif /* _NGX_OPEN_FILE_CACHE_H_INCLUDED_ */
