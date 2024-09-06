
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_PIPE_H_INCLUDED_
#define _NGX_EVENT_PIPE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


/* 定义事件管道结构体的前向声明 */
typedef struct ngx_event_pipe_s  ngx_event_pipe_t;

/* 定义输入过滤器函数指针类型 */
typedef ngx_int_t (*ngx_event_pipe_input_filter_pt)(ngx_event_pipe_t *p,
                                                    ngx_buf_t *buf);
/* 定义输出过滤器函数指针类型 */
typedef ngx_int_t (*ngx_event_pipe_output_filter_pt)(void *data,
                                                     ngx_chain_t *chain);


/* 事件管道结构体定义 */
struct ngx_event_pipe_s {
    ngx_connection_t  *upstream;    /* 上游连接 */
    ngx_connection_t  *downstream;  /* 下游连接 */

    ngx_chain_t       *free_raw_bufs;  /* 空闲的原始缓冲区链 */
    ngx_chain_t       *in;             /* 输入缓冲区链 */
    ngx_chain_t      **last_in;        /* 指向最后一个输入缓冲区的指针 */

    ngx_chain_t       *writing;        /* 正在写入的缓冲区链 */

    ngx_chain_t       *out;            /* 输出缓冲区链 */
    ngx_chain_t       *free;           /* 空闲缓冲区链 */
    ngx_chain_t       *busy;           /* 忙碌的缓冲区链 */

    /*
     * 输入过滤器，用于将HTTP/1.1块从原始缓冲区移动到输入链中
     */
    ngx_event_pipe_input_filter_pt    input_filter;  /* 输入过滤器函数指针 */
    void                             *input_ctx;     /* 输入过滤器的上下文 */

    /* 输出过滤器及其上下文 */
    ngx_event_pipe_output_filter_pt   output_filter;  /* 输出过滤器函数指针 */
    void                             *output_ctx;     /* 输出过滤器的上下文 */

#if (NGX_THREADS || NGX_COMPAT)
    /* 线程处理相关 */
    /* 线程处理函数指针，用于处理异步I/O任务 */
    ngx_int_t                       (*thread_handler)(ngx_thread_task_t *task,
                                                      ngx_file_t *file);
    
    /* 线程处理的上下文数据 */
    void                             *thread_ctx;
    
    /* 当前正在执行的线程任务 */
    ngx_thread_task_t                *thread_task;
#endif

    /* 各种标志位 */
    unsigned           read:1;              /* 是否可读 */
    unsigned           cacheable:1;         /* 是否可缓存 */
    unsigned           single_buf:1;        /* 是否使用单一缓冲区 */
    unsigned           free_bufs:1;         /* 是否有空闲缓冲区 */
    unsigned           upstream_done:1;     /* 上游是否完成 */
    unsigned           upstream_error:1;    /* 上游是否出错 */
    unsigned           upstream_eof:1;      /* 上游是否到达EOF */
    unsigned           upstream_blocked:1;  /* 上游是否被阻塞 */
    unsigned           downstream_done:1;   /* 下游是否完成 */
    unsigned           downstream_error:1;  /* 下游是否出错 */
    unsigned           cyclic_temp_file:1;  /* 是否使用循环临时文件 */
    unsigned           aio:1;               /* 是否使用异步I/O */

    ngx_int_t          allocated;    /* 已分配的缓冲区数量 */
    ngx_bufs_t         bufs;         /* 缓冲区配置 */
    ngx_buf_tag_t      tag;          /* 缓冲区标签 */

    ssize_t            busy_size;    /* 忙碌缓冲区的总大小 */

    off_t              read_length;  /* 已读取的数据长度 */
    off_t              length;       /* 总数据长度 */

    off_t              max_temp_file_size;    /* 临时文件最大大小 */
    ssize_t            temp_file_write_size;  /* 临时文件写入大小 */

    ngx_msec_t         read_timeout;   /* 读取超时时间 */
    ngx_msec_t         send_timeout;   /* 发送超时时间 */
    ssize_t            send_lowat;     /* 发送低水位标记 */

    ngx_pool_t        *pool;    /* 内存池 */
    ngx_log_t         *log;     /* 日志对象 */

    ngx_chain_t       *preread_bufs;  /* 预读缓冲区链 */
    size_t             preread_size;  /* 预读大小 */
    ngx_buf_t         *buf_to_file;   /* 待写入文件的缓冲区 */

    size_t             limit_rate;    /* 限速大小 */
    time_t             start_sec;     /* 开始时间 */

    ngx_temp_file_t   *temp_file;     /* 临时文件 */

    /* STUB */ int     num;           /* 调试用的编号 */
};


/* 事件管道处理函数 */
ngx_int_t ngx_event_pipe(ngx_event_pipe_t *p, ngx_int_t do_write);
/* 复制输入过滤器函数 */
ngx_int_t ngx_event_pipe_copy_input_filter(ngx_event_pipe_t *p, ngx_buf_t *buf);
/* 添加空闲缓冲区函数 */
ngx_int_t ngx_event_pipe_add_free_buf(ngx_event_pipe_t *p, ngx_buf_t *b);


#endif /* _NGX_EVENT_PIPE_H_INCLUDED_ */
