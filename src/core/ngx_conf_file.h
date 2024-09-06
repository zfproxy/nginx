
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONF_FILE_H_INCLUDED_
#define _NGX_CONF_FILE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 *        AAAA  number of arguments
 *      FF      command flags
 *    TT        command type, i.e. HTTP "location" or "server" command
 */

/* 定义配置指令参数数量的宏 */
#define NGX_CONF_NOARGS      0x00000001  /* 无参数 */
#define NGX_CONF_TAKE1       0x00000002  /* 接受1个参数 */
#define NGX_CONF_TAKE2       0x00000004  /* 接受2个参数 */
#define NGX_CONF_TAKE3       0x00000008  /* 接受3个参数 */
#define NGX_CONF_TAKE4       0x00000010  /* 接受4个参数 */
#define NGX_CONF_TAKE5       0x00000020  /* 接受5个参数 */
#define NGX_CONF_TAKE6       0x00000040  /* 接受6个参数 */
#define NGX_CONF_TAKE7       0x00000080  /* 接受7个参数 */

/* 定义配置指令最大参数数量 */
#define NGX_CONF_MAX_ARGS    8

/* 定义组合参数数量的宏 */
#define NGX_CONF_TAKE12      (NGX_CONF_TAKE1|NGX_CONF_TAKE2)  /* 接受1或2个参数 */
#define NGX_CONF_TAKE13      (NGX_CONF_TAKE1|NGX_CONF_TAKE3)  /* 接受1或3个参数 */

#define NGX_CONF_TAKE23      (NGX_CONF_TAKE2|NGX_CONF_TAKE3)  /* 接受2或3个参数 */

#define NGX_CONF_TAKE123     (NGX_CONF_TAKE1|NGX_CONF_TAKE2|NGX_CONF_TAKE3)  /* 接受1、2或3个参数 */
#define NGX_CONF_TAKE1234    (NGX_CONF_TAKE1|NGX_CONF_TAKE2|NGX_CONF_TAKE3   \
                              |NGX_CONF_TAKE4)  /* 接受1、2、3或4个参数 */

/* 定义配置指令类型的宏 */
#define NGX_CONF_ARGS_NUMBER 0x000000ff  /* 参数数量掩码 */
#define NGX_CONF_BLOCK       0x00000100  /* 块配置 */
#define NGX_CONF_FLAG        0x00000200  /* 布尔标志 */
#define NGX_CONF_ANY         0x00000400  /* 任意参数 */
#define NGX_CONF_1MORE       0x00000800  /* 至少1个参数 */
#define NGX_CONF_2MORE       0x00001000  /* 至少2个参数 */

/* 定义直接配置标志 */
#define NGX_DIRECT_CONF      0x00010000

/* 定义配置层级的宏 */
#define NGX_MAIN_CONF        0x01000000  /* 主配置 */
#define NGX_ANY_CONF         0xFF000000  /* 任意配置 */



/* 定义未设置的配置值 */
#define NGX_CONF_UNSET       -1                  /* 通用未设置值 */
#define NGX_CONF_UNSET_UINT  (ngx_uint_t) -1     /* 无符号整型未设置值 */
#define NGX_CONF_UNSET_PTR   (void *) -1         /* 指针未设置值 */
#define NGX_CONF_UNSET_SIZE  (size_t) -1         /* 大小未设置值 */
#define NGX_CONF_UNSET_MSEC  (ngx_msec_t) -1     /* 毫秒未设置值 */

/* 定义配置处理结果 */
#define NGX_CONF_OK          NULL                /* 配置处理成功 */
#define NGX_CONF_ERROR       (void *) -1         /* 配置处理错误 */

/* 定义配置块状态 */
#define NGX_CONF_BLOCK_START 1                   /* 配置块开始 */
#define NGX_CONF_BLOCK_DONE  2                   /* 配置块结束 */
#define NGX_CONF_FILE_DONE   3                   /* 配置文件处理完成 */

/* 定义模块类型 */
#define NGX_CORE_MODULE      0x45524F43  /* "CORE" */
#define NGX_CONF_MODULE      0x464E4F43  /* "CONF" */

/* 定义最大配置错误字符串长度 */
#define NGX_MAX_CONF_ERRSTR  1024


/**
 * @brief Nginx命令结构体
 *
 * 该结构体定义了Nginx配置文件中的一个命令。
 */
struct ngx_command_s {
    ngx_str_t             name;    /**< 命令名称 */
    ngx_uint_t            type;    /**< 命令类型 */
    char               *(*set)(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);  /**< 设置函数指针 */
    ngx_uint_t            conf;    /**< 配置类型 */
    ngx_uint_t            offset;  /**< 偏移量 */
    void                 *post;    /**< 后处理函数指针 */
};

/**
 * @brief 定义空命令
 */
#define ngx_null_command  { ngx_null_string, 0, NULL, 0, 0, NULL }

/**
 * @brief 打开文件结构体
 *
 * 该结构体用于表示一个打开的文件。
 */
struct ngx_open_file_s {
    ngx_fd_t              fd;      /**< 文件描述符 */
    ngx_str_t             name;    /**< 文件名 */

    void                (*flush)(ngx_open_file_t *file, ngx_log_t *log);  /**< 刷新函数指针 */
    void                 *data;    /**< 用户数据 */
};

/**
 * @brief 配置文件结构体
 *
 * 该结构体用于表示一个配置文件。
 */
typedef struct {
    ngx_file_t            file;    /**< 文件信息 */
    ngx_buf_t            *buffer;  /**< 缓冲区 */
    ngx_buf_t            *dump;    /**< 转储缓冲区 */
    ngx_uint_t            line;    /**< 当前行号 */
} ngx_conf_file_t;

/**
 * @brief 配置转储结构体
 *
 * 该结构体用于配置转储。
 */
typedef struct {
    ngx_str_t             name;    /**< 名称 */
    ngx_buf_t            *buffer;  /**< 缓冲区 */
} ngx_conf_dump_t;


/**
 * @brief 定义配置处理函数的函数指针类型
 *
 * 这个函数指针类型用于处理Nginx配置文件中的指令。
 * 它接收一个ngx_conf_t结构体指针作为参数，该结构体包含了配置解析的上下文信息。
 * 函数返回一个字符指针，通常用于指示处理结果（成功或错误信息）。
 *
 * @param cf 指向ngx_conf_t结构的指针，包含配置解析的上下文
 * @return 返回处理结果的字符串指针
 */
typedef char *(*ngx_conf_handler_pt)(ngx_conf_t *cf,
    ngx_command_t *dummy, void *conf);


/**
 * @brief Nginx配置结构体
 *
 * 这个结构体用于存储Nginx配置解析过程中的各种信息和上下文。
 * 它包含了配置文件的名称、参数、当前的cycle、内存池、日志等重要组件，
 * 以及用于处理特定配置指令的回调函数和相关上下文。
 */
struct ngx_conf_s {
    char                 *name;             // 当前配置指令的名称
    ngx_array_t          *args;             // 存储配置指令参数的数组

    ngx_cycle_t          *cycle;            // 指向当前nginx周期的指针
    ngx_pool_t           *pool;             // 主内存池
    ngx_pool_t           *temp_pool;        // 临时内存池，用于临时分配
    ngx_conf_file_t      *conf_file;        // 当前正在处理的配置文件信息
    ngx_log_t            *log;              // 日志对象

    void                 *ctx;              // 配置上下文，用于模块间传递信息
    ngx_uint_t            module_type;      // 当前处理的模块类型
    ngx_uint_t            cmd_type;         // 当前命令的类型

    ngx_conf_handler_pt   handler;          // 指向配置处理函数的指针
    void                 *handler_conf;     // 配置处理函数的附加配置数据
};


/**
 * @brief 配置后处理函数的函数指针类型
 *
 * 这个函数指针类型用于定义配置指令的后处理函数。
 * 后处理函数在配置指令被解析后执行，用于进行额外的验证或处理。
 *
 * @param cf 指向ngx_conf_t结构的指针，包含配置解析的上下文
 * @param data 指向与配置相关的数据
 * @param conf 指向配置结构体的指针
 * @return 返回处理结果的字符串指针，成功时通常返回NGX_CONF_OK
 */
typedef char *(*ngx_conf_post_handler_pt) (ngx_conf_t *cf,
    void *data, void *conf);

/**
 * @brief 配置后处理结构体
 *
 * 这个结构体用于定义配置指令的后处理操作。
 * 它包含一个指向后处理函数的指针。
 */
typedef struct {
    ngx_conf_post_handler_pt  post_handler;  // 指向后处理函数的指针
} ngx_conf_post_t;

/**
 * @brief 已弃用配置的结构体
 *
 * 这个结构体用于处理已弃用的配置指令。
 * 它包含后处理函数、旧指令名称和新指令名称。
 */
typedef struct {
    ngx_conf_post_handler_pt  post_handler;  // 指向后处理函数的指针
    char                     *old_name;      // 旧指令名称
    char                     *new_name;      // 新指令名称
} ngx_conf_deprecated_t;

/**
 * @brief 数值边界检查结构体
 *
 * 这个结构体用于定义数值类型配置的边界检查。
 * 它包含后处理函数和数值的上下限。
 */
typedef struct {
    ngx_conf_post_handler_pt  post_handler;  // 指向后处理函数的指针
    ngx_int_t                 low;           // 数值下限
    ngx_int_t                 high;          // 数值上限
} ngx_conf_num_bounds_t;

/**
 * @brief 枚举配置结构体
 *
 * 这个结构体用于定义枚举类型的配置选项。
 * 它包含选项的名称和对应的数值。
 */
typedef struct {
    ngx_str_t                 name;   // 枚举选项的名称
    ngx_uint_t                value;  // 枚举选项的值
} ngx_conf_enum_t;

/**
 * @brief 位掩码设置宏
 *
 * 这个宏定义用于表示位掩码配置中的设置操作。
 */
#define NGX_CONF_BITMASK_SET  1

/**
 * @brief 定义位掩码配置结构
 *
 * 这个结构用于表示Nginx配置中的位掩码选项。
 * 它包含一个名称字符串和一个对应的掩码值，
 * 通常用于处理可以组合多个选项的配置指令。
 */
typedef struct {
    ngx_str_t                 name;
    ngx_uint_t                mask;
} ngx_conf_bitmask_t;



/**
 * @brief 处理已弃用的配置指令
 *
 * 该函数用于处理Nginx配置文件中已被标记为弃用的指令。
 * 当遇到弃用的指令时，它会生成警告信息，并可能提供替代的新指令建议。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param post 指向后处理函数的指针，通常包含弃用信息
 * @param data 指向配置数据的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char * ngx_conf_deprecated(ngx_conf_t *cf, void *post, void *data);
/**
 * @brief 检查数值是否在指定范围内
 *
 * 该函数用于检查配置文件中的数值是否在指定的上下限范围内。
 * 如果数值超出范围，将会生成错误信息。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param post 指向后处理函数的指针，通常包含上下限信息
 * @param data 指向配置数据的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_check_num_bounds(ngx_conf_t *cf, void *post, void *data);


/**
 * @brief 从配置上下文中获取指定模块的配置
 *
 * 这个宏用于从给定的配置上下文中获取特定模块的配置。
 * 它通过模块的索引来访问配置上下文数组中的相应元素。
 *
 * @param conf_ctx 配置上下文数组
 * @param module 要获取配置的模块
 * @return 返回指定模块的配置
 */
#define ngx_get_conf(conf_ctx, module)  conf_ctx[module.index]



/**
 * @brief 初始化配置值的宏
 *
 * 这个宏用于初始化配置值。如果配置值未设置（等于NGX_CONF_UNSET），
 * 则将其设置为指定的默认值。
 *
 * @param conf 要初始化的配置变量
 * @param default 默认值
 */
#define ngx_conf_init_value(conf, default)                                   \
    if (conf == NGX_CONF_UNSET) {                                            \
        conf = default;                                                      \
    }

/**
 * @brief 初始化指针类型配置值的宏
 *
 * 这个宏用于初始化指针类型的配置值。如果配置值未设置（等于NGX_CONF_UNSET_PTR），
 * 则将其设置为指定的默认值。
 *
 * @param conf 要初始化的指针类型配置变量
 * @param default 默认值
 */
#define ngx_conf_init_ptr_value(conf, default)                               \
    if (conf == NGX_CONF_UNSET_PTR) {                                        \
        conf = default;                                                      \
    }

/**
 * @brief 初始化无符号整数类型配置值的宏
 *
 * 这个宏用于初始化无符号整数类型的配置值。如果配置值未设置（等于NGX_CONF_UNSET_UINT），
 * 则将其设置为指定的默认值。
 *
 * @param conf 要初始化的无符号整数类型配置变量
 * @param default 默认值
 */
#define ngx_conf_init_uint_value(conf, default)                              \
    if (conf == NGX_CONF_UNSET_UINT) {                                       \
        conf = default;                                                      \
    }

/**
 * @brief 初始化size_t类型配置值的宏
 *
 * 这个宏用于初始化size_t类型的配置值。如果配置值未设置（等于NGX_CONF_UNSET_SIZE），
 * 则将其设置为指定的默认值。
 *
 * @param conf 要初始化的size_t类型配置变量
 * @param default 默认值
 */
#define ngx_conf_init_size_value(conf, default)                              \
    if (conf == NGX_CONF_UNSET_SIZE) {                                       \
        conf = default;                                                      \
    }

/**
 * @brief 初始化毫秒级时间配置值的宏
 *
 * 这个宏用于初始化毫秒级时间类型的配置值。如果配置值未设置（等于NGX_CONF_UNSET_MSEC），
 * 则将其设置为指定的默认值。
 *
 * @param conf 要初始化的毫秒级时间类型配置变量
 * @param default 默认值
 */
#define ngx_conf_init_msec_value(conf, default)                              \
    if (conf == NGX_CONF_UNSET_MSEC) {                                       \
        conf = default;                                                      \
    }

/**
 * @brief 合并配置值的宏
 *
 * 这个宏用于合并配置值。如果当前配置值未设置（等于NGX_CONF_UNSET），
 * 则会检查前一级配置值。如果前一级配置值也未设置，则使用默认值。
 *
 * @param conf 当前配置值
 * @param prev 前一级配置值
 * @param default 默认值
 */
#define ngx_conf_merge_value(conf, prev, default)                            \
    if (conf == NGX_CONF_UNSET) {                                            \
        conf = (prev == NGX_CONF_UNSET) ? default : prev;                    \
    }

/**
 * @brief 合并指针类型配置值的宏
 *
 * 这个宏用于合并指针类型的配置值。如果当前配置值未设置（等于NGX_CONF_UNSET_PTR），
 * 则会检查前一级配置值。如果前一级配置值也未设置，则使用默认值。
 *
 * @param conf 当前配置值（指针类型）
 * @param prev 前一级配置值（指针类型）
 * @param default 默认值（指针类型）
 */
#define ngx_conf_merge_ptr_value(conf, prev, default)                        \
    if (conf == NGX_CONF_UNSET_PTR) {                                        \
        conf = (prev == NGX_CONF_UNSET_PTR) ? default : prev;                \
    }

/**
 * @brief 合并无符号整数类型配置值的宏
 *
 * 这个宏用于合并无符号整数类型的配置值。如果当前配置值未设置（等于NGX_CONF_UNSET_UINT），
 * 则会检查前一级配置值。如果前一级配置值也未设置，则使用默认值。
 *
 * @param conf 当前配置值（无符号整数类型）
 * @param prev 前一级配置值（无符号整数类型）
 * @param default 默认值（无符号整数类型）
 */
#define ngx_conf_merge_uint_value(conf, prev, default)                       \
    if (conf == NGX_CONF_UNSET_UINT) {                                       \
        conf = (prev == NGX_CONF_UNSET_UINT) ? default : prev;               \
    }

/**
 * @brief 合并毫秒级配置值的宏
 *
 * 这个宏用于合并毫秒级的配置值。如果当前配置值未设置（等于NGX_CONF_UNSET_MSEC），
 * 则会检查前一级配置值。如果前一级配置值也未设置，则使用默认值。
 *
 * @param conf 当前配置值（毫秒级）
 * @param prev 前一级配置值（毫秒级）
 * @param default 默认值（毫秒级）
 */
#define ngx_conf_merge_msec_value(conf, prev, default)                       \
    if (conf == NGX_CONF_UNSET_MSEC) {                                       \
        conf = (prev == NGX_CONF_UNSET_MSEC) ? default : prev;               \
    }

/**
 * @brief 合并秒级配置值的宏
 *
 * 这个宏用于合并秒级的配置值。如果当前配置值未设置（等于NGX_CONF_UNSET），
 * 则会检查前一级配置值。如果前一级配置值也未设置，则使用默认值。
 *
 * @param conf 当前配置值（秒级）
 * @param prev 前一级配置值（秒级）
 * @param default 默认值（秒级）
 */
#define ngx_conf_merge_sec_value(conf, prev, default)                        \
    if (conf == NGX_CONF_UNSET) {                                            \
        conf = (prev == NGX_CONF_UNSET) ? default : prev;                    \
    }

/**
 * @brief 合并size类型配置值的宏
 *
 * 这个宏用于合并size类型的配置值。如果当前配置值未设置（等于NGX_CONF_UNSET_SIZE），
 * 则会检查前一级配置值。如果前一级配置值也未设置，则使用默认值。
 *
 * @param conf 当前配置值（size类型）
 * @param prev 前一级配置值（size类型）
 * @param default 默认值（size类型）
 */
#define ngx_conf_merge_size_value(conf, prev, default)                       \
    if (conf == NGX_CONF_UNSET_SIZE) {                                       \
        conf = (prev == NGX_CONF_UNSET_SIZE) ? default : prev;               \
    }

/**
 * @brief 合并off_t类型配置值的宏
 *
 * 这个宏用于合并off_t类型的配置值。如果当前配置值未设置（等于NGX_CONF_UNSET），
 * 则会检查前一级配置值。如果前一级配置值也未设置，则使用默认值。
 *
 * @param conf 当前配置值（off_t类型）
 * @param prev 前一级配置值（off_t类型）
 * @param default 默认值（off_t类型）
 */
#define ngx_conf_merge_off_value(conf, prev, default)                        \
    if (conf == NGX_CONF_UNSET) {                                            \
        conf = (prev == NGX_CONF_UNSET) ? default : prev;                    \
    }

/**
 * @brief 合并字符串类型配置值的宏
 *
 * 这个宏用于合并字符串类型的配置值。如果当前配置值未设置（data为NULL），
 * 则会检查前一级配置值。如果前一级配置值也未设置，则使用默认值。
 *
 * @param conf 当前配置值（ngx_str_t类型）
 * @param prev 前一级配置值（ngx_str_t类型）
 * @param default 默认值（字符串字面量）
 */
#define ngx_conf_merge_str_value(conf, prev, default)                        \
    if (conf.data == NULL) {                                                 \
        if (prev.data) {                                                     \
            conf.len = prev.len;                                             \
            conf.data = prev.data;                                           \
        } else {                                                             \
            conf.len = sizeof(default) - 1;                                  \
            conf.data = (u_char *) default;                                  \
        }                                                                    \
    }

/**
 * @brief 合并缓冲区配置值的宏
 *
 * 这个宏用于合并缓冲区类型的配置值。如果当前配置的缓冲区数量为0，
 * 则会检查前一级配置值。如果前一级配置值也未设置，则使用默认的数量和大小。
 *
 * @param conf 当前配置值（通常是一个包含num和size字段的结构体）
 * @param prev 前一级配置值（同样是包含num和size字段的结构体）
 * @param default_num 默认的缓冲区数量
 * @param default_size 默认的缓冲区大小
 */
#define ngx_conf_merge_bufs_value(conf, prev, default_num, default_size)     \
    if (conf.num == 0) {                                                     \
        if (prev.num) {                                                      \
            conf.num = prev.num;                                             \
            conf.size = prev.size;                                           \
        } else {                                                             \
            conf.num = default_num;                                          \
            conf.size = default_size;                                        \
        }                                                                    \
    }

/**
 * @brief 合并位掩码配置值的宏
 *
 * 这个宏用于合并位掩码类型的配置值。如果当前配置值为0，
 * 则会检查前一级配置值。如果前一级配置值也为0，则使用默认值。
 *
 * @param conf 当前配置值（通常是无符号整数类型）
 * @param prev 前一级配置值（同样是无符号整数类型）
 * @param default 默认的位掩码值
 */
#define ngx_conf_merge_bitmask_value(conf, prev, default)                    \
    if (conf == 0) {                                                         \
        conf = (prev == 0) ? default : prev;                                 \
    }


/**
 * @brief 处理命令行参数中的配置指令
 *
 * 该函数用于解析和处理通过命令行参数"-g"传递的配置指令。
 * 它允许在不修改配置文件的情况下，通过命令行动态设置某些配置项。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_param(ngx_conf_t *cf);
/**
 * @brief 解析配置文件
 *
 * 该函数用于解析Nginx的配置文件。它会读取指定的配置文件，
 * 并根据文件内容设置相应的配置选项。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param filename 指向要解析的配置文件名的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_parse(ngx_conf_t *cf, ngx_str_t *filename);
/**
 * @brief 处理配置文件中的include指令
 *
 * 该函数用于处理Nginx配置文件中的include指令，允许包含其他配置文件。
 * 它会解析指定的文件或目录，并将其内容合并到当前的配置中。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_include(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


/**
 * @brief 获取配置文件的完整路径名
 *
 * 该函数用于获取配置文件的完整路径名。它会根据当前的Nginx周期和配置前缀，
 * 将相对路径转换为绝对路径。
 *
 * @param cycle 指向当前Nginx周期结构的指针
 * @param name 指向需要处理的文件名的指针，输入时可能是相对路径，输出时为完整路径
 * @param conf_prefix 标志位，指示是否使用配置文件前缀
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_conf_full_name(ngx_cycle_t *cycle, ngx_str_t *name,
    ngx_uint_t conf_prefix);
/**
 * @brief 打开配置文件
 *
 * 该函数用于打开指定的配置文件。它会在Nginx的文件缓存中查找文件，
 * 如果找不到则创建一个新的文件句柄。
 *
 * @param cycle 指向当前Nginx周期结构的指针
 * @param name 指向要打开的文件名的指针
 * @return 成功时返回指向ngx_open_file_t结构的指针，失败时返回NULL
 */
ngx_open_file_t *ngx_conf_open_file(ngx_cycle_t *cycle, ngx_str_t *name);
/**
 * @brief 记录配置文件解析过程中的错误日志
 *
 * 该函数用于在Nginx配置文件解析过程中记录错误日志。它可以记录不同级别的错误信息，
 * 并提供了格式化输出的功能。
 *
 * @param level 错误日志的级别
 * @param cf 指向当前Nginx配置结构的指针
 * @param err 错误码
 * @param fmt 格式化字符串
 * @param ... 可变参数列表，用于格式化输出
 */
void ngx_cdecl ngx_conf_log_error(ngx_uint_t level, ngx_conf_t *cf,
    ngx_err_t err, const char *fmt, ...);


/**
 * @brief 设置标志位配置项
 *
 * 该函数用于设置布尔类型的配置项。它将配置文件中的"on"或"off"值转换为
 * 相应的布尔值，并将其存储在指定的配置结构中。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_flag_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置字符串类型的配置项
 *
 * 该函数用于设置字符串类型的配置项。它将配置文件中的字符串值
 * 存储到指定的配置结构中。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_str_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置字符串数组类型的配置项
 *
 * 该函数用于设置字符串数组类型的配置项。它将配置文件中的多个字符串值
 * 存储到指定的配置结构中的数组中。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_str_array_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/**
 * @brief 设置键值对类型的配置项
 *
 * 该函数用于设置键值对类型的配置项。它将配置文件中的键值对
 * 解析并存储到指定的配置结构中。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_keyval_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置数值类型的配置项
 *
 * 该函数用于设置数值类型的配置项。它将配置文件中的数值
 * 解析并存储到指定的配置结构中。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_num_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置大小类型的配置项
 *
 * 该函数用于设置大小类型的配置项。它将配置文件中的大小值
 * （可能包含单位，如k、m等）解析并存储到指定的配置结构中。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置偏移量类型的配置项
 *
 * 该函数用于设置偏移量类型的配置项。它将配置文件中的偏移量值
 * （通常是一个带符号的整数）解析并存储到指定的配置结构中。
 * 偏移量通常用于表示文件位置或内存地址的相对位置。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_off_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置毫秒级时间类型的配置项
 *
 * 该函数用于设置毫秒级时间类型的配置项。它将配置文件中的时间值
 * （通常以毫秒为单位）解析并存储到指定的配置结构中。
 * 这对于需要精确到毫秒级别的时间设置非常有用，如超时设置等。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_msec_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置秒级时间类型的配置项
 *
 * 该函数用于设置秒级时间类型的配置项。它将配置文件中的时间值
 * （通常以秒为单位）解析并存储到指定的配置结构中。
 * 这对于需要以秒为单位的时间设置非常有用，如连接超时、缓存过期时间等。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_sec_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置缓冲区配置项
 *
 * 该函数用于设置缓冲区相关的配置项。它通常用于解析和设置
 * 涉及缓冲区数量和大小的配置指令。这对于需要管理内存缓冲区
 * 的模块非常有用，如处理大型请求或响应时。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_bufs_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置枚举类型的配置项
 *
 * 该函数用于设置枚举类型的配置项。它将配置文件中的枚举值
 * 解析并存储到指定的配置结构中。这对于需要从预定义的一组
 * 选项中选择一个值的配置非常有用，如日志级别、协议类型等。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_enum_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 设置位掩码类型的配置项
 *
 * 该函数用于设置位掩码类型的配置项。它将配置文件中的位掩码值
 * 解析并存储到指定的配置结构中。这对于需要使用位标志来表示
 * 多个开关或选项组合的配置非常有用，如权限设置、功能开关等。
 *
 * @param cf 指向当前Nginx配置结构的指针
 * @param cmd 指向当前正在处理的命令结构的指针
 * @param conf 指向模块配置结构的指针
 * @return 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 */
char *ngx_conf_set_bitmask_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


#endif /* _NGX_CONF_FILE_H_INCLUDED_ */
