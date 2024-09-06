
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CYCLE_H_INCLUDED_
#define _NGX_CYCLE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* 如果没有定义NGX_CYCLE_POOL_SIZE，则使用默认的内存池大小 */
#ifndef NGX_CYCLE_POOL_SIZE
#define NGX_CYCLE_POOL_SIZE     NGX_DEFAULT_POOL_SIZE
#endif


/* 调试点停止标志 */
#define NGX_DEBUG_POINTS_STOP   1
/* 调试点中止标志 */
#define NGX_DEBUG_POINTS_ABORT  2


/* 定义共享内存区域结构体的别名 */
typedef struct ngx_shm_zone_s  ngx_shm_zone_t;

/* 定义共享内存区域初始化函数指针类型 */
typedef ngx_int_t (*ngx_shm_zone_init_pt) (ngx_shm_zone_t *zone, void *data);

/**
 * @brief 共享内存区域结构体
 */
struct ngx_shm_zone_s {
    void                     *data;    /**< 用户自定义数据指针 */
    ngx_shm_t                 shm;     /**< 共享内存结构体 */
    ngx_shm_zone_init_pt      init;    /**< 共享内存初始化函数指针 */
    void                     *tag;     /**< 标记指针，用于识别共享内存区域 */
    void                     *sync;    /**< 同步对象指针 */
    ngx_uint_t                noreuse; /**< 是否不重用标志，原为位域 unsigned noreuse:1 */
};

/**
 * @brief Nginx主循环结构体
 */
struct ngx_cycle_s {
    void                  ****conf_ctx;        /**< 配置上下文 */
    ngx_pool_t               *pool;            /**< 内存池 */

    ngx_log_t                *log;             /**< 日志对象 */
    ngx_log_t                 new_log;         /**< 新的日志对象 */

    ngx_uint_t                log_use_stderr;  /**< 是否使用标准错误输出日志 */

    ngx_connection_t        **files;           /**< 文件描述符数组 */
    ngx_connection_t         *free_connections; /**< 空闲连接链表 */
    ngx_uint_t                free_connection_n; /**< 空闲连接数量 */

    ngx_module_t            **modules;         /**< 模块数组 */
    ngx_uint_t                modules_n;       /**< 模块数量 */
    ngx_uint_t                modules_used;    /**< 模块是否被使用 */

    ngx_queue_t               reusable_connections_queue; /**< 可重用连接队列 */
    ngx_uint_t                reusable_connections_n;     /**< 可重用连接数量 */
    time_t                    connections_reuse_time;     /**< 连接重用时间 */

    ngx_array_t               listening;       /**< 监听数组 */
    ngx_array_t               paths;           /**< 路径数组 */

    ngx_array_t               config_dump;     /**< 配置转储数组 */
    ngx_rbtree_t              config_dump_rbtree; /**< 配置转储红黑树 */
    ngx_rbtree_node_t         config_dump_sentinel; /**< 配置转储红黑树哨兵节点 */

    ngx_list_t                open_files;      /**< 打开的文件列表 */
    ngx_list_t                shared_memory;   /**< 共享内存列表 */

    ngx_uint_t                connection_n;    /**< 连接数量 */
    ngx_uint_t                files_n;         /**< 文件数量 */

    ngx_connection_t         *connections;     /**< 连接数组 */
    ngx_event_t              *read_events;     /**< 读事件数组 */
    ngx_event_t              *write_events;    /**< 写事件数组 */

    ngx_cycle_t              *old_cycle;       /**< 旧的cycle指针 */

    ngx_str_t                 conf_file;       /**< 配置文件路径 */
    ngx_str_t                 conf_param;      /**< 配置参数 */
    ngx_str_t                 conf_prefix;     /**< 配置文件前缀 */
    ngx_str_t                 prefix;          /**< 安装目录前缀 */
    ngx_str_t                 error_log;       /**< 错误日志路径 */
    ngx_str_t                 lock_file;       /**< 锁文件路径 */
    ngx_str_t                 hostname;        /**< 主机名 */
};

typedef struct {
    ngx_flag_t                daemon;          /**< 是否以守护进程方式运行 */
    ngx_flag_t                master;          /**< 是否以master进程方式运行 */

    ngx_msec_t                timer_resolution; /**< 定时器分辨率 */
    ngx_msec_t                shutdown_timeout; /**< 关闭超时时间 */

    ngx_int_t                 worker_processes; /**< worker进程数量 */
    ngx_int_t                 debug_points;    /**< 调试点设置 */

    ngx_int_t                 rlimit_nofile;   /**< 文件描述符限制 */
    off_t                     rlimit_core;     /**< 核心文件大小限制 */

    int                       priority;        /**< 进程优先级 */

    ngx_uint_t                cpu_affinity_auto; /**< 是否自动设置CPU亲和性 */
    ngx_uint_t                cpu_affinity_n;  /**< CPU亲和性设置数量 */
    ngx_cpuset_t             *cpu_affinity;    /**< CPU亲和性设置 */

    char                     *username;        /**< 运行用户名 */
    ngx_uid_t                 user;            /**< 运行用户ID */
    ngx_gid_t                 group;           /**< 运行用户组ID */

    ngx_str_t                 working_directory; /**< 工作目录 */
    ngx_str_t                 lock_file;       /**< 锁文件路径 */

    ngx_str_t                 pid;             /**< PID文件路径 */
    ngx_str_t                 oldpid;          /**< 旧PID文件路径 */

    ngx_array_t               env;             /**< 环境变量数组 */
    char                    **environment;     /**< 环境变量指针数组 */

    ngx_uint_t                transparent;     /**< 透明代理标志 */
} ngx_core_conf_t;


/**
 * @brief 判断当前cycle是否为初始化cycle
 * @param cycle 要判断的cycle
 * @return 如果是初始化cycle返回true，否则返回false
 */
#define ngx_is_init_cycle(cycle)  (cycle->conf_ctx == NULL)


/**
 * @brief 初始化新的cycle
 * @param old_cycle 旧的cycle
 * @return 初始化后的新cycle
 */
ngx_cycle_t *ngx_init_cycle(ngx_cycle_t *old_cycle);

/**
 * @brief 创建PID文件
 * @param name PID文件名
 * @param log 日志对象
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_create_pidfile(ngx_str_t *name, ngx_log_t *log);

/**
 * @brief 删除PID文件
 * @param cycle 当前cycle
 */
void ngx_delete_pidfile(ngx_cycle_t *cycle);

/**
 * @brief 向进程发送信号
 * @param cycle 当前cycle
 * @param sig 信号字符串
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_signal_process(ngx_cycle_t *cycle, char *sig);

/**
 * @brief 重新打开文件
 * @param cycle 当前cycle
 * @param user 用户ID
 */
void ngx_reopen_files(ngx_cycle_t *cycle, ngx_uid_t user);

/**
 * @brief 设置环境变量
 * @param cycle 当前cycle
 * @param last 环境变量数量
 * @return 环境变量数组
 */
char **ngx_set_environment(ngx_cycle_t *cycle, ngx_uint_t *last);

/**
 * @brief 执行新的二进制文件
 * @param cycle 当前cycle
 * @param argv 参数数组
 * @return 新进程的PID
 */
ngx_pid_t ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv);

/**
 * @brief 获取CPU亲和性设置
 * @param n CPU核心数
 * @return CPU亲和性设置
 */
ngx_cpuset_t *ngx_get_cpu_affinity(ngx_uint_t n);

/**
 * @brief 添加共享内存区域
 * @param cf 配置对象
 * @param name 共享内存区域名称
 * @param size 共享内存区域大小
 * @param tag 标识
 * @return 共享内存区域对象
 */
ngx_shm_zone_t *ngx_shared_memory_add(ngx_conf_t *cf, ngx_str_t *name,
    size_t size, void *tag);

/**
 * @brief 设置关闭定时器
 * @param cycle 当前cycle
 */
void ngx_set_shutdown_timer(ngx_cycle_t *cycle);


/**
 * @brief 当前运行的cycle指针
 * 
 * 这是一个指向当前正在运行的ngx_cycle_t结构的指针。
 * 它被声明为volatile，以确保在多线程环境中的正确行为。
 */
extern volatile ngx_cycle_t  *ngx_cycle;

/**
 * @brief 旧cycle数组
 * 
 * 这个数组用于存储旧的cycle实例，主要用于平滑升级和重启过程。
 */
extern ngx_array_t            ngx_old_cycles;

/**
 * @brief 核心模块
 * 
 * 这是Nginx的核心模块，负责处理基本的配置和初始化工作。
 */
extern ngx_module_t           ngx_core_module;

/**
 * @brief 测试配置标志
 * 
 * 当设置为非零值时，表示Nginx正在测试配置文件，而不是实际运行。
 */
extern ngx_uint_t             ngx_test_config;

/**
 * @brief 转储配置标志
 * 
 * 当设置为非零值时，表示Nginx应该转储其配置信息。
 */
extern ngx_uint_t             ngx_dump_config;

/**
 * @brief 安静模式标志
 * 
 * 当设置为非零值时，表示Nginx应该以安静模式运行，减少输出信息。
 */
extern ngx_uint_t             ngx_quiet_mode;


#endif /* _NGX_CYCLE_H_INCLUDED_ */
