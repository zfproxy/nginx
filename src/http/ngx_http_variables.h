
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_VARIABLES_H_INCLUDED_
#define _NGX_HTTP_VARIABLES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/*
 * 定义 ngx_http_variable_value_t 类型
 *
 * 这是一个类型别名，将 ngx_variable_value_t 重命名为 ngx_http_variable_value_t
 *
 * 主要用途:
 * 1. 在 HTTP 模块中表示变量值
 * 2. 保持与通用变量值类型 (ngx_variable_value_t) 的兼容性
 * 3. 提供 HTTP 模块特定的语义，使代码更易读
 *
 * 注意:
 * - 这个类型别名允许 HTTP 模块使用更具描述性的类型名
 * - 底层结构和功能与 ngx_variable_value_t 完全相同
 * - 在 HTTP 相关的变量操作中广泛使用
 */
typedef ngx_variable_value_t  ngx_http_variable_value_t;

/*
 * 定义 ngx_http_variable 宏
 *
 * 这个宏用于创建一个 ngx_http_variable_value_t 类型的静态初始化器
 *
 * 参数:
 *   v: 变量的字符串值
 *
 * 返回值:
 *   一个包含6个元素的初始化列表，用于初始化 ngx_http_variable_value_t 结构
 *
 * 主要用途:
 * 1. 快速初始化 HTTP 变量值
 * 2. 设置变量的长度、有效性和类型标志
 * 3. 将字符串转换为 HTTP 变量值格式
 *
 * 注意:
 * - sizeof(v) - 1 计算字符串长度（不包括结尾的空字符）
 * - 1 表示变量值有效
 * - 后面的三个 0 设置了其他标志位
 * - (u_char *) v 将字符串转换为无符号字符指针
 */
#define ngx_http_variable(v)     { sizeof(v) - 1, 1, 0, 0, 0, (u_char *) v }

/*
 * 定义 ngx_http_variable_t 类型
 *
 * 这是一个类型别名，将 struct ngx_http_variable_s 重命名为 ngx_http_variable_t
 *
 * 主要用途:
 * 1. 在 HTTP 模块中表示变量结构
 * 2. 提供一个简洁的类型名，使代码更易读
 * 3. 封装变量相关的属性和操作
 *
 * 注意:
 * - 这个类型别名通常用于声明和操作 HTTP 变量
 * - 具体的结构定义可能在其他地方提供
 * - 使用这种方式可以隐藏实现细节，提高代码的可维护性
 */
typedef struct ngx_http_variable_s  ngx_http_variable_t;

/*
 * 定义 ngx_http_set_variable_pt 函数指针类型
 *
 * 这个类型用于定义设置 HTTP 变量值的函数
 *
 * 参数:
 *   r: 指向 ngx_http_request_t 结构的指针，表示当前的 HTTP 请求
 *   v: 指向 ngx_http_variable_value_t 结构的指针，用于存储变量的值
 *   data: 用户定义的数据，通常用于传递额外的上下文信息
 *
 * 返回值: void
 *
 * 主要用途:
 * 1. 为 HTTP 模块提供一个统一的接口来设置变量值
 * 2. 允许动态计算和设置变量值
 * 3. 在变量系统中实现自定义变量的设置逻辑
 *
 * 注意:
 * - 这个函数指针类型通常用于 ngx_http_variable_t 结构中
 * - 实现此类函数时应注意线程安全和性能问题
 * - 可能需要考虑内存分配和释放的问题
 */
typedef void (*ngx_http_set_variable_pt) (ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
/*
 * 定义 ngx_http_get_variable_pt 函数指针类型
 *
 * 这个类型用于定义获取 HTTP 变量值的函数
 *
 * 参数:
 *   r: 指向 ngx_http_request_t 结构的指针，表示当前的 HTTP 请求
 *   v: 指向 ngx_http_variable_value_t 结构的指针，用于存储获取到的变量值
 *   data: 用户定义的数据，通常用于传递额外的上下文信息
 *
 * 返回值: ngx_int_t 类型，表示获取变量值的结果
 *   - 成功时通常返回 NGX_OK
 *   - 失败时可能返回错误码
 *
 * 主要用途:
 * 1. 为 HTTP 模块提供一个统一的接口来获取变量值
 * 2. 允许动态计算和获取变量值
 * 3. 在变量系统中实现自定义变量的获取逻辑
 *
 * 注意:
 * - 这个函数指针类型通常用于 ngx_http_variable_t 结构中
 * - 实现此类函数时应注意线程安全和性能问题
 * - 可能需要考虑缓存机制以提高效率
 */
typedef ngx_int_t (*ngx_http_get_variable_pt) (ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);


/*
 * 定义 NGX_HTTP_VAR_CHANGEABLE 宏
 *
 * 这个宏用于标识 HTTP 变量是可变的
 *
 * 值: 1
 *
 * 主要用途:
 * 1. 表示该变量的值可以在运行时被修改
 * 2. 用于设置 ngx_http_variable_t 结构的 flags 字段
 * 3. 影响变量的处理逻辑，允许动态更新变量值
 *
 * 注意:
 * - 设置此标志的变量可能需要额外的处理来确保线程安全
 * - 可变变量通常用于存储会话状态或动态生成的内容
 * - 在使用可变变量时需要考虑性能影响，因为它们可能不会被缓存
 */
#define NGX_HTTP_VAR_CHANGEABLE   1
/*
 * 定义 NGX_HTTP_VAR_NOCACHEABLE 宏
 *
 * 这个宏用于标识 HTTP 变量是不可缓存的
 *
 * 值: 2
 *
 * 主要用途:
 * 1. 表示该变量的值不应被缓存
 * 2. 用于设置 ngx_http_variable_t 结构的 flags 字段
 * 3. 影响变量的处理逻辑，防止不适当的缓存
 *
 * 注意:
 * - 设置此标志的变量每次访问时都会重新计算
 * - 适用于动态生成或频繁变化的变量
 * - 使用不可缓存变量可能会影响性能，因为它们需要频繁更新
 */
#define NGX_HTTP_VAR_NOCACHEABLE  2
/*
 * 定义 NGX_HTTP_VAR_INDEXED 宏
 *
 * 这个宏用于标识 HTTP 变量是索引的
 *
 * 值: 4
 *
 * 主要用途:
 * 1. 表示该变量可以通过索引快速访问
 * 2. 用于设置 ngx_http_variable_t 结构的 flags 字段
 * 3. 影响变量的存储和检索方式，提高访问效率
 *
 * 注意:
 * - 设置此标志的变量通常存储在预分配的数组中
 * - 索引变量可以通过数字索引而不是名称快速访问
 * - 使用索引变量可以提高性能，特别是在频繁访问的场景中
 */
#define NGX_HTTP_VAR_INDEXED      4
/*
 * 定义 NGX_HTTP_VAR_NOHASH 宏
 *
 * 这个宏用于标识 HTTP 变量不需要哈希处理
 *
 * 值: 8
 *
 * 主要用途:
 * 1. 表示该变量不需要被添加到变量哈希表中
 * 2. 用于设置 ngx_http_variable_t 结构的 flags 字段
 * 3. 影响变量的查找和存储方式，可能用于特殊处理的变量
 *
 * 注意:
 * - 设置此标志的变量通常是通过其他方式访问，而不是通过名称查找
 * - 可能用于内部变量或特殊用途的变量
 * - 使用此标志可以减少哈希表的开销，但可能限制变量的某些使用方式
 */
#define NGX_HTTP_VAR_NOHASH       8
/*
 * 定义 NGX_HTTP_VAR_WEAK 宏
 *
 * 这个宏用于标识 HTTP 变量是弱引用
 *
 * 值: 16
 *
 * 主要用途:
 * 1. 表示该变量是弱引用，可能在某些情况下被忽略或覆盖
 * 2. 用于设置 ngx_http_variable_t 结构的 flags 字段
 * 3. 影响变量的处理逻辑，可能在特定情况下不被使用
 *
 * 注意:
 * - 设置此标志的变量可能在某些上下文中被忽略
 * - 弱引用变量可能被其他同名的强引用变量覆盖
 * - 使用弱引用变量可以提供更灵活的变量处理机制
 */
#define NGX_HTTP_VAR_WEAK         16
/*
 * 定义 NGX_HTTP_VAR_PREFIX 宏
 *
 * 这个宏用于标识 HTTP 变量是前缀变量
 *
 * 值: 32
 *
 * 主要用途:
 * 1. 表示该变量是一个前缀变量，可能用于匹配多个相关变量
 * 2. 用于设置 ngx_http_variable_t 结构的 flags 字段
 * 3. 影响变量的匹配和处理逻辑，允许更灵活的变量使用
 *
 * 注意:
 * - 设置此标志的变量可能用于匹配以其名称为前缀的其他变量
 * - 前缀变量可能在配置解析或请求处理中有特殊的处理方式
 * - 使用前缀变量可以简化某些配置和处理逻辑，特别是在处理一组相关变量时
 */
#define NGX_HTTP_VAR_PREFIX       32


/*
 * 定义 ngx_http_variable_s 结构体
 *
 * 这个结构体用于表示 Nginx HTTP 模块中的变量
 *
 * 主要用途:
 * 1. 存储变量的名称、处理函数和其他属性
 * 2. 在 Nginx 的 HTTP 模块中管理和操作变量
 * 3. 提供变量的索引和标志信息
 *
 * 注意:
 * - 这个结构体是 Nginx HTTP 变量系统的核心
 * - 包含了变量的各种元数据和操作函数
 * - 在 HTTP 请求处理过程中广泛使用
 */
struct ngx_http_variable_s {
    ngx_str_t                     name;   /* 变量名称，必须是第一个成员以构建哈希表 */
    ngx_http_set_variable_pt      set_handler;  /* 设置变量值的处理函数 */
    ngx_http_get_variable_pt      get_handler;  /* 获取变量值的处理函数 */
    uintptr_t                     data;   /* 与变量相关的自定义数据 */
    ngx_uint_t                    flags;  /* 变量的标志位，用于指定变量的特性 */
    ngx_uint_t                    index;  /* 变量在索引数组中的位置 */
};

/*
 * 定义 ngx_http_null_variable 宏
 *
 * 这个宏用于创建一个空的 HTTP 变量结构
 *
 * 主要用途:
 * 1. 初始化一个空的 ngx_http_variable_t 结构
 * 2. 用作默认值或占位符
 *
 * 宏展开后的结构:
 * - name: ngx_null_string (空字符串)
 * - set_handler: NULL (无设置处理函数)
 * - get_handler: NULL (无获取处理函数)
 * - data: 0 (无数据)
 * - flags: 0 (无标志)
 * - index: 0 (无索引)
 *
 * 注意:
 * - 这个宏通常用于初始化或重置变量结构
 * - 在需要一个空变量时可以直接使用此宏
 */
#define ngx_http_null_variable  { ngx_null_string, NULL, NULL, 0, 0, 0 }


/**
 * @brief 向 Nginx HTTP 模块添加一个新变量
 *
 * 这个函数用于在 Nginx 配置阶段向 HTTP 模块添加一个新的变量。
 *
 * @param cf 指向 ngx_conf_t 结构的指针，包含当前配置上下文
 * @param name 指向 ngx_str_t 结构的指针，表示要添加的变量名
 * @param flags 变量的标志，用于指定变量的特性和行为
 *
 * @return 返回指向新添加的 ngx_http_variable_t 结构的指针，如果添加失败则返回 NULL
 *
 * 注意:
 * - 这个函数通常在模块的配置处理函数中调用
 * - 添加的变量可以在后续的配置和请求处理中使用
 * - flags 参数可以用来设置变量的各种属性，如是否可缓存等
 */
ngx_http_variable_t *ngx_http_add_variable(ngx_conf_t *cf, ngx_str_t *name,
    ngx_uint_t flags);
/**
 * @brief 获取 HTTP 变量的索引
 *
 * 这个函数用于获取指定名称的 HTTP 变量的索引值。
 *
 * @param cf 指向 ngx_conf_t 结构的指针，包含当前配置上下文
 * @param name 指向 ngx_str_t 结构的指针，表示要查找的变量名
 *
 * @return 返回变量的索引值（ngx_int_t 类型）
 *         如果变量不存在或发生错误，则返回 NGX_ERROR
 *
 * 注意:
 * - 这个函数通常在配置解析阶段使用，用于获取变量的索引以便后续快速访问
 * - 返回的索引值可以用于 ngx_http_get_indexed_variable 函数来获取变量值
 * - 如果变量不存在，函数可能会创建一个新的变量并返回其索引
 */
ngx_int_t ngx_http_get_variable_index(ngx_conf_t *cf, ngx_str_t *name);
/**
 * @brief 获取指定索引的 HTTP 变量值
 *
 * 这个函数用于根据给定的索引获取 HTTP 变量的值。
 *
 * @param r 指向 ngx_http_request_t 结构的指针，表示当前的 HTTP 请求
 * @param index 变量的索引值，通常由 ngx_http_get_variable_index 函数获得
 *
 * @return 返回指向 ngx_http_variable_value_t 结构的指针，包含变量的值
 *         如果变量不存在或发生错误，则返回 NULL
 *
 * 注意:
 * - 这个函数通常在请求处理阶段使用，用于快速获取变量的值
 * - 索引必须是有效的，否则可能导致未定义行为
 * - 返回的指针指向的内存由 Nginx 管理，不需要手动释放
 */
ngx_http_variable_value_t *ngx_http_get_indexed_variable(ngx_http_request_t *r,
    ngx_uint_t index);
/**
 * @brief 获取刷新后的 HTTP 变量值
 *
 * 这个函数用于获取经过刷新处理的 HTTP 变量的值。
 *
 * @param r 指向 ngx_http_request_t 结构的指针，表示当前的 HTTP 请求
 * @param index 变量的索引值，通常由 ngx_http_get_variable_index 函数获得
 *
 * @return 返回指向 ngx_http_variable_value_t 结构的指针，包含刷新后的变量值
 *         如果变量不存在或发生错误，则返回 NULL
 *
 * 注意:
 * - 这个函数与 ngx_http_get_indexed_variable 类似，但会强制刷新变量的值
 * - 适用于需要获取最新值的场景，特别是对于可能在请求处理过程中发生变化的变量
 * - 使用此函数可能会影响性能，因为它会绕过变量的缓存机制
 */
ngx_http_variable_value_t *ngx_http_get_flushed_variable(ngx_http_request_t *r,
    ngx_uint_t index);

/**
 * @brief 获取指定名称的 HTTP 变量值
 *
 * 这个函数用于根据给定的变量名获取 HTTP 变量的值。
 *
 * @param r 指向 ngx_http_request_t 结构的指针，表示当前的 HTTP 请求
 * @param name 指向 ngx_str_t 结构的指针，表示要获取的变量名
 * @param key 变量名的哈希值，用于加速查找过程
 *
 * @return 返回指向 ngx_http_variable_value_t 结构的指针，包含变量的值
 *         如果变量不存在或发生错误，则返回 NULL
 *
 * 注意:
 * - 这个函数通常在需要动态查找变量时使用，性能可能低于使用索引的方法
 * - 返回的指针指向的内存由 Nginx 管理，不需要手动释放
 * - 如果频繁使用同一变量，建议使用 ngx_http_get_variable_index 获取索引，
 *   然后使用 ngx_http_get_indexed_variable 来提高性能
 */
ngx_http_variable_value_t *ngx_http_get_variable(ngx_http_request_t *r,
    ngx_str_t *name, ngx_uint_t key);

/**
 * @brief 处理未知的 HTTP 头部变量
 *
 * 这个函数用于处理和获取未知或自定义的 HTTP 头部的值。
 *
 * @param r 指向 ngx_http_request_t 结构的指针，表示当前的 HTTP 请求
 * @param v 指向 ngx_http_variable_value_t 结构的指针，用于存储变量的值
 * @param var 指向 ngx_str_t 结构的指针，表示变量名
 * @param part 指向 ngx_list_part_t 结构的指针，表示头部链表的一部分
 * @param prefix 表示头部名称前缀的长度
 *
 * @return 返回 NGX_OK 表示成功，其他值表示错误
 *
 * 注意:
 * - 此函数通常用于处理不在标准 HTTP 头部列表中的自定义头部
 * - 可以用于实现动态头部处理或自定义变量
 * - 在处理复杂的 HTTP 头部需求时非常有用
 */
ngx_int_t ngx_http_variable_unknown_header(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, ngx_str_t *var, ngx_list_part_t *part,
    size_t prefix);


#if (NGX_PCRE)

/**
 * @brief 定义正则表达式变量结构
 *
 * 这个结构用于存储与正则表达式捕获组相关的信息。
 */
typedef struct {
    ngx_uint_t                    capture;  // 捕获组的索引
    ngx_int_t                     index;    // 变量在变量数组中的索引
} ngx_http_regex_variable_t;


/**
 * @brief 定义HTTP正则表达式结构
 *
 * 这个结构用于存储与HTTP请求处理相关的正则表达式信息。
 * 它包含了编译后的正则表达式、捕获组数量、变量信息等。
 */
typedef struct {
    ngx_regex_t                  *regex;      /* 编译后的正则表达式 */
    ngx_uint_t                    ncaptures;  /* 捕获组的数量 */
    ngx_http_regex_variable_t    *variables;  /* 与正则表达式关联的变量数组 */
    ngx_uint_t                    nvariables; /* 变量数组中的变量数量 */
    ngx_str_t                     name;       /* 正则表达式的名称 */
} ngx_http_regex_t;


/**
 * @brief 定义HTTP映射正则表达式结构
 *
 * 这个结构用于存储HTTP映射中与正则表达式相关的信息。
 * 它包含了指向正则表达式和对应值的指针。
 */
/**
 * @brief 定义HTTP映射正则表达式结构
 *
 * 这个结构用于存储HTTP映射中与正则表达式相关的信息。
 * 它包含了指向正则表达式和对应值的指针。
 */
typedef struct {
    ngx_http_regex_t             *regex;  /* 指向HTTP正则表达式结构的指针 */
    void                         *value;  /* 指向与正则表达式匹配的值的指针 */
} ngx_http_map_regex_t;


/**
 * @brief 编译HTTP正则表达式
 *
 * @param cf Nginx配置结构指针
 * @param rc 正则表达式编译结构指针
 * @return 返回编译后的ngx_http_regex_t结构指针，如果编译失败则返回NULL
 *
 * 此函数用于将给定的正则表达式编译为Nginx可用的HTTP正则表达式结构。
 * 它处理正则表达式的编译、捕获组的设置以及相关变量的初始化。
 */
ngx_http_regex_t *ngx_http_regex_compile(ngx_conf_t *cf,
    ngx_regex_compile_t *rc);
/**
 * @brief 执行HTTP正则表达式匹配
 *
 * @param r HTTP请求结构指针
 * @param re 编译后的HTTP正则表达式结构指针
 * @param s 待匹配的字符串
 * @return 返回匹配结果，成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 此函数用于对给定的字符串执行HTTP正则表达式匹配。
 * 它使用编译好的正则表达式结构进行匹配，并处理匹配结果。
 */
ngx_int_t ngx_http_regex_exec(ngx_http_request_t *r, ngx_http_regex_t *re,
    ngx_str_t *s);

#endif


/**
 * @brief 定义HTTP映射结构
 *
 * 这个结构用于存储HTTP映射相关的信息。
 * 它包含了组合哈希表和可选的正则表达式映射。
 */
typedef struct {
    ngx_hash_combined_t           hash;   /* 组合哈希表，用于快速查找非正则表达式的映射 */
#if (NGX_PCRE)
    ngx_http_map_regex_t         *regex;  /* 指向正则表达式映射数组的指针 */
    ngx_uint_t                    nregex; /* 正则表达式映射的数量 */
#endif
} ngx_http_map_t;


/**
 * @brief 在HTTP映射中查找匹配项
 *
 * @param r HTTP请求结构指针
 * @param map HTTP映射结构指针
 * @param match 待匹配的字符串
 * @return 返回匹配到的值指针，如果未找到匹配项则返回NULL
 *
 * 此函数用于在给定的HTTP映射中查找与指定字符串匹配的项。
 * 它会先在哈希表中查找，如果启用了正则表达式支持，还会尝试正则匹配。
 */
void *ngx_http_map_find(ngx_http_request_t *r, ngx_http_map_t *map,
    ngx_str_t *match);


/**
 * @brief 添加核心HTTP变量
 *
 * @param cf 配置结构指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 此函数用于向Nginx配置中添加核心HTTP变量。
 * 这些核心变量是Nginx HTTP模块的基础变量，在处理HTTP请求时经常使用。
 */
ngx_int_t ngx_http_variables_add_core_vars(ngx_conf_t *cf);
/**
 * @brief 初始化HTTP变量
 *
 * @param cf 配置结构指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 此函数用于初始化Nginx HTTP模块中的变量。
 * 它在Nginx启动时被调用，负责设置和准备所有预定义的HTTP变量。
 */
ngx_int_t ngx_http_variables_init_vars(ngx_conf_t *cf);


/**
 * @brief HTTP变量的空值
 *
 * 这是一个外部声明的全局变量，表示HTTP变量的空值。
 * 当一个HTTP变量没有被设置或者其值为空时，可以使用这个变量来表示。
 * 在Nginx的HTTP模块中，这个变量常用于初始化或者表示变量的默认状态。
 */
extern ngx_http_variable_value_t  ngx_http_variable_null_value;
/**
 * @brief HTTP变量的真值
 *
 * 这是一个外部声明的全局变量，表示HTTP变量的真值。
 * 在Nginx的HTTP模块中，这个变量通常用于表示布尔类型变量的真值状态。
 * 例如，在条件判断或者逻辑操作中，可以使用这个变量来表示"真"或"成立"的情况。
 * 它的具体值和实现可能在其他源文件中定义。
 */
extern ngx_http_variable_value_t  ngx_http_variable_true_value;


#endif /* _NGX_HTTP_VARIABLES_H_INCLUDED_ */
