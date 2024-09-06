
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_FILE_H_INCLUDED_
#define _NGX_FILE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 文件结构体
 *
 * 该结构体用于表示和管理文件相关的信息和操作。
 * 它包含了文件描述符、文件名、文件信息、偏移量等重要属性，
 * 以及用于日志记录、线程处理和异步I/O的相关字段。
 */
struct ngx_file_s {
    ngx_fd_t                   fd;          // 文件描述符
    ngx_str_t                  name;        // 文件名
    ngx_file_info_t            info;        // 文件信息结构体

    off_t                      offset;      // 文件偏移量
    off_t                      sys_offset;  // 系统文件偏移量

    ngx_log_t                 *log;         // 日志指针

#if (NGX_THREADS || NGX_COMPAT)
    ngx_int_t                (*thread_handler)(ngx_thread_task_t *task,
                                               ngx_file_t *file);  // 线程处理函数指针
    void                      *thread_ctx;   // 线程上下文
    ngx_thread_task_t         *thread_task;  // 线程任务指针
#endif

#if (NGX_HAVE_FILE_AIO || NGX_COMPAT)
    ngx_event_aio_t           *aio;          // 异步I/O事件指针
#endif

    unsigned                   valid_info:1; // 文件信息是否有效的标志位
    unsigned                   directio:1;   // 是否使用直接I/O的标志位
};


// 定义最大路径层级数
#define NGX_MAX_PATH_LEVEL  3


// 定义路径管理器函数指针类型
typedef ngx_msec_t (*ngx_path_manager_pt) (void *data);

// 定义路径清理器函数指针类型
typedef ngx_msec_t (*ngx_path_purger_pt) (void *data);

// 定义路径加载器函数指针类型
typedef void (*ngx_path_loader_pt) (void *data);



/**
 * @brief 路径结构体
 *
 * 该结构体用于表示和管理文件系统中的路径。
 * 它包含了路径的基本信息、管理函数以及相关的配置信息。
 */
typedef struct {
    ngx_str_t                  name;        // 路径名称
    size_t                     len;         // 路径长度
    size_t                     level[NGX_MAX_PATH_LEVEL];  // 路径层级

    ngx_path_manager_pt        manager;     // 路径管理器函数指针
    ngx_path_purger_pt         purger;      // 路径清理器函数指针
    ngx_path_loader_pt         loader;      // 路径加载器函数指针
    void                      *data;        // 用户自定义数据

    u_char                    *conf_file;   // 配置文件名
    ngx_uint_t                 line;        // 配置文件行号
} ngx_path_t;


typedef struct {
    ngx_str_t                  name;        // 路径名称
    size_t                     level[NGX_MAX_PATH_LEVEL];  // 路径层级
} ngx_path_init_t;


/**
 * @brief 临时文件结构体
 *
 * 该结构体用于表示和管理临时文件。
 * 它包含了文件的基本信息、路径、内存池以及相关的配置信息。
 */
typedef struct {
    ngx_file_t                 file;        // 文件结构
    off_t                      offset;      // 文件偏移量
    ngx_path_t                *path;        // 路径指针
    ngx_pool_t                *pool;        // 内存池指针
    char                      *warn;        // 警告信息

    ngx_uint_t                 access;      // 访问权限

    unsigned                   log_level:8; // 日志级别
    unsigned                   persistent:1;// 是否持久化
    unsigned                   clean:1;     // 是否清理
    unsigned                   thread_write:1; // 是否使用线程写入
} ngx_temp_file_t;


/**
 * @brief 扩展重命名文件结构体
 *
 * 该结构体用于表示和管理文件重命名操作的扩展信息。
 * 它包含了文件和路径的访问权限、时间戳、文件描述符等相关信息。
 */
typedef struct {
    ngx_uint_t                 access;      // 文件访问权限
    ngx_uint_t                 path_access; // 路径访问权限
    time_t                     time;        // 时间戳
    ngx_fd_t                   fd;          // 文件描述符

    unsigned                   create_path:1; // 是否创建路径
    unsigned                   delete_file:1; // 是否删除文件

    ngx_log_t                 *log;         // 日志指针
} ngx_ext_rename_file_t;


/**
 * @brief 文件复制结构体
 *
 * 该结构体用于表示和管理文件复制操作的相关信息。
 * 它包含了文件大小、缓冲区大小、访问权限、时间戳等属性，
 * 以及用于日志记录的指针。
 */
typedef struct {
    off_t                      size;        // 文件大小
    size_t                     buf_size;    // 缓冲区大小

    ngx_uint_t                 access;      // 访问权限
    time_t                     time;        // 时间戳

    ngx_log_t                 *log;         // 日志指针
} ngx_copy_file_t;


/**
 * @brief 树形结构上下文的前向声明
 */
typedef struct ngx_tree_ctx_s  ngx_tree_ctx_t;

/**
 * @brief 树形结构初始化处理函数的函数指针类型
 *
 * @param ctx 上下文指针
 * @param prev 前一个节点的指针
 * @return ngx_int_t 返回处理结果
 */
typedef ngx_int_t (*ngx_tree_init_handler_pt) (void *ctx, void *prev);

/**
 * @brief 树形结构处理函数的函数指针类型
 *
 * @param ctx 树形结构上下文指针
 * @param name 当前处理的文件或目录名
 * @return ngx_int_t 返回处理结果
 */
typedef ngx_int_t (*ngx_tree_handler_pt) (ngx_tree_ctx_t *ctx, ngx_str_t *name);

/**
 * @brief 树形结构上下文
 *
 * 该结构体用于表示和管理树形结构遍历过程中的上下文信息。
 */
struct ngx_tree_ctx_s {
    off_t                      size;        // 文件大小
    off_t                      fs_size;     // 文件系统大小
    ngx_uint_t                 access;      // 访问权限
    time_t                     mtime;       // 最后修改时间

    ngx_tree_init_handler_pt   init_handler;    // 初始化处理函数
    ngx_tree_handler_pt        file_handler;    // 文件处理函数
    ngx_tree_handler_pt        pre_tree_handler;    // 树前处理函数
    ngx_tree_handler_pt        post_tree_handler;   // 树后处理函数
    ngx_tree_handler_pt        spec_handler;    // 特殊处理函数

    void                      *data;        // 用户自定义数据
    size_t                     alloc;       // 分配的内存大小

    ngx_log_t                 *log;         // 日志指针
};


/**
 * @brief 获取完整的文件名
 *
 * @param pool 内存池
 * @param prefix 前缀
 * @param name 文件名
 * @return ngx_int_t 操作结果
 */
ngx_int_t ngx_get_full_name(ngx_pool_t *pool, ngx_str_t *prefix,
    ngx_str_t *name);

/**
 * @brief 将链表数据写入临时文件
 *
 * @param tf 临时文件结构
 * @param chain 数据链表
 * @return ssize_t 写入的字节数
 */
ssize_t ngx_write_chain_to_temp_file(ngx_temp_file_t *tf, ngx_chain_t *chain);

/**
 * @brief 创建临时文件
 *
 * @param file 文件结构
 * @param path 路径
 * @param pool 内存池
 * @param persistent 是否持久化
 * @param clean 是否清理
 * @param access 访问权限
 * @return ngx_int_t 操作结果
 */
ngx_int_t ngx_create_temp_file(ngx_file_t *file, ngx_path_t *path,
    ngx_pool_t *pool, ngx_uint_t persistent, ngx_uint_t clean,
    ngx_uint_t access);

/**
 * @brief 创建哈希文件名
 *
 * @param path 路径
 * @param file 文件名缓冲区
 * @param len 缓冲区长度
 */
void ngx_create_hashed_filename(ngx_path_t *path, u_char *file, size_t len);

/**
 * @brief 创建路径
 *
 * @param file 文件结构
 * @param path 路径
 * @return ngx_int_t 操作结果
 */
ngx_int_t ngx_create_path(ngx_file_t *file, ngx_path_t *path);

/**
 * @brief 创建完整路径
 *
 * @param dir 目录
 * @param access 访问权限
 * @return ngx_err_t 错误码
 */
ngx_err_t ngx_create_full_path(u_char *dir, ngx_uint_t access);

/**
 * @brief 添加路径
 *
 * @param cf 配置结构
 * @param slot 路径槽位
 * @return ngx_int_t 操作结果
 */
ngx_int_t ngx_add_path(ngx_conf_t *cf, ngx_path_t **slot);

/**
 * @brief 创建多个路径
 *
 * @param cycle 循环结构
 * @param user 用户ID
 * @return ngx_int_t 操作结果
 */
ngx_int_t ngx_create_paths(ngx_cycle_t *cycle, ngx_uid_t user);

/**
 * @brief 扩展重命名文件
 *
 * @param src 源文件名
 * @param to 目标文件名
 * @param ext 扩展重命名结构
 * @return ngx_int_t 操作结果
 */
ngx_int_t ngx_ext_rename_file(ngx_str_t *src, ngx_str_t *to,
    ngx_ext_rename_file_t *ext);

/**
 * @brief 复制文件
 *
 * @param from 源文件路径
 * @param to 目标文件路径
 * @param cf 复制文件结构
 * @return ngx_int_t 操作结果
 */
ngx_int_t ngx_copy_file(u_char *from, u_char *to, ngx_copy_file_t *cf);

/**
 * @brief 遍历目录树
 *
 * @param ctx 树形结构上下文
 * @param tree 树根路径
 * @return ngx_int_t 操作结果
 */
ngx_int_t ngx_walk_tree(ngx_tree_ctx_t *ctx, ngx_str_t *tree);

/**
 * @brief 生成下一个临时文件编号
 *
 * @param collision 碰撞次数
 * @return ngx_atomic_uint_t 生成的临时文件编号
 */
ngx_atomic_uint_t ngx_next_temp_number(ngx_uint_t collision);

/**
 * @brief 设置配置文件中的路径槽位
 *
 * @param cf 配置结构
 * @param cmd 命令结构
 * @param conf 配置指针
 * @return char* 设置结果字符串
 */
char *ngx_conf_set_path_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

/**
 * @brief 合并路径值
 *
 * @param cf 配置结构
 * @param path 路径指针的指针
 * @param prev 前一个路径
 * @param init 路径初始化结构
 * @return char* 合并结果字符串
 */
char *ngx_conf_merge_path_value(ngx_conf_t *cf, ngx_path_t **path,
    ngx_path_t *prev, ngx_path_init_t *init);

/**
 * @brief 设置配置文件中的访问权限槽位
 *
 * @param cf 配置结构
 * @param cmd 命令结构
 * @param conf 配置指针
 * @return char* 设置结果字符串
 */
char *ngx_conf_set_access_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


/**
 * @brief 临时文件编号的原子变量指针
 */
extern ngx_atomic_t      *ngx_temp_number;

/**
 * @brief 随机数的原子整型变量
 */
extern ngx_atomic_int_t   ngx_random_number;


#endif /* _NGX_FILE_H_INCLUDED_ */
