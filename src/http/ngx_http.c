
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


/*
 * ngx_http.c
 *
 * 该文件实现了Nginx的HTTP模块核心功能。
 *
 * 支持的功能:
 * 1. HTTP请求处理
 * 2. HTTP响应生成
 * 3. HTTP模块配置管理
 * 4. HTTP请求路由
 * 5. HTTP头部处理
 * 6. HTTP连接管理
 * 7. HTTP请求体处理
 * 8. HTTP响应缓冲
 * 9. HTTP模块初始化
 * 10. HTTP请求处理阶段管理
 *
 * 支持的指令:
 * - http: 定义HTTP服务器块
 *   语法: http { ... }
 *   上下文: main
 *
 * - server: 定义虚拟服务器
 *   语法: server { ... }
 *   上下文: http
 *
 * - location: 定义URI匹配规则
 *   语法: location [modifier] uri { ... }
 *   上下文: server, location
 *
 * 支持的变量:
 * - $http_*: 任意请求头字段
 * - $arg_*: 任意GET请求参数
 * - $request_method: 请求方法
 * - $remote_addr: 客户端IP地址
 * - $request_uri: 完整的原始请求URI
 *
 * 使用注意点:
 * 1. 合理配置HTTP模块以优化性能
 * 2. 注意请求处理阶段的顺序和优先级
 * 3. 正确处理HTTP头部以确保安全性
 * 4. 适当配置缓冲区大小以平衡内存使用和性能
 * 5. 在处理大文件时注意流式传输的配置
 * 6. 正确设置超时参数以避免连接堵塞
 * 7. 使用适当的日志级别进行调试和监控
 * 8. 注意URI匹配规则的优先级和冲突
 * 9. 合理使用变量以提高配置的灵活性
 * 10. 定期检查和更新模块以修复潜在的安全漏洞
 */



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief HTTP模块的主配置函数
 *
 * @param cf Nginx配置结构体指针
 * @param cmd 当前正在处理的命令结构体指针
 * @param conf 配置上下文指针
 * @return char* 成功时返回NGX_CONF_OK，失败时返回错误信息字符串
 *
 * 该函数负责解析和处理HTTP模块的配置块，设置HTTP相关的各种参数和处理程序。
 * 它是HTTP模块初始化过程中的核心函数，用于构建HTTP处理的基础结构。
 */
static char *ngx_http_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/**
 * @brief 初始化HTTP处理阶段
 *
 * @param cf Nginx配置结构体指针
 * @param cmcf HTTP核心模块主配置结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责初始化HTTP请求处理的各个阶段，设置处理函数和顺序。
 * 它是HTTP模块初始化过程中的重要步骤，为后续的请求处理流程奠定基础。
 */
static ngx_int_t ngx_http_init_phases(ngx_conf_t *cf,
    ngx_http_core_main_conf_t *cmcf);
/**
 * @brief 初始化HTTP请求头哈希表
 *
 * @param cf Nginx配置结构体指针
 * @param cmcf HTTP核心模块主配置结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责初始化用于快速查找HTTP请求头的哈希表。
 * 它将预定义的HTTP请求头名称添加到哈希表中，以便在处理请求时能够高效地访问和操作这些头部。
 * 这是HTTP模块初始化过程中的一个重要步骤，对于提高请求处理的性能至关重要。
 */
static ngx_int_t ngx_http_init_headers_in_hash(ngx_conf_t *cf,
    ngx_http_core_main_conf_t *cmcf);
/**
 * @brief 初始化HTTP请求处理阶段的处理器
 *
 * @param cf Nginx配置结构体指针
 * @param cmcf HTTP核心模块主配置结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责初始化HTTP请求处理的各个阶段的处理器。
 * 它会设置每个阶段的处理函数，并确定它们的执行顺序。
 * 这是HTTP模块初始化过程中的关键步骤，直接影响到请求的处理流程和顺序。
 */
static ngx_int_t ngx_http_init_phase_handlers(ngx_conf_t *cf,
    ngx_http_core_main_conf_t *cmcf);

/**
 * @brief 为HTTP服务器添加监听地址
 *
 * @param cf Nginx配置结构体指针
 * @param cscf HTTP核心服务器配置结构体指针
 * @param port 端口配置结构体指针
 * @param lsopt 监听选项结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责为HTTP服务器添加监听地址。它会处理配置中指定的IP地址和端口，
 * 并将其添加到服务器的监听列表中。这是设置HTTP服务器监听配置的关键步骤。
 */
static ngx_int_t ngx_http_add_addresses(ngx_conf_t *cf,
    ngx_http_core_srv_conf_t *cscf, ngx_http_conf_port_t *port,
    ngx_http_listen_opt_t *lsopt);
/**
 * @brief 为HTTP服务器添加单个监听地址
 *
 * @param cf Nginx配置结构体指针
 * @param cscf HTTP核心服务器配置结构体指针
 * @param port 端口配置结构体指针
 * @param lsopt 监听选项结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责为HTTP服务器添加单个监听地址。它处理特定的IP地址和端口配置，
 * 并将其添加到服务器的监听列表中。这是ngx_http_add_addresses函数的辅助函数，
 * 用于处理单个地址的添加操作。
 */
static ngx_int_t ngx_http_add_address(ngx_conf_t *cf,
    ngx_http_core_srv_conf_t *cscf, ngx_http_conf_port_t *port,
    ngx_http_listen_opt_t *lsopt);
/**
 * @brief 向HTTP配置中添加服务器
 *
 * @param cf Nginx配置结构体指针
 * @param cscf HTTP核心服务器配置结构体指针
 * @param addr HTTP配置地址结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责将新的服务器配置添加到HTTP模块的配置中。
 * 它处理服务器的具体配置信息，并将其与指定的地址关联。
 * 这是在Nginx配置解析过程中设置HTTP服务器的重要步骤。
 */
static ngx_int_t ngx_http_add_server(ngx_conf_t *cf,
    ngx_http_core_srv_conf_t *cscf, ngx_http_conf_addr_t *addr);

/**
 * @brief 合并HTTP服务器配置
 *
 * @param cf Nginx配置结构体指针
 * @param cmcf HTTP核心主配置结构体指针
 * @param module HTTP模块指针
 * @param ctx_index 上下文索引
 * @return char* 成功返回NULL，失败返回错误信息字符串
 *
 * 该函数负责合并HTTP服务器的配置。它处理不同级别的配置（如主配置和服务器特定配置），
 * 并将它们合并成最终的配置。这是Nginx配置处理过程中的一个重要步骤，
 * 确保所有的配置指令都被正确应用。
 */
static char *ngx_http_merge_servers(ngx_conf_t *cf,
    ngx_http_core_main_conf_t *cmcf, ngx_http_module_t *module,
    ngx_uint_t ctx_index);
/**
 * @brief 合并HTTP位置配置
 *
 * @param cf Nginx配置结构体指针
 * @param locations 位置队列指针
 * @param loc_conf 位置配置数组指针
 * @param module HTTP模块指针
 * @param ctx_index 上下文索引
 * @return char* 成功返回NULL，失败返回错误信息字符串
 *
 * 该函数负责合并HTTP位置的配置。它处理不同级别的位置配置，
 * 并将它们合并成最终的配置。这是Nginx配置处理过程中的一个重要步骤，
 * 确保所有的位置配置指令都被正确应用和继承。
 */
static char *ngx_http_merge_locations(ngx_conf_t *cf,
    ngx_queue_t *locations, void **loc_conf, ngx_http_module_t *module,
    ngx_uint_t ctx_index);
/**
 * @brief 初始化HTTP位置配置
 *
 * @param cf Nginx配置结构体指针
 * @param cscf HTTP核心服务器配置结构体指针
 * @param pclcf 父HTTP核心位置配置结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责初始化HTTP位置配置。它处理服务器配置中定义的所有位置，
 * 并设置它们的初始状态。这是Nginx配置处理过程中的一个重要步骤，
 * 为后续的请求处理做准备。
 */
static ngx_int_t ngx_http_init_locations(ngx_conf_t *cf,
    ngx_http_core_srv_conf_t *cscf, ngx_http_core_loc_conf_t *pclcf);
/**
 * @brief 初始化静态位置树
 *
 * @param cf Nginx配置结构体指针
 * @param pclcf 父HTTP核心位置配置结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责初始化HTTP静态位置的树形结构。
 * 它处理预定义的静态位置配置，并将它们组织成一个高效的树形结构，
 * 以便在请求处理过程中快速匹配和查找相应的位置配置。
 * 这是Nginx配置处理过程中的一个重要步骤，对提高位置匹配的性能至关重要。
 */
static ngx_int_t ngx_http_init_static_location_trees(ngx_conf_t *cf,
    ngx_http_core_loc_conf_t *pclcf);
/**
 * @brief 转义位置名称中的特殊字符
 *
 * @param cf Nginx配置结构体指针
 * @param clcf HTTP核心位置配置结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责转义HTTP位置配置中名称里的特殊字符。
 * 它处理位置名称，确保其中的特殊字符被正确转义，
 * 以防止在后续处理中出现解析错误或安全问题。
 * 这是Nginx配置处理过程中的一个重要步骤，
 * 有助于确保位置配置的正确性和安全性。
 */
static ngx_int_t ngx_http_escape_location_name(ngx_conf_t *cf,
    ngx_http_core_loc_conf_t *clcf);
/**
 * @brief 比较两个位置配置的优先级
 *
 * @param one 第一个位置配置的队列节点指针
 * @param two 第二个位置配置的队列节点指针
 * @return ngx_int_t 比较结果：如果one优先级高于two返回正值，相等返回0，低于返回负值
 *
 * 该函数用于比较两个HTTP位置配置的优先级。
 * 在Nginx处理位置配置时，需要确定不同位置指令的优先顺序，
 * 这个函数就是用来实现这一目的的。
 * 它考虑了位置指令的类型、精确度等因素来决定优先级。
 */
static ngx_int_t ngx_http_cmp_locations(const ngx_queue_t *one,
    const ngx_queue_t *two);
/**
 * @brief 合并精确匹配的位置配置
 *
 * @param cf Nginx配置结构体指针
 * @param locations 位置配置队列指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责合并具有相同URI的精确匹配位置配置。
 * 在Nginx配置中，可能存在多个精确匹配的位置块指向相同的URI。
 * 这个函数的作用是将这些重复的位置配置合并，以优化配置结构和提高效率。
 * 合并过程会保留优先级最高的配置，并确保不会丢失任何必要的指令。
 */
static ngx_int_t ngx_http_join_exact_locations(ngx_conf_t *cf,
    ngx_queue_t *locations);
/**
 * @brief 创建位置配置列表
 *
 * @param locations 位置配置队列指针
 * @param q 待处理的队列节点指针
 *
 * 该函数负责创建和组织HTTP位置配置的列表。
 * 它遍历给定的位置配置队列，根据配置的类型和优先级
 * 将它们组织成一个结构化的列表。这个列表随后
 * 可以用于快速查找和匹配请求URI对应的配置。
 * 函数的实现考虑了不同类型位置指令（如精确匹配、
 * 前缀匹配、正则表达式等）的处理逻辑。
 */
static void ngx_http_create_locations_list(ngx_queue_t *locations,
    ngx_queue_t *q);
/**
 * @brief 创建位置树节点
 *
 * @return ngx_http_location_tree_node_t* 返回创建的位置树节点指针
 *
 * 该函数负责创建HTTP位置配置的树形结构节点。
 * 位置树是一种优化的数据结构，用于快速匹配和查找HTTP请求的处理位置。
 * 通过构建这样的树结构，Nginx可以高效地路由请求到正确的处理程序。
 */
static ngx_http_location_tree_node_t *
    ngx_http_create_locations_tree(ngx_conf_t *cf, ngx_queue_t *locations,
    size_t prefix);

/**
 * @brief 优化服务器配置
 *
 * @param cf Nginx配置结构体指针
 * @param cmcf HTTP核心模块主配置结构体指针
 * @param ports 端口数组指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数用于优化Nginx的HTTP服务器配置。
 * 它处理服务器名称、监听端口等配置，
 * 以提高服务器在运行时的效率。
 * 优化过程包括整理服务器名称、
 * 合并相同的监听指令，以及
 * 为每个监听套接字建立服务器配置的快速查找结构。
 */
static ngx_int_t ngx_http_optimize_servers(ngx_conf_t *cf,
    ngx_http_core_main_conf_t *cmcf, ngx_array_t *ports);
/**
 * @brief 处理服务器名称配置
 *
 * @param cf Nginx配置结构体指针
 * @param cmcf HTTP核心模块主配置结构体指针
 * @param addr HTTP配置地址结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责处理和优化Nginx服务器的名称配置。
 * 它会遍历所有配置的服务器名称，处理通配符和正则表达式，
 * 并建立一个高效的查找结构，以便在运行时快速匹配请求的主机名。
 */
static ngx_int_t ngx_http_server_names(ngx_conf_t *cf,
    ngx_http_core_main_conf_t *cmcf, ngx_http_conf_addr_t *addr);
/**
 * @brief 比较两个HTTP配置地址
 *
 * @param one 第一个配置地址的指针
 * @param two 第二个配置地址的指针
 * @return ngx_int_t 比较结果
 *
 * 该函数用于比较两个HTTP配置地址。
 * 在Nginx的配置优化过程中，可能需要对配置地址进行排序或查找，
 * 这个函数提供了比较两个配置地址的方法。
 * 具体的比较逻辑取决于实现，可能会考虑IP地址、端口等因素。
 */
static ngx_int_t ngx_http_cmp_conf_addrs(const void *one, const void *two);
/**
 * @brief 比较两个DNS通配符
 *
 * @param one 第一个通配符的指针
 * @param two 第二个通配符的指针
 * @return int 比较结果
 *
 * 该函数用于比较两个DNS通配符。在处理服务器名称时，
 * 可能需要对包含通配符的域名进行排序或查找，
 * 这个函数提供了比较两个通配符的方法。
 * 函数使用 ngx_libc_cdecl 调用约定，以确保与C库函数兼容。
 */
static int ngx_libc_cdecl ngx_http_cmp_dns_wildcards(const void *one,
    const void *two);

/**
 * @brief 初始化HTTP监听
 *
 * @param cf Nginx配置结构体指针
 * @param port HTTP配置端口结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数负责初始化HTTP模块的监听配置。
 * 它会为每个配置的地址和端口创建监听套接字，
 * 并设置相应的处理函数和配置。
 * 这是HTTP模块启动过程中的重要步骤，
 * 确保服务器能够正确监听并处理incoming的HTTP请求。
 */
static ngx_int_t ngx_http_init_listening(ngx_conf_t *cf,
    ngx_http_conf_port_t *port);
/**
 * @brief 为HTTP模块添加一个监听套接字
 *
 * @param cf Nginx配置结构体指针
 * @param addr HTTP配置地址结构体指针
 * @return ngx_listening_t* 返回新创建的监听结构体指针，如果失败则返回NULL
 *
 * 该函数负责为指定的HTTP配置地址创建一个新的监听套接字。
 * 它会设置套接字的各种参数，如地址、端口、协议等，
 * 并将其添加到Nginx的监听列表中。
 * 这是HTTP模块初始化过程中的关键步骤，为后续处理HTTP请求做准备。
 */
static ngx_listening_t *ngx_http_add_listening(ngx_conf_t *cf,
    ngx_http_conf_addr_t *addr);
/**
 * @brief 为HTTP端口添加地址
 *
 * @param cf Nginx配置结构体指针
 * @param hport HTTP端口结构体指针
 * @param addr HTTP配置地址结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数用于为指定的HTTP端口添加一个或多个IP地址。
 * 它会处理IPv4地址的添加，包括设置地址、端口等信息。
 * 这是HTTP模块配置过程中的重要步骤，用于建立服务器的监听地址。
 */
static ngx_int_t ngx_http_add_addrs(ngx_conf_t *cf, ngx_http_port_t *hport,
    ngx_http_conf_addr_t *addr);
#if (NGX_HAVE_INET6)
/**
 * @brief 为HTTP端口添加IPv6地址
 *
 * @param cf Nginx配置结构体指针
 * @param hport HTTP端口结构体指针
 * @param addr HTTP配置地址结构体指针
 * @return ngx_int_t 成功返回NGX_OK，失败返回NGX_ERROR
 *
 * 该函数用于为指定的HTTP端口添加一个或多个IPv6地址。
 * 它会处理IPv6地址的添加，包括设置地址、端口等信息。
 * 这是HTTP模块配置过程中的重要步骤，用于建立服务器的IPv6监听地址。
 * 只有在编译时启用了IPv6支持（NGX_HAVE_INET6宏定义）时，此函数才会被编译。
 */
static ngx_int_t ngx_http_add_addrs6(ngx_conf_t *cf, ngx_http_port_t *hport,
    ngx_http_conf_addr_t *addr);
#endif

/**
 * @brief HTTP模块的最大数量
 *
 * 这个变量用于存储当前配置中HTTP模块的最大数量。
 * 它在HTTP模块初始化过程中被设置，并在后续的模块管理和配置中使用。
 * 这个值决定了Nginx可以同时加载和管理的HTTP模块的上限。
 */
ngx_uint_t   ngx_http_max_module;


/**
 * @brief HTTP响应头过滤器链的顶层函数指针
 *
 * 这个变量是一个函数指针，指向HTTP响应头过滤器链中的第一个过滤器函数。
 * 在处理HTTP响应时，所有的响应头都会通过这个过滤器链进行处理。
 * 模块可以通过设置这个指针来添加自定义的响应头处理逻辑。
 * 过滤器链的执行顺序是从最后添加的过滤器开始，一直到这个顶层过滤器。
 */
ngx_http_output_header_filter_pt  ngx_http_top_header_filter;
/**
 * @brief HTTP响应体过滤器链的顶层函数指针
 *
 * 这个变量是一个函数指针，指向HTTP响应体过滤器链中的第一个过滤器函数。
 * 在处理HTTP响应时，所有的响应体内容都会通过这个过滤器链进行处理。
 * 模块可以通过设置这个指针来添加自定义的响应体处理逻辑。
 * 过滤器链的执行顺序是从最后添加的过滤器开始，一直到这个顶层过滤器。
 */
ngx_http_output_body_filter_pt    ngx_http_top_body_filter;
/**
 * @brief HTTP请求体过滤器链的顶层函数指针
 *
 * 这个变量是一个函数指针，指向HTTP请求体过滤器链中的第一个过滤器函数。
 * 在处理HTTP请求时，所有的请求体内容都会通过这个过滤器链进行处理。
 * 模块可以通过设置这个指针来添加自定义的请求体处理逻辑。
 * 过滤器链的执行顺序是从最后添加的过滤器开始，一直到这个顶层过滤器。
 */
ngx_http_request_body_filter_pt   ngx_http_top_request_body_filter;


/**
 * @brief 默认的HTML MIME类型数组
 *
 * 这个数组定义了默认的HTML MIME类型。
 * 它用于在HTTP响应中设置Content-Type头，
 * 当服务器发送HTML内容时使用。
 * 数组中包含了最常见的HTML MIME类型。
 */
ngx_str_t  ngx_http_html_default_types[] = {
    ngx_string("text/html"),
    ngx_null_string
};


/**
 * @brief HTTP模块的命令数组
 *
 * 这个静态数组定义了HTTP模块支持的所有配置指令。
 * 每个元素都是一个ngx_command_t结构，描述了一个特定的配置指令。
 * 这个数组将被Nginx的配置解析器使用，以识别和处理HTTP相关的配置。
 */
static ngx_command_t  ngx_http_commands[] = {

    { ngx_string("http"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_http_block,
      0,
      0,
      NULL },

      ngx_null_command
};


/**
 * @brief HTTP模块的核心模块上下文
 *
 * 这个静态变量定义了HTTP模块作为核心模块的上下文信息。
 * 它包含了模块的名称和可能的初始化/退出函数（此处为NULL）。
 * 这个结构体用于在Nginx核心中注册HTTP模块。
 */
static ngx_core_module_t  ngx_http_module_ctx = {
    ngx_string("http"),
    NULL,
    NULL
};


/**
 * @brief HTTP模块的定义
 *
 * 这个结构定义了Nginx的HTTP模块。
 * 它包含了模块的基本信息、上下文、指令集等。
 * 这是HTTP模块的核心结构，用于将HTTP功能集成到Nginx中。
 */
ngx_module_t  ngx_http_module = {
    NGX_MODULE_V1,
    &ngx_http_module_ctx,                  /* module context */
    ngx_http_commands,                     /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static char *
ngx_http_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                        *rv;
    ngx_uint_t                   mi, m, s;
    ngx_conf_t                   pcf;
    ngx_http_module_t           *module;
    ngx_http_conf_ctx_t         *ctx;
    ngx_http_core_loc_conf_t    *clcf;
    ngx_http_core_srv_conf_t   **cscfp;
    ngx_http_core_main_conf_t   *cmcf;

    if (*(ngx_http_conf_ctx_t **) conf) {
        return "is duplicate";
    }

    /* the main http context */

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *(ngx_http_conf_ctx_t **) conf = ctx;


    /* count the number of the http modules and set up their indices */

    ngx_http_max_module = ngx_count_modules(cf->cycle, NGX_HTTP_MODULE);


    /* the http main_conf context, it is the same in the all http contexts */

    ctx->main_conf = ngx_pcalloc(cf->pool,
                                 sizeof(void *) * ngx_http_max_module);
    if (ctx->main_conf == NULL) {
        return NGX_CONF_ERROR;
    }


    /*
     * the http null srv_conf context, it is used to merge
     * the server{}s' srv_conf's
     */

    ctx->srv_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->srv_conf == NULL) {
        return NGX_CONF_ERROR;
    }


    /*
     * the http null loc_conf context, it is used to merge
     * the server{}s' loc_conf's
     */

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }


    /*
     * create the main_conf's, the null srv_conf's, and the null loc_conf's
     * of the all http modules
     */

    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;
        mi = cf->cycle->modules[m]->ctx_index;

        if (module->create_main_conf) {
            ctx->main_conf[mi] = module->create_main_conf(cf);
            if (ctx->main_conf[mi] == NULL) {
                return NGX_CONF_ERROR;
            }
        }

        if (module->create_srv_conf) {
            ctx->srv_conf[mi] = module->create_srv_conf(cf);
            if (ctx->srv_conf[mi] == NULL) {
                return NGX_CONF_ERROR;
            }
        }

        if (module->create_loc_conf) {
            ctx->loc_conf[mi] = module->create_loc_conf(cf);
            if (ctx->loc_conf[mi] == NULL) {
                return NGX_CONF_ERROR;
            }
        }
    }

    pcf = *cf;
    cf->ctx = ctx;

    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;

        if (module->preconfiguration) {
            if (module->preconfiguration(cf) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }
    }

    /* parse inside the http{} block */

    cf->module_type = NGX_HTTP_MODULE;
    cf->cmd_type = NGX_HTTP_MAIN_CONF;
    rv = ngx_conf_parse(cf, NULL);

    if (rv != NGX_CONF_OK) {
        goto failed;
    }

    /*
     * init http{} main_conf's, merge the server{}s' srv_conf's
     * and its location{}s' loc_conf's
     */

    cmcf = ctx->main_conf[ngx_http_core_module.ctx_index];
    cscfp = cmcf->servers.elts;

    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;
        mi = cf->cycle->modules[m]->ctx_index;

        /* init http{} main_conf's */

        if (module->init_main_conf) {
            rv = module->init_main_conf(cf, ctx->main_conf[mi]);
            if (rv != NGX_CONF_OK) {
                goto failed;
            }
        }

        rv = ngx_http_merge_servers(cf, cmcf, module, mi);
        if (rv != NGX_CONF_OK) {
            goto failed;
        }
    }


    /* create location trees */

    for (s = 0; s < cmcf->servers.nelts; s++) {

        clcf = cscfp[s]->ctx->loc_conf[ngx_http_core_module.ctx_index];

        if (ngx_http_init_locations(cf, cscfp[s], clcf) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        if (ngx_http_init_static_location_trees(cf, clcf) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }


    if (ngx_http_init_phases(cf, cmcf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (ngx_http_init_headers_in_hash(cf, cmcf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }


    for (m = 0; cf->cycle->modules[m]; m++) {
        if (cf->cycle->modules[m]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[m]->ctx;

        if (module->postconfiguration) {
            if (module->postconfiguration(cf) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }
    }

    if (ngx_http_variables_init_vars(cf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    /*
     * http{}'s cf->ctx was needed while the configuration merging
     * and in postconfiguration process
     */

    *cf = pcf;


    if (ngx_http_init_phase_handlers(cf, cmcf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }


    /* optimize the lists of ports, addresses and server names */

    if (ngx_http_optimize_servers(cf, cmcf, cmcf->ports) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;

failed:

    *cf = pcf;

    return rv;
}


static ngx_int_t
ngx_http_init_phases(ngx_conf_t *cf, ngx_http_core_main_conf_t *cmcf)
{
    if (ngx_array_init(&cmcf->phases[NGX_HTTP_POST_READ_PHASE].handlers,
                       cf->pool, 1, sizeof(ngx_http_handler_pt))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_array_init(&cmcf->phases[NGX_HTTP_SERVER_REWRITE_PHASE].handlers,
                       cf->pool, 1, sizeof(ngx_http_handler_pt))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_array_init(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers,
                       cf->pool, 1, sizeof(ngx_http_handler_pt))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_array_init(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers,
                       cf->pool, 1, sizeof(ngx_http_handler_pt))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_array_init(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers,
                       cf->pool, 2, sizeof(ngx_http_handler_pt))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_array_init(&cmcf->phases[NGX_HTTP_PRECONTENT_PHASE].handlers,
                       cf->pool, 2, sizeof(ngx_http_handler_pt))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_array_init(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers,
                       cf->pool, 4, sizeof(ngx_http_handler_pt))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_array_init(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers,
                       cf->pool, 1, sizeof(ngx_http_handler_pt))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_init_headers_in_hash(ngx_conf_t *cf, ngx_http_core_main_conf_t *cmcf)
{
    ngx_array_t         headers_in;
    ngx_hash_key_t     *hk;
    ngx_hash_init_t     hash;
    ngx_http_header_t  *header;

    if (ngx_array_init(&headers_in, cf->temp_pool, 32, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    for (header = ngx_http_headers_in; header->name.len; header++) {
        hk = ngx_array_push(&headers_in);
        if (hk == NULL) {
            return NGX_ERROR;
        }

        hk->key = header->name;
        hk->key_hash = ngx_hash_key_lc(header->name.data, header->name.len);
        hk->value = header;
    }

    hash.hash = &cmcf->headers_in_hash;
    hash.key = ngx_hash_key_lc;
    hash.max_size = 512;
    hash.bucket_size = ngx_align(64, ngx_cacheline_size);
    hash.name = "headers_in_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    if (ngx_hash_init(&hash, headers_in.elts, headers_in.nelts) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_init_phase_handlers(ngx_conf_t *cf, ngx_http_core_main_conf_t *cmcf)
{
    ngx_int_t                   j;
    ngx_uint_t                  i, n;
    ngx_uint_t                  find_config_index, use_rewrite, use_access;
    ngx_http_handler_pt        *h;
    ngx_http_phase_handler_t   *ph;
    ngx_http_phase_handler_pt   checker;

    cmcf->phase_engine.server_rewrite_index = (ngx_uint_t) -1;
    cmcf->phase_engine.location_rewrite_index = (ngx_uint_t) -1;
    find_config_index = 0;
    use_rewrite = cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers.nelts ? 1 : 0;
    use_access = cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers.nelts ? 1 : 0;

    n = 1                  /* find config phase */
        + use_rewrite      /* post rewrite phase */
        + use_access;      /* post access phase */

    for (i = 0; i < NGX_HTTP_LOG_PHASE; i++) {
        n += cmcf->phases[i].handlers.nelts;
    }

    ph = ngx_pcalloc(cf->pool,
                     n * sizeof(ngx_http_phase_handler_t) + sizeof(void *));
    if (ph == NULL) {
        return NGX_ERROR;
    }

    cmcf->phase_engine.handlers = ph;
    n = 0;

    for (i = 0; i < NGX_HTTP_LOG_PHASE; i++) {
        h = cmcf->phases[i].handlers.elts;

        switch (i) {

        case NGX_HTTP_SERVER_REWRITE_PHASE:
            if (cmcf->phase_engine.server_rewrite_index == (ngx_uint_t) -1) {
                cmcf->phase_engine.server_rewrite_index = n;
            }
            checker = ngx_http_core_rewrite_phase;

            break;

        case NGX_HTTP_FIND_CONFIG_PHASE:
            find_config_index = n;

            ph->checker = ngx_http_core_find_config_phase;
            n++;
            ph++;

            continue;

        case NGX_HTTP_REWRITE_PHASE:
            if (cmcf->phase_engine.location_rewrite_index == (ngx_uint_t) -1) {
                cmcf->phase_engine.location_rewrite_index = n;
            }
            checker = ngx_http_core_rewrite_phase;

            break;

        case NGX_HTTP_POST_REWRITE_PHASE:
            if (use_rewrite) {
                ph->checker = ngx_http_core_post_rewrite_phase;
                ph->next = find_config_index;
                n++;
                ph++;
            }

            continue;

        case NGX_HTTP_ACCESS_PHASE:
            checker = ngx_http_core_access_phase;
            n++;
            break;

        case NGX_HTTP_POST_ACCESS_PHASE:
            if (use_access) {
                ph->checker = ngx_http_core_post_access_phase;
                ph->next = n;
                ph++;
            }

            continue;

        case NGX_HTTP_CONTENT_PHASE:
            checker = ngx_http_core_content_phase;
            break;

        default:
            checker = ngx_http_core_generic_phase;
        }

        n += cmcf->phases[i].handlers.nelts;

        for (j = cmcf->phases[i].handlers.nelts - 1; j >= 0; j--) {
            ph->checker = checker;
            ph->handler = h[j];
            ph->next = n;
            ph++;
        }
    }

    return NGX_OK;
}


static char *
ngx_http_merge_servers(ngx_conf_t *cf, ngx_http_core_main_conf_t *cmcf,
    ngx_http_module_t *module, ngx_uint_t ctx_index)
{
    char                        *rv;
    ngx_uint_t                   s;
    ngx_http_conf_ctx_t         *ctx, saved;
    ngx_http_core_loc_conf_t    *clcf;
    ngx_http_core_srv_conf_t   **cscfp;

    cscfp = cmcf->servers.elts;
    ctx = (ngx_http_conf_ctx_t *) cf->ctx;
    saved = *ctx;
    rv = NGX_CONF_OK;

    for (s = 0; s < cmcf->servers.nelts; s++) {

        /* merge the server{}s' srv_conf's */

        ctx->srv_conf = cscfp[s]->ctx->srv_conf;

        if (module->merge_srv_conf) {
            rv = module->merge_srv_conf(cf, saved.srv_conf[ctx_index],
                                        cscfp[s]->ctx->srv_conf[ctx_index]);
            if (rv != NGX_CONF_OK) {
                goto failed;
            }
        }

        if (module->merge_loc_conf) {

            /* merge the server{}'s loc_conf */

            ctx->loc_conf = cscfp[s]->ctx->loc_conf;

            rv = module->merge_loc_conf(cf, saved.loc_conf[ctx_index],
                                        cscfp[s]->ctx->loc_conf[ctx_index]);
            if (rv != NGX_CONF_OK) {
                goto failed;
            }

            /* merge the locations{}' loc_conf's */

            clcf = cscfp[s]->ctx->loc_conf[ngx_http_core_module.ctx_index];

            rv = ngx_http_merge_locations(cf, clcf->locations,
                                          cscfp[s]->ctx->loc_conf,
                                          module, ctx_index);
            if (rv != NGX_CONF_OK) {
                goto failed;
            }
        }
    }

failed:

    *ctx = saved;

    return rv;
}


static char *
ngx_http_merge_locations(ngx_conf_t *cf, ngx_queue_t *locations,
    void **loc_conf, ngx_http_module_t *module, ngx_uint_t ctx_index)
{
    char                       *rv;
    ngx_queue_t                *q;
    ngx_http_conf_ctx_t        *ctx, saved;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_location_queue_t  *lq;

    if (locations == NULL) {
        return NGX_CONF_OK;
    }

    ctx = (ngx_http_conf_ctx_t *) cf->ctx;
    saved = *ctx;

    for (q = ngx_queue_head(locations);
         q != ngx_queue_sentinel(locations);
         q = ngx_queue_next(q))
    {
        lq = (ngx_http_location_queue_t *) q;

        clcf = lq->exact ? lq->exact : lq->inclusive;
        ctx->loc_conf = clcf->loc_conf;

        rv = module->merge_loc_conf(cf, loc_conf[ctx_index],
                                    clcf->loc_conf[ctx_index]);
        if (rv != NGX_CONF_OK) {
            return rv;
        }

        rv = ngx_http_merge_locations(cf, clcf->locations, clcf->loc_conf,
                                      module, ctx_index);
        if (rv != NGX_CONF_OK) {
            return rv;
        }
    }

    *ctx = saved;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_init_locations(ngx_conf_t *cf, ngx_http_core_srv_conf_t *cscf,
    ngx_http_core_loc_conf_t *pclcf)
{
    ngx_uint_t                   n;
    ngx_queue_t                 *q, *locations, *named, tail;
    ngx_http_core_loc_conf_t    *clcf;
    ngx_http_location_queue_t   *lq;
    ngx_http_core_loc_conf_t   **clcfp;
#if (NGX_PCRE)
    ngx_uint_t                   r;
    ngx_queue_t                 *regex;
#endif

    locations = pclcf->locations;

    if (locations == NULL) {
        return NGX_OK;
    }

    ngx_queue_sort(locations, ngx_http_cmp_locations);

    named = NULL;
    n = 0;
#if (NGX_PCRE)
    regex = NULL;
    r = 0;
#endif

    for (q = ngx_queue_head(locations);
         q != ngx_queue_sentinel(locations);
         q = ngx_queue_next(q))
    {
        lq = (ngx_http_location_queue_t *) q;

        clcf = lq->exact ? lq->exact : lq->inclusive;

        if (ngx_http_init_locations(cf, NULL, clcf) != NGX_OK) {
            return NGX_ERROR;
        }

#if (NGX_PCRE)

        if (clcf->regex) {
            r++;

            if (regex == NULL) {
                regex = q;
            }

            continue;
        }

#endif

        if (clcf->named) {
            n++;

            if (named == NULL) {
                named = q;
            }

            continue;
        }

        if (clcf->noname) {
            break;
        }
    }

    if (q != ngx_queue_sentinel(locations)) {
        ngx_queue_split(locations, q, &tail);
    }

    if (named) {
        clcfp = ngx_palloc(cf->pool,
                           (n + 1) * sizeof(ngx_http_core_loc_conf_t *));
        if (clcfp == NULL) {
            return NGX_ERROR;
        }

        cscf->named_locations = clcfp;

        for (q = named;
             q != ngx_queue_sentinel(locations);
             q = ngx_queue_next(q))
        {
            lq = (ngx_http_location_queue_t *) q;

            *(clcfp++) = lq->exact;
        }

        *clcfp = NULL;

        ngx_queue_split(locations, named, &tail);
    }

#if (NGX_PCRE)

    if (regex) {

        clcfp = ngx_palloc(cf->pool,
                           (r + 1) * sizeof(ngx_http_core_loc_conf_t *));
        if (clcfp == NULL) {
            return NGX_ERROR;
        }

        pclcf->regex_locations = clcfp;

        for (q = regex;
             q != ngx_queue_sentinel(locations);
             q = ngx_queue_next(q))
        {
            lq = (ngx_http_location_queue_t *) q;

            *(clcfp++) = lq->exact;
        }

        *clcfp = NULL;

        ngx_queue_split(locations, regex, &tail);
    }

#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_init_static_location_trees(ngx_conf_t *cf,
    ngx_http_core_loc_conf_t *pclcf)
{
    ngx_queue_t                *q, *locations;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_location_queue_t  *lq;

    locations = pclcf->locations;

    if (locations == NULL) {
        return NGX_OK;
    }

    if (ngx_queue_empty(locations)) {
        return NGX_OK;
    }

    for (q = ngx_queue_head(locations);
         q != ngx_queue_sentinel(locations);
         q = ngx_queue_next(q))
    {
        lq = (ngx_http_location_queue_t *) q;

        clcf = lq->exact ? lq->exact : lq->inclusive;

        if (ngx_http_init_static_location_trees(cf, clcf) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (ngx_http_join_exact_locations(cf, locations) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_http_create_locations_list(locations, ngx_queue_head(locations));

    pclcf->static_locations = ngx_http_create_locations_tree(cf, locations, 0);
    if (pclcf->static_locations == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_add_location(ngx_conf_t *cf, ngx_queue_t **locations,
    ngx_http_core_loc_conf_t *clcf)
{
    ngx_http_location_queue_t  *lq;

    if (*locations == NULL) {
        *locations = ngx_palloc(cf->temp_pool,
                                sizeof(ngx_http_location_queue_t));
        if (*locations == NULL) {
            return NGX_ERROR;
        }

        ngx_queue_init(*locations);
    }

    lq = ngx_palloc(cf->temp_pool, sizeof(ngx_http_location_queue_t));
    if (lq == NULL) {
        return NGX_ERROR;
    }

    if (clcf->exact_match
#if (NGX_PCRE)
        || clcf->regex
#endif
        || clcf->named || clcf->noname)
    {
        lq->exact = clcf;
        lq->inclusive = NULL;

    } else {
        lq->exact = NULL;
        lq->inclusive = clcf;
    }

    lq->name = &clcf->name;
    lq->file_name = cf->conf_file->file.name.data;
    lq->line = cf->conf_file->line;

    ngx_queue_init(&lq->list);

    ngx_queue_insert_tail(*locations, &lq->queue);

    if (ngx_http_escape_location_name(cf, clcf) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_escape_location_name(ngx_conf_t *cf, ngx_http_core_loc_conf_t *clcf)
{
    u_char     *p;
    size_t      len;
    uintptr_t   escape;

    escape = 2 * ngx_escape_uri(NULL, clcf->name.data, clcf->name.len,
                                NGX_ESCAPE_URI);

    if (escape) {
        len = clcf->name.len + escape;

        p = ngx_pnalloc(cf->pool, len);
        if (p == NULL) {
            return NGX_ERROR;
        }

        clcf->escaped_name.len = len;
        clcf->escaped_name.data = p;

        ngx_escape_uri(p, clcf->name.data, clcf->name.len, NGX_ESCAPE_URI);

    } else {
        clcf->escaped_name = clcf->name;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_cmp_locations(const ngx_queue_t *one, const ngx_queue_t *two)
{
    ngx_int_t                   rc;
    ngx_http_core_loc_conf_t   *first, *second;
    ngx_http_location_queue_t  *lq1, *lq2;

    lq1 = (ngx_http_location_queue_t *) one;
    lq2 = (ngx_http_location_queue_t *) two;

    first = lq1->exact ? lq1->exact : lq1->inclusive;
    second = lq2->exact ? lq2->exact : lq2->inclusive;

    if (first->noname && !second->noname) {
        /* shift no named locations to the end */
        return 1;
    }

    if (!first->noname && second->noname) {
        /* shift no named locations to the end */
        return -1;
    }

    if (first->noname || second->noname) {
        /* do not sort no named locations */
        return 0;
    }

    if (first->named && !second->named) {
        /* shift named locations to the end */
        return 1;
    }

    if (!first->named && second->named) {
        /* shift named locations to the end */
        return -1;
    }

    if (first->named && second->named) {
        return ngx_strcmp(first->name.data, second->name.data);
    }

#if (NGX_PCRE)

    if (first->regex && !second->regex) {
        /* shift the regex matches to the end */
        return 1;
    }

    if (!first->regex && second->regex) {
        /* shift the regex matches to the end */
        return -1;
    }

    if (first->regex || second->regex) {
        /* do not sort the regex matches */
        return 0;
    }

#endif

    rc = ngx_filename_cmp(first->name.data, second->name.data,
                          ngx_min(first->name.len, second->name.len) + 1);

    if (rc == 0 && !first->exact_match && second->exact_match) {
        /* an exact match must be before the same inclusive one */
        return 1;
    }

    return rc;
}


static ngx_int_t
ngx_http_join_exact_locations(ngx_conf_t *cf, ngx_queue_t *locations)
{
    ngx_queue_t                *q, *x;
    ngx_http_location_queue_t  *lq, *lx;

    q = ngx_queue_head(locations);

    while (q != ngx_queue_last(locations)) {

        x = ngx_queue_next(q);

        lq = (ngx_http_location_queue_t *) q;
        lx = (ngx_http_location_queue_t *) x;

        if (lq->name->len == lx->name->len
            && ngx_filename_cmp(lq->name->data, lx->name->data, lx->name->len)
               == 0)
        {
            if ((lq->exact && lx->exact) || (lq->inclusive && lx->inclusive)) {
                ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                              "duplicate location \"%V\" in %s:%ui",
                              lx->name, lx->file_name, lx->line);

                return NGX_ERROR;
            }

            lq->inclusive = lx->inclusive;

            ngx_queue_remove(x);

            continue;
        }

        q = ngx_queue_next(q);
    }

    return NGX_OK;
}


static void
ngx_http_create_locations_list(ngx_queue_t *locations, ngx_queue_t *q)
{
    u_char                     *name;
    size_t                      len;
    ngx_queue_t                *x, tail;
    ngx_http_location_queue_t  *lq, *lx;

    if (q == ngx_queue_last(locations)) {
        return;
    }

    lq = (ngx_http_location_queue_t *) q;

    if (lq->inclusive == NULL) {
        ngx_http_create_locations_list(locations, ngx_queue_next(q));
        return;
    }

    len = lq->name->len;
    name = lq->name->data;

    for (x = ngx_queue_next(q);
         x != ngx_queue_sentinel(locations);
         x = ngx_queue_next(x))
    {
        lx = (ngx_http_location_queue_t *) x;

        if (len > lx->name->len
            || ngx_filename_cmp(name, lx->name->data, len) != 0)
        {
            break;
        }
    }

    q = ngx_queue_next(q);

    if (q == x) {
        ngx_http_create_locations_list(locations, x);
        return;
    }

    ngx_queue_split(locations, q, &tail);
    ngx_queue_add(&lq->list, &tail);

    if (x == ngx_queue_sentinel(locations)) {
        ngx_http_create_locations_list(&lq->list, ngx_queue_head(&lq->list));
        return;
    }

    ngx_queue_split(&lq->list, x, &tail);
    ngx_queue_add(locations, &tail);

    ngx_http_create_locations_list(&lq->list, ngx_queue_head(&lq->list));

    ngx_http_create_locations_list(locations, x);
}


/*
 * to keep cache locality for left leaf nodes, allocate nodes in following
 * order: node, left subtree, right subtree, inclusive subtree
 */

static ngx_http_location_tree_node_t *
ngx_http_create_locations_tree(ngx_conf_t *cf, ngx_queue_t *locations,
    size_t prefix)
{
    size_t                          len;
    ngx_queue_t                    *q, tail;
    ngx_http_location_queue_t      *lq;
    ngx_http_location_tree_node_t  *node;

    q = ngx_queue_middle(locations);

    lq = (ngx_http_location_queue_t *) q;
    len = lq->name->len - prefix;

    node = ngx_palloc(cf->pool,
                      offsetof(ngx_http_location_tree_node_t, name) + len);
    if (node == NULL) {
        return NULL;
    }

    node->left = NULL;
    node->right = NULL;
    node->tree = NULL;
    node->exact = lq->exact;
    node->inclusive = lq->inclusive;

    node->auto_redirect = (u_char) ((lq->exact && lq->exact->auto_redirect)
                           || (lq->inclusive && lq->inclusive->auto_redirect));

    node->len = (u_short) len;
    ngx_memcpy(node->name, &lq->name->data[prefix], len);

    ngx_queue_split(locations, q, &tail);

    if (ngx_queue_empty(locations)) {
        /*
         * ngx_queue_split() insures that if left part is empty,
         * then right one is empty too
         */
        goto inclusive;
    }

    node->left = ngx_http_create_locations_tree(cf, locations, prefix);
    if (node->left == NULL) {
        return NULL;
    }

    ngx_queue_remove(q);

    if (ngx_queue_empty(&tail)) {
        goto inclusive;
    }

    node->right = ngx_http_create_locations_tree(cf, &tail, prefix);
    if (node->right == NULL) {
        return NULL;
    }

inclusive:

    if (ngx_queue_empty(&lq->list)) {
        return node;
    }

    node->tree = ngx_http_create_locations_tree(cf, &lq->list, prefix + len);
    if (node->tree == NULL) {
        return NULL;
    }

    return node;
}


ngx_int_t
ngx_http_add_listen(ngx_conf_t *cf, ngx_http_core_srv_conf_t *cscf,
    ngx_http_listen_opt_t *lsopt)
{
    in_port_t                   p;
    ngx_uint_t                  i;
    struct sockaddr            *sa;
    ngx_http_conf_port_t       *port;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    if (cmcf->ports == NULL) {
        cmcf->ports = ngx_array_create(cf->temp_pool, 2,
                                       sizeof(ngx_http_conf_port_t));
        if (cmcf->ports == NULL) {
            return NGX_ERROR;
        }
    }

    sa = lsopt->sockaddr;
    p = ngx_inet_get_port(sa);

    port = cmcf->ports->elts;
    for (i = 0; i < cmcf->ports->nelts; i++) {

        if (p != port[i].port
            || lsopt->type != port[i].type
            || sa->sa_family != port[i].family)
        {
            continue;
        }

        /* a port is already in the port list */

        return ngx_http_add_addresses(cf, cscf, &port[i], lsopt);
    }

    /* add a port to the port list */

    port = ngx_array_push(cmcf->ports);
    if (port == NULL) {
        return NGX_ERROR;
    }

    port->family = sa->sa_family;
    port->type = lsopt->type;
    port->port = p;
    port->addrs.elts = NULL;

    return ngx_http_add_address(cf, cscf, port, lsopt);
}


static ngx_int_t
ngx_http_add_addresses(ngx_conf_t *cf, ngx_http_core_srv_conf_t *cscf,
    ngx_http_conf_port_t *port, ngx_http_listen_opt_t *lsopt)
{
    ngx_uint_t             i, default_server, proxy_protocol,
                           protocols, protocols_prev;
    ngx_http_conf_addr_t  *addr;
#if (NGX_HTTP_SSL)
    ngx_uint_t             ssl;
#endif
#if (NGX_HTTP_V2)
    ngx_uint_t             http2;
#endif
#if (NGX_HTTP_V3)
    ngx_uint_t             quic;
#endif

    /*
     * we cannot compare whole sockaddr struct's as kernel
     * may fill some fields in inherited sockaddr struct's
     */

    addr = port->addrs.elts;

    for (i = 0; i < port->addrs.nelts; i++) {

        if (ngx_cmp_sockaddr(lsopt->sockaddr, lsopt->socklen,
                             addr[i].opt.sockaddr,
                             addr[i].opt.socklen, 0)
            != NGX_OK)
        {
            continue;
        }

        /* the address is already in the address list */

        if (ngx_http_add_server(cf, cscf, &addr[i]) != NGX_OK) {
            return NGX_ERROR;
        }

        /* preserve default_server bit during listen options overwriting */
        default_server = addr[i].opt.default_server;

        proxy_protocol = lsopt->proxy_protocol || addr[i].opt.proxy_protocol;
        protocols = lsopt->proxy_protocol;
        protocols_prev = addr[i].opt.proxy_protocol;

#if (NGX_HTTP_SSL)
        ssl = lsopt->ssl || addr[i].opt.ssl;
        protocols |= lsopt->ssl << 1;
        protocols_prev |= addr[i].opt.ssl << 1;
#endif
#if (NGX_HTTP_V2)
        http2 = lsopt->http2 || addr[i].opt.http2;
        protocols |= lsopt->http2 << 2;
        protocols_prev |= addr[i].opt.http2 << 2;
#endif
#if (NGX_HTTP_V3)
        quic = lsopt->quic || addr[i].opt.quic;
#endif

        if (lsopt->set) {

            if (addr[i].opt.set) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "duplicate listen options for %V",
                                   &addr[i].opt.addr_text);
                return NGX_ERROR;
            }

            addr[i].opt = *lsopt;
        }

        /* check the duplicate "default" server for this address:port */

        if (lsopt->default_server) {

            if (default_server) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "a duplicate default server for %V",
                                   &addr[i].opt.addr_text);
                return NGX_ERROR;
            }

            default_server = 1;
            addr[i].default_server = cscf;
        }

        /* check for conflicting protocol options */

        if ((protocols | protocols_prev) != protocols_prev) {

            /* options added */

            if ((addr[i].opt.set && !lsopt->set)
                || addr[i].protocols_changed
                || (protocols | protocols_prev) != protocols)
            {
                ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                                   "protocol options redefined for %V",
                                   &addr[i].opt.addr_text);
            }

            addr[i].protocols = protocols_prev;
            addr[i].protocols_set = 1;
            addr[i].protocols_changed = 1;

        } else if ((protocols_prev | protocols) != protocols) {

            /* options removed */

            if (lsopt->set
                || (addr[i].protocols_set && protocols != addr[i].protocols))
            {
                ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                                   "protocol options redefined for %V",
                                   &addr[i].opt.addr_text);
            }

            addr[i].protocols = protocols;
            addr[i].protocols_set = 1;
            addr[i].protocols_changed = 1;

        } else {

            /* the same options */

            if ((lsopt->set && addr[i].protocols_changed)
                || (addr[i].protocols_set && protocols != addr[i].protocols))
            {
                ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                                   "protocol options redefined for %V",
                                   &addr[i].opt.addr_text);
            }

            addr[i].protocols = protocols;
            addr[i].protocols_set = 1;
        }

        addr[i].opt.default_server = default_server;
        addr[i].opt.proxy_protocol = proxy_protocol;
#if (NGX_HTTP_SSL)
        addr[i].opt.ssl = ssl;
#endif
#if (NGX_HTTP_V2)
        addr[i].opt.http2 = http2;
#endif
#if (NGX_HTTP_V3)
        addr[i].opt.quic = quic;
#endif

        return NGX_OK;
    }

    /* add the address to the addresses list that bound to this port */

    return ngx_http_add_address(cf, cscf, port, lsopt);
}


/*
 * add the server address, the server names and the server core module
 * configurations to the port list
 */

static ngx_int_t
ngx_http_add_address(ngx_conf_t *cf, ngx_http_core_srv_conf_t *cscf,
    ngx_http_conf_port_t *port, ngx_http_listen_opt_t *lsopt)
{
    ngx_http_conf_addr_t  *addr;

    if (port->addrs.elts == NULL) {
        if (ngx_array_init(&port->addrs, cf->temp_pool, 4,
                           sizeof(ngx_http_conf_addr_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

#if (NGX_HTTP_V2 && NGX_HTTP_SSL                                              \
     && !defined TLSEXT_TYPE_application_layer_protocol_negotiation)

    if (lsopt->http2 && lsopt->ssl) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "nginx was built with OpenSSL that lacks ALPN "
                           "support, HTTP/2 is not enabled for %V",
                           &lsopt->addr_text);
    }

#endif

    addr = ngx_array_push(&port->addrs);
    if (addr == NULL) {
        return NGX_ERROR;
    }

    addr->opt = *lsopt;
    addr->protocols = 0;
    addr->protocols_set = 0;
    addr->protocols_changed = 0;
    addr->hash.buckets = NULL;
    addr->hash.size = 0;
    addr->wc_head = NULL;
    addr->wc_tail = NULL;
#if (NGX_PCRE)
    addr->nregex = 0;
    addr->regex = NULL;
#endif
    addr->default_server = cscf;
    addr->servers.elts = NULL;

    return ngx_http_add_server(cf, cscf, addr);
}


/* add the server core module configuration to the address:port */

static ngx_int_t
ngx_http_add_server(ngx_conf_t *cf, ngx_http_core_srv_conf_t *cscf,
    ngx_http_conf_addr_t *addr)
{
    ngx_uint_t                  i;
    ngx_http_core_srv_conf_t  **server;

    if (addr->servers.elts == NULL) {
        if (ngx_array_init(&addr->servers, cf->temp_pool, 4,
                           sizeof(ngx_http_core_srv_conf_t *))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

    } else {
        server = addr->servers.elts;
        for (i = 0; i < addr->servers.nelts; i++) {
            if (server[i] == cscf) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "a duplicate listen %V",
                                   &addr->opt.addr_text);
                return NGX_ERROR;
            }
        }
    }

    server = ngx_array_push(&addr->servers);
    if (server == NULL) {
        return NGX_ERROR;
    }

    *server = cscf;

    return NGX_OK;
}


static ngx_int_t
ngx_http_optimize_servers(ngx_conf_t *cf, ngx_http_core_main_conf_t *cmcf,
    ngx_array_t *ports)
{
    ngx_uint_t             p, a;
    ngx_http_conf_port_t  *port;
    ngx_http_conf_addr_t  *addr;

    if (ports == NULL) {
        return NGX_OK;
    }

    port = ports->elts;
    for (p = 0; p < ports->nelts; p++) {

        ngx_sort(port[p].addrs.elts, (size_t) port[p].addrs.nelts,
                 sizeof(ngx_http_conf_addr_t), ngx_http_cmp_conf_addrs);

        /*
         * check whether all name-based servers have the same
         * configuration as a default server for given address:port
         */

        addr = port[p].addrs.elts;
        for (a = 0; a < port[p].addrs.nelts; a++) {

            if (addr[a].servers.nelts > 1
#if (NGX_PCRE)
                || addr[a].default_server->captures
#endif
               )
            {
                if (ngx_http_server_names(cf, cmcf, &addr[a]) != NGX_OK) {
                    return NGX_ERROR;
                }
            }
        }

        if (ngx_http_init_listening(cf, &port[p]) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_server_names(ngx_conf_t *cf, ngx_http_core_main_conf_t *cmcf,
    ngx_http_conf_addr_t *addr)
{
    ngx_int_t                   rc;
    ngx_uint_t                  n, s;
    ngx_hash_init_t             hash;
    ngx_hash_keys_arrays_t      ha;
    ngx_http_server_name_t     *name;
    ngx_http_core_srv_conf_t  **cscfp;
#if (NGX_PCRE)
    ngx_uint_t                  regex, i;

    regex = 0;
#endif

    ngx_memzero(&ha, sizeof(ngx_hash_keys_arrays_t));

    ha.temp_pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, cf->log);
    if (ha.temp_pool == NULL) {
        return NGX_ERROR;
    }

    ha.pool = cf->pool;

    if (ngx_hash_keys_array_init(&ha, NGX_HASH_LARGE) != NGX_OK) {
        goto failed;
    }

    cscfp = addr->servers.elts;

    for (s = 0; s < addr->servers.nelts; s++) {

        name = cscfp[s]->server_names.elts;

        for (n = 0; n < cscfp[s]->server_names.nelts; n++) {

#if (NGX_PCRE)
            if (name[n].regex) {
                regex++;
                continue;
            }
#endif

            rc = ngx_hash_add_key(&ha, &name[n].name, name[n].server,
                                  NGX_HASH_WILDCARD_KEY);

            if (rc == NGX_ERROR) {
                goto failed;
            }

            if (rc == NGX_DECLINED) {
                ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                              "invalid server name or wildcard \"%V\" on %V",
                              &name[n].name, &addr->opt.addr_text);
                goto failed;
            }

            if (rc == NGX_BUSY) {
                ngx_log_error(NGX_LOG_WARN, cf->log, 0,
                              "conflicting server name \"%V\" on %V, ignored",
                              &name[n].name, &addr->opt.addr_text);
            }
        }
    }

    hash.key = ngx_hash_key_lc;
    hash.max_size = cmcf->server_names_hash_max_size;
    hash.bucket_size = cmcf->server_names_hash_bucket_size;
    hash.name = "server_names_hash";
    hash.pool = cf->pool;

    if (ha.keys.nelts) {
        hash.hash = &addr->hash;
        hash.temp_pool = NULL;

        if (ngx_hash_init(&hash, ha.keys.elts, ha.keys.nelts) != NGX_OK) {
            goto failed;
        }
    }

    if (ha.dns_wc_head.nelts) {

        ngx_qsort(ha.dns_wc_head.elts, (size_t) ha.dns_wc_head.nelts,
                  sizeof(ngx_hash_key_t), ngx_http_cmp_dns_wildcards);

        hash.hash = NULL;
        hash.temp_pool = ha.temp_pool;

        if (ngx_hash_wildcard_init(&hash, ha.dns_wc_head.elts,
                                   ha.dns_wc_head.nelts)
            != NGX_OK)
        {
            goto failed;
        }

        addr->wc_head = (ngx_hash_wildcard_t *) hash.hash;
    }

    if (ha.dns_wc_tail.nelts) {

        ngx_qsort(ha.dns_wc_tail.elts, (size_t) ha.dns_wc_tail.nelts,
                  sizeof(ngx_hash_key_t), ngx_http_cmp_dns_wildcards);

        hash.hash = NULL;
        hash.temp_pool = ha.temp_pool;

        if (ngx_hash_wildcard_init(&hash, ha.dns_wc_tail.elts,
                                   ha.dns_wc_tail.nelts)
            != NGX_OK)
        {
            goto failed;
        }

        addr->wc_tail = (ngx_hash_wildcard_t *) hash.hash;
    }

    ngx_destroy_pool(ha.temp_pool);

#if (NGX_PCRE)

    if (regex == 0) {
        return NGX_OK;
    }

    addr->nregex = regex;
    addr->regex = ngx_palloc(cf->pool, regex * sizeof(ngx_http_server_name_t));
    if (addr->regex == NULL) {
        return NGX_ERROR;
    }

    i = 0;

    for (s = 0; s < addr->servers.nelts; s++) {

        name = cscfp[s]->server_names.elts;

        for (n = 0; n < cscfp[s]->server_names.nelts; n++) {
            if (name[n].regex) {
                addr->regex[i++] = name[n];
            }
        }
    }

#endif

    return NGX_OK;

failed:

    ngx_destroy_pool(ha.temp_pool);

    return NGX_ERROR;
}


static ngx_int_t
ngx_http_cmp_conf_addrs(const void *one, const void *two)
{
    ngx_http_conf_addr_t  *first, *second;

    first = (ngx_http_conf_addr_t *) one;
    second = (ngx_http_conf_addr_t *) two;

    if (first->opt.wildcard) {
        /* a wildcard address must be the last resort, shift it to the end */
        return 1;
    }

    if (second->opt.wildcard) {
        /* a wildcard address must be the last resort, shift it to the end */
        return -1;
    }

    if (first->opt.bind && !second->opt.bind) {
        /* shift explicit bind()ed addresses to the start */
        return -1;
    }

    if (!first->opt.bind && second->opt.bind) {
        /* shift explicit bind()ed addresses to the start */
        return 1;
    }

    /* do not sort by default */

    return 0;
}


static int ngx_libc_cdecl
ngx_http_cmp_dns_wildcards(const void *one, const void *two)
{
    ngx_hash_key_t  *first, *second;

    first = (ngx_hash_key_t *) one;
    second = (ngx_hash_key_t *) two;

    return ngx_dns_strcmp(first->key.data, second->key.data);
}


static ngx_int_t
ngx_http_init_listening(ngx_conf_t *cf, ngx_http_conf_port_t *port)
{
    ngx_uint_t                 i, last, bind_wildcard;
    ngx_listening_t           *ls;
    ngx_http_port_t           *hport;
    ngx_http_conf_addr_t      *addr;

    addr = port->addrs.elts;
    last = port->addrs.nelts;

    /*
     * If there is a binding to an "*:port" then we need to bind() to
     * the "*:port" only and ignore other implicit bindings.  The bindings
     * have been already sorted: explicit bindings are on the start, then
     * implicit bindings go, and wildcard binding is in the end.
     */

    if (addr[last - 1].opt.wildcard) {
        addr[last - 1].opt.bind = 1;
        bind_wildcard = 1;

    } else {
        bind_wildcard = 0;
    }

    i = 0;

    while (i < last) {

        if (bind_wildcard && !addr[i].opt.bind) {
            i++;
            continue;
        }

        ls = ngx_http_add_listening(cf, &addr[i]);
        if (ls == NULL) {
            return NGX_ERROR;
        }

        hport = ngx_pcalloc(cf->pool, sizeof(ngx_http_port_t));
        if (hport == NULL) {
            return NGX_ERROR;
        }

        ls->servers = hport;

        hport->naddrs = i + 1;

        switch (ls->sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            if (ngx_http_add_addrs6(cf, hport, addr) != NGX_OK) {
                return NGX_ERROR;
            }
            break;
#endif
        default: /* AF_INET */
            if (ngx_http_add_addrs(cf, hport, addr) != NGX_OK) {
                return NGX_ERROR;
            }
            break;
        }

        addr++;
        last--;
    }

    return NGX_OK;
}


static ngx_listening_t *
ngx_http_add_listening(ngx_conf_t *cf, ngx_http_conf_addr_t *addr)
{
    ngx_listening_t           *ls;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;

    ls = ngx_create_listening(cf, addr->opt.sockaddr, addr->opt.socklen);
    if (ls == NULL) {
        return NULL;
    }

    ls->addr_ntop = 1;

    ls->handler = ngx_http_init_connection;

    cscf = addr->default_server;
    ls->pool_size = cscf->connection_pool_size;

    clcf = cscf->ctx->loc_conf[ngx_http_core_module.ctx_index];

    ls->logp = clcf->error_log;
    ls->log.data = &ls->addr_text;
    ls->log.handler = ngx_accept_log_error;

#if (NGX_WIN32)
    {
    ngx_iocp_conf_t  *iocpcf = NULL;

    if (ngx_get_conf(cf->cycle->conf_ctx, ngx_events_module)) {
        iocpcf = ngx_event_get_conf(cf->cycle->conf_ctx, ngx_iocp_module);
    }
    if (iocpcf && iocpcf->acceptex_read) {
        ls->post_accept_buffer_size = cscf->client_header_buffer_size;
    }
    }
#endif

    ls->type = addr->opt.type;
    ls->backlog = addr->opt.backlog;
    ls->rcvbuf = addr->opt.rcvbuf;
    ls->sndbuf = addr->opt.sndbuf;

    ls->keepalive = addr->opt.so_keepalive;
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    ls->keepidle = addr->opt.tcp_keepidle;
    ls->keepintvl = addr->opt.tcp_keepintvl;
    ls->keepcnt = addr->opt.tcp_keepcnt;
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    ls->accept_filter = addr->opt.accept_filter;
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
    ls->deferred_accept = addr->opt.deferred_accept;
#endif

#if (NGX_HAVE_INET6)
    ls->ipv6only = addr->opt.ipv6only;
#endif

#if (NGX_HAVE_SETFIB)
    ls->setfib = addr->opt.setfib;
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
    ls->fastopen = addr->opt.fastopen;
#endif

#if (NGX_HAVE_REUSEPORT)
    ls->reuseport = addr->opt.reuseport;
#endif

    ls->wildcard = addr->opt.wildcard;

#if (NGX_HTTP_V3)
    ls->quic = addr->opt.quic;
#endif

    return ls;
}


static ngx_int_t
ngx_http_add_addrs(ngx_conf_t *cf, ngx_http_port_t *hport,
    ngx_http_conf_addr_t *addr)
{
    ngx_uint_t                 i;
    ngx_http_in_addr_t        *addrs;
    struct sockaddr_in        *sin;
    ngx_http_virtual_names_t  *vn;

    hport->addrs = ngx_pcalloc(cf->pool,
                               hport->naddrs * sizeof(ngx_http_in_addr_t));
    if (hport->addrs == NULL) {
        return NGX_ERROR;
    }

    addrs = hport->addrs;

    for (i = 0; i < hport->naddrs; i++) {

        sin = (struct sockaddr_in *) addr[i].opt.sockaddr;
        addrs[i].addr = sin->sin_addr.s_addr;
        addrs[i].conf.default_server = addr[i].default_server;
#if (NGX_HTTP_SSL)
        addrs[i].conf.ssl = addr[i].opt.ssl;
#endif
#if (NGX_HTTP_V2)
        addrs[i].conf.http2 = addr[i].opt.http2;
#endif
#if (NGX_HTTP_V3)
        addrs[i].conf.quic = addr[i].opt.quic;
#endif
        addrs[i].conf.proxy_protocol = addr[i].opt.proxy_protocol;

        if (addr[i].hash.buckets == NULL
            && (addr[i].wc_head == NULL
                || addr[i].wc_head->hash.buckets == NULL)
            && (addr[i].wc_tail == NULL
                || addr[i].wc_tail->hash.buckets == NULL)
#if (NGX_PCRE)
            && addr[i].nregex == 0
#endif
            )
        {
            continue;
        }

        vn = ngx_palloc(cf->pool, sizeof(ngx_http_virtual_names_t));
        if (vn == NULL) {
            return NGX_ERROR;
        }

        addrs[i].conf.virtual_names = vn;

        vn->names.hash = addr[i].hash;
        vn->names.wc_head = addr[i].wc_head;
        vn->names.wc_tail = addr[i].wc_tail;
#if (NGX_PCRE)
        vn->nregex = addr[i].nregex;
        vn->regex = addr[i].regex;
#endif
    }

    return NGX_OK;
}


#if (NGX_HAVE_INET6)

static ngx_int_t
ngx_http_add_addrs6(ngx_conf_t *cf, ngx_http_port_t *hport,
    ngx_http_conf_addr_t *addr)
{
    ngx_uint_t                 i;
    ngx_http_in6_addr_t       *addrs6;
    struct sockaddr_in6       *sin6;
    ngx_http_virtual_names_t  *vn;

    hport->addrs = ngx_pcalloc(cf->pool,
                               hport->naddrs * sizeof(ngx_http_in6_addr_t));
    if (hport->addrs == NULL) {
        return NGX_ERROR;
    }

    addrs6 = hport->addrs;

    for (i = 0; i < hport->naddrs; i++) {

        sin6 = (struct sockaddr_in6 *) addr[i].opt.sockaddr;
        addrs6[i].addr6 = sin6->sin6_addr;
        addrs6[i].conf.default_server = addr[i].default_server;
#if (NGX_HTTP_SSL)
        addrs6[i].conf.ssl = addr[i].opt.ssl;
#endif
#if (NGX_HTTP_V2)
        addrs6[i].conf.http2 = addr[i].opt.http2;
#endif
#if (NGX_HTTP_V3)
        addrs6[i].conf.quic = addr[i].opt.quic;
#endif
        addrs6[i].conf.proxy_protocol = addr[i].opt.proxy_protocol;

        if (addr[i].hash.buckets == NULL
            && (addr[i].wc_head == NULL
                || addr[i].wc_head->hash.buckets == NULL)
            && (addr[i].wc_tail == NULL
                || addr[i].wc_tail->hash.buckets == NULL)
#if (NGX_PCRE)
            && addr[i].nregex == 0
#endif
            )
        {
            continue;
        }

        vn = ngx_palloc(cf->pool, sizeof(ngx_http_virtual_names_t));
        if (vn == NULL) {
            return NGX_ERROR;
        }

        addrs6[i].conf.virtual_names = vn;

        vn->names.hash = addr[i].hash;
        vn->names.wc_head = addr[i].wc_head;
        vn->names.wc_tail = addr[i].wc_tail;
#if (NGX_PCRE)
        vn->nregex = addr[i].nregex;
        vn->regex = addr[i].regex;
#endif
    }

    return NGX_OK;
}

#endif


char *
ngx_http_types_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_array_t     **types;
    ngx_str_t        *value, *default_type;
    ngx_uint_t        i, n, hash;
    ngx_hash_key_t   *type;

    types = (ngx_array_t **) (p + cmd->offset);

    if (*types == (void *) -1) {
        return NGX_CONF_OK;
    }

    default_type = cmd->post;

    if (*types == NULL) {
        *types = ngx_array_create(cf->temp_pool, 1, sizeof(ngx_hash_key_t));
        if (*types == NULL) {
            return NGX_CONF_ERROR;
        }

        if (default_type) {
            type = ngx_array_push(*types);
            if (type == NULL) {
                return NGX_CONF_ERROR;
            }

            type->key = *default_type;
            type->key_hash = ngx_hash_key(default_type->data,
                                          default_type->len);
            type->value = (void *) 4;
        }
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        if (value[i].len == 1 && value[i].data[0] == '*') {
            *types = (void *) -1;
            return NGX_CONF_OK;
        }

        hash = ngx_hash_strlow(value[i].data, value[i].data, value[i].len);
        value[i].data[value[i].len] = '\0';

        type = (*types)->elts;
        for (n = 0; n < (*types)->nelts; n++) {

            if (ngx_strcmp(value[i].data, type[n].key.data) == 0) {
                ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                                   "duplicate MIME type \"%V\"", &value[i]);
                goto next;
            }
        }

        type = ngx_array_push(*types);
        if (type == NULL) {
            return NGX_CONF_ERROR;
        }

        type->key = value[i];
        type->key_hash = hash;
        type->value = (void *) 4;

    next:

        continue;
    }

    return NGX_CONF_OK;
}


char *
ngx_http_merge_types(ngx_conf_t *cf, ngx_array_t **keys, ngx_hash_t *types_hash,
    ngx_array_t **prev_keys, ngx_hash_t *prev_types_hash,
    ngx_str_t *default_types)
{
    ngx_hash_init_t  hash;

    if (*keys) {

        if (*keys == (void *) -1) {
            return NGX_CONF_OK;
        }

        hash.hash = types_hash;
        hash.key = NULL;
        hash.max_size = 2048;
        hash.bucket_size = 64;
        hash.name = "test_types_hash";
        hash.pool = cf->pool;
        hash.temp_pool = NULL;

        if (ngx_hash_init(&hash, (*keys)->elts, (*keys)->nelts) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        return NGX_CONF_OK;
    }

    if (prev_types_hash->buckets == NULL) {

        if (*prev_keys == NULL) {

            if (ngx_http_set_default_types(cf, prev_keys, default_types)
                != NGX_OK)
            {
                return NGX_CONF_ERROR;
            }

        } else if (*prev_keys == (void *) -1) {
            *keys = *prev_keys;
            return NGX_CONF_OK;
        }

        hash.hash = prev_types_hash;
        hash.key = NULL;
        hash.max_size = 2048;
        hash.bucket_size = 64;
        hash.name = "test_types_hash";
        hash.pool = cf->pool;
        hash.temp_pool = NULL;

        if (ngx_hash_init(&hash, (*prev_keys)->elts, (*prev_keys)->nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    *types_hash = *prev_types_hash;

    return NGX_CONF_OK;

}


ngx_int_t
ngx_http_set_default_types(ngx_conf_t *cf, ngx_array_t **types,
    ngx_str_t *default_type)
{
    ngx_hash_key_t  *type;

    *types = ngx_array_create(cf->temp_pool, 1, sizeof(ngx_hash_key_t));
    if (*types == NULL) {
        return NGX_ERROR;
    }

    while (default_type->len) {

        type = ngx_array_push(*types);
        if (type == NULL) {
            return NGX_ERROR;
        }

        type->key = *default_type;
        type->key_hash = ngx_hash_key(default_type->data,
                                      default_type->len);
        type->value = (void *) 4;

        default_type++;
    }

    return NGX_OK;
}
