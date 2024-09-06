
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_CACHE_H_INCLUDED_
#define _NGX_HTTP_CACHE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/* HTTP缓存状态码定义 */
#define NGX_HTTP_CACHE_MISS          1   /* 缓存未命中 */
#define NGX_HTTP_CACHE_BYPASS        2   /* 绕过缓存 */
#define NGX_HTTP_CACHE_EXPIRED       3   /* 缓存已过期 */
#define NGX_HTTP_CACHE_STALE         4   /* 缓存已过时 */
#define NGX_HTTP_CACHE_UPDATING      5   /* 缓存正在更新 */
#define NGX_HTTP_CACHE_REVALIDATED   6   /* 缓存已重新验证 */
#define NGX_HTTP_CACHE_HIT           7   /* 缓存命中 */
#define NGX_HTTP_CACHE_SCARCE        8   /* 缓存资源稀缺 */

/* 缓存相关长度定义 */
#define NGX_HTTP_CACHE_KEY_LEN       16  /* 缓存键长度 */
#define NGX_HTTP_CACHE_ETAG_LEN      128 /* ETag长度 */
#define NGX_HTTP_CACHE_VARY_LEN      128 /* Vary头部长度 */

/* 缓存版本号 */
#define NGX_HTTP_CACHE_VERSION       5   /* 当前缓存版本 */


/*
 * HTTP缓存有效性结构体
 * 用于定义缓存的有效状态和时间
 */
typedef struct {
    ngx_uint_t                       status;
    time_t                           valid;
} ngx_http_cache_valid_t;


/*
 * HTTP文件缓存节点结构体
 * 用于表示缓存中的单个文件节点
 * 包含了红黑树节点、队列节点、缓存键等信息
 */
typedef struct {
    ngx_rbtree_node_t                node;        /* 红黑树节点 */
    ngx_queue_t                      queue;       /* 队列节点 */

    u_char                           key[NGX_HTTP_CACHE_KEY_LEN
                                         - sizeof(ngx_rbtree_key_t)]; /* 缓存键 */

    unsigned                         count:20;    /* 引用计数 */
    unsigned                         uses:10;     /* 使用次数 */
    unsigned                         valid_msec:10; /* 有效期(毫秒) */
    unsigned                         error:10;    /* 错误码 */
    unsigned                         exists:1;    /* 是否存在 */
    unsigned                         updating:1;  /* 是否正在更新 */
    unsigned                         deleting:1;  /* 是否正在删除 */
    unsigned                         purged:1;    /* 是否已清除 */
                                     /* 10 unused bits */

    ngx_file_uniq_t                  uniq;        /* 文件唯一标识符 */
    time_t                           expire;      /* 过期时间 */
    time_t                           valid_sec;   /* 有效期(秒) */
    size_t                           body_start;  /* 正文起始位置 */
    off_t                            fs_size;     /* 文件系统大小 */
    ngx_msec_t                       lock_time;   /* 锁定时间 */
} ngx_http_file_cache_node_t;


/*
 * HTTP缓存结构体
 * 用于存储和管理HTTP缓存相关的信息
 */
struct ngx_http_cache_s {
    ngx_file_t                       file;        /* 缓存文件 */
    ngx_array_t                      keys;        /* 缓存键数组 */
    uint32_t                         crc32;       /* CRC32校验和 */
    u_char                           key[NGX_HTTP_CACHE_KEY_LEN];  /* 缓存键 */
    u_char                           main[NGX_HTTP_CACHE_KEY_LEN]; /* 主缓存键 */

    ngx_file_uniq_t                  uniq;        /* 文件唯一标识符 */
    time_t                           valid_sec;   /* 有效期（秒） */
    time_t                           updating_sec; /* 更新时间（秒） */
    time_t                           error_sec;   /* 错误时间（秒） */
    time_t                           last_modified; /* 最后修改时间 */
    time_t                           date;        /* 日期 */

    ngx_str_t                        etag;        /* ETag */
    ngx_str_t                        vary;        /* Vary头 */
    u_char                           variant[NGX_HTTP_CACHE_KEY_LEN]; /* 变体键 */

    size_t                           buffer_size; /* 缓冲区大小 */
    size_t                           header_start; /* 头部起始位置 */
    size_t                           body_start;  /* 正文起始位置 */
    off_t                            length;      /* 内容长度 */
    off_t                            fs_size;     /* 文件系统大小 */

    ngx_uint_t                       min_uses;    /* 最小使用次数 */
    ngx_uint_t                       error;       /* 错误码 */
    ngx_uint_t                       valid_msec;  /* 有效期（毫秒） */
    ngx_uint_t                       vary_tag;    /* Vary标签 */

    ngx_buf_t                       *buf;         /* 缓冲区 */

    ngx_http_file_cache_t           *file_cache;  /* 文件缓存 */
    ngx_http_file_cache_node_t      *node;        /* 缓存节点 */

#if (NGX_THREADS || NGX_COMPAT)
    ngx_thread_task_t               *thread_task; /* 线程任务 */
#endif

    ngx_msec_t                       lock_timeout; /* 锁超时时间 */
    ngx_msec_t                       lock_age;    /* 锁年龄 */
    ngx_msec_t                       lock_time;   /* 锁定时间 */
    ngx_msec_t                       wait_time;   /* 等待时间 */

    ngx_event_t                      wait_event;  /* 等待事件 */

    unsigned                         lock:1;      /* 是否锁定 */
    unsigned                         waiting:1;   /* 是否等待中 */

    unsigned                         updated:1;   /* 是否已更新 */
    unsigned                         updating:1;  /* 是否正在更新 */
    unsigned                         exists:1;    /* 是否存在 */
    unsigned                         temp_file:1; /* 是否为临时文件 */
    unsigned                         purged:1;    /* 是否已清除 */
    unsigned                         reading:1;   /* 是否正在读取 */
    unsigned                         secondary:1; /* 是否为次要缓存 */
    unsigned                         update_variant:1; /* 是否更新变体 */
    unsigned                         background:1; /* 是否为后台操作 */

    unsigned                         stale_updating:1; /* 是否过期更新 */
    unsigned                         stale_error:1; /* 是否过期错误 */
};


/*
 * 定义HTTP文件缓存头部结构体
 * 该结构体包含了缓存文件的元数据信息，如版本、有效期、更新时间等
 */
typedef struct {
    ngx_uint_t                       version;
    time_t                           valid_sec;
    time_t                           updating_sec;
    time_t                           error_sec;
    time_t                           last_modified;
    time_t                           date;
    uint32_t                         crc32;
    u_short                          valid_msec;
    u_short                          header_start;
    u_short                          body_start;
    u_char                           etag_len;
    u_char                           etag[NGX_HTTP_CACHE_ETAG_LEN];
    u_char                           vary_len;
    u_char                           vary[NGX_HTTP_CACHE_VARY_LEN];
    u_char                           variant[NGX_HTTP_CACHE_KEY_LEN];
} ngx_http_file_cache_header_t;


/*
 * 定义共享内存中的文件缓存结构体
 * 该结构体包含了管理缓存文件的红黑树、队列和其他相关信息
 */
typedef struct {
    ngx_rbtree_t                     rbtree;
    ngx_rbtree_node_t                sentinel;
    ngx_queue_t                      queue;
    ngx_atomic_t                     cold;
    ngx_atomic_t                     loading;
    off_t                            size;
    ngx_uint_t                       count;
    ngx_uint_t                       watermark;
} ngx_http_file_cache_sh_t;


/*
 * 定义HTTP文件缓存结构体
 * 该结构体包含了文件缓存的各种配置和状态信息
 */
struct ngx_http_file_cache_s {
    ngx_http_file_cache_sh_t        *sh;        /* 指向共享内存中的文件缓存结构 */
    ngx_slab_pool_t                 *shpool;    /* 共享内存池 */

    ngx_path_t                      *path;      /* 缓存文件的路径 */

    off_t                            min_free;  /* 缓存目录的最小可用空间 */
    off_t                            max_size;  /* 缓存的最大大小 */
    size_t                           bsize;     /* 缓存文件的块大小 */

    time_t                           inactive;  /* 非活跃缓存项的过期时间 */

    time_t                           fail_time; /* 缓存失败后的重试时间 */

    ngx_uint_t                       files;     /* 当前缓存的文件数量 */
    ngx_uint_t                       loader_files;  /* 加载器每次处理的文件数 */
    ngx_msec_t                       last;      /* 上次缓存操作的时间戳 */
    ngx_msec_t                       loader_sleep;  /* 加载器的休眠时间 */
    ngx_msec_t                       loader_threshold;  /* 加载器的阈值时间 */

    ngx_uint_t                       manager_files;  /* 管理器每次处理的文件数 */
    ngx_msec_t                       manager_sleep;  /* 管理器的休眠时间 */
    ngx_msec_t                       manager_threshold;  /* 管理器的阈值时间 */

    ngx_shm_zone_t                  *shm_zone;  /* 共享内存区域 */

    ngx_uint_t                       use_temp_path;  /* 是否使用临时路径 */
                                     /* unsigned use_temp_path:1 */
};


/*
 * 为HTTP请求创建新的文件缓存
 * 
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功创建新的文件缓存
 *         NGX_ERROR 创建文件缓存失败
 */
ngx_int_t ngx_http_file_cache_new(ngx_http_request_t *r);
/*
 * 创建HTTP文件缓存
 * 
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功创建文件缓存
 *         NGX_ERROR 创建文件缓存失败
 */
ngx_int_t ngx_http_file_cache_create(ngx_http_request_t *r);
/*
 * 为HTTP请求创建文件缓存的键
 * 
 * @param r HTTP请求结构体指针
 * @return 无返回值
 */
void ngx_http_file_cache_create_key(ngx_http_request_t *r);
/*
 * 打开HTTP请求的文件缓存
 * 
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功打开文件缓存
 *         NGX_ERROR 打开文件缓存失败
 */
ngx_int_t ngx_http_file_cache_open(ngx_http_request_t *r);
/*
 * 设置HTTP文件缓存的头部信息
 * 
 * @param r HTTP请求结构体指针
 * @param buf 包含头部信息的缓冲区
 * @return NGX_OK 成功设置头部信息
 *         NGX_ERROR 设置头部信息失败
 */
ngx_int_t ngx_http_file_cache_set_header(ngx_http_request_t *r, u_char *buf);
/*
 * 更新HTTP请求的文件缓存
 * 
 * @param r HTTP请求结构体指针
 * @param tf 临时文件结构体指针
 * @return 无返回值
 */
void ngx_http_file_cache_update(ngx_http_request_t *r, ngx_temp_file_t *tf);
/*
 * 更新HTTP请求的文件缓存头部信息
 * 
 * @param r HTTP请求结构体指针
 * @return 无返回值
 */
void ngx_http_file_cache_update_header(ngx_http_request_t *r);

/*
 * 发送HTTP缓存响应
 *
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功发送缓存响应
 *         NGX_ERROR 发送缓存响应失败
 *         NGX_AGAIN 需要进一步处理
 *
 * 此函数负责将缓存的内容发送给客户端。它会检查缓存的有效性，
 * 设置适当的响应头，并将缓存的内容写入响应体。如果缓存已过期
 * 或需要重新验证，函数可能会返回NGX_AGAIN以指示需要进一步处理。
 */
ngx_int_t ngx_http_cache_send(ngx_http_request_t *r);
/*
 * 发送HTTP缓存响应
 * 
 * @param r HTTP请求结构体指针
 * @return NGX_OK 成功发送缓存响应
 *         NGX_ERROR 发送缓存响应失败
 *         NGX_AGAIN 需要进一步处理
 * 
 * 此函数负责将缓存的内容发送给客户端。它会检查缓存的有效性，
 * 设置适当的响应头，并将缓存的内容写入响应体。如果缓存已过期
 * 或需要重新验证，函数可能会返回NGX_AGAIN以指示需要进一步处理。
 */
void ngx_http_file_cache_free(ngx_http_cache_t *c, ngx_temp_file_t *tf);
/*
 * 计算HTTP缓存的有效时间
 * 
 * @param cache_valid 缓存有效性数组
 * @param status 缓存状态
 * @return 有效时间
 */
time_t ngx_http_file_cache_valid(ngx_array_t *cache_valid, ngx_uint_t status);

/*
 * 设置HTTP文件缓存的配置槽函数
 *
 * @param cf 配置结构体指针
 * @param cmd 命令结构体指针
 * @param conf 配置指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 */
char *ngx_http_file_cache_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 设置HTTP文件缓存有效性的配置槽函数
 *
 * @param cf 配置结构体指针
 * @param cmd 命令结构体指针
 * @param conf 配置指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误字符串
 */
char *ngx_http_file_cache_valid_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


/*
 * 定义HTTP缓存状态字符串数组
 * 这个数组包含了不同缓存状态的描述字符串
 * 例如: "MISS", "BYPASS", "EXPIRED", "UPDATING", "STALE", "REVALIDATED", "HIT"
 * 用于在日志和调试输出中表示缓存的当前状态
 */
extern ngx_str_t  ngx_http_cache_status[];


#endif /* _NGX_HTTP_CACHE_H_INCLUDED_ */
