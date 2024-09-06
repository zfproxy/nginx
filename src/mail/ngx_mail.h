
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MAIL_H_INCLUDED_
#define _NGX_MAIL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>

#if (NGX_MAIL_SSL)
#include <ngx_mail_ssl_module.h>
#endif



/**
 * @brief 邮件服务器配置上下文结构体
 */
typedef struct {
    void                  **main_conf;  // 主配置指针数组
    void                  **srv_conf;   // 服务器配置指针数组
} ngx_mail_conf_ctx_t;


/**
 * @brief 邮件服务器监听配置结构体
 */
typedef struct {
    struct sockaddr        *sockaddr;    // 套接字地址
    socklen_t               socklen;     // 套接字地址长度
    ngx_str_t               addr_text;   // 地址文本表示

    /* server ctx */
    ngx_mail_conf_ctx_t    *ctx;         // 服务器配置上下文

    unsigned                bind:1;      // 是否绑定
    unsigned                wildcard:1;  // 是否使用通配符
    unsigned                ssl:1;       // 是否启用SSL
#if (NGX_HAVE_INET6)
    unsigned                ipv6only:1;  // 是否仅支持IPv6
#endif
    unsigned                so_keepalive:2;  // SO_KEEPALIVE选项
    unsigned                proxy_protocol:1;  // 是否启用代理协议
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int                     tcp_keepidle;   // TCP keepalive空闲时间
    int                     tcp_keepintvl;  // TCP keepalive间隔
    int                     tcp_keepcnt;    // TCP keepalive探测次数
#endif
    int                     backlog;     // 监听队列长度
    int                     rcvbuf;      // 接收缓冲区大小
    int                     sndbuf;      // 发送缓冲区大小
} ngx_mail_listen_t;


/**
 * @brief 邮件服务器地址配置结构体
 */
typedef struct {
    ngx_mail_conf_ctx_t    *ctx;         // 配置上下文
    ngx_str_t               addr_text;   // 地址文本表示
    unsigned                ssl:1;       // 是否启用SSL
    unsigned                proxy_protocol:1;  // 是否启用代理协议
} ngx_mail_addr_conf_t;

/**
 * @brief IPv4地址配置结构体
 */
typedef struct {
    in_addr_t               addr;        // IPv4地址
    ngx_mail_addr_conf_t    conf;        // 地址配置
} ngx_mail_in_addr_t;


#if (NGX_HAVE_INET6)

/**
 * @brief IPv6地址配置结构体
 */
typedef struct {
    struct in6_addr         addr6;       // IPv6地址
    ngx_mail_addr_conf_t    conf;        // 地址配置
} ngx_mail_in6_addr_t;

#endif


/**
 * @brief 邮件服务器端口配置结构体
 */
typedef struct {
    /* ngx_mail_in_addr_t or ngx_mail_in6_addr_t */
    void                   *addrs;       // 地址数组
    ngx_uint_t              naddrs;      // 地址数量
} ngx_mail_port_t;


/**
 * @brief 邮件服务器配置端口结构体
 */
typedef struct {
    int                     family;      // 地址族
    in_port_t               port;        // 端口号
    ngx_array_t             addrs;       /* array of ngx_mail_conf_addr_t */
} ngx_mail_conf_port_t;


/**
 * @brief 邮件服务器配置地址结构体
 */
typedef struct {
    ngx_mail_listen_t       opt;         // 监听选项
} ngx_mail_conf_addr_t;


/**
 * @brief 邮件核心模块主配置结构体
 */
typedef struct {
    ngx_array_t             servers;     /* ngx_mail_core_srv_conf_t */
    ngx_array_t             listen;      /* ngx_mail_listen_t */
} ngx_mail_core_main_conf_t;


/**
 * @brief 邮件协议类型定义
 */
#define NGX_MAIL_POP3_PROTOCOL  0  // POP3协议
#define NGX_MAIL_IMAP_PROTOCOL  1  // IMAP协议
#define NGX_MAIL_SMTP_PROTOCOL  2  // SMTP协议


/**
 * @brief 邮件协议结构体前向声明
 */
typedef struct ngx_mail_protocol_s  ngx_mail_protocol_t;


/**
 * @brief 邮件核心模块服务器配置结构体
 */
typedef struct {
    ngx_mail_protocol_t    *protocol;    // 协议

    ngx_msec_t              timeout;     // 超时时间
    ngx_msec_t              resolver_timeout;  // 解析器超时时间

    ngx_uint_t              max_errors;  // 最大错误次数

    ngx_str_t               server_name;  // 服务器名称

    u_char                 *file_name;   // 配置文件名
    ngx_uint_t              line;        // 配置行号

    ngx_resolver_t         *resolver;    // 解析器
    ngx_log_t              *error_log;   // 错误日志

    /* server ctx */
    ngx_mail_conf_ctx_t    *ctx;         // 服务器配置上下文

    ngx_uint_t              listen;  /* unsigned  listen:1; */
} ngx_mail_core_srv_conf_t;


/**
 * @brief POP3协议状态枚举
 */
typedef enum {
    ngx_pop3_start = 0,                  // POP3会话初始状态
    ngx_pop3_user,                       // 用户名已输入，等待密码
    ngx_pop3_passwd,                     // 密码已输入，等待验证
    ngx_pop3_auth_login_username,        // AUTH LOGIN认证，等待用户名
    ngx_pop3_auth_login_password,        // AUTH LOGIN认证，等待密码
    ngx_pop3_auth_plain,                 // AUTH PLAIN认证状态
    ngx_pop3_auth_cram_md5,              // AUTH CRAM-MD5认证状态
    ngx_pop3_auth_external               // AUTH EXTERNAL认证状态
} ngx_pop3_state_e;


/**
 * @brief IMAP协议状态枚举
 */
typedef enum {
    ngx_imap_start = 0,                  // IMAP会话初始状态
    ngx_imap_auth_login_username,        // AUTH LOGIN认证，等待用户名
    ngx_imap_auth_login_password,        // AUTH LOGIN认证，等待密码
    ngx_imap_auth_plain,                 // AUTH PLAIN认证状态
    ngx_imap_auth_cram_md5,              // AUTH CRAM-MD5认证状态
    ngx_imap_auth_external,              // AUTH EXTERNAL认证状态
    ngx_imap_login,                      // LOGIN命令状态
    ngx_imap_user,                       // USER命令状态
    ngx_imap_passwd                      // 密码输入状态
} ngx_imap_state_e;


/**
 * @brief SMTP协议状态枚举
 */
typedef enum {
    ngx_smtp_start = 0,                  // SMTP会话初始状态
    ngx_smtp_auth_login_username,        // AUTH LOGIN认证，等待用户名
    ngx_smtp_auth_login_password,        // AUTH LOGIN认证，等待密码
    ngx_smtp_auth_plain,                 // AUTH PLAIN认证状态
    ngx_smtp_auth_cram_md5,              // AUTH CRAM-MD5认证状态
    ngx_smtp_auth_external,              // AUTH EXTERNAL认证状态
    ngx_smtp_helo,                       // HELO命令状态
    ngx_smtp_helo_xclient,               // HELO命令后的XCLIENT状态
    ngx_smtp_helo_auth,                  // HELO命令后的认证状态
    ngx_smtp_helo_from,                  // HELO命令后的FROM状态
    ngx_smtp_xclient,                    // XCLIENT命令状态
    ngx_smtp_xclient_from,               // XCLIENT命令后的FROM状态
    ngx_smtp_xclient_helo,               // XCLIENT命令后的HELO状态
    ngx_smtp_xclient_auth,               // XCLIENT命令后的认证状态
    ngx_smtp_from,                       // FROM命令状态
    ngx_smtp_to                          // TO命令状态
} ngx_smtp_state_e;


/**
 * @brief 邮件代理上下文结构体
 */
typedef struct {
    ngx_peer_connection_t   upstream;    // 上游连接
    ngx_buf_t              *buffer;      // 缓冲区
    ngx_uint_t              proxy_protocol;  /* unsigned  proxy_protocol:1; */
} ngx_mail_proxy_ctx_t;



typedef struct {
    uint32_t                signature;         /* "MAIL" */

    ngx_connection_t       *connection;        // 与客户端的连接

    ngx_str_t               out;               // 输出缓冲区
    ngx_buf_t              *buffer;            // 输入缓冲区

    void                  **ctx;               // 模块上下文
    void                  **main_conf;         // 主配置
    void                  **srv_conf;          // 服务器配置

    ngx_resolver_ctx_t     *resolver_ctx;      // 解析器上下文

    ngx_mail_proxy_ctx_t   *proxy;             // 代理上下文

    ngx_uint_t              mail_state;        // 邮件会话状态

    unsigned                ssl:1;             // 是否使用SSL
    unsigned                protocol:3;        // 协议类型
    unsigned                blocked:1;         // 是否被阻塞
    unsigned                quit:1;            // 是否退出
    unsigned                quoted:1;          // 是否引用
    unsigned                backslash:1;       // 是否反斜杠
    unsigned                no_sync_literal:1; // 是否非同步字面量
    unsigned                starttls:1;        // 是否启用STARTTLS
    unsigned                esmtp:1;           // 是否ESMTP
    unsigned                auth_method:3;     // 认证方法
    unsigned                auth_wait:1;       // 是否等待认证

    ngx_str_t               login;             // 登录用户名
    ngx_str_t               passwd;            // 登录密码

    ngx_str_t               salt;              // 盐值
    ngx_str_t               tag;               // 标签
    ngx_str_t               tagged_line;       // 带标签的行
    ngx_str_t               text;              // 文本内容

    ngx_str_t              *addr_text;         // 地址文本
    ngx_str_t               host;              // 主机名
    ngx_str_t               smtp_helo;         // SMTP HELO命令
    ngx_str_t               smtp_from;         // SMTP FROM命令
    ngx_str_t               smtp_to;           // SMTP TO命令

    ngx_str_t               cmd;               // 当前命令

    ngx_uint_t              command;           // 命令类型
    ngx_array_t             args;              // 命令参数

    ngx_uint_t              errors;            // 错误计数
    ngx_uint_t              login_attempt;     // 登录尝试次数

    /* used to parse POP3/IMAP/SMTP command */

    ngx_uint_t              state;             // 解析状态
    u_char                 *tag_start;         // 标签起始位置
    u_char                 *cmd_start;         // 命令起始位置
    u_char                 *arg_start;         // 参数起始位置
    ngx_uint_t              literal_len;       // 字面量长度
} ngx_mail_session_t;


// 邮件日志上下文结构体
typedef struct {
    ngx_str_t              *client;    // 客户端信息
    ngx_mail_session_t     *session;   // 邮件会话信息
} ngx_mail_log_ctx_t;


// POP3命令定义
#define NGX_POP3_USER          1   // 用户名命令
#define NGX_POP3_PASS          2   // 密码命令
#define NGX_POP3_CAPA          3   // 能力查询命令
#define NGX_POP3_QUIT          4   // 退出命令
#define NGX_POP3_NOOP          5   // 空操作命令
#define NGX_POP3_STLS          6   // 启动TLS命令
#define NGX_POP3_APOP          7   // APOP认证命令
#define NGX_POP3_AUTH          8   // 认证命令
#define NGX_POP3_STAT          9   // 邮箱状态命令
#define NGX_POP3_LIST          10  // 邮件列表命令
#define NGX_POP3_RETR          11  // 获取邮件命令
#define NGX_POP3_DELE          12  // 删除邮件命令
#define NGX_POP3_RSET          13  // 重置命令
#define NGX_POP3_TOP           14  // 获取邮件头和部分内容命令
#define NGX_POP3_UIDL          15  // 获取邮件唯一标识符命令


// IMAP命令定义
#define NGX_IMAP_LOGIN         1   // 登录命令
#define NGX_IMAP_LOGOUT        2   // 登出命令
#define NGX_IMAP_CAPABILITY    3   // 能力查询命令
#define NGX_IMAP_NOOP          4   // 空操作命令
#define NGX_IMAP_STARTTLS      5   // 启动TLS命令

#define NGX_IMAP_NEXT          6   // 下一个命令

#define NGX_IMAP_AUTHENTICATE  7   // 认证命令


// SMTP命令定义
#define NGX_SMTP_HELO          1   // HELO命令
#define NGX_SMTP_EHLO          2   // EHLO命令
#define NGX_SMTP_AUTH          3   // 认证命令
#define NGX_SMTP_QUIT          4   // 退出命令
#define NGX_SMTP_NOOP          5   // 空操作命令
#define NGX_SMTP_MAIL          6   // 邮件发送命令
#define NGX_SMTP_RSET          7   // 重置命令
#define NGX_SMTP_RCPT          8   // 收件人命令
#define NGX_SMTP_DATA          9   // 数据命令
#define NGX_SMTP_VRFY          10  // 验证命令
#define NGX_SMTP_EXPN          11  // 扩展命令
#define NGX_SMTP_HELP          12  // 帮助命令
#define NGX_SMTP_STARTTLS      13  // 启动TLS命令


// 邮件认证方法定义
#define NGX_MAIL_AUTH_PLAIN             0   // 明文认证
#define NGX_MAIL_AUTH_LOGIN             1   // 登录认证
#define NGX_MAIL_AUTH_LOGIN_USERNAME    2   // 登录用户名认证
#define NGX_MAIL_AUTH_APOP              3   // APOP认证
#define NGX_MAIL_AUTH_CRAM_MD5          4   // CRAM-MD5认证
#define NGX_MAIL_AUTH_EXTERNAL          5   // 外部认证
#define NGX_MAIL_AUTH_NONE              6   // 无认证


// 邮件认证方法启用标志
#define NGX_MAIL_AUTH_PLAIN_ENABLED     0x0002  // 明文认证启用
#define NGX_MAIL_AUTH_LOGIN_ENABLED     0x0004  // 登录认证启用
#define NGX_MAIL_AUTH_APOP_ENABLED      0x0008  // APOP认证启用
#define NGX_MAIL_AUTH_CRAM_MD5_ENABLED  0x0010  // CRAM-MD5认证启用
#define NGX_MAIL_AUTH_EXTERNAL_ENABLED  0x0020  // 外部认证启用
#define NGX_MAIL_AUTH_NONE_ENABLED      0x0040  // 无认证启用


// 邮件解析无效命令标志
#define NGX_MAIL_PARSE_INVALID_COMMAND  20


// 定义邮件会话初始化函数指针类型
typedef void (*ngx_mail_init_session_pt)(ngx_mail_session_t *s,
    ngx_connection_t *c);
// 定义邮件协议初始化函数指针类型
typedef void (*ngx_mail_init_protocol_pt)(ngx_event_t *rev);
// 定义邮件认证状态处理函数指针类型
typedef void (*ngx_mail_auth_state_pt)(ngx_event_t *rev);
// 定义邮件命令解析函数指针类型
typedef ngx_int_t (*ngx_mail_parse_command_pt)(ngx_mail_session_t *s);


// 邮件协议结构体定义
struct ngx_mail_protocol_s {
    ngx_str_t                   name;           // 协议名称
    ngx_str_t                   alpn;           // ALPN（应用层协议协商）标识符
    in_port_t                   port[4];        // 支持的端口号数组
    ngx_uint_t                  type;           // 协议类型

    ngx_mail_init_session_pt    init_session;   // 会话初始化函数
    ngx_mail_init_protocol_pt   init_protocol;  // 协议初始化函数
    ngx_mail_parse_command_pt   parse_command;  // 命令解析函数
    ngx_mail_auth_state_pt      auth_state;     // 认证状态处理函数

    ngx_str_t                   internal_server_error;  // 内部服务器错误消息
    ngx_str_t                   cert_error;             // 证书错误消息
    ngx_str_t                   no_cert;                // 无证书错误消息
};


// 邮件模块结构体定义
typedef struct {
    ngx_mail_protocol_t        *protocol;  // 指向邮件协议结构体的指针

    // 创建主配置函数指针
    void                       *(*create_main_conf)(ngx_conf_t *cf);
    // 初始化主配置函数指针
    char                       *(*init_main_conf)(ngx_conf_t *cf, void *conf);

    // 创建服务器配置函数指针
    void                       *(*create_srv_conf)(ngx_conf_t *cf);
    // 合并服务器配置函数指针
    char                       *(*merge_srv_conf)(ngx_conf_t *cf, void *prev,
                                                  void *conf);
} ngx_mail_module_t;


// 定义邮件模块的标识符
#define NGX_MAIL_MODULE         0x4C49414D     /* "MAIL" */

// 定义邮件主配置和服务器配置的标识符
#define NGX_MAIL_MAIN_CONF      0x02000000
#define NGX_MAIL_SRV_CONF       0x04000000

// 定义获取邮件配置上下文中主配置和服务器配置的偏移量
#define NGX_MAIL_MAIN_CONF_OFFSET  offsetof(ngx_mail_conf_ctx_t, main_conf)
#define NGX_MAIL_SRV_CONF_OFFSET   offsetof(ngx_mail_conf_ctx_t, srv_conf)

// 定义获取、设置和删除邮件会话上下文的宏
#define ngx_mail_get_module_ctx(s, module)     (s)->ctx[module.ctx_index]
#define ngx_mail_set_ctx(s, c, module)         s->ctx[module.ctx_index] = c;
#define ngx_mail_delete_ctx(s, module)         s->ctx[module.ctx_index] = NULL;

// 定义获取邮件会话主配置和服务器配置的宏
#define ngx_mail_get_module_main_conf(s, module)                             \
    (s)->main_conf[module.ctx_index]
#define ngx_mail_get_module_srv_conf(s, module)  (s)->srv_conf[module.ctx_index]

// 定义获取邮件配置上下文中主配置和服务器配置的宏
#define ngx_mail_conf_get_module_main_conf(cf, module)                       \
    ((ngx_mail_conf_ctx_t *) cf->ctx)->main_conf[module.ctx_index]
#define ngx_mail_conf_get_module_srv_conf(cf, module)                        \
    ((ngx_mail_conf_ctx_t *) cf->ctx)->srv_conf[module.ctx_index]

// 如果启用了邮件SSL支持
#if (NGX_MAIL_SSL)
// 声明STARTTLS处理函数
void ngx_mail_starttls_handler(ngx_event_t *rev);
// 声明仅STARTTLS函数
ngx_int_t ngx_mail_starttls_only(ngx_mail_session_t *s, ngx_connection_t *c);
#endif

// 声明邮件连接初始化函数
void ngx_mail_init_connection(ngx_connection_t *c);

// 声明各种邮件认证相关函数
// 生成邮件认证盐值
ngx_int_t ngx_mail_salt(ngx_mail_session_t *s, ngx_connection_t *c,
    ngx_mail_core_srv_conf_t *cscf);

// 处理PLAIN认证方法
ngx_int_t ngx_mail_auth_plain(ngx_mail_session_t *s, ngx_connection_t *c,
    ngx_uint_t n);

// 处理LOGIN认证方法的用户名阶段
ngx_int_t ngx_mail_auth_login_username(ngx_mail_session_t *s,
    ngx_connection_t *c, ngx_uint_t n);

// 处理LOGIN认证方法的密码阶段
ngx_int_t ngx_mail_auth_login_password(ngx_mail_session_t *s,
    ngx_connection_t *c);

// 生成CRAM-MD5认证方法的盐值
ngx_int_t ngx_mail_auth_cram_md5_salt(ngx_mail_session_t *s,
    ngx_connection_t *c, char *prefix, size_t len);

// 处理CRAM-MD5认证方法
ngx_int_t ngx_mail_auth_cram_md5(ngx_mail_session_t *s, ngx_connection_t *c);

// 处理EXTERNAL认证方法
ngx_int_t ngx_mail_auth_external(ngx_mail_session_t *s, ngx_connection_t *c,
    ngx_uint_t n);

// 解析邮件认证信息
ngx_int_t ngx_mail_auth_parse(ngx_mail_session_t *s, ngx_connection_t *c);

// 声明邮件发送函数
void ngx_mail_send(ngx_event_t *wev);
// 声明邮件命令读取函数
ngx_int_t ngx_mail_read_command(ngx_mail_session_t *s, ngx_connection_t *c);
// 声明邮件认证函数
void ngx_mail_auth(ngx_mail_session_t *s, ngx_connection_t *c);
// 声明关闭邮件连接函数
void ngx_mail_close_connection(ngx_connection_t *c);
// 声明邮件会话内部服务器错误处理函数
void ngx_mail_session_internal_server_error(ngx_mail_session_t *s);
// 声明邮件日志错误函数
u_char *ngx_mail_log_error(ngx_log_t *log, u_char *buf, size_t len);

// 声明邮件功能配置函数
char *ngx_mail_capabilities(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

// STUB（存根）函数声明
// 初始化邮件代理会话
void ngx_mail_proxy_init(ngx_mail_session_t *s, ngx_addr_t *peer);

// 初始化HTTP认证过程
void ngx_mail_auth_http_init(ngx_mail_session_t *s);

// 处理真实IP地址
ngx_int_t ngx_mail_realip_handler(ngx_mail_session_t *s);

// 声明邮件模块最大数量和邮件核心模块
extern ngx_uint_t    ngx_mail_max_module;
extern ngx_module_t  ngx_mail_core_module;


#endif /* _NGX_MAIL_H_INCLUDED_ */
