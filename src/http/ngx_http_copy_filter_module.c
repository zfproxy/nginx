
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


/*
 * ngx_http_copy_filter_module.c
 *
 * 该模块实现了Nginx的复制过滤器功能，用于处理和优化HTTP响应的数据传输。
 *
 * 支持的功能:
 * 1. 高效的内存到内存和文件到内存的数据复制
 * 2. 支持异步I/O操作（如果系统支持）
 * 3. 支持多线程处理（如果启用）
 * 4. 自适应输出缓冲区管理
 *
 * 支持的指令:
 * - output_buffers: 设置输出缓冲区的数量和大小
 *   语法: output_buffers number size;
 *   默认值: output_buffers 1 32k;
 *   上下文: http, server, location
 *
 * 相关变量:
 * 本模块不直接提供可在配置中使用的变量。
 *
 * 使用注意点:
 * 1. 合理配置output_buffers可以优化内存使用和响应速度
 * 2. 在处理大文件时，考虑启用异步I/O和多线程支持以提高性能
 * 3. 该模块通常不需要直接配置，作为Nginx核心功能自动运行
 * 4. 在调试性能问题时，可能需要关注此模块的行为
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/**
 * @brief 定义复制过滤器配置结构体
 *
 * 这个结构体用于存储复制过滤器模块的配置信息。
 * 它包含了与输出缓冲区相关的设置，这些设置影响了
 * Nginx如何处理和传输响应数据。
 */
typedef struct {
    ngx_bufs_t  bufs;
} ngx_http_copy_filter_conf_t;


#if (NGX_HAVE_FILE_AIO)
/**
 * @brief 异步I/O复制处理函数
 *
 * @param ctx 输出链上下文
 * @param file 文件对象（在函数声明中未显示，但可能在函数体中使用）
 *
 * 该函数用于处理异步I/O操作的复制过程。
 * 它在支持文件异步I/O的系统上使用，以提高文件传输的效率。
 */
static void ngx_http_copy_aio_handler(ngx_output_chain_ctx_t *ctx,
    ngx_file_t *file);
/**
 * @brief 异步I/O事件处理函数
 *
 * @param ev 事件对象指针
 *
 * 该函数用于处理异步I/O操作完成后的事件。
 * 它在异步I/O操作完成时被调用，用于继续处理复制过滤器的后续操作。
 * 这个函数是异步I/O机制的一个重要组成部分，确保了在I/O操作完成后能够及时响应和处理结果。
 */
static void ngx_http_copy_aio_event_handler(ngx_event_t *ev);
#endif
#if (NGX_THREADS)
/**
 * @brief 复制过滤器的线程处理函数
 *
 * @param task 线程任务结构体指针
 * @param file 文件对象指针（在函数声明中未显示，但可能在函数体中使用）
 * @return ngx_int_t 返回处理结果的状态码
 *
 * 该函数用于在多线程环境下处理复制过滤器的操作。
 * 它作为一个线程任务被执行，用于处理可能耗时的I/O操作，
 * 以避免阻塞主线程。这有助于提高Nginx在处理大文件或高并发
 * 场景下的性能。
 */
static ngx_int_t ngx_http_copy_thread_handler(ngx_thread_task_t *task,
    ngx_file_t *file);
/**
 * @brief 复制过滤器的线程事件处理函数
 *
 * @param ev 事件对象指针
 *
 * 该函数用于处理复制过滤器中与线程相关的事件。
 * 在多线程环境下，当线程完成复制操作后，
 * 这个函数会被调用来处理后续的事件和操作。
 * 它是确保线程安全和正确处理异步操作结果的关键部分。
 */
static void ngx_http_copy_thread_event_handler(ngx_event_t *ev);
#endif

/**
 * @brief 创建复制过滤器的配置结构
 *
 * @param cf Nginx配置对象指针
 * @return void* 返回新创建的配置结构指针
 *
 * 该函数用于为复制过滤器模块创建配置结构。
 * 它在Nginx启动时被调用，为每个位置块分配并初始化一个新的配置结构。
 * 这个配置结构用于存储复制过滤器的相关设置，如缓冲区大小等。
 */
static void *ngx_http_copy_filter_create_conf(ngx_conf_t *cf);
/**
 * @brief 合并复制过滤器的配置
 *
 * @param cf Nginx配置对象指针
 * @param parent 父配置结构指针
 * @param child 子配置结构指针
 * @return char* 返回成功时的NULL，或失败时的错误信息字符串
 *
 * 该函数用于合并复制过滤器的父子配置。
 * 它在Nginx配置阶段被调用，用于将父配置（如server块）和子配置（如location块）
 * 的设置合并，确保正确的配置继承和覆盖。
 * 这个过程对于实现配置的层次结构和灵活性至关重要。
 */
static char *ngx_http_copy_filter_merge_conf(ngx_conf_t *cf,
    void *parent, void *child);
/**
 * @brief 初始化复制过滤器
 *
 * @param cf Nginx配置对象指针
 * @return ngx_int_t 返回初始化结果的状态码
 *
 * 该函数用于初始化复制过滤器模块。
 * 它在Nginx配置阶段的后期被调用，用于设置过滤器链和其他必要的初始化操作。
 * 这个函数通常会将复制过滤器插入到输出过滤器链中，
 * 确保它能够正确地处理响应体数据。
 */
static ngx_int_t ngx_http_copy_filter_init(ngx_conf_t *cf);


/**
 * @brief 定义复制过滤器模块的命令数组
 *
 * 这个静态数组包含了复制过滤器模块支持的Nginx配置指令。
 * 每个数组元素代表一个配置指令，定义了指令的名称、类型、处理函数等信息。
 * 通过这个数组，Nginx可以识别和处理与复制过滤器相关的配置指令。
 */
static ngx_command_t  ngx_http_copy_filter_commands[] = {

    { ngx_string("output_buffers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_copy_filter_conf_t, bufs),
      NULL },

      ngx_null_command
};


/**
 * @brief 定义复制过滤器模块的上下文结构
 *
 * 这个静态变量定义了复制过滤器模块的上下文结构。
 * 它包含了一系列函数指针，用于模块的配置创建、合并和初始化。
 * 这个结构体是ngx_http_module_t类型的，是Nginx HTTP模块的标准结构。
 */
static ngx_http_module_t  ngx_http_copy_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_copy_filter_init,             /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_copy_filter_create_conf,      /* create location configuration */
    ngx_http_copy_filter_merge_conf        /* merge location configuration */
};


/**
 * @brief 定义复制过滤器模块结构
 *
 * 这个静态变量定义了复制过滤器模块的主要结构。
 * 它包含了模块的上下文、指令、类型以及各种生命周期回调函数。
 * 这个结构体是ngx_module_t类型的，是Nginx模块的标准结构。
 */
ngx_module_t  ngx_http_copy_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_copy_filter_module_ctx,      /* module context */
    ngx_http_copy_filter_commands,         /* module directives */
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


/**
 * @brief 下一个HTTP响应体过滤器的函数指针
 *
 * 这个静态变量存储了下一个要执行的HTTP响应体过滤器的函数指针。
 * 在Nginx的过滤器链中，每个过滤器处理完数据后，会调用下一个过滤器继续处理。
 * 这个变量就是用来保存下一个过滤器的函数地址，以便当前过滤器完成处理后调用下一个过滤器。
 */
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


/**
 * @brief HTTP复制过滤器主函数
 *
 * @param r 指向当前HTTP请求的指针
 * @param in 指向输入缓冲链的指针
 * @return NGX_OK 成功时返回
 * @return NGX_ERROR 发生错误时返回
 * @return NGX_AGAIN 需要进一步处理时返回
 *
 * 该函数是HTTP复制过滤器的核心实现。它负责处理和转发HTTP响应体数据。
 * 主要功能包括:
 * 1. 初始化过滤器上下文（如果尚未初始化）
 * 2. 处理输入缓冲链
 * 3. 根据需要进行内存分配和数据复制
 * 4. 调用下一个过滤器继续处理数据
 */
static ngx_int_t
ngx_http_copy_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t                     rc;
    ngx_connection_t             *c;
    ngx_output_chain_ctx_t       *ctx;
    ngx_http_core_loc_conf_t     *clcf;
    ngx_http_copy_filter_conf_t  *conf;

    c = r->connection;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http copy filter: \"%V?%V\"", &r->uri, &r->args);

    ctx = ngx_http_get_module_ctx(r, ngx_http_copy_filter_module);

    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_output_chain_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_copy_filter_module);

        conf = ngx_http_get_module_loc_conf(r, ngx_http_copy_filter_module);
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        ctx->sendfile = c->sendfile;
        ctx->need_in_memory = r->main_filter_need_in_memory
                              || r->filter_need_in_memory;
        ctx->need_in_temp = r->filter_need_temporary;

        ctx->alignment = clcf->directio_alignment;

        ctx->pool = r->pool;
        ctx->bufs = conf->bufs;
        ctx->tag = (ngx_buf_tag_t) &ngx_http_copy_filter_module;

        ctx->output_filter = (ngx_output_chain_filter_pt)
                                  ngx_http_next_body_filter;
        ctx->filter_ctx = r;

#if (NGX_HAVE_FILE_AIO)
        if (ngx_file_aio && clcf->aio == NGX_HTTP_AIO_ON) {
            ctx->aio_handler = ngx_http_copy_aio_handler;
        }
#endif

#if (NGX_THREADS)
        if (clcf->aio == NGX_HTTP_AIO_THREADS) {
            ctx->thread_handler = ngx_http_copy_thread_handler;
        }
#endif

        if (in && in->buf && ngx_buf_size(in->buf)) {
            r->request_output = 1;
        }
    }

#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
    ctx->aio = r->aio;
#endif

    rc = ngx_output_chain(ctx, in);

    if (ctx->in == NULL) {
        r->buffered &= ~NGX_HTTP_COPY_BUFFERED;

    } else {
        r->buffered |= NGX_HTTP_COPY_BUFFERED;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http copy filter: %i \"%V?%V\"", rc, &r->uri, &r->args);

    return rc;
}


#if (NGX_HAVE_FILE_AIO)

/**
 * @brief 异步I/O处理函数
 *
 * 该函数用于处理异步I/O操作。它设置异步I/O的处理程序和相关参数，
 * 并更新请求的状态以反映异步操作的开始。
 *
 * @param ctx 输出链上下文结构体指针
 * @param file 文件结构体指针
 *
 * 主要功能:
 * 1. 设置文件的异步I/O数据和处理程序
 * 2. 添加一个60秒的定时器，用于处理潜在的超时情况
 * 3. 更新请求的阻塞状态和异步标志
 * 4. 更新上下文的异步标志
 *
 * 注意:
 * - 这个函数假设异步I/O操作已经被初始化
 * - 它增加了主请求的阻塞计数，这可能影响请求的处理流程
 * - 设置的定时器用于防止异步操作无限期挂起
 */
static void
ngx_http_copy_aio_handler(ngx_output_chain_ctx_t *ctx, ngx_file_t *file)
{
    ngx_http_request_t *r;

    r = ctx->filter_ctx;

    file->aio->data = r;
    file->aio->handler = ngx_http_copy_aio_event_handler;

    ngx_add_timer(&file->aio->event, 60000);

    r->main->blocked++;
    r->aio = 1;
    ctx->aio = 1;
}


/**
 * @brief 异步I/O事件处理函数
 *
 * 该函数处理异步I/O操作完成后的事件。它负责清理定时器、更新请求状态，
 * 并触发适当的处理程序来继续处理请求。
 *
 * @param ev 指向触发该处理程序的事件结构体的指针
 *
 * 主要功能:
 * 1. 从事件中获取相关的请求和连接信息
 * 2. 设置日志上下文
 * 3. 检查并处理超时情况
 * 4. 清理定时器
 * 5. 更新请求的阻塞状态和异步标志
 * 6. 根据请求状态触发适当的后续处理
 *
 * 注意:
 * - 如果事件超时，会记录一个警告日志
 * - 如果请求已终止，会触发连接的写事件处理程序
 * - 否则，会调用请求的写事件处理程序并运行已发布的请求
 */
static void
ngx_http_copy_aio_event_handler(ngx_event_t *ev)
{
    ngx_event_aio_t     *aio;
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    aio = ev->data;
    r = aio->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http aio: \"%V?%V\"", &r->uri, &r->args);

    if (ev->timedout) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "aio operation took too long");
        ev->timedout = 0;
        return;
    }

    if (ev->timer_set) {
        ngx_del_timer(ev);
    }

    r->main->blocked--;
    r->aio = 0;

    if (r->main->terminated) {
        /*
         * trigger connection event handler if the request was
         * terminated
         */

        c->write->handler(c->write);

    } else {
        r->write_event_handler(r);
        ngx_http_run_posted_requests(c);
    }
}

#endif


#if (NGX_THREADS)

/**
 * @brief 线程任务完成后的处理函数
 *
 * 此函数在线程任务完成后被调用，用于处理线程任务的结果。
 *
 * @param task 指向完成的线程任务结构的指针
 * @param data 指向与任务相关的数据的指针，通常是请求上下文
 *
 * 主要功能:
 * 1. 获取与任务相关的HTTP请求和连接信息
 * 2. 重置请求的AIO标志
 * 3. 处理请求终止的情况
 * 4. 调用请求的写事件处理程序
 * 5. 运行已发布的请求
 *
 * 注意:
 * - 此函数在主线程中执行，用于同步线程任务的结果
 * - 它处理了请求可能在线程任务执行期间被终止的情况
 * - 函数结束后，通常会继续处理请求或清理资源
 */
static void
ngx_http_copy_thread_event_handler(ngx_event_t *ev)
{
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    r = ev->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http thread: \"%V?%V\"", &r->uri, &r->args);

    r->main->blocked--;
    r->aio = 0;

    if (r->main->terminated) {
        /*
         * 如果请求已终止，触发连接的事件处理程序
         */
        c->write->handler(c->write);

    } else {
        r->write_event_handler(r);
        ngx_http_run_posted_requests(c);
    }
}

/**
 * @brief 线程任务完成后的事件处理函数
 *
 * 此函数在线程任务完成后被添加到事件循环中，用于处理线程任务的结果。
 *
 * @param ev 指向事件结构的指针
 *
 * 主要功能:
 * 1. 获取与事件相关的HTTP请求和连接信息
 * 2. 设置日志请求上下文
 * 3. 记录调试日志
 * 4. 更新请求的阻塞状态和AIO标志
 * 5. 根据请求状态调用相应的处理程序
 *
 * 注意:
 * - 此函数在主线程的事件循环中执行
 * - 它处理了请求可能在线程任务执行期间被终止的情况
 * - 函数结束后，会继续处理请求或清理资源
 */
static ngx_int_t
ngx_http_copy_thread_handler(ngx_thread_task_t *task, ngx_file_t *file)
{
    ngx_str_t                  name;
    ngx_connection_t          *c;
    ngx_thread_pool_t         *tp;
    ngx_http_request_t        *r;
    ngx_output_chain_ctx_t    *ctx;
    ngx_http_core_loc_conf_t  *clcf;

    r = file->thread_ctx;

    if (r->aio) {
        /*
         * tolerate sendfile() calls if another operation is already
         * running; this can happen due to subrequests, multiple calls
         * of the next body filter from a filter, or in HTTP/2 due to
         * a write event on the main connection
         */

        c = r->connection;

#if (NGX_HTTP_V2)
        if (r->stream) {
            c = r->stream->connection->connection;
        }
#endif

        if (task == c->sendfile_task) {
            return NGX_OK;
        }
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    tp = clcf->thread_pool;

    if (tp == NULL) {
        if (ngx_http_complex_value(r, clcf->thread_pool_value, &name)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        tp = ngx_thread_pool_get((ngx_cycle_t *) ngx_cycle, &name);

        if (tp == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "thread pool \"%V\" not found", &name);
            return NGX_ERROR;
        }
    }

    task->event.data = r;
    task->event.handler = ngx_http_copy_thread_event_handler;

    if (ngx_thread_task_post(tp, task) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_add_timer(&task->event, 60000);

    r->main->blocked++;
    r->aio = 1;

    ctx = ngx_http_get_module_ctx(r, ngx_http_copy_filter_module);
    ctx->aio = 1;

    return NGX_OK;
}


/**
 * @brief 线程任务完成后的事件处理函数
 *
 * 该函数在线程任务完成后被调用，用于处理线程操作的结果并继续请求的处理流程。
 *
 * @param ev 指向触发该处理程序的事件结构体的指针
 *
 * 主要功能:
 * 1. 从事件中获取相关的请求和连接信息
 * 2. 设置日志上下文
 * 3. 记录调试日志
 * 4. 检查并处理超时情况
 * 5. 清理定时器
 * 6. 更新请求的阻塞状态和异步标志
 * 7. 对于HTTP/2请求，更新写事件状态
 * 8. 根据请求状态触发适当的后续处理
 *
 * 注意:
 * - 如果事件超时，会记录一个警告日志
 * - 对于HTTP/2请求，会特别处理写事件以确保正确处理sendfile()
 * - 如果请求已完成或终止，会触发连接的写事件处理程序
 */
static void
ngx_http_copy_thread_event_handler(ngx_event_t *ev)
{
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    r = ev->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http thread: \"%V?%V\"", &r->uri, &r->args);

    if (ev->timedout) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "thread operation took too long");
        ev->timedout = 0;
        return;
    }

    if (ev->timer_set) {
        ngx_del_timer(ev);
    }

    r->main->blocked--;
    r->aio = 0;

#if (NGX_HTTP_V2)

    if (r->stream) {
        /*
         * for HTTP/2, update write event to make sure processing will
         * reach the main connection to handle sendfile() in threads
         */

        c->write->ready = 1;
        c->write->active = 0;
    }

#endif

    if (r->done || r->main->terminated) {
        /*
         * trigger connection event handler if the subrequest was
         * already finalized (this can happen if the handler is used
         * for sendfile() in threads), or if the request was terminated
         */

        c->write->handler(c->write);

    } else {
        r->write_event_handler(r);
        ngx_http_run_posted_requests(c);
    }
}

#endif


/**
 * @brief 创建复制过滤器的配置结构
 *
 * @param cf Nginx配置对象指针
 * @return void* 返回创建的配置结构指针，如果分配内存失败则返回NULL
 *
 * 该函数用于创建复制过滤器的配置结构。主要功能包括:
 * 1. 为配置结构分配内存
 * 2. 初始化配置结构中的缓冲区数量为0
 *
 * 注意:
 * - 使用ngx_palloc分配内存，这是Nginx的内存池分配函数
 * - 初始化bufs.num为0，表示还未设置缓冲区数量
 * - 这个函数在Nginx配置阶段被调用，用于为每个配置上下文创建一个新的配置结构
 */
static void *
ngx_http_copy_filter_create_conf(ngx_conf_t *cf)
{
    ngx_http_copy_filter_conf_t *conf;

    conf = ngx_palloc(cf->pool, sizeof(ngx_http_copy_filter_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->bufs.num = 0;

    return conf;
}


/**
 * @brief 合并复制过滤器的配置
 *
 * @param cf Nginx配置对象指针
 * @param parent 父配置结构指针
 * @param child 子配置结构指针
 * @return char* 返回NULL表示成功，否则返回错误信息字符串
 *
 * 该函数用于合并复制过滤器的父子配置。主要功能包括:
 * 1. 合并父配置和子配置中的缓冲区设置
 * 2. 如果子配置中未设置缓冲区，则使用父配置的设置
 * 3. 默认设置为2个32768字节的缓冲区
 *
 * 注意:
 * - 使用ngx_conf_merge_bufs_value宏来合并配置
 * - 这个函数在Nginx配置阶段被调用，用于确保正确的配置继承
 */
static char *
ngx_http_copy_filter_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_copy_filter_conf_t *prev = parent;
    ngx_http_copy_filter_conf_t *conf = child;

    ngx_conf_merge_bufs_value(conf->bufs, prev->bufs, 2, 32768);

    return NULL;
}


/**
 * @brief 初始化复制过滤器
 *
 * @param cf Nginx配置对象指针
 * @return NGX_OK 始终返回成功
 *
 * 该函数用于初始化HTTP复制过滤器模块。主要完成以下工作:
 * 1. 保存当前的顶层body过滤器函数指针
 * 2. 将ngx_http_copy_filter设置为新的顶层body过滤器
 *
 * 这个过程确保了复制过滤器被正确地插入到HTTP处理的过滤器链中，
 * 使其能够拦截和处理响应体数据。
 *
 * 注意:
 * - 这个函数在Nginx配置阶段被调用，而不是在请求处理阶段
 * - 它修改了全局的过滤器链结构，影响所有后续的HTTP请求处理
 */

static ngx_int_t
ngx_http_copy_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_copy_filter;

    return NGX_OK;
}

