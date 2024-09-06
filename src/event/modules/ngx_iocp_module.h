
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_IOCP_MODULE_H_INCLUDED_
#define _NGX_IOCP_MODULE_H_INCLUDED_


/**
 * @brief IOCP模块配置结构体
 */
typedef struct {
    int  threads;        /**< IOCP线程池中的线程数量 */
    int  post_acceptex;  /**< 是否使用AcceptEx函数进行异步接受连接 */
    int  acceptex_read;  /**< 是否在AcceptEx调用时预读取数据 */
} ngx_iocp_conf_t;


/**
 * @brief IOCP模块的外部声明
 */
extern ngx_module_t  ngx_iocp_module;


#endif /* _NGX_IOCP_MODULE_H_INCLUDED_ */
