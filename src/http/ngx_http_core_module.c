
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_core_module.c
 *
 * 该模块是Nginx HTTP核心模块，提供了HTTP服务器的基本功能和框架。
 *
 * 支持的功能:
 * 1. HTTP请求的接收和处理
 * 2. HTTP响应的生成和发送
 * 3. 请求路由和location匹配
 * 4. 请求体处理
 * 5. keepalive连接管理
 * 6. 访问控制和认证
 * 7. 错误处理和日志记录
 *
 * 支持的指令:
 * - server: 定义虚拟服务器
 * - location: 定义URI匹配规则和处理块
 * - root: 设置请求的根目录
 * - alias: 为请求设置别名路径
 * - index: 定义默认索引文件
 * - error_page: 自定义错误页面
 * - client_max_body_size: 设置客户端请求体的最大允许大小
 * - client_body_buffer_size: 设置读取客户端请求体的缓冲区大小
 * - sendfile: 启用或禁用sendfile功能
 * - keepalive_timeout: 设置keepalive连接的超时时间
 * - send_timeout: 设置向客户端发送响应的超时时间
 * - server_name: 设置虚拟服务器的名称
 * - types: 设置文件扩展名与MIME类型的映射
 * - default_type: 设置默认MIME类型
 *
 * 支持的变量:
 * - $request: 完整的原始请求行
 * - $request_method: 请求方法
 * - $request_uri: 完整的请求URI
 * - $uri: 当前请求的URI，不包括参数
 * - $args: 请求行中的参数
 * - $request_body: 请求体
 * - $http_*: 请求头字段
 * - $sent_http_*: 响应头字段
 * - $hostname: 主机名
 * - $remote_addr: 客户端地址
 * - $remote_port: 客户端端口
 * - $server_addr: 服务器地址
 * - $server_port: 服务器端口
 * - $server_protocol: 请求使用的协议，通常是"HTTP/1.0"或"HTTP/1.1"
 *
 * 使用注意点:
 * 1. 合理配置client_max_body_size和client_body_buffer_size，以适应不同的应用场景
 * 2. 谨慎使用sendfile指令，确保系统支持且适合您的应用
 * 3. 正确设置keepalive_timeout，以平衡性能和资源使用
 * 4. 使用location指令时注意优先级和匹配规则
 * 5. 合理使用error_page指令，提供友好的错误提示
 * 6. 注意server_name的配置，特别是在使用多个虚拟主机时
 * 7. 谨慎处理和存储$request_body，特别是对于大型请求体
 * 8. 在处理$http_*和$sent_http_*变量时注意安全性，避免信息泄露
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/*
 * 定义一个结构体
 * 这个结构体可能用于存储HTTP核心模块的配置信息
 * 具体字段将在后续代码中定义
 */
typedef struct {
    u_char    *name;
    uint32_t   method;
} ngx_http_method_name_t;


/*
 * 定义HTTP请求体文件处理的选项
 * NGX_HTTP_REQUEST_BODY_FILE_OFF: 不将请求体保存到文件
 */
#define NGX_HTTP_REQUEST_BODY_FILE_OFF    0
/*
 * 定义HTTP请求体文件处理的选项
 * NGX_HTTP_REQUEST_BODY_FILE_ON: 将请求体保存到文件
 */
#define NGX_HTTP_REQUEST_BODY_FILE_ON     1
/*
 * 定义HTTP请求体文件处理的选项
 * NGX_HTTP_REQUEST_BODY_FILE_CLEAN: 将请求体保存到文件，并在请求结束后清理该文件
 * 这个选项可能用于临时存储大型请求体，并在处理完成后自动删除文件以节省磁盘空间
 */
#define NGX_HTTP_REQUEST_BODY_FILE_CLEAN  2


/*
 * 处理HTTP认证延迟的函数
 * @param r HTTP请求结构体指针
 * @return 返回ngx_int_t类型的状态码
 * 
 * 这个函数可能用于实现认证延迟机制，以防止暴力破解攻击。
 * 它可能会在认证失败时引入一个短暂的延迟，从而增加攻击者的时间成本。
 */
static ngx_int_t ngx_http_core_auth_delay(ngx_http_request_t *r);
/*
 * 函数: ngx_http_core_auth_delay_handler
 * 功能: 处理HTTP认证延迟的事件
 * 参数:
 *   r: HTTP请求结构体指针
 * 描述:
 *   这个函数是认证延迟机制的事件处理器。
 *   它可能在认证延迟时间到期后被调用，用于继续处理请求或执行其他相关操作。
 *   这个处理器配合ngx_http_core_auth_delay函数使用，共同实现认证延迟功能，
 *   有助于防止暴力破解攻击。
 */
static void ngx_http_core_auth_delay_handler(ngx_http_request_t *r);

/*
 * 函数: ngx_http_core_find_location
 * 功能: 为给定的HTTP请求查找匹配的location配置
 * 参数:
 *   r: HTTP请求结构体指针
 * 返回值: ngx_int_t类型，表示查找结果
 * 描述:
 *   此函数负责在Nginx的location配置中查找与当前请求URI最匹配的location。
 *   它可能会遍历location树结构，比较URI与各个location的匹配规则，
 *   并返回最佳匹配的location。这个过程对于确定如何处理请求至关重要，
 *   因为不同的location可能有不同的处理指令和配置。
 */
static ngx_int_t ngx_http_core_find_location(ngx_http_request_t *r);
/*
 * 函数: ngx_http_core_find_static_location
 * 功能: 在静态location树中查找匹配的location配置
 * 参数:
 *   r: HTTP请求结构体指针
 *   node: location树的节点指针
 * 返回值:
 *   NGX_OK: 精确匹配
 *   NGX_DONE: 自动重定向
 *   NGX_AGAIN: 包含性匹配
 *   NGX_DECLINED: 没有匹配
 * 描述:
 *   此函数通过遍历location树来查找与请求URI最匹配的location配置。
 *   它使用二叉树搜索算法来提高查找效率。函数会比较URI与每个节点的名称，
 *   根据比较结果决定是继续向左子树还是右子树搜索，或者返回匹配结果。
 *   对于包含性匹配，函数可能会递归调用自身来处理嵌套的location。
 */
static ngx_int_t ngx_http_core_find_static_location(ngx_http_request_t *r,
    ngx_http_location_tree_node_t *node);

/*
 * 函数: ngx_http_core_preconfiguration
 * 功能: 执行HTTP核心模块的预配置
 * 参数:
 *   cf: Nginx配置结构体指针
 * 返回值: 成功返回NGX_OK，失败返回NGX_ERROR
 * 描述:
 *   此函数在Nginx配置解析的早期阶段被调用，用于执行HTTP核心模块的预配置工作。
 *   它可能会初始化一些全局变量、注册一些回调函数，或者执行其他需要在主配置之前完成的任务。
 *   这个阶段的配置通常用于为后续的配置过程做准备，确保核心模块能够正确处理其他模块的配置。
 */
static ngx_int_t ngx_http_core_preconfiguration(ngx_conf_t *cf);
/*
 * 函数: ngx_http_core_postconfiguration
 * 功能: 执行HTTP核心模块的后配置
 * 参数:
 *   cf: Nginx配置结构体指针
 * 返回值: 成功返回NGX_OK，失败返回NGX_ERROR
 * 描述:
 *   此函数在Nginx配置解析的最后阶段被调用，用于执行HTTP核心模块的后配置工作。
 *   它可能会进行一些最终的设置调整、资源初始化，或者执行其他需要在所有配置完成后进行的任务。
 *   这个阶段的配置通常用于确保所有模块的配置都已正确加载和处理，并为即将开始的服务做好准备。
 */
static ngx_int_t ngx_http_core_postconfiguration(ngx_conf_t *cf);
/*
 * 函数: ngx_http_core_create_main_conf
 * 功能: 创建HTTP核心模块的主配置结构
 * 参数:
 *   cf: Nginx配置结构体指针
 * 返回值: 新创建的主配置结构指针，失败时返回NULL
 * 描述:
 *   此函数负责为HTTP核心模块创建主配置结构。
 *   它可能会分配内存，初始化默认值，并设置一些全局级别的HTTP配置。
 *   这个配置结构将用于存储影响整个HTTP处理过程的核心设置。
 *   在Nginx配置解析过程中，只会调用一次此函数来创建HTTP核心模块的主配置。
 */
static void *ngx_http_core_create_main_conf(ngx_conf_t *cf);
/*
 * 函数: ngx_http_core_init_main_conf
 * 功能: 初始化HTTP核心模块的主配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   conf: 指向主配置结构的指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数负责初始化HTTP核心模块的主配置。
 *   它可能会设置一些默认值，验证配置的有效性，
 *   或者执行其他需要在配置完全解析后进行的初始化操作。
 *   这个函数通常在所有配置指令都被处理后调用，
 *   用于确保主配置结构中的所有必要设置都已正确初始化。
 */
static char *ngx_http_core_init_main_conf(ngx_conf_t *cf, void *conf);
/*
 * 函数: ngx_http_core_create_srv_conf
 * 功能: 创建HTTP服务器配置结构
 * 参数:
 *   cf: Nginx配置结构体指针
 * 返回值: 新创建的服务器配置结构指针，失败时返回NULL
 * 描述:
 *   此函数负责为每个HTTP服务器块创建一个新的配置结构。
 *   它可能会分配内存，初始化默认值，并设置一些基本的服务器级别配置。
 *   这个配置结构将用于存储特定于服务器的设置，如监听端口、服务器名称等。
 *   在Nginx配置解析过程中，对于每个server块，都会调用此函数来创建相应的配置结构。
 */
static void *ngx_http_core_create_srv_conf(ngx_conf_t *cf);

/*
 * 函数: ngx_http_core_merge_srv_conf
 * 功能: 合并HTTP服务器配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   parent: 父配置结构体指针
 *   child: 子配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数负责合并父级和子级的HTTP服务器配置。
 *   它会检查子配置中未设置的选项，并从父配置中继承相应的值。
 *   这个过程确保了配置的层次结构和继承机制正常工作。
 */
static char *ngx_http_core_merge_srv_conf(ngx_conf_t *cf,
    void *parent, void *child);
/*
 * 函数: ngx_http_core_create_loc_conf
 * 功能: 创建HTTP location配置结构
 * 参数:
 *   cf: Nginx配置结构体指针
 * 返回值: 新创建的location配置结构指针，失败时返回NULL
 * 描述:
 *   此函数负责为每个HTTP location块创建一个新的配置结构。
 *   它会分配内存，初始化默认值，并设置一些基本的location级别配置。
 *   这个配置结构将用于存储特定于location的设置，如root路径、index文件等。
 *   在Nginx配置解析过程中，对于每个location块，都会调用此函数来创建相应的配置结构。
 */
static void *ngx_http_core_create_loc_conf(ngx_conf_t *cf);
/*
 * 函数: ngx_http_core_merge_loc_conf
 * 功能: 合并HTTP location配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   parent: 父配置结构体指针
 *   child: 子配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数负责合并父级和子级的HTTP location配置。
 *   它会检查子配置中未设置的选项，并从父配置中继承相应的值。
 *   这个过程确保了配置的层次结构和继承机制正常工作，
 *   使得子location可以覆盖或继承父location的设置。
 */
static char *ngx_http_core_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

/*
 * 函数: ngx_http_core_server
 * 功能: 处理server指令，创建新的HTTP服务器配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前命令结构体指针
 *   dummy: 未使用的参数，通常为NULL
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数负责处理Nginx配置文件中的server指令。
 *   它会创建一个新的HTTP服务器配置结构，并设置相关的默认值。
 *   这个函数是HTTP模块中server配置块的入口点。
 */
static char *ngx_http_core_server(ngx_conf_t *cf, ngx_command_t *cmd,
    void *dummy);
/*
 * 函数: ngx_http_core_location
 * 功能: 处理location指令，创建新的HTTP location配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前命令结构体指针
 *   dummy: 未使用的参数，通常为NULL
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数负责处理Nginx配置文件中的location指令。
 *   它会创建一个新的HTTP location配置结构，并设置相关的默认值。
 *   这个函数是HTTP模块中location配置块的入口点。
 */
static char *ngx_http_core_location(ngx_conf_t *cf, ngx_command_t *cmd,
    void *dummy);
/*
 * 函数: ngx_http_core_regex_location
 * 功能: 处理正则表达式location配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   clcf: HTTP核心location配置结构体指针
 *   regex: 正则表达式字符串指针
 *   caseless: 是否大小写不敏感的标志
 * 返回值: 成功返回NGX_OK，失败返回NGX_ERROR
 * 描述:
 *   此函数用于处理使用正则表达式定义的location。
 *   它会编译正则表达式，并将其添加到location配置中。
 *   这对于实现灵活的URL匹配非常重要。
 */
static ngx_int_t ngx_http_core_regex_location(ngx_conf_t *cf,
    ngx_http_core_loc_conf_t *clcf, ngx_str_t *regex, ngx_uint_t caseless);

/*
 * 函数: ngx_http_core_types
 * 功能: 处理types指令，设置MIME类型映射
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前命令结构体指针
 *   conf: 配置上下文指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数负责解析和设置MIME类型映射。它处理文件扩展名到MIME类型的对应关系，
 *   这对于Nginx正确地设置HTTP响应的Content-Type头部非常重要。
 *   函数会更新配置结构中的相关字段，以存储这些映射信息。
 */
static char *ngx_http_core_types(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 函数: ngx_http_core_types
 * 功能: 处理types指令，设置MIME类型映射
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前命令结构体指针
 *   conf: 配置上下文指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数负责解析和设置MIME类型映射。它可能会处理文件扩展名到MIME类型的对应关系，
 *   这对于Nginx正确地设置HTTP响应的Content-Type头部非常重要。
 *   函数可能会更新配置结构中的相关字段，以存储这些映射信息。
 */
static char *ngx_http_core_type(ngx_conf_t *cf, ngx_command_t *dummy,
    void *conf);

/* 
 * 处理 listen 指令的函数
 * @param cf 配置上下文
 * @param cmd 当前命令
 */
static char *ngx_http_core_listen(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 处理 server_name 指令的函数
 * @param cf 配置上下文
 * @param cmd 当前命令
 * @param conf 配置结构体指针
 * @return 成功返回 NGX_CONF_OK，失败返回错误信息字符串
 */
static char *ngx_http_core_server_name(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 处理 root 指令的函数
 * @param cf 配置上下文
 * @param cmd 当前命令
 * @param conf 配置结构体指针
 * @return 成功返回 NGX_CONF_OK，失败返回错误信息字符串
 */
static char *ngx_http_core_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
/*
 * 处理 limit_except 指令的函数
 * @param cf 配置上下文
 * @param cmd 当前命令
 * @param conf 配置结构体指针
 * @return 成功返回 NGX_CONF_OK，失败返回错误信息字符串
 */
/*
 * 处理 limit_except 指令的函数
 * @param cf 配置上下文
 * @param cmd 当前命令
 * @param conf 配置结构体指针
 * @return 成功返回 NGX_CONF_OK，失败返回错误信息字符串
 */
static char *ngx_http_core_limit_except(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 函数: ngx_http_core_set_aio
 * 功能: 设置异步I/O (AIO) 配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置异步I/O相关的配置指令。
 *   它可能会根据配置参数启用或禁用AIO，设置AIO的具体参数等。
 *   AIO的配置对于提高Nginx在处理大量并发连接时的性能非常重要。
 */
static char *ngx_http_core_set_aio(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 函数: ngx_http_core_directio
 * 功能: 处理 directio 指令的配置
 * 参数:
 *   cf: Nginx 配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针
 * 返回值: 成功返回 NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置 directio 相关的配置指令。
 *   directio 指令用于控制 Nginx 是否使用直接 I/O 来读取文件，
 *   这可能会在某些情况下提高性能，特别是对于大文件的处理。
 */
static char *ngx_http_core_directio(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 函数: ngx_http_core_error_page
 * 功能: 处理错误页面配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置错误页面相关的配置指令。
 *   它允许管理员为不同的HTTP错误状态码指定自定义的错误页面，
 *   从而提供更好的用户体验和更详细的错误信息。
 */
static char *ngx_http_core_error_page(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 函数: ngx_http_core_open_file_cache
 * 功能: 处理打开文件缓存的配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置打开文件缓存相关的配置指令。
 *   打开文件缓存可以提高Nginx的性能，特别是在处理静态文件时。
 *   它允许Nginx缓存打开的文件描述符、文件大小和修改时间等信息。
 */
static char *ngx_http_core_open_file_cache(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 函数: ngx_http_core_error_log
 * 功能: 处理错误日志配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置错误日志相关的配置指令。
 *   它允许管理员配置Nginx的错误日志级别、日志文件路径等，
 *   以便更好地监控和调试Nginx服务器的运行状况。
 */
static char *ngx_http_core_error_log(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 函数: ngx_http_core_keepalive
 * 功能: 处理 keepalive 连接的配置
 * 参数:
 *   cf: Nginx 配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针
 * 返回值: 成功返回 NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置 HTTP keepalive 连接相关的配置指令。
 *   它允许管理员配置 keepalive 连接的超时时间、最大请求数等参数，
 *   以优化服务器性能和资源利用。
 */
static char *ngx_http_core_keepalive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 函数: ngx_http_core_internal
 * 功能: 处理内部请求的配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置内部请求相关的配置指令。
 *   内部请求通常用于Nginx内部的重定向或子请求处理，
 *   对于提高Nginx的灵活性和功能性非常重要。
 */
static char *ngx_http_core_internal(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
/*
 * 函数: ngx_http_core_resolver
 * 功能: 处理DNS解析器配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置DNS解析器相关的配置指令。
 *   它允许管理员配置Nginx使用的DNS服务器、解析超时时间等参数，
 *   对于处理域名解析和upstream动态解析等功能非常重要。
 */
static char *ngx_http_core_resolver(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#if (NGX_HTTP_GZIP)
/*
 * 函数: ngx_http_gzip_accept_encoding
 * 功能: 检查客户端是否接受gzip编码
 * 参数:
 *   ae: 指向包含Accept-Encoding头部值的ngx_str_t结构体指针
 * 返回值: 如果客户端接受gzip编码则返回非零值，否则返回0
 * 描述:
 *   此函数用于解析HTTP请求的Accept-Encoding头部，
 *   判断客户端是否支持gzip压缩。这对于决定是否对响应进行gzip压缩非常重要，
 *   可以有效减少网络传输数据量，提高性能。
 */
static ngx_int_t ngx_http_gzip_accept_encoding(ngx_str_t *ae);
/*
 * 函数: ngx_http_gzip_quantity
 * 功能: 计算gzip压缩质量参数
 * 参数:
 *   p: 指向待解析字符串的起始位置
 *   last: 指向待解析字符串的结束位置
 * 返回值: 解析得到的gzip压缩质量值
 * 描述:
 *   此函数用于解析HTTP请求头中的gzip压缩质量参数。
 *   它从给定的字符串中提取数值，该数值表示客户端期望的gzip压缩质量。
 *   这个信息可以用来调整服务器的gzip压缩设置，以平衡压缩率和性能。
 */
static ngx_uint_t ngx_http_gzip_quantity(u_char *p, u_char *last);
/*
 * 函数: ngx_http_gzip_disable
 * 功能: 处理gzip压缩禁用配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置gzip压缩禁用的条件。
 *   它允许管理员配置在特定情况下（如特定的User-Agent）禁用gzip压缩，
 *   以确保与不兼容gzip的客户端的兼容性。
 */
static char *ngx_http_gzip_disable(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif
/*
 * 函数: ngx_http_get_forwarded_addr_internal
 * 功能: 获取经过代理转发的客户端真实IP地址
 * 参数:
 *   r: HTTP请求结构体指针
 *   addr: 用于存储获取到的IP地址信息
 *   xff: X-Forwarded-For头部的值
 *   xfflen: X-Forwarded-For头部值的长度
 *   proxies: 可信代理IP地址列表
 *   recursive: 是否递归处理X-Forwarded-For头部
 * 返回值: 成功返回NGX_OK，失败返回错误码
 * 描述:
 *   此函数用于解析X-Forwarded-For头部，获取经过代理转发的原始客户端IP地址。
 *   它会考虑可信代理列表，以确保获取到的IP地址是可信的。
 *   这对于在使用反向代理时正确识别客户端IP非常重要，可用于日志记录、访问控制等功能。
 */
static ngx_int_t ngx_http_get_forwarded_addr_internal(ngx_http_request_t *r,
    ngx_addr_t *addr, u_char *xff, size_t xfflen, ngx_array_t *proxies,
    int recursive);
#if (NGX_HAVE_OPENAT)
/*
 * 函数: ngx_http_disable_symlinks
 * 功能: 处理禁用符号链接的配置指令
 * 参数:
 *   cf: Nginx配置结构体指针
 *   cmd: 当前正在处理的命令结构体指针
 *   conf: 模块配置结构体指针（未在函数签名中显示，但通常作为第三个参数）
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数用于解析和设置禁用符号链接的配置。
 *   它允许管理员控制Nginx如何处理文件系统中的符号链接，
 *   这对于提高安全性和防止某些类型的攻击非常重要。
 */
static char *ngx_http_disable_symlinks(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif

/*
 * 函数: ngx_http_core_lowat_check
 * 功能: 检查并验证低水位标记(low watermark)设置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   post: 后处理回调函数指针
 *   data: 待检查的数据指针
 * 返回值: 成功返回NULL，失败返回错误信息字符串
 * 描述:
 *   此函数用于检查HTTP核心模块中低水位标记的配置值是否有效。
 *   低水位标记通常用于套接字缓冲区的优化，影响数据传输的效率。
 *   函数会验证设置的值是否在合理范围内，并可能进行必要的调整。
 */
static char *ngx_http_core_lowat_check(ngx_conf_t *cf, void *post, void *data);
/*
 * 函数: ngx_http_core_pool_size
 * 功能: 验证和设置HTTP连接内存池的大小
 * 参数:
 *   cf: Nginx配置结构体指针
 *   post: 后处理回调函数指针
 *   data: 待验证的内存池大小数据指针
 * 返回值: 成功返回NULL，失败返回错误信息字符串
 * 描述:
 *   此函数用于检查和设置HTTP连接使用的内存池大小。
 *   它会验证配置中指定的内存池大小是否在合理范围内，
 *   并可能根据系统限制或最佳实践进行必要的调整。
 *   合理的内存池大小对于优化内存使用和提高性能很重要。
 */
static char *ngx_http_core_pool_size(ngx_conf_t *cf, void *post, void *data);

/*
 * 结构体: ngx_http_core_lowat_post
 * 类型: ngx_conf_post_t
 * 功能: 定义HTTP核心模块的低水位标记后处理回调
 * 描述:
 *   这个静态变量定义了一个配置后处理结构，用于HTTP核心模块的低水位标记设置。
 *   它与ngx_http_core_lowat_check函数相关联，该函数用于验证和处理低水位标记的配置值。
 *   低水位标记通常用于优化套接字缓冲区的性能，影响数据传输的效率。
 */
static ngx_conf_post_t  ngx_http_core_lowat_post =
    { ngx_http_core_lowat_check };

/*
 * 变量: ngx_http_core_pool_size_p
 * 类型: ngx_conf_post_handler_pt
 * 功能: 定义HTTP核心模块的内存池大小后处理函数指针
 * 描述:
 *   这个静态变量定义了一个函数指针，指向ngx_http_core_pool_size函数。
 *   该函数用于验证和处理HTTP连接内存池大小的配置值。
 *   合理设置内存池大小对于优化内存使用和提高Nginx性能很重要。
 */
static ngx_conf_post_handler_pt  ngx_http_core_pool_size_p =
    ngx_http_core_pool_size;


/*
 * 枚举数组: ngx_http_core_request_body_in_file
 * 类型: ngx_conf_enum_t
 * 功能: 定义请求体存储在文件中的配置选项
 * 描述:
 *   这个静态数组定义了HTTP请求体存储在文件中的不同选项。
 *   它用于配置指令中，允许管理员选择如何处理大型请求体。
 *   选项包括:
 *   - off: 不将请求体存储到文件中
 *   - on: 将请求体存储到文件中
 *   - clean: 将请求体存储到文件中，并在请求结束后清理文件
 */
static ngx_conf_enum_t  ngx_http_core_request_body_in_file[] = {
    { ngx_string("off"), NGX_HTTP_REQUEST_BODY_FILE_OFF },
    { ngx_string("on"), NGX_HTTP_REQUEST_BODY_FILE_ON },
    { ngx_string("clean"), NGX_HTTP_REQUEST_BODY_FILE_CLEAN },
    { ngx_null_string, 0 }
};


/*
 * 枚举数组: ngx_http_core_satisfy
 * 类型: ngx_conf_enum_t
 * 功能: 定义访问控制满足条件的配置选项
 * 描述:
 *   这个静态数组定义了HTTP模块中satisfy指令的可选值。
 *   它用于配置多个访问控制模块的组合逻辑。
 *   选项包括:
 *   - all: 要求满足所有访问控制模块的条件
 *   - any: 只需满足任一访问控制模块的条件
 */
static ngx_conf_enum_t  ngx_http_core_satisfy[] = {
    { ngx_string("all"), NGX_HTTP_SATISFY_ALL },  // 要求满足所有访问控制模块的条件
    { ngx_string("any"), NGX_HTTP_SATISFY_ANY },  // 只需满足任一访问控制模块的条件
    { ngx_null_string, 0 }  // 数组结束标志
};


/*
 * 枚举数组: ngx_http_core_lingering_close
 * 类型: ngx_conf_enum_t
 * 功能: 定义lingering_close指令的配置选项
 * 描述:
 *   这个静态数组定义了HTTP模块中lingering_close指令的可选值。
 *   它用于配置Nginx如何处理关闭连接时的延迟关闭行为。
 *   选项包括:
 *   - off: 禁用延迟关闭
 *   - on: 启用延迟关闭
 *   - always: 始终使用延迟关闭
 */
static ngx_conf_enum_t  ngx_http_core_lingering_close[] = {
    { ngx_string("off"), NGX_HTTP_LINGERING_OFF },    // 禁用延迟关闭
    { ngx_string("on"), NGX_HTTP_LINGERING_ON },      // 启用延迟关闭
    { ngx_string("always"), NGX_HTTP_LINGERING_ALWAYS }, // 始终使用延迟关闭
    { ngx_null_string, 0 }                            // 数组结束标志
};


/*
 * 枚举数组: ngx_http_core_server_tokens
 * 类型: ngx_conf_enum_t
 * 功能: 定义server_tokens指令的配置选项
 * 描述:
 *   这个静态数组定义了HTTP模块中server_tokens指令的可选值。
 *   它用于配置Nginx在响应头中是否包含服务器版本信息。
 *   选项将在后续的数组元素中定义。
 */
static ngx_conf_enum_t  ngx_http_core_server_tokens[] = {
    { ngx_string("off"), NGX_HTTP_SERVER_TOKENS_OFF },    // 关闭server_tokens,不在响应头中显示Nginx版本信息
    { ngx_string("on"), NGX_HTTP_SERVER_TOKENS_ON },      // 开启server_tokens,在响应头中显示Nginx版本信息
    { ngx_string("build"), NGX_HTTP_SERVER_TOKENS_BUILD },// 显示Nginx版本和编译信息
    { ngx_null_string, 0 }                                // 数组结束标志
};


/*
 * 枚举数组: ngx_http_core_if_modified_since
 * 类型: ngx_conf_enum_t
 * 功能: 定义if_modified_since指令的配置选项
 * 描述:
 *   这个静态数组定义了HTTP模块中if_modified_since指令的可选值。
 *   它用于配置Nginx如何处理带有If-Modified-Since头的请求。
 *   选项包括:
 *   - off: 禁用If-Modified-Since处理
 *   - exact: 精确匹配Last-Modified时间
 *   - before: 允许早于Last-Modified的时间
 */
static ngx_conf_enum_t  ngx_http_core_if_modified_since[] = {
    { ngx_string("off"), NGX_HTTP_IMS_OFF },    // 禁用If-Modified-Since处理
    { ngx_string("exact"), NGX_HTTP_IMS_EXACT }, // 精确匹配Last-Modified时间
    { ngx_string("before"), NGX_HTTP_IMS_BEFORE }, // 允许早于Last-Modified的时间
    { ngx_null_string, 0 }  // 数组结束标志
};


/*
 * 位掩码数组: ngx_http_core_keepalive_disable
 * 类型: ngx_conf_bitmask_t
 * 功能: 定义keepalive_disable指令的配置选项
 * 描述:
 *   这个静态数组定义了HTTP模块中keepalive_disable指令的可选值。
 *   它用于配置在哪些情况下禁用HTTP keepalive功能。
 *   具体选项将在后续的数组元素中定义。
 */
static ngx_conf_bitmask_t  ngx_http_core_keepalive_disable[] = {
    { ngx_string("none"), NGX_HTTP_KEEPALIVE_DISABLE_NONE },    // 不禁用keepalive
    { ngx_string("msie6"), NGX_HTTP_KEEPALIVE_DISABLE_MSIE6 },  // 为IE6禁用keepalive
    { ngx_string("safari"), NGX_HTTP_KEEPALIVE_DISABLE_SAFARI },// 为Safari浏览器禁用keepalive
    { ngx_null_string, 0 }                                      // 数组结束标志
};


/*
 * 路径初始化结构体: ngx_http_client_temp_path
 * 类型: ngx_path_init_t
 * 功能: 定义HTTP客户端临时文件路径
 * 描述:
 *   这个静态变量定义了HTTP模块中客户端临时文件的存储路径。
 *   它用于配置上传文件等临时数据的存储位置。
 *   具体的路径和其他属性将在结构体初始化中设置。
 */
static ngx_path_init_t  ngx_http_client_temp_path = {
    ngx_string(NGX_HTTP_CLIENT_TEMP_PATH), { 0, 0, 0 }
};


#if (NGX_HTTP_GZIP)

/*
 * 枚举数组: ngx_http_gzip_http_version
 * 类型: ngx_conf_enum_t
 * 功能: 定义gzip压缩支持的HTTP版本
 * 描述:
 *   这个静态数组定义了HTTP模块中gzip压缩功能支持的HTTP版本。
 *   它用于配置在哪些HTTP版本下启用gzip压缩。
 *   具体的版本选项将在后续的数组元素中定义。
 */
static ngx_conf_enum_t  ngx_http_gzip_http_version[] = {
    { ngx_string("1.0"), NGX_HTTP_VERSION_10 },  // 定义HTTP/1.0版本的gzip支持
    { ngx_string("1.1"), NGX_HTTP_VERSION_11 },  // 定义HTTP/1.1版本的gzip支持
    { ngx_null_string, 0 }  // 数组结束标志
};


/*
 * 位掩码数组: ngx_http_gzip_proxied_mask
 * 类型: ngx_conf_bitmask_t
 * 功能: 定义gzip_proxied指令的配置选项
 * 描述:
 *   这个静态数组定义了HTTP模块中gzip_proxied指令的可选值。
 *   它用于配置在代理请求中何时启用gzip压缩。
 *   具体的选项将在后续的数组元素中定义，包括off、expired、no-cache等。
 */
static ngx_conf_bitmask_t  ngx_http_gzip_proxied_mask[] = {
    { ngx_string("off"), NGX_HTTP_GZIP_PROXIED_OFF },      // 禁用代理请求的gzip压缩
    { ngx_string("expired"), NGX_HTTP_GZIP_PROXIED_EXPIRED },  // 对已过期的响应启用gzip压缩
    { ngx_string("no-cache"), NGX_HTTP_GZIP_PROXIED_NO_CACHE },  // 对标记为no-cache的响应启用gzip压缩
    { ngx_string("no-store"), NGX_HTTP_GZIP_PROXIED_NO_STORE },  // 对标记为no-store的响应启用gzip压缩
    { ngx_string("private"), NGX_HTTP_GZIP_PROXIED_PRIVATE },  // 对私有（private）响应启用gzip压缩
    { ngx_string("no_last_modified"), NGX_HTTP_GZIP_PROXIED_NO_LM },  // 对没有Last-Modified头的响应启用gzip压缩
    { ngx_string("no_etag"), NGX_HTTP_GZIP_PROXIED_NO_ETAG },  // 对没有ETag头的响应启用gzip压缩
    { ngx_string("auth"), NGX_HTTP_GZIP_PROXIED_AUTH },  // 对需要身份验证的请求启用gzip压缩
    { ngx_string("any"), NGX_HTTP_GZIP_PROXIED_ANY },  // 对所有代理请求启用gzip压缩
    { ngx_null_string, 0 }  // 数组结束标志
};


/*
 * 静态变量: ngx_http_gzip_no_cache
 * 类型: ngx_str_t
 * 功能: 定义gzip压缩的"no-cache"标志字符串
 * 描述:
 *   这个静态变量定义了一个表示"no-cache"的ngx_str_t类型字符串。
 *   它用于在gzip压缩相关的处理中，标识不应该被缓存的内容。
 *   这通常用于控制代理服务器或浏览器不缓存压缩后的响应。
 */
static ngx_str_t  ngx_http_gzip_no_cache = ngx_string("no-cache");
/*
 * 静态变量: ngx_http_gzip_no_store
 * 类型: ngx_str_t
 * 功能: 定义gzip压缩的"no-store"标志字符串
 * 描述:
 *   这个静态变量定义了一个表示"no-store"的ngx_str_t类型字符串。
 *   它用于在gzip压缩相关的处理中，标识不应该被存储的内容。
 *   这通常用于指示代理服务器或浏览器不应该将压缩后的响应存储在任何缓存中。
 */
static ngx_str_t  ngx_http_gzip_no_store = ngx_string("no-store");
/*
 * 静态变量: ngx_http_gzip_private
 * 类型: ngx_str_t
 * 功能: 定义gzip压缩的"private"标志字符串
 * 描述:
 *   这个静态变量定义了一个表示"private"的ngx_str_t类型字符串。
 *   它用于在gzip压缩相关的处理中，标识私有或个人化的内容。
 *   这通常用于指示响应是针对特定用户的，不应该被共享缓存所缓存。
 */
static ngx_str_t  ngx_http_gzip_private = ngx_string("private");

#endif


/*
 * 静态数组: ngx_http_core_commands
 * 类型: ngx_command_t
 * 功能: 定义HTTP核心模块的配置指令
 * 描述:
 *   这个静态数组包含了HTTP核心模块所有支持的配置指令。
 *   每个数组元素都是一个ngx_command_t结构，定义了一个具体的配置指令。
 *   这些指令涵盖了HTTP处理的各个方面，如服务器配置、位置块、请求处理等。
 *   通过这个数组，Nginx的配置解析器能够识别和处理HTTP核心模块的所有指令。
 */
/*
 * 静态数组: ngx_http_core_commands
 * 类型: ngx_command_t
 * 功能: 定义HTTP核心模块的配置指令
 * 描述:
 *   这个静态数组包含了HTTP核心模块所有支持的配置指令。
 *   每个数组元素都是一个ngx_command_t结构，定义了一个具体的配置指令。
 *   这些指令涵盖了HTTP处理的各个方面，如服务器配置、位置块、请求处理等。
 *   通过这个数组，Nginx的配置解析器能够识别和处理HTTP核心模块的所有指令。
 */
static ngx_command_t  ngx_http_core_commands[] = {

    /* 变量哈希表最大大小配置 */
    { ngx_string("variables_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, variables_hash_max_size),
      NULL },

    /* 变量哈希表桶大小配置 */
    { ngx_string("variables_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, variables_hash_bucket_size),
      NULL },

    /* 服务器名称哈希表最大大小配置 */
    { ngx_string("server_names_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, server_names_hash_max_size),
      NULL },

    /* 服务器名称哈希表桶大小配置 */
    { ngx_string("server_names_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, server_names_hash_bucket_size),
      NULL },

    /* 服务器配置块 */
    { ngx_string("server"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_http_core_server,
      0,
      0,
      NULL },

    /* 连接池大小配置 */
    { ngx_string("connection_pool_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, connection_pool_size),
      &ngx_http_core_pool_size_p },

    /* 请求池大小配置 */
    { ngx_string("request_pool_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, request_pool_size),
      &ngx_http_core_pool_size_p },

    /* 客户端头部超时配置 */
    { ngx_string("client_header_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, client_header_timeout),
      NULL },

    /* 客户端头部缓冲区大小配置 */
    { ngx_string("client_header_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, client_header_buffer_size),
      NULL },

    /* 大客户端头部缓冲区配置 */
    { ngx_string("large_client_header_buffers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, large_client_header_buffers),
      NULL },

    /* 忽略无效头部配置 */
    { ngx_string("ignore_invalid_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, ignore_invalid_headers),
      NULL },

    /* 合并斜杠配置 */
    { ngx_string("merge_slashes"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, merge_slashes),
      NULL },

    /* 头部中允许下划线配置 */
    { ngx_string("underscores_in_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, underscores_in_headers),
      NULL },

    /* 位置配置块 */
    { ngx_string("location"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE12,
      ngx_http_core_location,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    /* 监听配置 */
    { ngx_string("listen"),
      NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_core_listen,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    /* 服务器名称配置 */
    { ngx_string("server_name"),
      NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_core_server_name,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    /* MIME类型哈希表最大大小配置 */
    { ngx_string("types_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, types_hash_max_size),
      NULL },

    /* MIME类型哈希表桶大小配置 */
    { ngx_string("types_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, types_hash_bucket_size),
      NULL },

    /* MIME类型配置块 */
    { ngx_string("types"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF
                                          |NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_http_core_types,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 默认MIME类型配置 */
    { ngx_string("default_type"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, default_type),
      NULL },

    /* 根目录配置 */
    { ngx_string("root"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_core_root,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 别名配置 */
    { ngx_string("alias"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_core_root,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 限制HTTP方法配置 */
    { ngx_string("limit_except"),
      NGX_HTTP_LOC_CONF|NGX_CONF_BLOCK|NGX_CONF_1MORE,
      ngx_http_core_limit_except,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 客户端最大请求体大小配置 */
    { ngx_string("client_max_body_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_off_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_max_body_size),
      NULL },

    /* 客户端请求体缓冲区大小配置 */
    { ngx_string("client_body_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_buffer_size),
      NULL },

    /* 客户端请求体超时配置 */
    { ngx_string("client_body_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_timeout),
      NULL },

    /* 客户端请求体临时文件路径配置 */
    { ngx_string("client_body_temp_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1234,
      ngx_conf_set_path_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_temp_path),
      NULL },

    /* 客户端请求体仅保存在文件中配置 */
    { ngx_string("client_body_in_file_only"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_in_file_only),
      &ngx_http_core_request_body_in_file },

    /* 客户端请求体使用单一缓冲区配置 */
    { ngx_string("client_body_in_single_buffer"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_in_single_buffer),
      NULL },

    /* sendfile配置 */
    { ngx_string("sendfile"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, sendfile),
      NULL },

    /* sendfile最大块大小配置 */
    { ngx_string("sendfile_max_chunk"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, sendfile_max_chunk),
      NULL },

    /* 子请求输出缓冲区大小配置 */
    { ngx_string("subrequest_output_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, subrequest_output_buffer_size),
      NULL },

    /* AIO（异步I/O）配置指令 */
    { ngx_string("aio"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_core_set_aio,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* AIO写入配置指令 */
    { ngx_string("aio_write"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, aio_write),
      NULL },

    /* 预读取大小配置指令 */
    { ngx_string("read_ahead"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, read_ahead),
      NULL },

    /* 直接I/O配置指令 */
    { ngx_string("directio"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_core_directio,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 直接I/O对齐配置指令 */
    { ngx_string("directio_alignment"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_off_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, directio_alignment),
      NULL },

    /* TCP NOPUSH配置指令 */
    { ngx_string("tcp_nopush"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, tcp_nopush),
      NULL },

    /* TCP NODELAY配置指令 */
    { ngx_string("tcp_nodelay"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, tcp_nodelay),
      NULL },

    /* 发送超时配置指令 */
    { ngx_string("send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, send_timeout),
      NULL },

    /* 发送低水位标记配置指令 */
    { ngx_string("send_lowat"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, send_lowat),
      &ngx_http_core_lowat_post },

    /* 延迟输出配置指令 */
    { ngx_string("postpone_output"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, postpone_output),
      NULL },

    /* 限制速率配置指令 */
    { ngx_string("limit_rate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_set_complex_value_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, limit_rate),
      NULL },

    /* 限制速率后配置指令 */
    { ngx_string("limit_rate_after"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_set_complex_value_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, limit_rate_after),
      NULL },

    /* 保持连接时间配置指令 */
    { ngx_string("keepalive_time"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, keepalive_time),
      NULL },

    /* 保持连接超时配置指令 */
    { ngx_string("keepalive_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_core_keepalive,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 保持连接请求数配置指令 */
    { ngx_string("keepalive_requests"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, keepalive_requests),
      NULL },

    /* 禁用保持连接配置指令 */
    { ngx_string("keepalive_disable"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, keepalive_disable),
      &ngx_http_core_keepalive_disable },

    /* 满足条件配置指令 */
    { ngx_string("satisfy"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, satisfy),
      &ngx_http_core_satisfy },

    /* 认证延迟配置指令 */
    { ngx_string("auth_delay"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, auth_delay),
      NULL },

    /* 内部配置指令 */
    { ngx_string("internal"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_core_internal,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 延迟关闭配置指令 */
    { ngx_string("lingering_close"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, lingering_close),
      &ngx_http_core_lingering_close },

    /* 延迟时间配置指令 */
    { ngx_string("lingering_time"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, lingering_time),
      NULL },

    /* 延迟超时配置指令 */
    { ngx_string("lingering_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, lingering_timeout),
      NULL },

    /* 重置超时连接配置指令 */
    { ngx_string("reset_timedout_connection"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, reset_timedout_connection),
      NULL },

    /* 绝对重定向配置指令 */
    { ngx_string("absolute_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, absolute_redirect),
      NULL },

    /* 重定向中包含服务器名称配置指令 */
    { ngx_string("server_name_in_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, server_name_in_redirect),
      NULL },

    /* 重定向中包含端口配置指令 */
    { ngx_string("port_in_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, port_in_redirect),
      NULL },

    /* MSIE填充配置指令 */
    { ngx_string("msie_padding"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, msie_padding),
      NULL },

    /* MSIE刷新配置指令 */
    { ngx_string("msie_refresh"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, msie_refresh),
      NULL },

    /* 记录未找到配置指令 */
    { ngx_string("log_not_found"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, log_not_found),
      NULL },

    /* 记录子请求配置指令 */
    { ngx_string("log_subrequest"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, log_subrequest),
      NULL },

    /* 递归错误页面配置指令 */
    { ngx_string("recursive_error_pages"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, recursive_error_pages),
      NULL },

    /* 服务器标记配置指令 */
    { ngx_string("server_tokens"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, server_tokens),
      &ngx_http_core_server_tokens },

    /* 配置指令：if_modified_since
     * 作用：设置如何处理"If-Modified-Since"请求头
     * 上下文：http, server, location
     * 参数：枚举值
     */
    { ngx_string("if_modified_since"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, if_modified_since),
      &ngx_http_core_if_modified_since },

    /* 配置指令：max_ranges
     * 作用：设置单个请求中允许的最大范围数
     * 上下文：http, server, location
     * 参数：数字
     */
    { ngx_string("max_ranges"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, max_ranges),
      NULL },

    /* 配置指令：chunked_transfer_encoding
     * 作用：启用或禁用分块传输编码
     * 上下文：http, server, location
     * 参数：on/off
     */
    { ngx_string("chunked_transfer_encoding"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, chunked_transfer_encoding),
      NULL },

    /* 配置指令：etag
     * 作用：启用或禁用自动生成ETag响应头
     * 上下文：http, server, location
     * 参数：on/off
     */
    { ngx_string("etag"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, etag),
      NULL },

    /* 配置指令：error_page
     * 作用：定义错误页面
     * 上下文：http, server, location, if in location
     * 参数：错误代码 [=替换代码] URI
     */
    { ngx_string("error_page"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_2MORE,
      ngx_http_core_error_page,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 配置指令：post_action
     * 作用：定义请求完成后要执行的操作
     * 上下文：http, server, location, if in location
     * 参数：URI
     */
    { ngx_string("post_action"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, post_action),
      NULL },

    /* 配置指令：error_log
     * 作用：设置错误日志的路径、级别和格式
     * 上下文：http, server, location
     * 参数：文件路径 [日志级别]
     */
    { ngx_string("error_log"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_core_error_log,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 配置指令：open_file_cache
     * 作用：配置打开文件缓存
     * 上下文：http, server, location
     * 参数：max=N [inactive=time] | off
     */
    { ngx_string("open_file_cache"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_core_open_file_cache,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache),
      NULL },

    /* 配置指令：open_file_cache_valid
     * 作用：设置检查打开文件缓存中元素有效性的时间间隔
     * 上下文：http, server, location
     * 参数：时间
     */
    { ngx_string("open_file_cache_valid"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_sec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_valid),
      NULL },

    /* 配置指令：open_file_cache_min_uses
     * 作用：设置文件被访问多少次后保留在打开文件缓存中
     * 上下文：http, server, location
     * 参数：次数
     */
    { ngx_string("open_file_cache_min_uses"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_min_uses),
      NULL },

    /* 配置指令：open_file_cache_errors
     * 作用：是否缓存文件查找错误
     * 上下文：http, server, location
     * 参数：on/off
     */
    { ngx_string("open_file_cache_errors"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_errors),
      NULL },

    /* 配置指令：open_file_cache_events
     * 作用：是否在打开文件缓存中缓存文件事件
     * 上下文：http, server, location
     * 参数：on/off
     */
    { ngx_string("open_file_cache_events"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_events),
      NULL },

    /* 配置指令：resolver
     * 作用：配置用于解析上游服务器名称的DNS服务器
     * 上下文：http, server, location
     * 参数：地址 [有效时间]
     */
    { ngx_string("resolver"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_core_resolver,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* 配置指令：resolver_timeout
     * 作用：设置DNS解析超时时间
     * 上下文：http, server, location
     * 参数：时间
     */
    { ngx_string("resolver_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, resolver_timeout),
      NULL },

#if (NGX_HTTP_GZIP)

    /* 配置指令：gzip_vary
     * 作用：是否在使用gzip时发送"Vary: Accept-Encoding"响应头
     * 上下文：http, server, location
     * 参数：on/off
     */
    { ngx_string("gzip_vary"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, gzip_vary),
      NULL },

    /* 配置指令：gzip_http_version
     * 作用：设置使用gzip压缩的最低HTTP协议版本
     * 上下文：http, server, location
     * 参数：1.0/1.1
     */
    { ngx_string("gzip_http_version"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, gzip_http_version),
      &ngx_http_gzip_http_version },

    /* 配置指令：gzip_proxied
     * 作用：为代理请求的响应启用gzip压缩
     * 上下文：http, server, location
     * 参数：off | expired | no-cache | no-store | private | no_last_modified | no_etag | auth | any ...
     */
    { ngx_string("gzip_proxied"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, gzip_proxied),
      &ngx_http_gzip_proxied_mask },

    /* 配置指令：gzip_disable
     * 作用：针对特定User-Agent禁用gzip压缩
     * 上下文：http, server, location
     * 参数：正则表达式
     */
    { ngx_string("gzip_disable"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_gzip_disable,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

#endif

#if (NGX_HAVE_OPENAT)

    /* 配置指令：disable_symlinks
     * 作用：配置如何处理符号链接
     * 上下文：http, server, location
     * 参数：off | on | if_not_owner [from=part]
     */
    { ngx_string("disable_symlinks"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_disable_symlinks,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

#endif

      ngx_null_command
};


/*
 * HTTP核心模块上下文结构
 *
 * 此结构定义了HTTP核心模块的各个回调函数，用于处理模块的生命周期事件。
 * 包括配置前后的处理、主配置的创建和初始化、服务器配置的创建和合并，
 * 以及位置配置的创建和合并。这些函数在Nginx处理HTTP请求的不同阶段被调用，
 * 确保了核心模块的正确初始化和配置。
 */
static ngx_http_module_t  ngx_http_core_module_ctx = {
    ngx_http_core_preconfiguration,        /* preconfiguration */
    ngx_http_core_postconfiguration,       /* postconfiguration */

    ngx_http_core_create_main_conf,        /* create main configuration */
    ngx_http_core_init_main_conf,          /* init main configuration */

    ngx_http_core_create_srv_conf,         /* create server configuration */
    ngx_http_core_merge_srv_conf,          /* merge server configuration */

    ngx_http_core_create_loc_conf,         /* create location configuration */
    ngx_http_core_merge_loc_conf           /* merge location configuration */
};


ngx_module_t  ngx_http_core_module = {
    NGX_MODULE_V1,
    &ngx_http_core_module_ctx,             /* module context */
    ngx_http_core_commands,                /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


ngx_str_t  ngx_http_core_get_method = { 3, (u_char *) "GET" };


void
ngx_http_handler(ngx_http_request_t *r)
{
    ngx_http_core_main_conf_t  *cmcf;

    // 清除连接日志的action字段
    r->connection->log->action = NULL;

    if (!r->internal) {
        // 处理非内部请求
        switch (r->headers_in.connection_type) {
        case 0:
            // 如果HTTP版本大于1.0，默认启用keepalive
            r->keepalive = (r->http_version > NGX_HTTP_VERSION_10);
            break;

        case NGX_HTTP_CONNECTION_CLOSE:
            // 明确指定关闭连接
            r->keepalive = 0;
            break;

        case NGX_HTTP_CONNECTION_KEEP_ALIVE:
            // 明确指定保持连接
            r->keepalive = 1;
            break;
        }

        // 如果请求有内容长度或使用分块传输，启用lingering_close
        r->lingering_close = (r->headers_in.content_length_n > 0
                              || r->headers_in.chunked);
        // 从第一个阶段处理器开始
        r->phase_handler = 0;

    } else {
        // 处理内部请求
        cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
        // 从server_rewrite阶段开始处理
        r->phase_handler = cmcf->phase_engine.server_rewrite_index;
    }

    // 标记location为有效
    r->valid_location = 1;
#if (NGX_HTTP_GZIP)
    // 初始化GZIP相关标志
    r->gzip_tested = 0;
    r->gzip_ok = 0;
    r->gzip_vary = 0;
#endif

    // 设置写事件处理函数
    r->write_event_handler = ngx_http_core_run_phases;
    // 开始运行请求处理阶段
    ngx_http_core_run_phases(r);
}


void
ngx_http_core_run_phases(ngx_http_request_t *r)
{
    ngx_int_t                   rc;
    ngx_http_phase_handler_t   *ph;
    ngx_http_core_main_conf_t  *cmcf;

    // 获取HTTP核心模块的主配置
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    // 获取阶段处理器数组
    ph = cmcf->phase_engine.handlers;

    // 循环执行各个阶段的处理器
    while (ph[r->phase_handler].checker) {
        // 调用当前阶段的检查器函数
        rc = ph[r->phase_handler].checker(r, &ph[r->phase_handler]);

        // 如果检查器返回NGX_OK，表示请求处理完成，直接返回
        if (rc == NGX_OK) {
            return;
        }
        // 如果返回其他值，继续下一个阶段的处理
    }
}


ngx_int_t
ngx_http_core_generic_phase(ngx_http_request_t *r, ngx_http_phase_handler_t *ph)
{
    ngx_int_t  rc;

    /*
     * 通用阶段检查器,
     * 用于post read和pre-access阶段
     */

    // 记录调试日志，显示当前阶段处理器的索引
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "generic phase: %ui", r->phase_handler);

    // 调用当前阶段的处理函数
    rc = ph->handler(r);

    if (rc == NGX_OK) {
        // 如果处理函数返回OK，移动到下一个阶段处理器
        r->phase_handler = ph->next;
        return NGX_AGAIN;
    }

    if (rc == NGX_DECLINED) {
        // 如果处理函数拒绝处理，移动到下一个处理器
        r->phase_handler++;
        return NGX_AGAIN;
    }

    if (rc == NGX_AGAIN || rc == NGX_DONE) {
        // 如果处理函数需要稍后继续处理或已完成，返回OK
        return NGX_OK;
    }

    /* rc == NGX_ERROR || rc == NGX_HTTP_...  */
    // 如果发生错误或返回了特定的HTTP状态码，结束请求处理

    // 结束请求处理，传递返回的状态码
    ngx_http_finalize_request(r, rc);

    return NGX_OK;
}


ngx_int_t
ngx_http_core_rewrite_phase(ngx_http_request_t *r, ngx_http_phase_handler_t *ph)
{
    ngx_int_t  rc;

    // 记录调试日志，显示当前重写阶段处理器的索引
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "rewrite phase: %ui", r->phase_handler);

    // 调用当前重写阶段的处理函数
    rc = ph->handler(r);

    if (rc == NGX_DECLINED) {
        // 如果处理函数拒绝处理，移动到下一个处理器
        r->phase_handler++;
        return NGX_AGAIN;
    }

    if (rc == NGX_DONE) {
        // 如果处理函数已完成，返回OK
        return NGX_OK;
    }

    /* NGX_OK, NGX_AGAIN, NGX_ERROR, NGX_HTTP_...  */
    // 处理其他返回值：NGX_OK, NGX_AGAIN, NGX_ERROR, 或特定的HTTP状态码

    // 结束请求处理，传递返回的状态码
    ngx_http_finalize_request(r, rc);

    return NGX_OK;
}


ngx_int_t
ngx_http_core_find_config_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    u_char                    *p;
    size_t                     len;
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *clcf;

    // 重置内容处理器和URI变更标志
    r->content_handler = NULL;
    r->uri_changed = 0;

    // 查找匹配的location配置
    rc = ngx_http_core_find_location(r);

    // 如果查找过程中发生错误，返回500内部服务器错误
    if (rc == NGX_ERROR) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_OK;
    }

    // 获取当前location的核心配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 检查是否为内部请求访问仅限内部的location
    if (!r->internal && clcf->internal) {
        ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
        return NGX_OK;
    }

    // 记录调试日志，显示正在使用的配置
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "using configuration \"%s%V\"",
                   (clcf->noname ? "*" : (clcf->exact_match ? "=" : "")),
                   &clcf->name);

    // 更新请求的location配置
    ngx_http_update_location_config(r);

    // 记录调试日志，显示内容长度和最大允许的请求体大小
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http cl:%O max:%O",
                   r->headers_in.content_length_n, clcf->client_max_body_size);

    // 检查请求体大小是否超过限制
    if (r->headers_in.content_length_n != -1
        && !r->discard_body
        && clcf->client_max_body_size
        && clcf->client_max_body_size < r->headers_in.content_length_n)
    {
        // 记录错误日志
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "client intended to send too large body: %O bytes",
                      r->headers_in.content_length_n);

        // 标记已测试Expect头
        r->expect_tested = 1;
        // 丢弃请求体
        (void) ngx_http_discard_request_body(r);
        // 返回413请求实体过大错误
        ngx_http_finalize_request(r, NGX_HTTP_REQUEST_ENTITY_TOO_LARGE);
        return NGX_OK;
    }

    // 处理重定向情况
    if (rc == NGX_DONE) {
        // 清除之前的location信息
        ngx_http_clear_location(r);

        // 创建新的Location头
        r->headers_out.location = ngx_list_push(&r->headers_out.headers);
        if (r->headers_out.location == NULL) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_OK;
        }

        // 设置Location头的属性
        r->headers_out.location->hash = 1;
        r->headers_out.location->next = NULL;
        ngx_str_set(&r->headers_out.location->key, "Location");

        // 构建重定向URL
        if (r->args.len == 0) {
            // 如果没有查询参数，直接使用转义后的名称
            r->headers_out.location->value = clcf->escaped_name;
        } else {
            // 如果有查询参数，将其附加到URL后
            len = clcf->escaped_name.len + 1 + r->args.len;
            p = ngx_pnalloc(r->pool, len);

            if (p == NULL) {
                ngx_http_clear_location(r);
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_OK;
            }

            r->headers_out.location->value.len = len;
            r->headers_out.location->value.data = p;

            p = ngx_cpymem(p, clcf->escaped_name.data, clcf->escaped_name.len);
            *p++ = '?';
            ngx_memcpy(p, r->args.data, r->args.len);
        }

        // 返回301永久重定向
        ngx_http_finalize_request(r, NGX_HTTP_MOVED_PERMANENTLY);
        return NGX_OK;
    }

    // 移动到下一个阶段处理器
    r->phase_handler++;
    return NGX_AGAIN;
}


ngx_int_t
ngx_http_core_post_rewrite_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    ngx_http_core_srv_conf_t  *cscf;

    // 记录调试日志，显示当前阶段处理器的索引
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post rewrite phase: %ui", r->phase_handler);

    // 如果URI没有改变，直接进入下一个阶段
    if (!r->uri_changed) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    // 记录调试日志，显示URI改变的次数
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "uri changes: %d", r->uri_changes);

    /*
     * gcc before 3.3 compiles the broken code for
     *     if (r->uri_changes-- == 0)
     * if the r->uri_changes is defined as
     *     unsigned  uri_changes:4
     */

    // 减少URI改变次数计数
    r->uri_changes--;

    // 如果URI改变次数达到上限（0），记录错误并终止请求
    if (r->uri_changes == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while processing \"%V\"", &r->uri);

        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_OK;
    }

    // 设置下一个阶段处理器
    r->phase_handler = ph->next;

    // 获取服务器配置并更新位置配置
    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    r->loc_conf = cscf->ctx->loc_conf;

    // 返回NGX_AGAIN，表示需要继续处理
    return NGX_AGAIN;
}


ngx_int_t
ngx_http_core_access_phase(ngx_http_request_t *r, ngx_http_phase_handler_t *ph)
{
    ngx_int_t                  rc;
    ngx_table_elt_t           *h;
    ngx_http_core_loc_conf_t  *clcf;

    // 如果不是主请求，直接进入下一个阶段
    if (r != r->main) {
        r->phase_handler = ph->next;
        return NGX_AGAIN;
    }

    // 记录调试日志，显示当前访问阶段处理器的索引
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "access phase: %ui", r->phase_handler);

    // 调用当前阶段的处理函数
    rc = ph->handler(r);

    // 如果处理函数返回NGX_DECLINED，移动到下一个处理器
    if (rc == NGX_DECLINED) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    // 如果处理函数返回NGX_AGAIN或NGX_DONE，表示需要稍后继续处理
    if (rc == NGX_AGAIN || rc == NGX_DONE) {
        return NGX_OK;
    }

    // 获取当前location的核心配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 处理"satisfy all"的情况
    if (clcf->satisfy == NGX_HTTP_SATISFY_ALL) {
        // 如果返回OK，继续下一个处理器
        if (rc == NGX_OK) {
            r->phase_handler++;
            return NGX_AGAIN;
        }
    } else {
        // 处理"satisfy any"的情况
        if (rc == NGX_OK) {
            // 清除访问代码和WWW-Authenticate头
            r->access_code = 0;
            for (h = r->headers_out.www_authenticate; h; h = h->next) {
                h->hash = 0;
            }
            // 进入下一个阶段
            r->phase_handler = ph->next;
            return NGX_AGAIN;
        }

        // 处理禁止访问或未授权的情况
        if (rc == NGX_HTTP_FORBIDDEN || rc == NGX_HTTP_UNAUTHORIZED) {
            if (r->access_code != NGX_HTTP_UNAUTHORIZED) {
                r->access_code = rc;
            }
            // 继续下一个处理器
            r->phase_handler++;
            return NGX_AGAIN;
        }
    }

    /* rc == NGX_ERROR || rc == NGX_HTTP_...  */

    // 处理未授权的情况
    if (rc == NGX_HTTP_UNAUTHORIZED) {
        return ngx_http_core_auth_delay(r);
    }

    // 结束请求处理，返回最终的状态码
    ngx_http_finalize_request(r, rc);
    return NGX_OK;
}


ngx_int_t
ngx_http_core_post_access_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    ngx_int_t  access_code;

    // 记录调试日志，显示当前阶段处理器的索引
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post access phase: %ui", r->phase_handler);

    // 获取访问控制的结果代码
    access_code = r->access_code;

    if (access_code) {
        // 重置访问控制代码
        r->access_code = 0;

        if (access_code == NGX_HTTP_FORBIDDEN) {
            // 如果访问被禁止，记录错误日志
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "access forbidden by rule");
        }

        if (access_code == NGX_HTTP_UNAUTHORIZED) {
            // 如果未经授权，调用认证延迟处理函数
            return ngx_http_core_auth_delay(r);
        }

        // 结束请求处理，返回访问控制的结果代码
        ngx_http_finalize_request(r, access_code);
        return NGX_OK;
    }

    // 如果没有访问控制代码，进入下一个阶段处理器
    r->phase_handler++;
    return NGX_AGAIN;
}


static ngx_int_t
ngx_http_core_auth_delay(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;

    // 获取当前location的核心配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->auth_delay == 0) {
        // 如果没有设置认证延迟，直接返回未授权状态
        ngx_http_finalize_request(r, NGX_HTTP_UNAUTHORIZED);
        return NGX_OK;
    }

    // 记录未授权请求的延迟信息
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                  "delaying unauthorized request");

    if (r->connection->read->ready) {
        // 如果读事件已就绪，将其加入到posted事件队列
        ngx_post_event(r->connection->read, &ngx_posted_events);

    } else {
        // 否则，设置读事件处理
        if (ngx_handle_read_event(r->connection->read, 0) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    // 设置读写事件的处理函数
    r->read_event_handler = ngx_http_test_reading;
    r->write_event_handler = ngx_http_core_auth_delay_handler;

    // 标记写事件为延迟状态，并添加定时器
    r->connection->write->delayed = 1;
    ngx_add_timer(r->connection->write, clcf->auth_delay);

    /*
     * 触发一个额外的事件循环迭代
     * 以确保恒定时间的处理
     */

    ngx_post_event(r->connection->write, &ngx_posted_next_events);

    return NGX_OK;
}


static void
ngx_http_core_auth_delay_handler(ngx_http_request_t *r)
{
    ngx_event_t  *wev;

    // 记录调试日志
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "auth delay handler");

    // 获取写事件
    wev = r->connection->write;

    // 检查写事件是否处于延迟状态
    if (wev->delayed) {
        // 如果仍在延迟中，重新处理写事件
        if (ngx_handle_write_event(wev, 0) != NGX_OK) {
            // 处理失败时，以内部服务器错误结束请求
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        }

        return;
    }

    // 延迟结束，以未授权状态结束请求
    ngx_http_finalize_request(r, NGX_HTTP_UNAUTHORIZED);
}


ngx_int_t
ngx_http_core_content_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    size_t     root;
    ngx_int_t  rc;
    ngx_str_t  path;

    // 检查是否有内容处理器
    if (r->content_handler) {
        // 设置写事件处理器为空
        r->write_event_handler = ngx_http_request_empty_handler;
        // 调用内容处理器并结束请求
        ngx_http_finalize_request(r, r->content_handler(r));
        return NGX_OK;
    }

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "content phase: %ui", r->phase_handler);

    // 调用当前阶段处理器
    rc = ph->handler(r);

    // 如果处理器没有拒绝处理，结束请求
    if (rc != NGX_DECLINED) {
        ngx_http_finalize_request(r, rc);
        return NGX_OK;
    }

    /* rc == NGX_DECLINED */

    // 移动到下一个处理器
    ph++;

    // 如果还有检查器，继续处理下一个阶段
    if (ph->checker) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    /* no content handler was found */

    // 检查URI是否以'/'结尾
    if (r->uri.data[r->uri.len - 1] == '/') {
        // 尝试将URI映射到文件路径
        if (ngx_http_map_uri_to_path(r, &path, &root, 0) != NULL) {
            // 记录错误日志，禁止目录索引
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "directory index of \"%s\" is forbidden", path.data);
        }

        // 以禁止访问状态结束请求
        ngx_http_finalize_request(r, NGX_HTTP_FORBIDDEN);
        return NGX_OK;
    }

    // 记录错误日志，未找到处理器
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "no handler found");

    // 以未找到状态结束请求
    ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
    return NGX_OK;
}


void
ngx_http_update_location_config(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;

    // 获取当前请求的HTTP核心模块的location配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 检查请求方法是否受限制
    if (r->method & clcf->limit_except) {
        // 如果受限制，使用特定的限制配置
        r->loc_conf = clcf->limit_except_loc_conf;
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    }

    // 如果是主请求，设置连接的错误日志
    if (r == r->main) {
        ngx_set_connection_log(r->connection, clcf->error_log);
    }

    // 检查是否支持sendfile并且配置允许使用
    if ((ngx_io.flags & NGX_IO_SENDFILE) && clcf->sendfile) {
        r->connection->sendfile = 1;
    } else {
        r->connection->sendfile = 0;
    }

    // 设置请求体处理相关的标志
    if (clcf->client_body_in_file_only) {
        r->request_body_in_file_only = 1;
        r->request_body_in_persistent_file = 1;
        r->request_body_in_clean_file =
            clcf->client_body_in_file_only == NGX_HTTP_REQUEST_BODY_FILE_CLEAN;
        r->request_body_file_log_level = NGX_LOG_NOTICE;
    } else {
        r->request_body_file_log_level = NGX_LOG_WARN;
    }

    // 设置是否将请求体存储在单个缓冲区中
    r->request_body_in_single_buf = clcf->client_body_in_single_buffer;

    // 处理keepalive相关的设置
    if (r->keepalive) {
        if (clcf->keepalive_timeout == 0) {
            r->keepalive = 0;
        } else if (r->connection->requests >= clcf->keepalive_requests) {
            r->keepalive = 0;
        } else if (ngx_current_msec - r->connection->start_time
                   > clcf->keepalive_time)
        {
            r->keepalive = 0;
        } else if (r->headers_in.msie6
                   && r->method == NGX_HTTP_POST
                   && (clcf->keepalive_disable
                       & NGX_HTTP_KEEPALIVE_DISABLE_MSIE6))
        {
            // 对MSIE6的POST请求特殊处理
            r->keepalive = 0;
        } else if (r->headers_in.safari
                   && (clcf->keepalive_disable
                       & NGX_HTTP_KEEPALIVE_DISABLE_SAFARI))
        {
            // 对Safari浏览器特殊处理
            r->keepalive = 0;
        }
    }

    // 设置TCP_NOPUSH/TCP_CORK选项
    if (!clcf->tcp_nopush) {
        r->connection->tcp_nopush = NGX_TCP_NOPUSH_DISABLED;
    }

    // 设置内容处理器
    if (clcf->handler) {
        r->content_handler = clcf->handler;
    }
}


/*
 * NGX_OK       - exact or regex match
 * NGX_DONE     - auto redirect
 * NGX_AGAIN    - inclusive match
 * NGX_ERROR    - regex error
 * NGX_DECLINED - no match
 */

/*
 * 函数: ngx_http_core_find_location
 * 功能: 查找匹配的location配置
 * 参数:
 *   r: HTTP请求结构体指针
 * 返回值:
 *   NGX_OK: 精确或正则表达式匹配
 *   NGX_DONE: 自动重定向
 *   NGX_AGAIN: 包含性匹配
 *   NGX_ERROR: 正则表达式错误
 *   NGX_DECLINED: 没有匹配
 */
static ngx_int_t
ngx_http_core_find_location(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *pclcf;
#if (NGX_PCRE)
    ngx_int_t                  n;
    ngx_uint_t                 noregex;
    ngx_http_core_loc_conf_t  *clcf, **clcfp;

    noregex = 0;
#endif

    // 获取当前location的配置
    pclcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 在静态location中查找匹配
    rc = ngx_http_core_find_static_location(r, pclcf->static_locations);

    if (rc == NGX_AGAIN) {

#if (NGX_PCRE)
        // 获取当前location的配置
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        // 检查是否禁用正则表达式匹配
        noregex = clcf->noregex;
#endif

        /* 查找嵌套的locations */

        rc = ngx_http_core_find_location(r);
    }

    // 如果找到精确匹配或需要自动重定向，直接返回结果
    if (rc == NGX_OK || rc == NGX_DONE) {
        return rc;
    }

    /* rc == NGX_DECLINED 或在嵌套location中 rc == NGX_AGAIN */

#if (NGX_PCRE)

    // 如果允许正则表达式匹配且存在正则表达式location
    if (noregex == 0 && pclcf->regex_locations) {

        // 遍历所有正则表达式location
        for (clcfp = pclcf->regex_locations; *clcfp; clcfp++) {

            // 记录日志，用于调试
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "test location: ~ \"%V\"", &(*clcfp)->name);

            // 执行正则表达式匹配
            n = ngx_http_regex_exec(r, (*clcfp)->regex, &r->uri);

            if (n == NGX_OK) {
                // 如果匹配成功，设置新的location配置
                r->loc_conf = (*clcfp)->loc_conf;

                /* 查找嵌套的locations */

                rc = ngx_http_core_find_location(r);

                // 返回结果，如果出错则返回NGX_ERROR，否则返回NGX_OK
                return (rc == NGX_ERROR) ? rc : NGX_OK;
            }

            if (n == NGX_DECLINED) {
                // 如果不匹配，继续下一个正则表达式
                continue;
            }

            // 如果出现错误，返回NGX_ERROR
            return NGX_ERROR;
        }
    }
#endif

    // 返回最终的查找结果
    return rc;
}


/*
 * NGX_OK       - exact match
 * NGX_DONE     - auto redirect
 * NGX_AGAIN    - inclusive match
 * NGX_DECLINED - no match
 */
/*
 * 函数: ngx_http_core_find_static_location
 * 功能: 在静态location树中查找匹配的location配置
 * 参数:
 *   r: HTTP请求结构体指针
 *   node: location树的节点指针
 * 返回值:
 *   NGX_OK: 精确匹配
 *   NGX_DONE: 自动重定向
 *   NGX_AGAIN: 包含性匹配
 *   NGX_DECLINED: 没有匹配
 * 描述:
 *   此函数通过遍历location树来查找与请求URI最匹配的location配置。
 *   它使用二叉树搜索算法来提高查找效率。函数会比较URI与每个节点的名称，
 *   根据比较结果决定是继续向左子树还是右子树搜索，或者返回匹配结果。
 *   对于包含性匹配，函数可能会递归调用自身来处理嵌套的location。
 */
static ngx_int_t
ngx_http_core_find_static_location(ngx_http_request_t *r,
    ngx_http_location_tree_node_t *node)
{
    u_char     *uri;
    size_t      len, n;
    ngx_int_t   rc, rv;

    len = r->uri.len;
    uri = r->uri.data;

    rv = NGX_DECLINED;  // 初始化返回值为"未匹配"

    for ( ;; ) {

        if (node == NULL) {
            return rv;  // 如果节点为空，返回当前的匹配结果
        }

        // 记录调试日志，显示当前正在测试的location
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "test location: \"%*s\"",
                       (size_t) node->len, node->name);

        // 确定比较长度，取URI和节点名称中较短的一个
        n = (len <= (size_t) node->len) ? len : node->len;

        // 比较URI和节点名称
        rc = ngx_filename_cmp(uri, node->name, n);

        if (rc != 0) {
            // 如果不匹配，根据比较结果选择左子树或右子树继续搜索
            node = (rc < 0) ? node->left : node->right;
            continue;
        }

        if (len > (size_t) node->len) {
            // URI长度大于节点名称长度

            if (node->inclusive) {
                // 如果是包含性匹配

                r->loc_conf = node->inclusive->loc_conf;
                rv = NGX_AGAIN;  // 设置返回值为"包含性匹配"

                // 继续搜索子树
                node = node->tree;
                uri += n;
                len -= n;

                continue;
            }

            // 如果只允许精确匹配，继续搜索右子树
            node = node->right;
            continue;
        }

        if (len == (size_t) node->len) {
            // URI长度等于节点名称长度

            if (node->exact) {
                // 如果存在精确匹配
                r->loc_conf = node->exact->loc_conf;
                return NGX_OK;  // 返回"精确匹配"
            } else {
                // 否则返回包含性匹配
                r->loc_conf = node->inclusive->loc_conf;
                return NGX_AGAIN;
            }
        }

        // URI长度小于节点名称长度
        if (len + 1 == (size_t) node->len && node->auto_redirect) {
            // 如果差一个字符且允许自动重定向

            r->loc_conf = (node->exact) ? node->exact->loc_conf:
                                          node->inclusive->loc_conf;
            rv = NGX_DONE;  // 设置返回值为"自动重定向"
        }

        // 继续搜索左子树
        node = node->left;
    }
}


void *
ngx_http_test_content_type(ngx_http_request_t *r, ngx_hash_t *types_hash)
{
    u_char      c, *lowcase;
    size_t      len;
    ngx_uint_t  i, hash;

    // 如果类型哈希表为空，返回特殊值
    if (types_hash->size == 0) {
        return (void *) 4;
    }

    // 如果请求的Content-Type长度为0，返回NULL
    if (r->headers_out.content_type.len == 0) {
        return NULL;
    }

    len = r->headers_out.content_type_len;

    // 如果Content-Type的小写形式还未生成
    if (r->headers_out.content_type_lowcase == NULL) {

        // 分配内存用于存储小写形式的Content-Type
        lowcase = ngx_pnalloc(r->pool, len);
        if (lowcase == NULL) {
            return NULL;
        }

        r->headers_out.content_type_lowcase = lowcase;

        hash = 0;

        // 将Content-Type转换为小写，并计算哈希值
        for (i = 0; i < len; i++) {
            c = ngx_tolower(r->headers_out.content_type.data[i]);
            hash = ngx_hash(hash, c);
            lowcase[i] = c;
        }

        // 存储计算得到的哈希值
        r->headers_out.content_type_hash = hash;
    }

    // 在类型哈希表中查找匹配的Content-Type
    return ngx_hash_find(types_hash, r->headers_out.content_type_hash,
                         r->headers_out.content_type_lowcase, len);
}


ngx_int_t
ngx_http_set_content_type(ngx_http_request_t *r)
{
    u_char                     c, *exten;
    ngx_str_t                 *type;
    ngx_uint_t                 i, hash;
    ngx_http_core_loc_conf_t  *clcf;

    // 如果已经设置了Content-Type，直接返回
    if (r->headers_out.content_type.len) {
        return NGX_OK;
    }

    // 获取当前location的配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 如果请求URI有扩展名
    if (r->exten.len) {

        hash = 0;

        // 遍历扩展名，计算哈希值
        for (i = 0; i < r->exten.len; i++) {
            c = r->exten.data[i];

            // 如果扩展名中包含大写字母
            if (c >= 'A' && c <= 'Z') {

                // 分配内存用于存储小写形式的扩展名
                exten = ngx_pnalloc(r->pool, r->exten.len);
                if (exten == NULL) {
                    return NGX_ERROR;
                }

                // 将扩展名转换为小写，并计算哈希值
                hash = ngx_hash_strlow(exten, r->exten.data, r->exten.len);

                // 更新扩展名为小写形式
                r->exten.data = exten;

                break;
            }

            // 累加计算哈希值
            hash = ngx_hash(hash, c);
        }

        // 根据扩展名在types_hash中查找对应的MIME类型
        type = ngx_hash_find(&clcf->types_hash, hash,
                             r->exten.data, r->exten.len);

        // 如果找到对应的MIME类型
        if (type) {
            // 设置Content-Type
            r->headers_out.content_type_len = type->len;
            r->headers_out.content_type = *type;

            return NGX_OK;
        }
    }

    // 如果没有找到对应的MIME类型，使用默认类型
    r->headers_out.content_type_len = clcf->default_type.len;
    r->headers_out.content_type = clcf->default_type;

    return NGX_OK;
}


/*
 * 函数名: ngx_http_set_exten
 * 功能: 设置请求的文件扩展名
 * 参数: r - HTTP请求结构体指针
 */
void
ngx_http_set_exten(ngx_http_request_t *r)
{
    ngx_int_t  i;

    // 初始化扩展名为空
    ngx_str_null(&r->exten);

    // 从URI的末尾向前遍历
    for (i = r->uri.len - 1; i > 1; i--) {
        // 如果找到点号，且前一个字符不是斜杠，则认为找到了扩展名
        if (r->uri.data[i] == '.' && r->uri.data[i - 1] != '/') {
            // 设置扩展名的长度和数据指针
            r->exten.len = r->uri.len - i - 1;
            r->exten.data = &r->uri.data[i + 1];
            return;
        } else if (r->uri.data[i] == '/') {
            // 如果遇到斜杠，说明没有扩展名，直接返回
            return;
        }
    }

    // 如果遍历完整个URI都没找到扩展名，返回
    return;
}


/*
 * 函数名: ngx_http_set_etag
 * 功能: 设置HTTP响应的ETag头
 * 参数: r - HTTP请求结构体指针
 * 返回值: NGX_OK 成功, NGX_ERROR 失败
 */
ngx_int_t
ngx_http_set_etag(ngx_http_request_t *r)
{
    ngx_table_elt_t           *etag;
    ngx_http_core_loc_conf_t  *clcf;

    // 获取核心模块的location配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 如果配置中禁用了ETag，直接返回
    if (!clcf->etag) {
        return NGX_OK;
    }

    // 在响应头列表中添加ETag头
    etag = ngx_list_push(&r->headers_out.headers);
    if (etag == NULL) {
        return NGX_ERROR;
    }

    // 设置ETag头的属性
    etag->hash = 1;
    etag->next = NULL;
    ngx_str_set(&etag->key, "ETag");

    // 分配内存用于存储ETag值
    etag->value.data = ngx_pnalloc(r->pool, NGX_OFF_T_LEN + NGX_TIME_T_LEN + 3);
    if (etag->value.data == NULL) {
        etag->hash = 0;
        return NGX_ERROR;
    }

    // 生成ETag值，格式为 "时间戳-内容长度"
    etag->value.len = ngx_sprintf(etag->value.data, "\"%xT-%xO\"",
                                  r->headers_out.last_modified_time,
                                  r->headers_out.content_length_n)
                      - etag->value.data;

    // 将生成的ETag设置到响应头中
    r->headers_out.etag = etag;

    return NGX_OK;
}


/**
 * 函数名: ngx_http_weak_etag
 * 功能: 将强ETag转换为弱ETag
 * 参数: r - HTTP请求结构体指针
 */
void
ngx_http_weak_etag(ngx_http_request_t *r)
{
    size_t            len;
    u_char           *p;
    ngx_table_elt_t  *etag;

    // 获取响应头中的ETag
    etag = r->headers_out.etag;

    // 如果ETag不存在，直接返回
    if (etag == NULL) {
        return;
    }

    // 如果ETag已经是弱ETag（以"W/"开头），直接返回
    if (etag->value.len > 2
        && etag->value.data[0] == 'W'
        && etag->value.data[1] == '/')
    {
        return;
    }

    // 如果ETag格式不正确（不以双引号开始），清除ETag并返回
    if (etag->value.len < 1 || etag->value.data[0] != '"') {
        r->headers_out.etag->hash = 0;
        r->headers_out.etag = NULL;
        return;
    }

    // 分配内存用于存储新的弱ETag
    p = ngx_pnalloc(r->pool, etag->value.len + 2);
    if (p == NULL) {
        // 内存分配失败，清除ETag并返回
        r->headers_out.etag->hash = 0;
        r->headers_out.etag = NULL;
        return;
    }

    // 生成新的弱ETag，格式为"W/原ETag"
    len = ngx_sprintf(p, "W/%V", &etag->value) - p;

    // 更新ETag的值
    etag->value.data = p;
    etag->value.len = len;
}


/**
 * 发送HTTP响应
 * 
 * @param r HTTP请求结构体指针
 * @param status HTTP状态码
 * @param ct 内容类型字符串指针
 * @param cv 复杂值结构体指针，用于生成响应内容
 * @return NGX_OK 成功，其他值表示错误
 */
ngx_int_t
ngx_http_send_response(ngx_http_request_t *r, ngx_uint_t status,
    ngx_str_t *ct, ngx_http_complex_value_t *cv)
{
    ngx_int_t     rc;
    ngx_str_t     val;
    ngx_buf_t    *b;
    ngx_chain_t   out;

    // 丢弃请求体
    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    // 设置响应状态码
    r->headers_out.status = status;

    // 计算复杂值
    if (ngx_http_complex_value(r, cv, &val) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 处理重定向状态码
    if (status == NGX_HTTP_MOVED_PERMANENTLY
        || status == NGX_HTTP_MOVED_TEMPORARILY
        || status == NGX_HTTP_SEE_OTHER
        || status == NGX_HTTP_TEMPORARY_REDIRECT
        || status == NGX_HTTP_PERMANENT_REDIRECT)
    {
        // 清除已有的Location头
        ngx_http_clear_location(r);

        // 添加新的Location头
        r->headers_out.location = ngx_list_push(&r->headers_out.headers);
        if (r->headers_out.location == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        r->headers_out.location->hash = 1;
        r->headers_out.location->next = NULL;
        ngx_str_set(&r->headers_out.location->key, "Location");
        r->headers_out.location->value = val;

        return status;
    }

    // 设置内容长度
    r->headers_out.content_length_n = val.len;

    // 设置内容类型
    if (ct) {
        r->headers_out.content_type_len = ct->len;
        r->headers_out.content_type = *ct;

    } else {
        if (ngx_http_set_content_type(r) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    // 分配缓冲区
    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // 设置缓冲区属性
    b->pos = val.data;
    b->last = val.data + val.len;
    b->memory = val.len ? 1 : 0;
    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;
    b->sync = (b->last_buf || b->memory) ? 0 : 1;

    // 创建输出链
    out.buf = b;
    out.next = NULL;

    // 发送响应头
    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    // 发送响应体
    return ngx_http_output_filter(r, &out);
}


/*
 * 函数名: ngx_http_send_header
 * 功能: 发送HTTP响应头
 * 参数: r - HTTP请求结构体指针
 * 返回值: NGX_OK - 成功, NGX_ERROR - 失败
 */
ngx_int_t
ngx_http_send_header(ngx_http_request_t *r)
{
    // 如果是post_action请求，直接返回成功
    if (r->post_action) {
        return NGX_OK;
    }

    // 检查头部是否已经发送，避免重复发送
    if (r->header_sent) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "header already sent");
        return NGX_ERROR;
    }

    // 如果存在错误状态，设置响应状态码并清空状态行
    if (r->err_status) {
        r->headers_out.status = r->err_status;
        r->headers_out.status_line.len = 0;
    }

    // 调用顶层头部过滤器处理并发送响应头
    return ngx_http_top_header_filter(r);
}


ngx_int_t
ngx_http_output_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t          rc;
    ngx_connection_t  *c;

    // 获取请求的连接
    c = r->connection;

    // 记录调试日志，输出URI和参数
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http output filter \"%V?%V\"", &r->uri, &r->args);

    // 调用顶层body过滤器
    rc = ngx_http_top_body_filter(r, in);

    if (rc == NGX_ERROR) {
        /* NGX_ERROR may be returned by any filter */
        // 如果返回错误，设置连接的错误标志
        c->error = 1;
    }

    // 返回过滤器的处理结果
    return rc;
}


u_char *
ngx_http_map_uri_to_path(ngx_http_request_t *r, ngx_str_t *path,
    size_t *root_length, size_t reserved)
{
    u_char                    *last;
    size_t                     alias;
    ngx_http_core_loc_conf_t  *clcf;

    // 获取当前请求的核心位置配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 获取别名配置
    alias = clcf->alias;

    // 检查别名配置的有效性
    if (alias && !r->valid_location) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "\"alias\" cannot be used in location \"%V\" "
                      "where URI was rewritten", &clcf->name);
        return NULL;
    }

    // 处理静态根路径的情况
    if (clcf->root_lengths == NULL) {

        *root_length = clcf->root.len;

        // 计算路径长度
        path->len = clcf->root.len + reserved + r->uri.len - alias + 1;

        // 分配内存
        path->data = ngx_pnalloc(r->pool, path->len);
        if (path->data == NULL) {
            return NULL;
        }

        // 复制根路径
        last = ngx_copy(path->data, clcf->root.data, clcf->root.len);

    } else {
        // 处理动态根路径的情况

        // 计算保留长度
        if (alias == NGX_MAX_SIZE_T_VALUE) {
            reserved += r->add_uri_to_alias ? r->uri.len + 1 : 1;
        } else {
            reserved += r->uri.len - alias + 1;
        }

        // 运行脚本获取根路径
        if (ngx_http_script_run(r, path, clcf->root_lengths->elts, reserved,
                                clcf->root_values->elts)
            == NULL)
        {
            return NULL;
        }

        // 获取完整路径名
        if (ngx_get_full_name(r->pool, (ngx_str_t *) &ngx_cycle->prefix, path)
            != NGX_OK)
        {
            return NULL;
        }

        *root_length = path->len - reserved;
        last = path->data + *root_length;

        // 处理特殊的别名情况
        if (alias == NGX_MAX_SIZE_T_VALUE) {
            if (!r->add_uri_to_alias) {
                *last = '\0';
                return last;
            }

            alias = 0;
        }
    }

    // 将URI复制到路径末尾
    last = ngx_copy(last, r->uri.data + alias, r->uri.len - alias);
    *last = '\0';

    return last;
}


/*
 * 函数：ngx_http_auth_basic_user
 * 功能：解析HTTP基本认证信息
 * 参数：r - HTTP请求结构体
 * 返回值：NGX_OK - 成功解析认证信息
 *         NGX_DECLINED - 无认证信息或格式不正确
 *         NGX_ERROR - 内存分配失败
 */
ngx_int_t
ngx_http_auth_basic_user(ngx_http_request_t *r)
{
    ngx_str_t   auth, encoded;
    ngx_uint_t  len;

    // 检查用户名是否已设置
    if (r->headers_in.user.len == 0 && r->headers_in.user.data != NULL) {
        return NGX_DECLINED;
    }

    // 检查是否存在Authorization头
    if (r->headers_in.authorization == NULL) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    // 获取Authorization头的值
    encoded = r->headers_in.authorization->value;

    // 检查Authorization头是否以"Basic "开头
    if (encoded.len < sizeof("Basic ") - 1
        || ngx_strncasecmp(encoded.data, (u_char *) "Basic ",
                           sizeof("Basic ") - 1)
           != 0)
    {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    // 移除"Basic "前缀
    encoded.len -= sizeof("Basic ") - 1;
    encoded.data += sizeof("Basic ") - 1;

    // 去除前导空格
    while (encoded.len && encoded.data[0] == ' ') {
        encoded.len--;
        encoded.data++;
    }

    // 检查编码后的认证信息是否为空
    if (encoded.len == 0) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    // 分配内存用于存储解码后的认证信息
    auth.len = ngx_base64_decoded_length(encoded.len);
    auth.data = ngx_pnalloc(r->pool, auth.len + 1);
    if (auth.data == NULL) {
        return NGX_ERROR;
    }

    // Base64解码
    if (ngx_decode_base64(&auth, &encoded) != NGX_OK) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    // 添加字符串结束符
    auth.data[auth.len] = '\0';

    // 查找用户名和密码的分隔符':'
    for (len = 0; len < auth.len; len++) {
        if (auth.data[len] == ':') {
            break;
        }
    }

    // 检查用户名和密码格式是否正确
    if (len == 0 || len == auth.len) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    // 设置用户名和密码
    r->headers_in.user.len = len;
    r->headers_in.user.data = auth.data;
    r->headers_in.passwd.len = auth.len - len - 1;
    r->headers_in.passwd.data = &auth.data[len + 1];

    return NGX_OK;
}


#if (NGX_HTTP_GZIP)

ngx_int_t
ngx_http_gzip_ok(ngx_http_request_t *r)
{
    time_t                     date, expires;
    ngx_uint_t                 p;
    ngx_table_elt_t           *e, *d, *ae, *cc;
    ngx_http_core_loc_conf_t  *clcf;

    // 标记请求已经进行过gzip测试
    r->gzip_tested = 1;

    // 只处理主请求，子请求不进行gzip压缩
    if (r != r->main) {
        return NGX_DECLINED;
    }

    // 获取Accept-Encoding头
    ae = r->headers_in.accept_encoding;
    if (ae == NULL) {
        return NGX_DECLINED;
    }

    // 检查Accept-Encoding头的长度是否足够
    if (ae->value.len < sizeof("gzip") - 1) {
        return NGX_DECLINED;
    }

    /*
     * 测试最常见的情况 "gzip,...":
     *   MSIE:    "gzip, deflate"
     *   Firefox: "gzip,deflate"
     *   Chrome:  "gzip,deflate,sdch"
     *   Safari:  "gzip, deflate"
     *   Opera:   "gzip, deflate"
     */

    // 检查Accept-Encoding头是否包含gzip
    if (ngx_memcmp(ae->value.data, "gzip,", 5) != 0
        && ngx_http_gzip_accept_encoding(&ae->value) != NGX_OK)
    {
        return NGX_DECLINED;
    }

    // 获取location配置
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    // 检查是否为MSIE6浏览器，并且配置禁用了MSIE6的gzip
    if (r->headers_in.msie6 && clcf->gzip_disable_msie6) {
        return NGX_DECLINED;
    }

    // 检查HTTP版本是否满足要求
    if (r->http_version < clcf->gzip_http_version) {
        return NGX_DECLINED;
    }

    // 如果没有Via头，直接允许gzip
    if (r->headers_in.via == NULL) {
        goto ok;
    }

    // 获取gzip代理配置
    p = clcf->gzip_proxied;

    // 如果禁用了代理请求的gzip
    if (p & NGX_HTTP_GZIP_PROXIED_OFF) {
        return NGX_DECLINED;
    }

    // 如果允许所有代理请求的gzip
    if (p & NGX_HTTP_GZIP_PROXIED_ANY) {
        goto ok;
    }

    // 如果请求包含Authorization头，并且配置允许带认证的请求使用gzip
    if (r->headers_in.authorization && (p & NGX_HTTP_GZIP_PROXIED_AUTH)) {
        goto ok;
    }

    // 获取Expires头
    e = r->headers_out.expires;

    if (e) {
        // 如果配置不允许已过期的响应使用gzip
        if (!(p & NGX_HTTP_GZIP_PROXIED_EXPIRED)) {
            return NGX_DECLINED;
        }

        // 解析Expires时间
        expires = ngx_parse_http_time(e->value.data, e->value.len);
        if (expires == NGX_ERROR) {
            return NGX_DECLINED;
        }

        // 获取Date头
        d = r->headers_out.date;

        if (d) {
            // 解析Date时间
            date = ngx_parse_http_time(d->value.data, d->value.len);
            if (date == NGX_ERROR) {
                return NGX_DECLINED;
            }
        } else {
            // 如果没有Date头，使用当前时间
            date = ngx_time();
        }

        // 如果响应已过期，允许使用gzip
        if (expires < date) {
            goto ok;
        }

        return NGX_DECLINED;
    }

    // 获取Cache-Control头
    cc = r->headers_out.cache_control;

    if (cc) {
        // 检查各种Cache-Control指令
        if ((p & NGX_HTTP_GZIP_PROXIED_NO_CACHE)
            && ngx_http_parse_multi_header_lines(r, cc, &ngx_http_gzip_no_cache,
                                                 NULL)
               != NULL)
        {
            goto ok;
        }

        if ((p & NGX_HTTP_GZIP_PROXIED_NO_STORE)
            && ngx_http_parse_multi_header_lines(r, cc, &ngx_http_gzip_no_store,
                                                 NULL)
               != NULL)
        {
            goto ok;
        }

        if ((p & NGX_HTTP_GZIP_PROXIED_PRIVATE)
            && ngx_http_parse_multi_header_lines(r, cc, &ngx_http_gzip_private,
                                                 NULL)
               != NULL)
        {
            goto ok;
        }

        return NGX_DECLINED;
    }

    // 检查Last-Modified头
    if ((p & NGX_HTTP_GZIP_PROXIED_NO_LM) && r->headers_out.last_modified) {
        return NGX_DECLINED;
    }

    // 检查ETag头
    if ((p & NGX_HTTP_GZIP_PROXIED_NO_ETAG) && r->headers_out.etag) {
        return NGX_DECLINED;
    }

ok:

#if (NGX_PCRE)
    // 检查是否需要根据User-Agent禁用gzip
    if (clcf->gzip_disable && r->headers_in.user_agent) {
        if (ngx_regex_exec_array(clcf->gzip_disable,
                                 &r->headers_in.user_agent->value,
                                 r->connection->log)
            != NGX_DECLINED)
        {
            return NGX_DECLINED;
        }
    }

#endif

    // 标记请求可以使用gzip
    r->gzip_ok = 1;

    return NGX_OK;
}


/*
 * gzip is enabled for the following quantities:
 *     "gzip; q=0.001" ... "gzip; q=1.000"
 * gzip is disabled for the following quantities:
 *     "gzip; q=0" ... "gzip; q=0.000", and for any invalid cases
 */

/*
 * 函数名: ngx_http_gzip_accept_encoding
 * 功能: 检查HTTP请求头中的Accept-Encoding字段是否支持gzip压缩
 * 参数: ae - 指向Accept-Encoding头值的ngx_str_t结构指针
 * 返回值: NGX_OK 表示支持gzip，NGX_DECLINED 表示不支持
 */
static ngx_int_t
ngx_http_gzip_accept_encoding(ngx_str_t *ae)
{
    u_char  *p, *start, *last;

    start = ae->data;
    last = start + ae->len;

    // 在Accept-Encoding头中查找"gzip"
    for ( ;; ) {
        p = ngx_strcasestrn(start, "gzip", 4 - 1);
        if (p == NULL) {
            return NGX_DECLINED;  // 未找到"gzip"，不支持gzip压缩
        }

        // 确保"gzip"是独立的词，不是其他词的一部分
        if (p == start || (*(p - 1) == ',' || *(p - 1) == ' ')) {
            break;
        }

        start = p + 4;
    }

    p += 4;  // 移动指针到"gzip"之后

    // 检查"gzip"后面的字符
    while (p < last) {
        switch (*p++) {
        case ',':
            return NGX_OK;  // 遇到逗号，表示gzip被接受
        case ';':
            goto quantity;  // 遇到分号，可能有q值，跳转到quantity处理
        case ' ':
            continue;  // 忽略空格
        default:
            return NGX_DECLINED;  // 其他字符，格式不正确，不支持gzip
        }
    }

    return NGX_OK;  // 到达字符串末尾，gzip被接受

quantity:
    // 处理q值
    while (p < last) {
        switch (*p++) {
        case 'q':
        case 'Q':
            goto equal;  // 找到q或Q，跳转到equal处理
        case ' ':
            continue;  // 忽略空格
        default:
            return NGX_DECLINED;  // 其他字符，格式不正确，不支持gzip
        }
    }

    return NGX_OK;  // 到达字符串末尾，没有q值，gzip被接受

equal:
    // 检查等号和q值
    if (p + 2 > last || *p++ != '=') {
        return NGX_DECLINED;  // 格式不正确，不支持gzip
    }

    // 使用ngx_http_gzip_quantity函数解析q值
    if (ngx_http_gzip_quantity(p, last) == 0) {
        return NGX_DECLINED;  // q值为0或格式不正确，不支持gzip
    }

    return NGX_OK;  // q值有效且非0，支持gzip
}


/*
 * 函数名: ngx_http_gzip_quantity
 * 功能: 解析HTTP头中gzip压缩质量参数的值
 * 参数:
 *   p: 指向质量参数值开始的指针
 *   last: 指向参数字符串结束的指针
 * 返回值: 解析后的质量值（0-100），如果解析失败返回0
 */
static ngx_uint_t
ngx_http_gzip_quantity(u_char *p, u_char *last)
{
    u_char      c;
    ngx_uint_t  n, q;

    c = *p++;

    // 检查第一个字符是否为'0'或'1'
    if (c != '0' && c != '1') {
        return 0;
    }

    // 将字符转换为整数值（0或100）
    q = (c - '0') * 100;

    // 如果已到字符串末尾，直接返回结果
    if (p == last) {
        return q;
    }

    c = *p++;

    // 如果下一个字符是逗号或空格，返回当前结果
    if (c == ',' || c == ' ') {
        return q;
    }

    // 如果不是小数点，格式错误，返回0
    if (c != '.') {
        return 0;
    }

    n = 0;  // 用于计数小数点后的位数

    // 解析小数点后的数字
    while (p < last) {
        c = *p++;

        // 遇到逗号或空格，结束解析
        if (c == ',' || c == ' ') {
            break;
        }

        // 如果是数字，累加到结果中
        if (c >= '0' && c <= '9') {
            q += c - '0';
            n++;
            continue;
        }

        // 遇到非数字字符，格式错误，返回0
        return 0;
    }

    // 检查最终结果是否有效（不超过100且小数点后不超过3位）
    if (q > 100 || n > 3) {
        return 0;
    }

    return q;
}

#endif


ngx_int_t
ngx_http_subrequest(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args, ngx_http_request_t **psr,
    ngx_http_post_subrequest_t *ps, ngx_uint_t flags)
{
    ngx_time_t                    *tp;
    ngx_connection_t              *c;
    ngx_http_request_t            *sr;
    ngx_http_core_srv_conf_t      *cscf;
    ngx_http_postponed_request_t  *pr, *p;

    // 检查子请求数量是否已达到限制
    if (r->subrequests == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "subrequests cycle while processing \"%V\"", uri);
        return NGX_ERROR;
    }

    // 检查请求引用计数是否超出限制
    if (r->main->count >= 65535 - 1000) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "request reference counter overflow "
                      "while processing \"%V\"", uri);
        return NGX_ERROR;
    }

    // 检查是否存在嵌套的内存子请求
    if (r->subrequest_in_memory) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "nested in-memory subrequest \"%V\"", uri);
        return NGX_ERROR;
    }

    // 为子请求分配内存
    sr = ngx_pcalloc(r->pool, sizeof(ngx_http_request_t));
    if (sr == NULL) {
        return NGX_ERROR;
    }

    sr->signature = NGX_HTTP_MODULE;

    c = r->connection;
    sr->connection = c;

    // 为子请求的上下文分配内存
    sr->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (sr->ctx == NULL) {
        return NGX_ERROR;
    }

    // 初始化子请求的输出头部列表
    if (ngx_list_init(&sr->headers_out.headers, r->pool, 20,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 初始化子请求的尾部列表
    if (ngx_list_init(&sr->headers_out.trailers, r->pool, 4,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 设置子请求的配置
    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    sr->main_conf = cscf->ctx->main_conf;
    sr->srv_conf = cscf->ctx->srv_conf;
    sr->loc_conf = cscf->ctx->loc_conf;

    sr->pool = r->pool;

    // 复制输入头部
    sr->headers_in = r->headers_in;

    // 清除一些响应头部
    ngx_http_clear_content_length(sr);
    ngx_http_clear_accept_ranges(sr);
    ngx_http_clear_last_modified(sr);

    sr->request_body = r->request_body;

#if (NGX_HTTP_V2)
    sr->stream = r->stream;
#endif

    // 设置子请求的方法和HTTP版本
    sr->method = NGX_HTTP_GET;
    sr->http_version = r->http_version;

    sr->request_line = r->request_line;
    sr->uri = *uri;

    if (args) {
        sr->args = *args;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http subrequest \"%V?%V\"", uri, &sr->args);

    // 设置子请求的标志
    sr->subrequest_in_memory = (flags & NGX_HTTP_SUBREQUEST_IN_MEMORY) != 0;
    sr->waited = (flags & NGX_HTTP_SUBREQUEST_WAITED) != 0;
    sr->background = (flags & NGX_HTTP_SUBREQUEST_BACKGROUND) != 0;

    // 复制一些请求相关的信息
    sr->unparsed_uri = r->unparsed_uri;
    sr->method_name = ngx_http_core_get_method;
    sr->http_protocol = r->http_protocol;
    sr->schema = r->schema;

    ngx_http_set_exten(sr);

    // 设置子请求的关系
    sr->main = r->main;
    sr->parent = r;
    sr->post_subrequest = ps;
    sr->read_event_handler = ngx_http_request_empty_handler;
    sr->write_event_handler = ngx_http_handler;

    sr->variables = r->variables;

    sr->log_handler = r->log_handler;

    if (sr->subrequest_in_memory) {
        sr->filter_need_in_memory = 1;
    }

    // 处理非后台子请求
    if (!sr->background) {
        if (c->data == r && r->postponed == NULL) {
            c->data = sr;
        }

        // 创建并添加postponed请求
        pr = ngx_palloc(r->pool, sizeof(ngx_http_postponed_request_t));
        if (pr == NULL) {
            return NGX_ERROR;
        }

        pr->request = sr;
        pr->out = NULL;
        pr->next = NULL;

        if (r->postponed) {
            for (p = r->postponed; p->next; p = p->next) { /* void */ }
            p->next = pr;

        } else {
            r->postponed = pr;
        }
    }

    sr->internal = 1;

    // 复制一些请求标志
    sr->discard_body = r->discard_body;
    sr->expect_tested = 1;
    sr->main_filter_need_in_memory = r->main_filter_need_in_memory;

    sr->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
    sr->subrequests = r->subrequests - 1;

    // 记录子请求的开始时间
    tp = ngx_timeofday();
    sr->start_sec = tp->sec;
    sr->start_msec = tp->msec;

    r->main->count++;

    *psr = sr;

    // 处理克隆标志
    if (flags & NGX_HTTP_SUBREQUEST_CLONE) {
        sr->method = r->method;
        sr->method_name = r->method_name;
        sr->loc_conf = r->loc_conf;
        sr->valid_location = r->valid_location;
        sr->valid_unparsed_uri = r->valid_unparsed_uri;
        sr->content_handler = r->content_handler;
        sr->phase_handler = r->phase_handler;
        sr->write_event_handler = ngx_http_core_run_phases;

#if (NGX_PCRE)
        sr->ncaptures = r->ncaptures;
        sr->captures = r->captures;
        sr->captures_data = r->captures_data;
        sr->realloc_captures = 1;
        r->realloc_captures = 1;
#endif

        ngx_http_update_location_config(sr);
    }

    // 发布子请求
    return ngx_http_post_request(sr, NULL);
}


/*
 * 函数名: ngx_http_internal_redirect
 * 功能: 处理内部重定向请求
 * 参数:
 *   r: HTTP请求结构体指针
 *   uri: 重定向的目标URI
 *   args: 重定向的参数（可选）
 * 返回值: NGX_DONE 表示重定向已处理
 */
ngx_int_t
ngx_http_internal_redirect(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args)
{
    ngx_http_core_srv_conf_t  *cscf;

    // 减少URI变更次数计数
    r->uri_changes--;

    // 检查是否达到最大重定向次数
    if (r->uri_changes == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while internally redirecting to \"%V\"", uri);

        r->main->count++;
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }

    // 更新请求的URI
    r->uri = *uri;

    // 更新请求的参数
    if (args) {
        r->args = *args;
    } else {
        ngx_str_null(&r->args);
    }

    // 记录调试日志
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "internal redirect: \"%V?%V\"", uri, &r->args);

    // 设置请求的扩展名
    ngx_http_set_exten(r);

    /* 清除模块上下文 */
    ngx_memzero(r->ctx, sizeof(void *) * ngx_http_max_module);

    // 获取核心服务器配置
    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    r->loc_conf = cscf->ctx->loc_conf;

    // 更新location配置
    ngx_http_update_location_config(r);

#if (NGX_HTTP_CACHE)
    // 清除缓存
    r->cache = NULL;
#endif

    // 设置内部请求标志
    r->internal = 1;
    r->valid_unparsed_uri = 0;
    r->add_uri_to_alias = 0;
    r->main->count++;

    // 重新处理请求
    ngx_http_handler(r);

    return NGX_DONE;
}


/*
 * 函数名: ngx_http_named_location
 * 功能: 处理命名location的重定向
 * 参数:
 *   r: HTTP请求结构体
 *   name: 命名location的名称
 * 返回值: NGX_DONE
 */
ngx_int_t
ngx_http_named_location(ngx_http_request_t *r, ngx_str_t *name)
{
    ngx_http_core_srv_conf_t    *cscf;
    ngx_http_core_loc_conf_t   **clcfp;
    ngx_http_core_main_conf_t   *cmcf;

    // 增加主请求计数，减少URI变更次数
    r->main->count++;
    r->uri_changes--;

    // 检查是否存在重定向循环
    if (r->uri_changes == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while redirect to named location \"%V\"", name);

        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }

    // 检查URI是否为空
    if (r->uri.len == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "empty URI in redirect to named location \"%V\"", name);

        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }

    // 获取HTTP核心模块的服务器配置
    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

    // 如果存在命名locations，则遍历查找匹配的location
    if (cscf->named_locations) {

        for (clcfp = cscf->named_locations; *clcfp; clcfp++) {

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "test location: \"%V\"", &(*clcfp)->name);

            // 比较location名称
            if (name->len != (*clcfp)->name.len
                || ngx_strncmp(name->data, (*clcfp)->name.data, name->len) != 0)
            {
                continue;
            }

            ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "using location: %V \"%V?%V\"",
                           name, &r->uri, &r->args);

            // 设置内部请求标志和相关参数
            r->internal = 1;
            r->content_handler = NULL;
            r->uri_changed = 0;
            r->loc_conf = (*clcfp)->loc_conf;

            // 清除模块上下文
            ngx_memzero(r->ctx, sizeof(void *) * ngx_http_max_module);

            // 更新location配置
            ngx_http_update_location_config(r);

            // 获取HTTP核心模块的主配置
            cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

            // 设置阶段处理器为location rewrite阶段
            r->phase_handler = cmcf->phase_engine.location_rewrite_index;

            // 设置写事件处理函数并运行阶段处理器
            r->write_event_handler = ngx_http_core_run_phases;
            ngx_http_core_run_phases(r);

            return NGX_DONE;
        }
    }

    // 如果未找到匹配的命名location，记录错误并返回500错误
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "could not find named location \"%V\"", name);

    ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);

    return NGX_DONE;
}


// 添加HTTP请求清理回调函数
ngx_http_cleanup_t *
ngx_http_cleanup_add(ngx_http_request_t *r, size_t size)
{
    ngx_http_cleanup_t  *cln;

    // 确保使用主请求
    r = r->main;

    // 分配清理回调结构体内存
    cln = ngx_palloc(r->pool, sizeof(ngx_http_cleanup_t));
    if (cln == NULL) {
        return NULL;
    }

    // 如果指定了大小，为数据分配内存
    if (size) {
        cln->data = ngx_palloc(r->pool, size);
        if (cln->data == NULL) {
            return NULL;
        }

    } else {
        cln->data = NULL;
    }

    // 初始化回调函数指针和下一个清理节点
    cln->handler = NULL;
    cln->next = r->cleanup;

    // 将新的清理回调添加到请求的清理链表头部
    r->cleanup = cln;

    // 记录调试日志
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http cleanup add: %p", cln);

    return cln;
}


ngx_int_t
ngx_http_set_disable_symlinks(ngx_http_request_t *r,
    ngx_http_core_loc_conf_t *clcf, ngx_str_t *path, ngx_open_file_info_t *of)
{
#if (NGX_HAVE_OPENAT)
    u_char     *p;
    ngx_str_t   from;

    // 设置禁用符号链接的选项
    of->disable_symlinks = clcf->disable_symlinks;

    // 如果没有设置禁用符号链接的起始路径，直接返回
    if (clcf->disable_symlinks_from == NULL) {
        return NGX_OK;
    }

    // 获取禁用符号链接的起始路径
    if (ngx_http_complex_value(r, clcf->disable_symlinks_from, &from)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    // 检查起始路径是否有效
    if (from.len == 0
        || from.len > path->len
        || ngx_memcmp(path->data, from.data, from.len) != 0)
    {
        return NGX_OK;
    }

    // 如果起始路径等于完整路径，关闭符号链接禁用
    if (from.len == path->len) {
        of->disable_symlinks = NGX_DISABLE_SYMLINKS_OFF;
        return NGX_OK;
    }

    // 检查起始路径后的字符
    p = path->data + from.len;

    // 如果紧跟着是斜杠，设置禁用符号链接的起始位置
    if (*p == '/') {
        of->disable_symlinks_from = from.len;
        return NGX_OK;
    }

    p--;

    // 如果前一个字符是斜杠，设置禁用符号链接的起始位置
    if (*p == '/') {
        of->disable_symlinks_from = from.len - 1;
    }
#endif

    return NGX_OK;
}


ngx_int_t
ngx_http_get_forwarded_addr(ngx_http_request_t *r, ngx_addr_t *addr,
    ngx_table_elt_t *headers, ngx_str_t *value, ngx_array_t *proxies,
    int recursive)
{
    ngx_int_t         rc;
    ngx_uint_t        found;
    ngx_table_elt_t  *h, *next;

    // 如果没有提供headers，直接调用内部函数处理
    if (headers == NULL) {
        return ngx_http_get_forwarded_addr_internal(r, addr, value->data,
                                                    value->len, proxies,
                                                    recursive);
    }

    // 反转headers的顺序
    for (h = headers, headers = NULL; h; h = next) {
        next = h->next;
        h->next = headers;
        headers = h;
    }

    // 以相反的顺序遍历所有headers

    rc = NGX_DECLINED;
    found = 0;

    for (h = headers; h; h = h->next) {
        // 对每个header调用内部函数处理
        rc = ngx_http_get_forwarded_addr_internal(r, addr, h->value.data,
                                                  h->value.len, proxies,
                                                  recursive);

        // 如果不是递归模式，处理一次后就退出
        if (!recursive) {
            break;
        }

        // 如果处理失败但已找到有效地址，设置为完成状态并退出
        if (rc == NGX_DECLINED && found) {
            rc = NGX_DONE;
            break;
        }

        // 如果处理出错，退出循环
        if (rc != NGX_OK) {
            break;
        }

        found = 1;
    }

    // 恢复headers的原始顺序
    for (h = headers, headers = NULL; h; h = next) {
        next = h->next;
        h->next = headers;
        headers = h;
    }

    return rc;
}


static ngx_int_t
ngx_http_get_forwarded_addr_internal(ngx_http_request_t *r, ngx_addr_t *addr,
    u_char *xff, size_t xfflen, ngx_array_t *proxies, int recursive)
{
    u_char      *p;
    ngx_addr_t   paddr;
    ngx_uint_t   found;

    found = 0;

    do {
        // 检查当前地址是否在受信任的代理列表中
        if (ngx_cidr_match(addr->sockaddr, proxies) != NGX_OK) {
            return found ? NGX_DONE : NGX_DECLINED;
        }

        // 从后向前跳过空格和逗号
        for (p = xff + xfflen - 1; p > xff; p--, xfflen--) {
            if (*p != ' ' && *p != ',') {
                break;
            }
        }

        // 从后向前查找IP地址的起始位置
        for ( /* void */ ; p > xff; p--) {
            if (*p == ' ' || *p == ',') {
                p++;
                break;
            }
        }

        // 解析找到的IP地址
        if (ngx_parse_addr_port(r->pool, &paddr, p, xfflen - (p - xff))
            != NGX_OK)
        {
            return found ? NGX_DONE : NGX_DECLINED;
        }

        // 更新地址信息
        *addr = paddr;
        found = 1;
        xfflen = p - 1 - xff;

    } while (recursive && p > xff);  // 如果是递归模式且还有更多IP，继续处理

    return NGX_OK;
}


ngx_int_t
ngx_http_link_multi_headers(ngx_http_request_t *r)
{
    ngx_uint_t        i, j;
    ngx_list_part_t  *part, *ppart;
    ngx_table_elt_t  *header, *pheader, **ph;

    // 如果已经链接过多值头部，直接返回
    if (r->headers_in.multi_linked) {
        return NGX_OK;
    }

    r->headers_in.multi_linked = 1;

    part = &r->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* void */; i++) {
        // 遍历所有头部
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        header[i].next = NULL;

        // 搜索具有相同名称的先前头部
        // 如果找到，将它们链接在一起

        ppart = &r->headers_in.headers.part;
        pheader = ppart->elts;

        for (j = 0; /* void */; j++) {
            if (j >= ppart->nelts) {
                if (ppart->next == NULL) {
                    break;
                }

                ppart = ppart->next;
                pheader = ppart->elts;
                j = 0;
            }

            if (part == ppart && i == j) {
                break;
            }

            // 比较头部名称
            if (header[i].key.len == pheader[j].key.len
                && ngx_strncasecmp(header[i].key.data, pheader[j].key.data,
                                   header[i].key.len)
                   == 0)
            {
                // 找到相同名称的头部，将当前头部链接到末尾
                ph = &pheader[j].next;
                while (*ph) { ph = &(*ph)->next; }
                *ph = &header[i];

                r->headers_in.multi = 1;

                break;
            }
        }
    }

    return NGX_OK;
}


static char *
ngx_http_core_server(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    char                        *rv;
    void                        *mconf;
    size_t                       len;
    u_char                      *p;
    ngx_uint_t                   i;
    ngx_conf_t                   pcf;
    ngx_http_module_t           *module;
    struct sockaddr_in          *sin;
    ngx_http_conf_ctx_t         *ctx, *http_ctx;
    ngx_http_listen_opt_t        lsopt;
    ngx_http_core_srv_conf_t    *cscf, **cscfp;
    ngx_http_core_main_conf_t   *cmcf;

    // 为新的HTTP上下文分配内存
    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    http_ctx = cf->ctx;
    ctx->main_conf = http_ctx->main_conf;

    /* 为server{}的srv_conf分配内存 */

    ctx->srv_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->srv_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    /* 为server{}的loc_conf分配内存 */

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    // 遍历所有HTTP模块，创建它们的srv_conf和loc_conf
    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[i]->ctx;

        if (module->create_srv_conf) {
            mconf = module->create_srv_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->srv_conf[cf->cycle->modules[i]->ctx_index] = mconf;
        }

        if (module->create_loc_conf) {
            mconf = module->create_loc_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->loc_conf[cf->cycle->modules[i]->ctx_index] = mconf;
        }
    }


    /* 设置server配置上下文 */

    cscf = ctx->srv_conf[ngx_http_core_module.ctx_index];
    cscf->ctx = ctx;


    cmcf = ctx->main_conf[ngx_http_core_module.ctx_index];

    // 将新的server配置添加到主配置的servers数组中
    cscfp = ngx_array_push(&cmcf->servers);
    if (cscfp == NULL) {
        return NGX_CONF_ERROR;
    }

    *cscfp = cscf;


    /* 解析server{}块内的配置 */

    pcf = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NGX_HTTP_SRV_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    // 如果解析成功但没有设置监听端口，则添加默认监听配置
    if (rv == NGX_CONF_OK && !cscf->listen) {
        ngx_memzero(&lsopt, sizeof(ngx_http_listen_opt_t));

        p = ngx_pcalloc(cf->pool, sizeof(struct sockaddr_in));
        if (p == NULL) {
            return NGX_CONF_ERROR;
        }

        lsopt.sockaddr = (struct sockaddr *) p;

        sin = (struct sockaddr_in *) p;

        sin->sin_family = AF_INET;
#if (NGX_WIN32)
        sin->sin_port = htons(80);
#else
        sin->sin_port = htons((getuid() == 0) ? 80 : 8000);
#endif
        sin->sin_addr.s_addr = INADDR_ANY;

        lsopt.socklen = sizeof(struct sockaddr_in);

        lsopt.backlog = NGX_LISTEN_BACKLOG;
        lsopt.type = SOCK_STREAM;
        lsopt.rcvbuf = -1;
        lsopt.sndbuf = -1;
#if (NGX_HAVE_SETFIB)
        lsopt.setfib = -1;
#endif
#if (NGX_HAVE_TCP_FASTOPEN)
        lsopt.fastopen = -1;
#endif
        lsopt.wildcard = 1;

        len = NGX_INET_ADDRSTRLEN + sizeof(":65535") - 1;

        p = ngx_pnalloc(cf->pool, len);
        if (p == NULL) {
            return NGX_CONF_ERROR;
        }

        lsopt.addr_text.data = p;
        lsopt.addr_text.len = ngx_sock_ntop(lsopt.sockaddr, lsopt.socklen, p,
                                            len, 1);

        // 添加默认监听配置
        if (ngx_http_add_listen(cf, cscf, &lsopt) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return rv;
}


static char *
ngx_http_core_location(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    char                      *rv;
    u_char                    *mod;
    size_t                     len;
    ngx_str_t                 *value, *name;
    ngx_uint_t                 i;
    ngx_conf_t                 save;
    ngx_http_module_t         *module;
    ngx_http_conf_ctx_t       *ctx, *pctx;
    ngx_http_core_loc_conf_t  *clcf, *pclcf;

    // 为新的location块分配配置上下文
    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    pctx = cf->ctx;
    ctx->main_conf = pctx->main_conf;
    ctx->srv_conf = pctx->srv_conf;

    // 为每个HTTP模块分配location配置
    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    // 遍历所有HTTP模块，创建它们的location配置
    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[i]->ctx;

        if (module->create_loc_conf) {
            ctx->loc_conf[cf->cycle->modules[i]->ctx_index] =
                                                   module->create_loc_conf(cf);
            if (ctx->loc_conf[cf->cycle->modules[i]->ctx_index] == NULL) {
                return NGX_CONF_ERROR;
            }
        }
    }

    // 获取核心模块的location配置
    clcf = ctx->loc_conf[ngx_http_core_module.ctx_index];
    clcf->loc_conf = ctx->loc_conf;

    value = cf->args->elts;

    // 解析location指令的参数
    if (cf->args->nelts == 3) {

        len = value[1].len;
        mod = value[1].data;
        name = &value[2];

        // 处理不同类型的location匹配
        if (len == 1 && mod[0] == '=') {
            // 精确匹配
            clcf->name = *name;
            clcf->exact_match = 1;

        } else if (len == 2 && mod[0] == '^' && mod[1] == '~') {
            // 前缀匹配，不使用正则
            clcf->name = *name;
            clcf->noregex = 1;

        } else if (len == 1 && mod[0] == '~') {
            // 区分大小写的正则匹配
            if (ngx_http_core_regex_location(cf, clcf, name, 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else if (len == 2 && mod[0] == '~' && mod[1] == '*') {
            // 不区分大小写的正则匹配
            if (ngx_http_core_regex_location(cf, clcf, name, 1) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid location modifier \"%V\"", &value[1]);
            return NGX_CONF_ERROR;
        }

    } else {
        // 处理没有修饰符的location
        name = &value[1];

        if (name->data[0] == '=') {
            // 精确匹配
            clcf->name.len = name->len - 1;
            clcf->name.data = name->data + 1;
            clcf->exact_match = 1;

        } else if (name->data[0] == '^' && name->data[1] == '~') {
            // 前缀匹配，不使用正则
            clcf->name.len = name->len - 2;
            clcf->name.data = name->data + 2;
            clcf->noregex = 1;

        } else if (name->data[0] == '~') {
            // 正则匹配
            name->len--;
            name->data++;

            if (name->data[0] == '*') {
                // 不区分大小写的正则匹配
                name->len--;
                name->data++;

                if (ngx_http_core_regex_location(cf, clcf, name, 1) != NGX_OK) {
                    return NGX_CONF_ERROR;
                }

            } else {
                // 区分大小写的正则匹配
                if (ngx_http_core_regex_location(cf, clcf, name, 0) != NGX_OK) {
                    return NGX_CONF_ERROR;
                }
            }

        } else {
            // 普通前缀匹配
            clcf->name = *name;

            if (name->data[0] == '@') {
                clcf->named = 1;
            }
        }
    }

    pclcf = pctx->loc_conf[ngx_http_core_module.ctx_index];

    // 处理嵌套的location
    if (cf->cmd_type == NGX_HTTP_LOC_CONF) {

        // 检查嵌套location的合法性
        if (pclcf->exact_match) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" cannot be inside "
                               "the exact location \"%V\"",
                               &clcf->name, &pclcf->name);
            return NGX_CONF_ERROR;
        }

        if (pclcf->named) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" cannot be inside "
                               "the named location \"%V\"",
                               &clcf->name, &pclcf->name);
            return NGX_CONF_ERROR;
        }

        if (clcf->named) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "named location \"%V\" can be "
                               "on the server level only",
                               &clcf->name);
            return NGX_CONF_ERROR;
        }

        len = pclcf->name.len;

        // 检查嵌套location的路径是否合法
#if (NGX_PCRE)
        if (clcf->regex == NULL
            && ngx_filename_cmp(clcf->name.data, pclcf->name.data, len) != 0)
#else
        if (ngx_filename_cmp(clcf->name.data, pclcf->name.data, len) != 0)
#endif
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" is outside location \"%V\"",
                               &clcf->name, &pclcf->name);
            return NGX_CONF_ERROR;
        }
    }

    // 将新的location配置添加到父location的locations列表中
    if (ngx_http_add_location(cf, &pclcf->locations, clcf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    // 保存当前配置上下文，并设置新的上下文
    save = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NGX_HTTP_LOC_CONF;

    // 解析location块内的配置指令
    rv = ngx_conf_parse(cf, NULL);

    // 恢复原来的配置上下文
    *cf = save;

    return rv;
}


static ngx_int_t
ngx_http_core_regex_location(ngx_conf_t *cf, ngx_http_core_loc_conf_t *clcf,
    ngx_str_t *regex, ngx_uint_t caseless)
{
#if (NGX_PCRE)
    ngx_regex_compile_t  rc;
    u_char               errstr[NGX_MAX_CONF_ERRSTR];

    // 初始化正则表达式编译结构体
    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pattern = *regex;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

#if (NGX_HAVE_CASELESS_FILESYSTEM)
    rc.options = NGX_REGEX_CASELESS;
#else
    // 根据caseless参数设置是否忽略大小写
    rc.options = caseless ? NGX_REGEX_CASELESS : 0;
#endif

    // 编译正则表达式
    clcf->regex = ngx_http_regex_compile(cf, &rc);
    if (clcf->regex == NULL) {
        return NGX_ERROR;
    }

    // 设置location的名称为正则表达式
    clcf->name = *regex;

    return NGX_OK;

#else

    // 如果没有PCRE库支持，输出错误日志
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "using regex \"%V\" requires PCRE library",
                       regex);
    return NGX_ERROR;

#endif
}


static char *
ngx_http_core_types(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    char        *rv;
    ngx_conf_t   save;

    // 如果types数组还未初始化，则创建它
    if (clcf->types == NULL) {
        clcf->types = ngx_array_create(cf->pool, 64, sizeof(ngx_hash_key_t));
        if (clcf->types == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    // 保存当前配置上下文
    save = *cf;
    // 设置处理函数为ngx_http_core_type
    cf->handler = ngx_http_core_type;
    cf->handler_conf = conf;

    // 解析配置块
    rv = ngx_conf_parse(cf, NULL);

    // 恢复配置上下文
    *cf = save;

    return rv;
}


static char *
ngx_http_core_type(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t       *value, *content_type, *old;
    ngx_uint_t       i, n, hash;
    ngx_hash_key_t  *type;

    value = cf->args->elts;

    // 处理include指令
    if (ngx_strcmp(value[0].data, "include") == 0) {
        if (cf->args->nelts != 2) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid number of arguments"
                               " in \"include\" directive");
            return NGX_CONF_ERROR;
        }

        return ngx_conf_include(cf, dummy, conf);
    }

    // 分配内存给content_type
    content_type = ngx_palloc(cf->pool, sizeof(ngx_str_t));
    if (content_type == NULL) {
        return NGX_CONF_ERROR;
    }

    *content_type = value[0];

    // 处理每个文件扩展名
    for (i = 1; i < cf->args->nelts; i++) {

        // 计算扩展名的哈希值
        hash = ngx_hash_strlow(value[i].data, value[i].data, value[i].len);

        // 检查是否有重复的扩展名
        type = clcf->types->elts;
        for (n = 0; n < clcf->types->nelts; n++) {
            if (ngx_strcmp(value[i].data, type[n].key.data) == 0) {
                old = type[n].value;
                type[n].value = content_type;

                // 输出警告日志
                ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                                   "duplicate extension \"%V\", "
                                   "content type: \"%V\", "
                                   "previous content type: \"%V\"",
                                   &value[i], content_type, old);
                goto next;
            }
        }

        // 添加新的类型映射
        type = ngx_array_push(clcf->types);
        if (type == NULL) {
            return NGX_CONF_ERROR;
        }

        type->key = value[i];
        type->key_hash = hash;
        type->value = content_type;

    next:
        continue;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_core_preconfiguration(ngx_conf_t *cf)
{
    // 添加核心变量
    return ngx_http_variables_add_core_vars(cf);
}


static ngx_int_t
ngx_http_core_postconfiguration(ngx_conf_t *cf)
{
    // 设置请求体过滤器
    ngx_http_top_request_body_filter = ngx_http_request_body_save_filter;

    return NGX_OK;
}

/**
 * @brief 创建HTTP核心模块的主配置
 *
 * @param cf Nginx配置结构体指针
 * @return 返回创建的主配置结构体指针，如果失败则返回NULL
 *
 * 此函数负责为HTTP核心模块创建主配置结构。它会分配内存，
 * 初始化servers数组，并设置一些默认值。这个配置结构将用于
 * 存储影响整个HTTP处理过程的核心设置。
 */
static void *
ngx_http_core_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t  *cmcf;

    // 分配内存给主配置结构体
    cmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_core_main_conf_t));
    if (cmcf == NULL) {
        return NULL;
    }

    // 初始化servers数组
    if (ngx_array_init(&cmcf->servers, cf->pool, 4,
                       sizeof(ngx_http_core_srv_conf_t *))
        != NGX_OK)
    {
        return NULL;
    }

    // 设置默认值
    cmcf->server_names_hash_max_size = NGX_CONF_UNSET_UINT;
    cmcf->server_names_hash_bucket_size = NGX_CONF_UNSET_UINT;

    cmcf->variables_hash_max_size = NGX_CONF_UNSET_UINT;
    cmcf->variables_hash_bucket_size = NGX_CONF_UNSET_UINT;

    return cmcf;
}

/**
 * @brief 初始化HTTP核心模块的主配置
 *
 * @param cf Nginx配置结构体指针
 * @param conf 指向主配置结构的指针
 * @return 成功返回NGX_CONF_OK，失败返回错误信息字符串
 *
 * 此函数负责初始化HTTP核心模块的主配置。它设置默认值，
 * 调整哈希表大小，并对齐内存。主要完成以下任务:
 * 1. 初始化服务器名称哈希表的大小
 * 2. 初始化变量哈希表的大小
 * 3. 调整捕获数量（如果有的话）
 * 这个函数在所有配置指令都被处理后调用，确保主配置结构中的所有必要设置都已正确初始化。
 */
static char *
ngx_http_core_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_core_main_conf_t *cmcf = conf;

    // 初始化服务器名称哈希表的最大大小，如果未设置则默认为512
    ngx_conf_init_uint_value(cmcf->server_names_hash_max_size, 512);
    // 初始化服务器名称哈希表的桶大小，如果未设置则默认为缓存行大小
    ngx_conf_init_uint_value(cmcf->server_names_hash_bucket_size,
                             ngx_cacheline_size);

    // 将服务器名称哈希表的桶大小对齐到缓存行大小
    cmcf->server_names_hash_bucket_size =
            ngx_align(cmcf->server_names_hash_bucket_size, ngx_cacheline_size);

    // 初始化变量哈希表的最大大小，如果未设置则默认为1024
    ngx_conf_init_uint_value(cmcf->variables_hash_max_size, 1024);
    // 初始化变量哈希表的桶大小，如果未设置则默认为64
    ngx_conf_init_uint_value(cmcf->variables_hash_bucket_size, 64);

    // 将变量哈希表的桶大小对齐到缓存行大小
    cmcf->variables_hash_bucket_size =
               ngx_align(cmcf->variables_hash_bucket_size, ngx_cacheline_size);

    // 如果有捕获，调整捕获数量
    if (cmcf->ncaptures) {
        cmcf->ncaptures = (cmcf->ncaptures + 1) * 3;
    }

    return NGX_CONF_OK;
}

/**
 * @brief 创建HTTP服务器配置结构
 *
 * @param cf Nginx配置结构体指针
 * @return 新创建的服务器配置结构指针，失败时返回NULL
 *
 * 此函数负责为每个HTTP服务器块创建一个新的配置结构。
 * 它分配内存，初始化默认值，并设置一些基本的服务器级别配置。
 * 主要完成以下任务:
 * 1. 分配内存并初始化服务器配置结构
 * 2. 初始化服务器名称数组
 * 3. 设置未配置项的默认值
 * 4. 记录配置文件名和行号
 * 在Nginx配置解析过程中，对于每个server块，都会调用此函数来创建相应的配置结构。
 */
static void *
ngx_http_core_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_core_srv_conf_t  *cscf;

    // 为服务器配置结构体分配内存
    cscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_core_srv_conf_t));
    if (cscf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->client_large_buffers.num = 0;
     */

    // 初始化服务器名称数组
    if (ngx_array_init(&cscf->server_names, cf->temp_pool, 4,
                       sizeof(ngx_http_server_name_t))
        != NGX_OK)
    {
        return NULL;
    }

    // 设置未设置的配置项为未设置状态
    cscf->connection_pool_size = NGX_CONF_UNSET_SIZE;
    cscf->request_pool_size = NGX_CONF_UNSET_SIZE;
    cscf->client_header_timeout = NGX_CONF_UNSET_MSEC;
    cscf->client_header_buffer_size = NGX_CONF_UNSET_SIZE;
    cscf->ignore_invalid_headers = NGX_CONF_UNSET;
    cscf->merge_slashes = NGX_CONF_UNSET;
    cscf->underscores_in_headers = NGX_CONF_UNSET;

    // 设置配置文件名和行号
    cscf->file_name = cf->conf_file->file.name.data;
    cscf->line = cf->conf_file->line;

    return cscf;
}

/*
 * 函数: ngx_http_core_merge_srv_conf
 * 功能: 合并HTTP服务器配置
 * 参数:
 *   cf: Nginx配置结构体指针
 *   parent: 父配置结构体指针
 *   child: 子配置结构体指针
 * 返回值: 成功返回NGX_CONF_OK，失败返回错误信息字符串
 * 描述:
 *   此函数负责合并父级和子级的HTTP服务器配置。
 *   它会检查子配置中未设置的选项，并从父配置中继承相应的值。
 *   同时，它还会进行一些配置项的验证和初始化工作。
 *   这个过程确保了配置的层次结构和继承机制正常工作，
 *   并为每个服务器块提供了完整的配置信息。
 */
static char *
ngx_http_core_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_core_srv_conf_t *prev = parent;
    ngx_http_core_srv_conf_t *conf = child;

    ngx_str_t                name;
    ngx_http_server_name_t  *sn;

    /* TODO: it does not merge, it inits only */

    // 合并各种配置项，如果子配置未设置则使用父配置的值
    ngx_conf_merge_size_value(conf->connection_pool_size,
                              prev->connection_pool_size, 64 * sizeof(void *));
    ngx_conf_merge_size_value(conf->request_pool_size,
                              prev->request_pool_size, 4096);
    ngx_conf_merge_msec_value(conf->client_header_timeout,
                              prev->client_header_timeout, 60000);
    ngx_conf_merge_size_value(conf->client_header_buffer_size,
                              prev->client_header_buffer_size, 1024);
    ngx_conf_merge_bufs_value(conf->large_client_header_buffers,
                              prev->large_client_header_buffers,
                              4, 8192);

    // 检查large_client_header_buffers的大小是否合法
    if (conf->large_client_header_buffers.size < conf->connection_pool_size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the \"large_client_header_buffers\" size must be "
                           "equal to or greater than \"connection_pool_size\"");
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->ignore_invalid_headers,
                              prev->ignore_invalid_headers, 1);

    ngx_conf_merge_value(conf->merge_slashes, prev->merge_slashes, 1);

    ngx_conf_merge_value(conf->underscores_in_headers,
                              prev->underscores_in_headers, 0);

    // 如果没有设置服务器名称，添加一个空的服务器名称
    if (conf->server_names.nelts == 0) {
        /* the array has 4 empty preallocated elements, so push cannot fail */
        sn = ngx_array_push(&conf->server_names);
#if (NGX_PCRE)
        sn->regex = NULL;
#endif
        sn->server = conf;
        ngx_str_set(&sn->name, "");
    }

    sn = conf->server_names.elts;
    name = sn[0].name;

#if (NGX_PCRE)
    if (sn->regex) {
        name.len++;
        name.data--;
    } else
#endif

    // 如果服务器名称以点开头，去掉点
    if (name.data[0] == '.') {
        name.len--;
        name.data++;
    }

    // 设置服务器名称
    conf->server_name.len = name.len;
    conf->server_name.data = ngx_pstrdup(cf->pool, &name);
    if (conf->server_name.data == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


// 创建HTTP核心模块的位置配置
static void *
ngx_http_core_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_core_loc_conf_t  *clcf;

    // 分配内存
    clcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_core_loc_conf_t));
    if (clcf == NULL) {
        return NULL;
    }

    /*
     * 以下字段由ngx_pcalloc()设置为0或NULL:
     *
     *     clcf->escaped_name = { 0, NULL };
     *     clcf->root = { 0, NULL };
     *     clcf->limit_except = 0;
     *     clcf->post_action = { 0, NULL };
     *     clcf->types = NULL;
     *     clcf->default_type = { 0, NULL };
     *     clcf->error_log = NULL;
     *     clcf->error_pages = NULL;
     *     clcf->client_body_path = NULL;
     *     clcf->regex = NULL;
     *     clcf->exact_match = 0;
     *     clcf->auto_redirect = 0;
     *     clcf->alias = 0;
     *     clcf->gzip_proxied = 0;
     *     clcf->keepalive_disable = 0;
     */

    // 设置各种配置项的初始值
    clcf->client_max_body_size = NGX_CONF_UNSET;
    clcf->client_body_buffer_size = NGX_CONF_UNSET_SIZE;
    clcf->client_body_timeout = NGX_CONF_UNSET_MSEC;
    clcf->satisfy = NGX_CONF_UNSET_UINT;
    clcf->auth_delay = NGX_CONF_UNSET_MSEC;
    clcf->if_modified_since = NGX_CONF_UNSET_UINT;
    clcf->max_ranges = NGX_CONF_UNSET_UINT;
    clcf->client_body_in_file_only = NGX_CONF_UNSET_UINT;
    clcf->client_body_in_single_buffer = NGX_CONF_UNSET;
    clcf->internal = NGX_CONF_UNSET;
    clcf->sendfile = NGX_CONF_UNSET;
    clcf->sendfile_max_chunk = NGX_CONF_UNSET_SIZE;
    clcf->subrequest_output_buffer_size = NGX_CONF_UNSET_SIZE;
    clcf->aio = NGX_CONF_UNSET;
    clcf->aio_write = NGX_CONF_UNSET;
#if (NGX_THREADS)
    clcf->thread_pool = NGX_CONF_UNSET_PTR;
    clcf->thread_pool_value = NGX_CONF_UNSET_PTR;
#endif
    clcf->read_ahead = NGX_CONF_UNSET_SIZE;
    clcf->directio = NGX_CONF_UNSET;
    clcf->directio_alignment = NGX_CONF_UNSET;
    clcf->tcp_nopush = NGX_CONF_UNSET;
    clcf->tcp_nodelay = NGX_CONF_UNSET;
    clcf->send_timeout = NGX_CONF_UNSET_MSEC;
    clcf->send_lowat = NGX_CONF_UNSET_SIZE;
    clcf->postpone_output = NGX_CONF_UNSET_SIZE;
    clcf->limit_rate = NGX_CONF_UNSET_PTR;
    clcf->limit_rate_after = NGX_CONF_UNSET_PTR;
    clcf->keepalive_time = NGX_CONF_UNSET_MSEC;
    clcf->keepalive_timeout = NGX_CONF_UNSET_MSEC;
    clcf->keepalive_header = NGX_CONF_UNSET;
    clcf->keepalive_requests = NGX_CONF_UNSET_UINT;
    clcf->lingering_close = NGX_CONF_UNSET_UINT;
    clcf->lingering_time = NGX_CONF_UNSET_MSEC;
    clcf->lingering_timeout = NGX_CONF_UNSET_MSEC;
    clcf->resolver_timeout = NGX_CONF_UNSET_MSEC;
    clcf->reset_timedout_connection = NGX_CONF_UNSET;
    clcf->absolute_redirect = NGX_CONF_UNSET;
    clcf->server_name_in_redirect = NGX_CONF_UNSET;
    clcf->port_in_redirect = NGX_CONF_UNSET;
    clcf->msie_padding = NGX_CONF_UNSET;
    clcf->msie_refresh = NGX_CONF_UNSET;
    clcf->log_not_found = NGX_CONF_UNSET;
    clcf->log_subrequest = NGX_CONF_UNSET;
    clcf->recursive_error_pages = NGX_CONF_UNSET;
    clcf->chunked_transfer_encoding = NGX_CONF_UNSET;
    clcf->etag = NGX_CONF_UNSET;
    clcf->server_tokens = NGX_CONF_UNSET_UINT;
    clcf->types_hash_max_size = NGX_CONF_UNSET_UINT;
    clcf->types_hash_bucket_size = NGX_CONF_UNSET_UINT;

    clcf->open_file_cache = NGX_CONF_UNSET_PTR;
    clcf->open_file_cache_valid = NGX_CONF_UNSET;
    clcf->open_file_cache_min_uses = NGX_CONF_UNSET_UINT;
    clcf->open_file_cache_errors = NGX_CONF_UNSET;
    clcf->open_file_cache_events = NGX_CONF_UNSET;

#if (NGX_HTTP_GZIP)
    clcf->gzip_vary = NGX_CONF_UNSET;
    clcf->gzip_http_version = NGX_CONF_UNSET_UINT;
#if (NGX_PCRE)
    clcf->gzip_disable = NGX_CONF_UNSET_PTR;
#endif
    clcf->gzip_disable_msie6 = 3;
#if (NGX_HTTP_DEGRADATION)
    clcf->gzip_disable_degradation = 3;
#endif
#endif

#if (NGX_HAVE_OPENAT)
    clcf->disable_symlinks = NGX_CONF_UNSET_UINT;
    clcf->disable_symlinks_from = NGX_CONF_UNSET_PTR;
#endif

    return clcf;
}


// 定义默认的MIME类型
static ngx_str_t  ngx_http_core_text_html_type = ngx_string("text/html");
static ngx_str_t  ngx_http_core_image_gif_type = ngx_string("image/gif");
static ngx_str_t  ngx_http_core_image_jpeg_type = ngx_string("image/jpeg");

// 定义默认的文件扩展名到MIME类型的映射
static ngx_hash_key_t  ngx_http_core_default_types[] = {
    { ngx_string("html"), 0, &ngx_http_core_text_html_type },
    { ngx_string("gif"), 0, &ngx_http_core_image_gif_type },
    { ngx_string("jpg"), 0, &ngx_http_core_image_jpeg_type },
    { ngx_null_string, 0, NULL }
};

/**
 * @brief 合并HTTP location配置
 *
 * @param cf Nginx配置结构体指针
 * @param parent 父配置结构体指针
 * @param child 子配置结构体指针
 * @return 成功返回NGX_CONF_OK，失败返回错误信息字符串
 *
 * 此函数负责合并父级和子级的HTTP location配置。
 * 它会检查子配置中未设置的选项，并从父配置中继承相应的值。
 * 同时，它还会进行一些配置项的验证和初始化工作。
 * 这个过程确保了配置的层次结构和继承机制正常工作，
 * 并为每个location块提供了完整的配置信息。
 */
static char *
ngx_http_core_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_core_loc_conf_t *prev = parent;
    ngx_http_core_loc_conf_t *conf = child;

    ngx_uint_t        i;
    ngx_hash_key_t   *type;
    ngx_hash_init_t   types_hash;

    // 如果当前配置的root未设置，则继承父配置的相关设置
    if (conf->root.data == NULL) {

        conf->alias = prev->alias;
        conf->root = prev->root;
        conf->root_lengths = prev->root_lengths;
        conf->root_values = prev->root_values;

        // 如果父配置的root也未设置，则设置默认值为"html"
        if (prev->root.data == NULL) {
            ngx_str_set(&conf->root, "html");

            if (ngx_conf_full_name(cf->cycle, &conf->root, 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }
    }

    // 合并post_action配置
    if (conf->post_action.data == NULL) {
        conf->post_action = prev->post_action;
    }

    // 合并types_hash相关配置
    ngx_conf_merge_uint_value(conf->types_hash_max_size,
                              prev->types_hash_max_size, 1024);

    ngx_conf_merge_uint_value(conf->types_hash_bucket_size,
                              prev->types_hash_bucket_size, 64);

    conf->types_hash_bucket_size = ngx_align(conf->types_hash_bucket_size,
                                             ngx_cacheline_size);

    // 特殊处理"http"部分的"types"指令，以继承http的conf->types_hash到所有服务器
    if (prev->types && prev->types_hash.buckets == NULL) {

        types_hash.hash = &prev->types_hash;
        types_hash.key = ngx_hash_key_lc;
        types_hash.max_size = conf->types_hash_max_size;
        types_hash.bucket_size = conf->types_hash_bucket_size;
        types_hash.name = "types_hash";
        types_hash.pool = cf->pool;
        types_hash.temp_pool = NULL;

        if (ngx_hash_init(&types_hash, prev->types->elts, prev->types->nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    // 如果当前配置的types未设置，则继承父配置的types
    if (conf->types == NULL) {
        conf->types = prev->types;
        conf->types_hash = prev->types_hash;
    }

    // 如果types仍未设置，则创建默认的types配置
    if (conf->types == NULL) {
        conf->types = ngx_array_create(cf->pool, 3, sizeof(ngx_hash_key_t));
        if (conf->types == NULL) {
            return NGX_CONF_ERROR;
        }

        for (i = 0; ngx_http_core_default_types[i].key.len; i++) {
            type = ngx_array_push(conf->types);
            if (type == NULL) {
                return NGX_CONF_ERROR;
            }

            type->key = ngx_http_core_default_types[i].key;
            type->key_hash =
                       ngx_hash_key_lc(ngx_http_core_default_types[i].key.data,
                                       ngx_http_core_default_types[i].key.len);
            type->value = ngx_http_core_default_types[i].value;
        }
    }

    // 初始化types_hash
    if (conf->types_hash.buckets == NULL) {

        types_hash.hash = &conf->types_hash;
        types_hash.key = ngx_hash_key_lc;
        types_hash.max_size = conf->types_hash_max_size;
        types_hash.bucket_size = conf->types_hash_bucket_size;
        types_hash.name = "types_hash";
        types_hash.pool = cf->pool;
        types_hash.temp_pool = NULL;

        if (ngx_hash_init(&types_hash, conf->types->elts, conf->types->nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    // 合并error_log配置
    if (conf->error_log == NULL) {
        if (prev->error_log) {
            conf->error_log = prev->error_log;
        } else {
            conf->error_log = &cf->cycle->new_log;
        }
    }

    // 合并error_pages配置
    if (conf->error_pages == NULL && prev->error_pages) {
        conf->error_pages = prev->error_pages;
    }

    // 合并其他各种配置项
    ngx_conf_merge_str_value(conf->default_type,
                              prev->default_type, "text/plain");

    ngx_conf_merge_off_value(conf->client_max_body_size,
                              prev->client_max_body_size, 1 * 1024 * 1024);
    ngx_conf_merge_size_value(conf->client_body_buffer_size,
                              prev->client_body_buffer_size,
                              (size_t) 2 * ngx_pagesize);
    ngx_conf_merge_msec_value(conf->client_body_timeout,
                              prev->client_body_timeout, 60000);

    ngx_conf_merge_bitmask_value(conf->keepalive_disable,
                              prev->keepalive_disable,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_KEEPALIVE_DISABLE_MSIE6));
    ngx_conf_merge_uint_value(conf->satisfy, prev->satisfy,
                              NGX_HTTP_SATISFY_ALL);
    ngx_conf_merge_msec_value(conf->auth_delay, prev->auth_delay, 0);
    ngx_conf_merge_uint_value(conf->if_modified_since, prev->if_modified_since,
                              NGX_HTTP_IMS_EXACT);
    ngx_conf_merge_uint_value(conf->max_ranges, prev->max_ranges,
                              NGX_MAX_INT32_VALUE);
    ngx_conf_merge_uint_value(conf->client_body_in_file_only,
                              prev->client_body_in_file_only,
                              NGX_HTTP_REQUEST_BODY_FILE_OFF);
    ngx_conf_merge_value(conf->client_body_in_single_buffer,
                              prev->client_body_in_single_buffer, 0);
    ngx_conf_merge_value(conf->internal, prev->internal, 0);
    ngx_conf_merge_value(conf->sendfile, prev->sendfile, 0);
    ngx_conf_merge_size_value(conf->sendfile_max_chunk,
                              prev->sendfile_max_chunk, 2 * 1024 * 1024);
    ngx_conf_merge_size_value(conf->subrequest_output_buffer_size,
                              prev->subrequest_output_buffer_size,
                              (size_t) ngx_pagesize);
    ngx_conf_merge_value(conf->aio, prev->aio, NGX_HTTP_AIO_OFF);
    ngx_conf_merge_value(conf->aio_write, prev->aio_write, 0);
#if (NGX_THREADS)
    ngx_conf_merge_ptr_value(conf->thread_pool, prev->thread_pool, NULL);
    ngx_conf_merge_ptr_value(conf->thread_pool_value, prev->thread_pool_value,
                             NULL);
#endif
    ngx_conf_merge_size_value(conf->read_ahead, prev->read_ahead, 0);
    ngx_conf_merge_off_value(conf->directio, prev->directio,
                              NGX_OPEN_FILE_DIRECTIO_OFF);
    ngx_conf_merge_off_value(conf->directio_alignment, prev->directio_alignment,
                              512);
    ngx_conf_merge_value(conf->tcp_nopush, prev->tcp_nopush, 0);
    ngx_conf_merge_value(conf->tcp_nodelay, prev->tcp_nodelay, 1);

    ngx_conf_merge_msec_value(conf->send_timeout, prev->send_timeout, 60000);
    ngx_conf_merge_size_value(conf->send_lowat, prev->send_lowat, 0);
    ngx_conf_merge_size_value(conf->postpone_output, prev->postpone_output,
                              1460);

    ngx_conf_merge_ptr_value(conf->limit_rate, prev->limit_rate, NULL);
    ngx_conf_merge_ptr_value(conf->limit_rate_after,
                              prev->limit_rate_after, NULL);

    ngx_conf_merge_msec_value(conf->keepalive_time,
                              prev->keepalive_time, 3600000);
    ngx_conf_merge_msec_value(conf->keepalive_timeout,
                              prev->keepalive_timeout, 75000);
    ngx_conf_merge_sec_value(conf->keepalive_header,
                              prev->keepalive_header, 0);
    ngx_conf_merge_uint_value(conf->keepalive_requests,
                              prev->keepalive_requests, 1000);
    ngx_conf_merge_uint_value(conf->lingering_close,
                              prev->lingering_close, NGX_HTTP_LINGERING_ON);
    ngx_conf_merge_msec_value(conf->lingering_time,
                              prev->lingering_time, 30000);
    ngx_conf_merge_msec_value(conf->lingering_timeout,
                              prev->lingering_timeout, 5000);
    ngx_conf_merge_msec_value(conf->resolver_timeout,
                              prev->resolver_timeout, 30000);

    // 合并resolver配置
    if (conf->resolver == NULL) {

        if (prev->resolver == NULL) {

            // 在http {}上下文中创建一个虚拟的resolver，以便所有服务器继承
            prev->resolver = ngx_resolver_create(cf, NULL, 0);
            if (prev->resolver == NULL) {
                return NGX_CONF_ERROR;
            }
        }

        conf->resolver = prev->resolver;
    }

    // 合并client_body_temp_path配置
    if (ngx_conf_merge_path_value(cf, &conf->client_body_temp_path,
                              prev->client_body_temp_path,
                              &ngx_http_client_temp_path)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    // 合并其他各种配置项
    ngx_conf_merge_value(conf->reset_timedout_connection,
                              prev->reset_timedout_connection, 0);
    ngx_conf_merge_value(conf->absolute_redirect,
                              prev->absolute_redirect, 1);
    ngx_conf_merge_value(conf->server_name_in_redirect,
                              prev->server_name_in_redirect, 0);
    ngx_conf_merge_value(conf->port_in_redirect, prev->port_in_redirect, 1);
    ngx_conf_merge_value(conf->msie_padding, prev->msie_padding, 1);
    ngx_conf_merge_value(conf->msie_refresh, prev->msie_refresh, 0);
    ngx_conf_merge_value(conf->log_not_found, prev->log_not_found, 1);
    ngx_conf_merge_value(conf->log_subrequest, prev->log_subrequest, 0);
    ngx_conf_merge_value(conf->recursive_error_pages,
                              prev->recursive_error_pages, 0);
    ngx_conf_merge_value(conf->chunked_transfer_encoding,
                              prev->chunked_transfer_encoding, 1);
    ngx_conf_merge_value(conf->etag, prev->etag, 1);

    ngx_conf_merge_uint_value(conf->server_tokens, prev->server_tokens,
                              NGX_HTTP_SERVER_TOKENS_ON);

    ngx_conf_merge_ptr_value(conf->open_file_cache,
                              prev->open_file_cache, NULL);

    ngx_conf_merge_sec_value(conf->open_file_cache_valid,
                              prev->open_file_cache_valid, 60);

    ngx_conf_merge_uint_value(conf->open_file_cache_min_uses,
                              prev->open_file_cache_min_uses, 1);

    ngx_conf_merge_sec_value(conf->open_file_cache_errors,
                              prev->open_file_cache_errors, 0);

    ngx_conf_merge_sec_value(conf->open_file_cache_events,
                              prev->open_file_cache_events, 0);
#if (NGX_HTTP_GZIP)

    ngx_conf_merge_value(conf->gzip_vary, prev->gzip_vary, 0);
    ngx_conf_merge_uint_value(conf->gzip_http_version, prev->gzip_http_version,
                              NGX_HTTP_VERSION_11);
    ngx_conf_merge_bitmask_value(conf->gzip_proxied, prev->gzip_proxied,
                              (NGX_CONF_BITMASK_SET|NGX_HTTP_GZIP_PROXIED_OFF));

#if (NGX_PCRE)
    ngx_conf_merge_ptr_value(conf->gzip_disable, prev->gzip_disable, NULL);
#endif

    if (conf->gzip_disable_msie6 == 3) {
        conf->gzip_disable_msie6 =
            (prev->gzip_disable_msie6 == 3) ? 0 : prev->gzip_disable_msie6;
    }

#if (NGX_HTTP_DEGRADATION)

    if (conf->gzip_disable_degradation == 3) {
        conf->gzip_disable_degradation =
            (prev->gzip_disable_degradation == 3) ?
                 0 : prev->gzip_disable_degradation;
    }

#endif
#endif

#if (NGX_HAVE_OPENAT)
    ngx_conf_merge_uint_value(conf->disable_symlinks, prev->disable_symlinks,
                              NGX_DISABLE_SYMLINKS_OFF);
    ngx_conf_merge_ptr_value(conf->disable_symlinks_from,
                             prev->disable_symlinks_from, NULL);
#endif

    return NGX_CONF_OK;
}

/**
 * @brief 处理HTTP核心模块的listen指令
 *
 * @param cf Nginx配置结构体指针
 * @param cmd 当前处理的命令结构体指针
 * @param conf 指向当前模块配置结构的指针
 * @return 成功返回NGX_CONF_OK，失败返回NGX_CONF_ERROR
 *
 * 此函数负责解析和处理HTTP核心模块的listen指令。
 * 它会解析监听地址和端口，设置相关的监听选项，
 * 并将解析后的监听配置添加到服务器配置中。
 * 主要完成以下任务:
 * 1. 解析listen指令的参数
 * 2. 设置监听选项（如backlog、rcvbuf等）
 * 3. 处理IPv6相关配置（如果支持）
 * 4. 添加监听配置到服务器配置结构中
 */
static char *
ngx_http_core_listen(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_srv_conf_t *cscf = conf;

    ngx_str_t              *value, size;
    ngx_url_t               u;
    ngx_uint_t              n, i, backlog;
    ngx_http_listen_opt_t   lsopt;

    // 标记该服务器块已配置监听指令
    cscf->listen = 1;

    value = cf->args->elts;

    ngx_memzero(&u, sizeof(ngx_url_t));

    // 设置URL相关参数
    u.url = value[1];
    u.listen = 1;
    u.default_port = 80;

    // 解析URL
    if (ngx_parse_url(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "%s in \"%V\" of the \"listen\" directive",
                               u.err, &u.url);
        }

        return NGX_CONF_ERROR;
    }

    // 初始化监听选项结构体
    ngx_memzero(&lsopt, sizeof(ngx_http_listen_opt_t));

    // 设置默认值
    lsopt.backlog = NGX_LISTEN_BACKLOG;
    lsopt.type = SOCK_STREAM;
    lsopt.rcvbuf = -1;
    lsopt.sndbuf = -1;
#if (NGX_HAVE_SETFIB)
    lsopt.setfib = -1;
#endif
#if (NGX_HAVE_TCP_FASTOPEN)
    lsopt.fastopen = -1;
#endif
#if (NGX_HAVE_INET6)
    lsopt.ipv6only = 1;
#endif

    backlog = 0;

    // 解析listen指令的其他参数
    for (n = 2; n < cf->args->nelts; n++) {

        if (ngx_strcmp(value[n].data, "default_server") == 0
            || ngx_strcmp(value[n].data, "default") == 0)
        {
            lsopt.default_server = 1;
            continue;
        }

        if (ngx_strcmp(value[n].data, "bind") == 0) {
            lsopt.set = 1;
            lsopt.bind = 1;
            continue;
        }

#if (NGX_HAVE_SETFIB)
        // 设置FIB
        if (ngx_strncmp(value[n].data, "setfib=", 7) == 0) {
            lsopt.setfib = ngx_atoi(value[n].data + 7, value[n].len - 7);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.setfib == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid setfib \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
        // 设置TCP Fast Open
        if (ngx_strncmp(value[n].data, "fastopen=", 9) == 0) {
            lsopt.fastopen = ngx_atoi(value[n].data + 9, value[n].len - 9);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.fastopen == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid fastopen \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }
#endif

        // 设置backlog
        if (ngx_strncmp(value[n].data, "backlog=", 8) == 0) {
            lsopt.backlog = ngx_atoi(value[n].data + 8, value[n].len - 8);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.backlog == NGX_ERROR || lsopt.backlog == 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid backlog \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            backlog = 1;

            continue;
        }

        // 设置接收缓冲区大小
        if (ngx_strncmp(value[n].data, "rcvbuf=", 7) == 0) {
            size.len = value[n].len - 7;
            size.data = value[n].data + 7;

            lsopt.rcvbuf = ngx_parse_size(&size);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.rcvbuf == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid rcvbuf \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        // 设置发送缓冲区大小
        if (ngx_strncmp(value[n].data, "sndbuf=", 7) == 0) {
            size.len = value[n].len - 7;
            size.data = value[n].data + 7;

            lsopt.sndbuf = ngx_parse_size(&size);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.sndbuf == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid sndbuf \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        // 设置accept过滤器
        if (ngx_strncmp(value[n].data, "accept_filter=", 14) == 0) {
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
            lsopt.accept_filter = (char *) &value[n].data[14];
            lsopt.set = 1;
            lsopt.bind = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "accept filters \"%V\" are not supported "
                               "on this platform, ignored",
                               &value[n]);
#endif
            continue;
        }

        // 设置延迟接受连接
        if (ngx_strcmp(value[n].data, "deferred") == 0) {
#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
            lsopt.deferred_accept = 1;
            lsopt.set = 1;
            lsopt.bind = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the deferred accept is not supported "
                               "on this platform, ignored");
#endif
            continue;
        }

        // 设置IPv6 only选项
        if (ngx_strncmp(value[n].data, "ipv6only=o", 10) == 0) {
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
            if (ngx_strcmp(&value[n].data[10], "n") == 0) {
                lsopt.ipv6only = 1;

            } else if (ngx_strcmp(&value[n].data[10], "ff") == 0) {
                lsopt.ipv6only = 0;

            } else {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid ipv6only flags \"%s\"",
                                   &value[n].data[9]);
                return NGX_CONF_ERROR;
            }

            lsopt.set = 1;
            lsopt.bind = 1;

            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "ipv6only is not supported "
                               "on this platform");
            return NGX_CONF_ERROR;
#endif
        }

        // 设置端口复用
        if (ngx_strcmp(value[n].data, "reuseport") == 0) {
#if (NGX_HAVE_REUSEPORT)
            lsopt.reuseport = 1;
            lsopt.set = 1;
            lsopt.bind = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "reuseport is not supported "
                               "on this platform, ignored");
#endif
            continue;
        }

        // 设置SSL
        if (ngx_strcmp(value[n].data, "ssl") == 0) {
#if (NGX_HTTP_SSL)
            lsopt.ssl = 1;
            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the \"ssl\" parameter requires "
                               "ngx_http_ssl_module");
            return NGX_CONF_ERROR;
#endif
        }

        // 设置HTTP/2
        if (ngx_strcmp(value[n].data, "http2") == 0) {
#if (NGX_HTTP_V2)
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "the \"listen ... http2\" directive "
                               "is deprecated, use "
                               "the \"http2\" directive instead");

            lsopt.http2 = 1;
            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the \"http2\" parameter requires "
                               "ngx_http_v2_module");
            return NGX_CONF_ERROR;
#endif
        }

        // 设置QUIC
        if (ngx_strcmp(value[n].data, "quic") == 0) {
#if (NGX_HTTP_V3)
            lsopt.quic = 1;
            lsopt.type = SOCK_DGRAM;
            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the \"quic\" parameter requires "
                               "ngx_http_v3_module");
            return NGX_CONF_ERROR;
#endif
        }

        // 设置TCP keepalive
        if (ngx_strncmp(value[n].data, "so_keepalive=", 13) == 0) {

            if (ngx_strcmp(&value[n].data[13], "on") == 0) {
                lsopt.so_keepalive = 1;

            } else if (ngx_strcmp(&value[n].data[13], "off") == 0) {
                lsopt.so_keepalive = 2;

            } else {

#if (NGX_HAVE_KEEPALIVE_TUNABLE)
                u_char     *p, *end;
                ngx_str_t   s;

                end = value[n].data + value[n].len;
                s.data = value[n].data + 13;

                p = ngx_strlchr(s.data, end, ':');
                if (p == NULL) {
                    p = end;
                }

                if (p > s.data) {
                    s.len = p - s.data;

                    lsopt.tcp_keepidle = ngx_parse_time(&s, 1);
                    if (lsopt.tcp_keepidle == (time_t) NGX_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                s.data = (p < end) ? (p + 1) : end;

                p = ngx_strlchr(s.data, end, ':');
                if (p == NULL) {
                    p = end;
                }

                if (p > s.data) {
                    s.len = p - s.data;

                    lsopt.tcp_keepintvl = ngx_parse_time(&s, 1);
                    if (lsopt.tcp_keepintvl == (time_t) NGX_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                s.data = (p < end) ? (p + 1) : end;

                if (s.data < end) {
                    s.len = end - s.data;

                    lsopt.tcp_keepcnt = ngx_atoi(s.data, s.len);
                    if (lsopt.tcp_keepcnt == NGX_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                if (lsopt.tcp_keepidle == 0 && lsopt.tcp_keepintvl == 0
                    && lsopt.tcp_keepcnt == 0)
                {
                    goto invalid_so_keepalive;
                }

                lsopt.so_keepalive = 1;

#else

                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "the \"so_keepalive\" parameter accepts "
                                   "only \"on\" or \"off\" on this platform");
                return NGX_CONF_ERROR;

#endif
            }

            lsopt.set = 1;
            lsopt.bind = 1;

            continue;

#if (NGX_HAVE_KEEPALIVE_TUNABLE)
        invalid_so_keepalive:

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid so_keepalive value: \"%s\"",
                               &value[n].data[13]);
            return NGX_CONF_ERROR;
#endif
        }

        // 设置代理协议
        if (ngx_strcmp(value[n].data, "proxy_protocol") == 0) {
            lsopt.proxy_protocol = 1;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[n]);
        return NGX_CONF_ERROR;
    }

    // QUIC相关参数检查
    if (lsopt.quic) {
#if (NGX_HAVE_TCP_FASTOPEN)
        if (lsopt.fastopen != -1) {
            return "\"fastopen\" parameter is incompatible with \"quic\"";
        }
#endif

        if (backlog) {
            return "\"backlog\" parameter is incompatible with \"quic\"";
        }

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
        if (lsopt.accept_filter) {
            return "\"accept_filter\" parameter is incompatible with \"quic\"";
        }
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
        if (lsopt.deferred_accept) {
            return "\"deferred\" parameter is incompatible with \"quic\"";
        }
#endif

#if (NGX_HTTP_SSL)
        if (lsopt.ssl) {
            return "\"ssl\" parameter is incompatible with \"quic\"";
        }
#endif

#if (NGX_HTTP_V2)
        if (lsopt.http2) {
            return "\"http2\" parameter is incompatible with \"quic\"";
        }
#endif

        if (lsopt.so_keepalive) {
            return "\"so_keepalive\" parameter is incompatible with \"quic\"";
        }

        if (lsopt.proxy_protocol) {
            return "\"proxy_protocol\" parameter is incompatible with \"quic\"";
        }
    }

    for (n = 0; n < u.naddrs; n++) {

        for (i = 0; i < n; i++) {
            if (ngx_cmp_sockaddr(u.addrs[n].sockaddr, u.addrs[n].socklen,
                                 u.addrs[i].sockaddr, u.addrs[i].socklen, 1)
                == NGX_OK)
            {
                goto next;
            }
        }

        lsopt.sockaddr = u.addrs[n].sockaddr;
        lsopt.socklen = u.addrs[n].socklen;
        lsopt.addr_text = u.addrs[n].name;
        lsopt.wildcard = ngx_inet_wildcard(lsopt.sockaddr);

        if (ngx_http_add_listen(cf, cscf, &lsopt) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

    next:
        continue;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_core_server_name(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_srv_conf_t *cscf = conf;

    u_char                   ch;
    ngx_str_t               *value;
    ngx_uint_t               i;
    ngx_http_server_name_t  *sn;

    value = cf->args->elts;

    // 遍历所有服务器名称参数
    for (i = 1; i < cf->args->nelts; i++) {

        ch = value[i].data[0];

        // 检查服务器名称的有效性
        if ((ch == '*' && (value[i].len < 3 || value[i].data[1] != '.'))
            || (ch == '.' && value[i].len < 2))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "server name \"%V\" is invalid", &value[i]);
            return NGX_CONF_ERROR;
        }

        // 警告可疑的服务器名称
        if (ngx_strchr(value[i].data, '/')) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "server name \"%V\" has suspicious symbols",
                               &value[i]);
        }

        // 为新的服务器名称分配内存
        sn = ngx_array_push(&cscf->server_names);
        if (sn == NULL) {
            return NGX_CONF_ERROR;
        }

#if (NGX_PCRE)
        sn->regex = NULL;
#endif
        sn->server = cscf;

        // 处理特殊的 $hostname 服务器名称
        if (ngx_strcasecmp(value[i].data, (u_char *) "$hostname") == 0) {
            sn->name = cf->cycle->hostname;

        } else {
            sn->name = value[i];
        }

        // 处理非正则表达式的服务器名称
        if (value[i].data[0] != '~') {
            ngx_strlow(sn->name.data, sn->name.data, sn->name.len);
            continue;
        }

#if (NGX_PCRE)
        {
        u_char               *p;
        ngx_regex_compile_t   rc;
        u_char                errstr[NGX_MAX_CONF_ERRSTR];

        // 检查正则表达式是否为空
        if (value[i].len == 1) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "empty regex in server name \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        // 移除正则表达式开头的 '~' 字符
        value[i].len--;
        value[i].data++;

        ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

        rc.pattern = value[i];
        rc.err.len = NGX_MAX_CONF_ERRSTR;
        rc.err.data = errstr;

        // 检查是否需要不区分大小写的正则表达式
        for (p = value[i].data; p < value[i].data + value[i].len; p++) {
            if (*p >= 'A' && *p <= 'Z') {
                rc.options = NGX_REGEX_CASELESS;
                break;
            }
        }

        // 编译正则表达式
        sn->regex = ngx_http_regex_compile(cf, &rc);
        if (sn->regex == NULL) {
            return NGX_CONF_ERROR;
        }

        sn->name = value[i];
        cscf->captures = (rc.captures > 0);
        }
#else
        // 如果没有 PCRE 支持，则无法使用正则表达式
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "using regex \"%V\" "
                           "requires PCRE library", &value[i]);

        return NGX_CONF_ERROR;
#endif
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_core_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t                  *value;
    ngx_int_t                   alias;
    ngx_uint_t                  n;
    ngx_http_script_compile_t   sc;

    // 判断是否为别名指令
    alias = (cmd->name.len == sizeof("alias") - 1) ? 1 : 0;

    // 检查是否已经设置了根目录或别名
    if (clcf->root.data) {

        if ((clcf->alias != 0) == alias) {
            return "is duplicate";
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" directive is duplicate, "
                           "\"%s\" directive was specified earlier",
                           &cmd->name, clcf->alias ? "alias" : "root");

        return NGX_CONF_ERROR;
    }

    // 检查是否在命名location中使用了alias指令
    if (clcf->named && alias) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the \"alias\" directive cannot be used "
                           "inside the named location");

        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    // 检查是否使用了$document_root变量
    if (ngx_strstr(value[1].data, "$document_root")
        || ngx_strstr(value[1].data, "${document_root}"))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the $document_root variable cannot be used "
                           "in the \"%V\" directive",
                           &cmd->name);

        return NGX_CONF_ERROR;
    }

    // 检查是否使用了$realpath_root变量
    if (ngx_strstr(value[1].data, "$realpath_root")
        || ngx_strstr(value[1].data, "${realpath_root}"))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the $realpath_root variable cannot be used "
                           "in the \"%V\" directive",
                           &cmd->name);

        return NGX_CONF_ERROR;
    }

    // 设置别名长度和根目录
    clcf->alias = alias ? clcf->name.len : 0;
    clcf->root = value[1];

    // 如果不是别名且根目录以'/'结尾，则去掉最后的'/'
    if (!alias && clcf->root.len > 0
        && clcf->root.data[clcf->root.len - 1] == '/')
    {
        clcf->root.len--;
    }

    // 如果根目录不是以'$'开头，则获取完整路径
    if (clcf->root.data[0] != '$') {
        if (ngx_conf_full_name(cf->cycle, &clcf->root, 0) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    // 计算根目录中的变量数量
    n = ngx_http_script_variables_count(&clcf->root);

    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
    sc.variables = n;

#if (NGX_PCRE)
    // 如果是别名且使用了正则表达式，设置特殊的别名长度
    if (alias && clcf->regex) {
        clcf->alias = NGX_MAX_SIZE_T_VALUE;
        n = 1;
    }
#endif

    // 如果存在变量，编译脚本
    if (n) {
        sc.cf = cf;
        sc.source = &clcf->root;
        sc.lengths = &clcf->root_lengths;
        sc.values = &clcf->root_values;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


// 定义HTTP方法名称和对应的位掩码
static ngx_http_method_name_t  ngx_methods_names[] = {
    { (u_char *) "GET",       (uint32_t) ~NGX_HTTP_GET },
    { (u_char *) "HEAD",      (uint32_t) ~NGX_HTTP_HEAD },
    { (u_char *) "POST",      (uint32_t) ~NGX_HTTP_POST },
    { (u_char *) "PUT",       (uint32_t) ~NGX_HTTP_PUT },
    { (u_char *) "DELETE",    (uint32_t) ~NGX_HTTP_DELETE },
    { (u_char *) "MKCOL",     (uint32_t) ~NGX_HTTP_MKCOL },
    { (u_char *) "COPY",      (uint32_t) ~NGX_HTTP_COPY },
    { (u_char *) "MOVE",      (uint32_t) ~NGX_HTTP_MOVE },
    { (u_char *) "OPTIONS",   (uint32_t) ~NGX_HTTP_OPTIONS },
    { (u_char *) "PROPFIND",  (uint32_t) ~NGX_HTTP_PROPFIND },
    { (u_char *) "PROPPATCH", (uint32_t) ~NGX_HTTP_PROPPATCH },
    { (u_char *) "LOCK",      (uint32_t) ~NGX_HTTP_LOCK },
    { (u_char *) "UNLOCK",    (uint32_t) ~NGX_HTTP_UNLOCK },
    { (u_char *) "PATCH",     (uint32_t) ~NGX_HTTP_PATCH },
    { NULL, 0 }
};


// 处理limit_except指令的函数
static char *
ngx_http_core_limit_except(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *pclcf = conf;

    char                      *rv;
    void                      *mconf;
    ngx_str_t                 *value;
    ngx_uint_t                 i;
    ngx_conf_t                 save;
    ngx_http_module_t         *module;
    ngx_http_conf_ctx_t       *ctx, *pctx;
    ngx_http_method_name_t    *name;
    ngx_http_core_loc_conf_t  *clcf;

    // 检查是否重复设置limit_except
    if (pclcf->limit_except) {
        return "is duplicate";
    }

    // 初始化limit_except为所有方法都允许
    pclcf->limit_except = 0xffffffff;

    value = cf->args->elts;

    // 解析指定的HTTP方法
    for (i = 1; i < cf->args->nelts; i++) {
        for (name = ngx_methods_names; name->name; name++) {

            if (ngx_strcasecmp(value[i].data, name->name) == 0) {
                pclcf->limit_except &= name->method;
                goto next;
            }
        }

        // 如果指定了无效的HTTP方法，返回错误
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid method \"%V\"", &value[i]);
        return NGX_CONF_ERROR;

    next:
        continue;
    }

    // 如果限制了GET方法，也限制HEAD方法
    if (!(pclcf->limit_except & NGX_HTTP_GET)) {
        pclcf->limit_except &= (uint32_t) ~NGX_HTTP_HEAD;
    }

    // 创建新的配置上下文
    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    pctx = cf->ctx;
    ctx->main_conf = pctx->main_conf;
    ctx->srv_conf = pctx->srv_conf;

    // 为每个HTTP模块创建新的位置配置
    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[i]->ctx;

        if (module->create_loc_conf) {

            mconf = module->create_loc_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->loc_conf[cf->cycle->modules[i]->ctx_index] = mconf;
        }
    }

    // 设置新的位置配置
    clcf = ctx->loc_conf[ngx_http_core_module.ctx_index];
    pclcf->limit_except_loc_conf = ctx->loc_conf;
    clcf->loc_conf = ctx->loc_conf;
    clcf->name = pclcf->name;
    clcf->noname = 1;
    clcf->lmt_excpt = 1;

    // 添加新的位置配置到locations列表
    if (ngx_http_add_location(cf, &pclcf->locations, clcf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    // 保存当前配置上下文，并设置新的上下文
    save = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NGX_HTTP_LMT_CONF;

    // 解析limit_except块内的配置指令
    rv = ngx_conf_parse(cf, NULL);

    // 恢复原来的配置上下文
    *cf = save;

    return rv;
}


/**
 * @brief 设置AIO（异步I/O）配置
 *
 * @param cf 配置结构体指针
 * @param cmd 命令结构体指针
 * @param conf 配置指针
 * @return char* 返回配置处理结果
 */
static char *
ngx_http_core_set_aio(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t  *value;

    // 检查是否已经设置过AIO
    if (clcf->aio != NGX_CONF_UNSET) {
        return "is duplicate";
    }

#if (NGX_THREADS)
    // 初始化线程池相关配置
    clcf->thread_pool = NULL;
    clcf->thread_pool_value = NULL;
#endif

    value = cf->args->elts;

    // 处理"aio off"配置
    if (ngx_strcmp(value[1].data, "off") == 0) {
        clcf->aio = NGX_HTTP_AIO_OFF;
        return NGX_CONF_OK;
    }

    // 处理"aio on"配置
    if (ngx_strcmp(value[1].data, "on") == 0) {
#if (NGX_HAVE_FILE_AIO)
        clcf->aio = NGX_HTTP_AIO_ON;
        return NGX_CONF_OK;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"aio on\" "
                           "is unsupported on this platform");
        return NGX_CONF_ERROR;
#endif
    }

    // 处理"aio threads"配置
    if (ngx_strncmp(value[1].data, "threads", 7) == 0
        && (value[1].len == 7 || value[1].data[7] == '='))
    {
#if (NGX_THREADS)
        ngx_str_t                          name;
        ngx_thread_pool_t                 *tp;
        ngx_http_complex_value_t           cv;
        ngx_http_compile_complex_value_t   ccv;

        clcf->aio = NGX_HTTP_AIO_THREADS;

        // 处理线程池名称
        if (value[1].len >= 8) {
            name.len = value[1].len - 8;
            name.data = value[1].data + 8;

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

            ccv.cf = cf;
            ccv.value = &name;
            ccv.complex_value = &cv;

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            // 处理复杂值
            if (cv.lengths != NULL) {
                clcf->thread_pool_value = ngx_palloc(cf->pool,
                                    sizeof(ngx_http_complex_value_t));
                if (clcf->thread_pool_value == NULL) {
                    return NGX_CONF_ERROR;
                }

                *clcf->thread_pool_value = cv;

                return NGX_CONF_OK;
            }

            tp = ngx_thread_pool_add(cf, &name);

        } else {
            tp = ngx_thread_pool_add(cf, NULL);
        }

        if (tp == NULL) {
            return NGX_CONF_ERROR;
        }

        clcf->thread_pool = tp;

        return NGX_CONF_OK;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"aio threads\" "
                           "is unsupported on this platform");
        return NGX_CONF_ERROR;
#endif
    }

    return "invalid value";
}


/**
 * @brief 设置directio配置
 *
 * @param cf 配置结构体指针
 * @param cmd 命令结构体指针
 * @param conf 配置指针
 * @return char* 返回配置处理结果
 */
static char *
ngx_http_core_directio(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t  *value;

    // 检查是否已经设置过directio
    if (clcf->directio != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 处理"directio off"配置
    if (ngx_strcmp(value[1].data, "off") == 0) {
        clcf->directio = NGX_OPEN_FILE_DIRECTIO_OFF;
        return NGX_CONF_OK;
    }

    // 解析directio的值
    clcf->directio = ngx_parse_offset(&value[1]);
    if (clcf->directio == (off_t) NGX_ERROR) {
        return "invalid value";
    }

    return NGX_CONF_OK;
}


/**
 * @brief 处理错误页面配置
 *
 * @param cf 配置结构体指针
 * @param cmd 命令结构体指针
 * @param conf 配置指针
 * @return char* 返回配置处理结果
 */
static char *
ngx_http_core_error_page(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    u_char                            *p;
    ngx_int_t                          overwrite;
    ngx_str_t                         *value, uri, args;
    ngx_uint_t                         i, n;
    ngx_http_err_page_t               *err;
    ngx_http_complex_value_t           cv;
    ngx_http_compile_complex_value_t   ccv;

    // 如果错误页面数组未初始化，则创建
    if (clcf->error_pages == NULL) {
        clcf->error_pages = ngx_array_create(cf->pool, 4,
                                             sizeof(ngx_http_err_page_t));
        if (clcf->error_pages == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    i = cf->args->nelts - 2;

    // 检查是否有覆盖状态码的设置
    if (value[i].data[0] == '=') {
        if (i == 1) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid value \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (value[i].len > 1) {
            overwrite = ngx_atoi(&value[i].data[1], value[i].len - 1);

            if (overwrite == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

        } else {
            overwrite = 0;
        }

        n = 2;

    } else {
        overwrite = -1;
        n = 1;
    }

    // 获取URI
    uri = value[cf->args->nelts - 1];

    // 编译复杂值
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &uri;
    ccv.complex_value = &cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ngx_str_null(&args);

    // 处理URI中的参数
    if (cv.lengths == NULL && uri.len && uri.data[0] == '/') {
        p = (u_char *) ngx_strchr(uri.data, '?');

        if (p) {
            cv.value.len = p - uri.data;
            cv.value.data = uri.data;
            p++;
            args.len = (uri.data + uri.len) - p;
            args.data = p;
        }
    }

    // 处理每个错误状态码
    for (i = 1; i < cf->args->nelts - n; i++) {
        err = ngx_array_push(clcf->error_pages);
        if (err == NULL) {
            return NGX_CONF_ERROR;
        }

        // 解析状态码
        err->status = ngx_atoi(value[i].data, value[i].len);

        if (err->status == NGX_ERROR || err->status == 499) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid value \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (err->status < 300 || err->status > 599) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "value \"%V\" must be between 300 and 599",
                               &value[i]);
            return NGX_CONF_ERROR;
        }

        err->overwrite = overwrite;

        // 特殊状态码的处理
        if (overwrite == -1) {
            switch (err->status) {
                case NGX_HTTP_TO_HTTPS:
                case NGX_HTTPS_CERT_ERROR:
                case NGX_HTTPS_NO_CERT:
                case NGX_HTTP_REQUEST_HEADER_TOO_LARGE:
                    err->overwrite = NGX_HTTP_BAD_REQUEST;
            }
        }

        err->value = cv;
        err->args = args;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_core_open_file_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    time_t       inactive;
    ngx_str_t   *value, s;
    ngx_int_t    max;
    ngx_uint_t   i;

    // 检查是否已经设置了open_file_cache
    if (clcf->open_file_cache != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 初始化默认值
    max = 0;
    inactive = 60;

    // 解析配置参数
    for (i = 1; i < cf->args->nelts; i++) {

        // 解析max参数
        if (ngx_strncmp(value[i].data, "max=", 4) == 0) {

            max = ngx_atoi(value[i].data + 4, value[i].len - 4);
            if (max <= 0) {
                goto failed;
            }

            continue;
        }

        // 解析inactive参数
        if (ngx_strncmp(value[i].data, "inactive=", 9) == 0) {

            s.len = value[i].len - 9;
            s.data = value[i].data + 9;

            inactive = ngx_parse_time(&s, 1);
            if (inactive == (time_t) NGX_ERROR) {
                goto failed;
            }

            continue;
        }

        // 处理off参数
        if (ngx_strcmp(value[i].data, "off") == 0) {

            clcf->open_file_cache = NULL;

            continue;
        }

    failed:

        // 参数解析失败，记录错误日志
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid \"open_file_cache\" parameter \"%V\"",
                           &value[i]);
        return NGX_CONF_ERROR;
    }

    // 如果设置为off，直接返回
    if (clcf->open_file_cache == NULL) {
        return NGX_CONF_OK;
    }

    // 检查是否设置了max参数
    if (max == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "\"open_file_cache\" must have the \"max\" parameter");
        return NGX_CONF_ERROR;
    }

    // 初始化open_file_cache
    clcf->open_file_cache = ngx_open_file_cache_init(cf->pool, max, inactive);
    if (clcf->open_file_cache) {
        return NGX_CONF_OK;
    }

    return NGX_CONF_ERROR;
}


static char *
ngx_http_core_error_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    // 设置错误日志
    return ngx_log_set_log(cf, &clcf->error_log);
}


// 处理keepalive指令的函数
static char *
ngx_http_core_keepalive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t  *value;

    // 检查是否已经设置过keepalive_timeout
    if (clcf->keepalive_timeout != NGX_CONF_UNSET_MSEC) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 解析第一个参数作为keepalive超时时间
    clcf->keepalive_timeout = ngx_parse_time(&value[1], 0);

    // 检查解析结果是否有效
    if (clcf->keepalive_timeout == (ngx_msec_t) NGX_ERROR) {
        return "invalid value";
    }

    // 如果只有一个参数，直接返回成功
    if (cf->args->nelts == 2) {
        return NGX_CONF_OK;
    }

    // 解析第二个参数作为keepalive头部超时时间
    clcf->keepalive_header = ngx_parse_time(&value[2], 1);

    // 检查解析结果是否有效
    if (clcf->keepalive_header == (time_t) NGX_ERROR) {
        return "invalid value";
    }

    return NGX_CONF_OK;
}


// 处理internal指令的函数
static char *
ngx_http_core_internal(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    // 检查是否已经设置过internal
    if (clcf->internal != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    // 设置internal标志为1
    clcf->internal = 1;

    return NGX_CONF_OK;
}


// 处理resolver指令的函数
static char *
ngx_http_core_resolver(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf = conf;

    ngx_str_t  *value;

    // 检查是否已经设置过resolver
    if (clcf->resolver) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 创建resolver
    clcf->resolver = ngx_resolver_create(cf, &value[1], cf->args->nelts - 1);
    if (clcf->resolver == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


#if (NGX_HTTP_GZIP)

// 处理gzip_disable指令的函数
static char *
ngx_http_gzip_disable(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf = conf;

#if (NGX_PCRE)

    ngx_str_t            *value;
    ngx_uint_t            i;
    ngx_regex_elt_t      *re;
    ngx_regex_compile_t   rc;
    u_char                errstr[NGX_MAX_CONF_ERRSTR];

    // 初始化gzip_disable数组
    if (clcf->gzip_disable == NGX_CONF_UNSET_PTR) {
        clcf->gzip_disable = ngx_array_create(cf->pool, 2,
                                              sizeof(ngx_regex_elt_t));
        if (clcf->gzip_disable == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pool = cf->pool;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

    // 遍历所有参数
    for (i = 1; i < cf->args->nelts; i++) {

        // 处理msie6特殊情况
        if (ngx_strcmp(value[i].data, "msie6") == 0) {
            clcf->gzip_disable_msie6 = 1;
            continue;
        }

#if (NGX_HTTP_DEGRADATION)

        // 处理degradation特殊情况
        if (ngx_strcmp(value[i].data, "degradation") == 0) {
            clcf->gzip_disable_degradation = 1;
            continue;
        }

#endif

        // 为其他情况编译正则表达式
        re = ngx_array_push(clcf->gzip_disable);
        if (re == NULL) {
            return NGX_CONF_ERROR;
        }

        rc.pattern = value[i];
        rc.options = NGX_REGEX_CASELESS;

        if (ngx_regex_compile(&rc) != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &rc.err);
            return NGX_CONF_ERROR;
        }

        re->regex = rc.regex;
        re->name = value[i].data;
    }

    return NGX_CONF_OK;

#else
    ngx_str_t   *value;
    ngx_uint_t   i;

    value = cf->args->elts;

    // 在没有PCRE库的情况下，只支持msie6和degradation
    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strcmp(value[i].data, "msie6") == 0) {
            clcf->gzip_disable_msie6 = 1;
            continue;
        }

#if (NGX_HTTP_DEGRADATION)

        if (ngx_strcmp(value[i].data, "degradation") == 0) {
            clcf->gzip_disable_degradation = 1;
            continue;
        }

#endif

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "without PCRE library \"gzip_disable\" supports "
                           "builtin \"msie6\" and \"degradation\" mask only");

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;

#endif
}

#endif


#if (NGX_HAVE_OPENAT)

// 处理disable_symlinks指令的函数
static char *
ngx_http_disable_symlinks(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t                         *value;
    ngx_uint_t                         i;
    ngx_http_compile_complex_value_t   ccv;

    // 检查是否已经设置过disable_symlinks
    if (clcf->disable_symlinks != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;

    // 遍历所有参数
    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strcmp(value[i].data, "off") == 0) {
            clcf->disable_symlinks = NGX_DISABLE_SYMLINKS_OFF;
            continue;
        }

        if (ngx_strcmp(value[i].data, "if_not_owner") == 0) {
            clcf->disable_symlinks = NGX_DISABLE_SYMLINKS_NOTOWNER;
            continue;
        }

        if (ngx_strcmp(value[i].data, "on") == 0) {
            clcf->disable_symlinks = NGX_DISABLE_SYMLINKS_ON;
            continue;
        }

        // 处理from=参数
        if (ngx_strncmp(value[i].data, "from=", 5) == 0) {
            value[i].len -= 5;
            value[i].data += 5;

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

            ccv.cf = cf;
            ccv.value = &value[i];
            ccv.complex_value = ngx_palloc(cf->pool,
                                           sizeof(ngx_http_complex_value_t));
            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            clcf->disable_symlinks_from = ccv.complex_value;

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    // 检查是否设置了有效的disable_symlinks选项
    if (clcf->disable_symlinks == NGX_CONF_UNSET_UINT) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"off\", \"on\" "
                           "or \"if_not_owner\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    // 如果只有一个参数，直接返回成功
    if (cf->args->nelts == 2) {
        clcf->disable_symlinks_from = NULL;
        return NGX_CONF_OK;
    }

    // 检查是否重复设置了from参数
    if (clcf->disable_symlinks_from == NGX_CONF_UNSET_PTR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate parameters \"%V %V\"",
                           &value[1], &value[2]);
        return NGX_CONF_ERROR;
    }

    // 检查from参数是否与off选项一起使用
    if (clcf->disable_symlinks == NGX_DISABLE_SYMLINKS_OFF) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"from=\" cannot be used with \"off\" parameter");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

#endif


// 检查send_lowat配置的回调函数
static char *
ngx_http_core_lowat_check(ngx_conf_t *cf, void *post, void *data)
{
#if (NGX_FREEBSD)
    ssize_t *np = data;

    // 在FreeBSD上，检查send_lowat是否小于系统设置的net.inet.tcp.sendspace
    if ((u_long) *np >= ngx_freebsd_net_inet_tcp_sendspace) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"send_lowat\" must be less than %d "
                           "(sysctl net.inet.tcp.sendspace)",
                           ngx_freebsd_net_inet_tcp_sendspace);

        return NGX_CONF_ERROR;
    }

#elif !(NGX_HAVE_SO_SNDLOWAT)
    ssize_t *np = data;

    // 如果系统不支持SO_SNDLOWAT，记录警告并忽略设置
    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"send_lowat\" is not supported, ignored");

    *np = 0;

#endif

    return NGX_CONF_OK;
}


// 检查client_body_buffer_size配置的回调函数
static char *
ngx_http_core_pool_size(ngx_conf_t *cf, void *post, void *data)
{
    size_t *sp = data;

    // 检查pool size是否小于最小允许值
    if (*sp < NGX_MIN_POOL_SIZE) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the pool size must be no less than %uz",
                           NGX_MIN_POOL_SIZE);
        return NGX_CONF_ERROR;
    }

    // 检查pool size是否是NGX_POOL_ALIGNMENT的倍数
    if (*sp % NGX_POOL_ALIGNMENT) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the pool size must be a multiple of %uz",
                           NGX_POOL_ALIGNMENT);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
