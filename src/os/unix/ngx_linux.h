
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_LINUX_H_INCLUDED_
#define _NGX_LINUX_H_INCLUDED_


/**
 * @brief 使用Linux的sendfile系统调用发送文件链
 * @param c 连接对象指针
 * @param in 输入链表
 * @param limit 发送限制
 * @return 剩余未发送的链表，NULL表示全部发送完成，NGX_CHAIN_ERROR表示错误
 */
ngx_chain_t *ngx_linux_sendfile_chain(ngx_connection_t *c, ngx_chain_t *in,
    off_t limit);


#endif /* _NGX_LINUX_H_INCLUDED_ */
