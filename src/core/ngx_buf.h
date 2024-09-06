
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_BUF_H_INCLUDED_
#define _NGX_BUF_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 定义缓冲区标记类型
 *
 * ngx_buf_tag_t 是一个通用指针类型，用于标记缓冲区的特殊用途。
 * 它可以指向任何类型的数据，通常用于在缓冲区操作中传递额外的上下文信息。
 */
typedef void *            ngx_buf_tag_t;

/**
 * @brief 定义缓冲区结构体类型别名
 *
 * ngx_buf_t 是 struct ngx_buf_s 的类型别名，用于简化代码并提高可读性。
 * 这个结构体用于表示Nginx中的缓冲区，包含了缓冲区的各种属性和状态信息。
 */
typedef struct ngx_buf_s  ngx_buf_t;

struct ngx_buf_s {
    u_char          *pos;           /* 当前缓冲区读写位置 */
    u_char          *last;          /* 缓冲区有效数据的结束位置 */
    off_t            file_pos;      /* 文件读写偏移量 */
    off_t            file_last;     /* 文件有效数据结束位置 */

    u_char          *start;         /* 缓冲区起始位置 */
    u_char          *end;           /* 缓冲区结束位置 */
    ngx_buf_tag_t    tag;           /* 缓冲区标记，用于特殊用途 */
    ngx_file_t      *file;          /* 引用的文件 */
    ngx_buf_t       *shadow;        /* 影子缓冲区 */


    /* 缓冲区内容可以被修改 */
    unsigned         temporary:1;

    /*
     * 缓冲区内容在内存缓存或只读内存中，
     * 不能被修改
     */
    unsigned         memory:1;

    /* 缓冲区内容是mmap()映射的，不能被修改 */
    unsigned         mmap:1;

    unsigned         recycled:1;    /* 缓冲区可以被回收 */
    unsigned         in_file:1;     /* 缓冲区引用的是文件 */
    unsigned         flush:1;       /* 需要进行flush操作 */
    unsigned         sync:1;        /* 是否需要同步操作 */
    unsigned         last_buf:1;    /* 是否是最后一个缓冲区 */
    unsigned         last_in_chain:1; /* 是否是chain中的最后一个缓冲区 */

    unsigned         last_shadow:1; /* 是否是最后一个影子缓冲区 */
    unsigned         temp_file:1;   /* 是否是临时文件 */

    /* STUB */ int   num;           /* 调试用的编号 */
};


/**
 * @brief 定义缓冲区链结构体
 *
 * 这个结构体用于创建缓冲区链表，每个节点包含一个缓冲区和指向下一个节点的指针。
 * 在Nginx中广泛用于处理数据流和I/O操作。
 */
struct ngx_chain_s {
    ngx_buf_t    *buf;  /**< 指向当前节点的缓冲区 */
    ngx_chain_t  *next; /**< 指向链表中的下一个节点 */
};


/**
 * @brief 定义缓冲区配置结构体
 *
 * 这个结构体用于配置缓冲区的数量和大小。
 * 在Nginx中常用于设置各种缓冲区的参数，如请求体缓冲区、响应缓冲区等。
 */
typedef struct {
    ngx_int_t    num;   /**< 缓冲区的数量 */
    size_t       size;  /**< 每个缓冲区的大小（字节） */
} ngx_bufs_t;


/**
 * @brief 定义输出链上下文结构体类型
 *
 * 这个类型定义用于声明输出链上下文结构体。
 * 输出链上下文结构体包含了处理输出链时所需的各种信息和状态，
 * 如缓冲区、输入链、空闲链、忙碌链等。
 * 它在Nginx的输出处理过程中扮演着重要角色，用于管理和控制数据的输出流程。
 */
typedef struct ngx_output_chain_ctx_s  ngx_output_chain_ctx_t;

/**
 * @brief 定义输出链过滤器函数指针类型
 *
 * 这个函数指针类型用于定义输出链的过滤器函数。
 * 过滤器函数用于处理或修改输出链中的数据。
 *
 * @param ctx 过滤器的上下文，可以包含过滤器所需的任何数据
 * @param in 输入链，包含要处理的数据
 * @return 返回NGX_OK表示成功，其他值表示错误
 */
typedef ngx_int_t (*ngx_output_chain_filter_pt)(void *ctx, ngx_chain_t *in);

/**
 * @brief 定义异步I/O操作的函数指针类型
 *
 * 这个函数指针类型用于异步I/O操作的回调函数。
 * 当异步I/O操作完成时，会调用这个函数来处理结果。
 *
 * @param ctx 输出链上下文，包含了输出链的相关信息
 * @param file 与异步I/O操作相关的文件对象
 */
typedef void (*ngx_output_chain_aio_pt)(ngx_output_chain_ctx_t *ctx,
    ngx_file_t *file);

/**
 * @brief 输出链上下文结构体
 *
 * 该结构体用于管理输出链的处理过程，包含了处理输出数据所需的各种信息和状态。
 */
struct ngx_output_chain_ctx_s {
    ngx_buf_t                   *buf;           /**< 当前正在处理的缓冲区 */
    ngx_chain_t                 *in;            /**< 输入链表 */
    ngx_chain_t                 *free;          /**< 空闲缓冲区链表 */
    ngx_chain_t                 *busy;          /**< 正在使用的缓冲区链表 */

    unsigned                     sendfile:1;    /**< 是否使用sendfile */
    unsigned                     directio:1;    /**< 是否使用直接I/O */
    unsigned                     unaligned:1;   /**< 是否未对齐 */
    unsigned                     need_in_memory:1; /**< 是否需要在内存中 */
    unsigned                     need_in_temp:1;   /**< 是否需要在临时文件中 */
    unsigned                     aio:1;         /**< 是否使用异步I/O */

#if (NGX_HAVE_FILE_AIO || NGX_COMPAT)
    ngx_output_chain_aio_pt      aio_handler;   /**< 异步I/O处理函数 */
#endif

#if (NGX_THREADS || NGX_COMPAT)
    ngx_int_t                  (*thread_handler)(ngx_thread_task_t *task,
                                                 ngx_file_t *file);  /**< 线程处理函数 */
    ngx_thread_task_t           *thread_task;   /**< 线程任务 */
#endif

    off_t                        alignment;     /**< 对齐大小 */

    ngx_pool_t                  *pool;          /**< 内存池 */
    ngx_int_t                    allocated;     /**< 已分配的缓冲区数量 */
    ngx_bufs_t                   bufs;          /**< 缓冲区配置 */
    ngx_buf_tag_t                tag;           /**< 缓冲区标签 */

    ngx_output_chain_filter_pt   output_filter; /**< 输出过滤器函数 */
    void                        *filter_ctx;    /**< 过滤器上下文 */
};


/**
 * @brief 链式写入器上下文结构体
 *
 * 该结构体用于管理链式写入操作的上下文信息。
 */
typedef struct {
    ngx_chain_t                 *out;           /**< 输出链表 */
    ngx_chain_t                **last;          /**< 指向输出链表最后一个节点的指针 */
    ngx_connection_t            *connection;    /**< 相关的连接对象 */
    ngx_pool_t                  *pool;          /**< 内存池 */
    off_t                        limit;         /**< 写入限制大小 */
} ngx_chain_writer_ctx_t;


/**
 * @brief 定义链表错误状态的宏
 *
 * 这个宏定义了一个表示链表错误状态的常量。
 * 它将NGX_ERROR强制转换为ngx_chain_t指针类型，
 * 用于在返回ngx_chain_t指针的函数中表示错误情况。
 */
#define NGX_CHAIN_ERROR     (ngx_chain_t *) NGX_ERROR


/**
 * @brief 判断缓冲区是否在内存中
 *
 * 这个宏用于检查给定的缓冲区是否在内存中。
 * 如果缓冲区是临时的、在内存中的或者是内存映射的，则认为它在内存中。
 *
 * @param b 要检查的ngx_buf_t缓冲区指针
 * @return 如果缓冲区在内存中，返回非零值；否则返回0
 */
#define ngx_buf_in_memory(b)       ((b)->temporary || (b)->memory || (b)->mmap)
/**
 * @brief 判断缓冲区是否仅在内存中
 *
 * 这个宏用于检查给定的缓冲区是否仅存在于内存中，而不在文件中。
 * 它首先使用ngx_buf_in_memory宏检查缓冲区是否在内存中，
 * 然后确保缓冲区不在文件中(!(b)->in_file)。
 *
 * @param b 要检查的ngx_buf_t缓冲区指针
 * @return 如果缓冲区仅在内存中，返回非零值；否则返回0
 */
#define ngx_buf_in_memory_only(b)  (ngx_buf_in_memory(b) && !(b)->in_file)

/**
 * @brief 判断缓冲区是否为特殊缓冲区
 *
 * 这个宏用于检查给定的缓冲区是否为特殊缓冲区。
 * 特殊缓冲区是指那些不包含实际数据，但具有特殊标志（如flush、last_buf或sync）的缓冲区。
 * 同时，特殊缓冲区既不在内存中，也不在文件中。
 *
 * @param b 要检查的ngx_buf_t缓冲区指针
 * @return 如果缓冲区是特殊缓冲区，返回非零值；否则返回0
 */
#define ngx_buf_special(b)                                                   \
    (((b)->flush || (b)->last_buf || (b)->sync)                              \
     && !ngx_buf_in_memory(b) && !(b)->in_file)

/**
 * @brief 判断缓冲区是否仅为同步缓冲区
 *
 * 这个宏用于检查给定的缓冲区是否仅为同步缓冲区。
 * 同步缓冲区是指那些只设置了sync标志，但不包含实际数据的缓冲区。
 * 它既不在内存中，也不在文件中，且不是flush或last_buf。
 *
 * @param b 要检查的ngx_buf_t缓冲区指针
 * @return 如果缓冲区仅为同步缓冲区，返回非零值；否则返回0
 */
#define ngx_buf_sync_only(b)                                                 \
    ((b)->sync && !ngx_buf_in_memory(b)                                      \
     && !(b)->in_file && !(b)->flush && !(b)->last_buf)

/**
 * @brief 计算缓冲区的大小
 *
 * 这个宏用于计算给定缓冲区的大小。它会根据缓冲区的类型（内存中或文件中）
 * 来选择适当的计算方法。
 *
 * 对于内存中的缓冲区，大小为 last 指针和 pos 指针之间的差值。
 * 对于文件中的缓冲区，大小为 file_last 和 file_pos 之间的差值。
 *
 * @param b 要计算大小的 ngx_buf_t 缓冲区指针
 * @return 返回缓冲区的大小（字节数），类型为 off_t
 */
#define ngx_buf_size(b)                                                      \
    (ngx_buf_in_memory(b) ? (off_t) ((b)->last - (b)->pos):                  \
                            ((b)->file_last - (b)->file_pos))

/**
 * @brief 创建一个临时缓冲区
 *
 * 该函数用于创建一个指定大小的临时缓冲区。临时缓冲区通常用于临时存储数据，
 * 并在使用完毕后被释放。
 *
 * @param pool 用于分配内存的内存池
 * @param size 要创建的缓冲区大小（字节）
 * @return 返回创建的ngx_buf_t结构体指针，如果创建失败则返回NULL
 */
ngx_buf_t *ngx_create_temp_buf(ngx_pool_t *pool, size_t size);
ngx_chain_t *ngx_create_chain_of_bufs(ngx_pool_t *pool, ngx_bufs_t *bufs);


/**
 * @brief 分配一个新的缓冲区结构体
 *
 * 这个宏定义用于从指定的内存池中分配一个新的ngx_buf_t结构体。
 * 它使用ngx_palloc函数来分配内存，大小为ngx_buf_t结构体的大小。
 *
 * @param pool 用于分配内存的内存池指针
 * @return 返回一个指向新分配的ngx_buf_t结构体的指针
 */
#define ngx_alloc_buf(pool)  ngx_palloc(pool, sizeof(ngx_buf_t))
/**
 * @brief 分配并初始化一个新的缓冲区结构体
 *
 * 这个宏定义用于从指定的内存池中分配一个新的ngx_buf_t结构体，并将其内存初始化为零。
 * 它使用ngx_pcalloc函数来分配内存，大小为ngx_buf_t结构体的大小。
 * 与ngx_alloc_buf不同，ngx_calloc_buf会将分配的内存全部设置为0，
 * 这对于需要初始化所有字段为默认值的情况很有用。
 *
 * @param pool 用于分配内存的内存池指针
 * @return 返回一个指向新分配并已初始化的ngx_buf_t结构体的指针
 */
#define ngx_calloc_buf(pool) ngx_pcalloc(pool, sizeof(ngx_buf_t))

/**
 * @brief 分配一个新的链表节点
 *
 * 该函数用于从指定的内存池中分配一个新的ngx_chain_t结构体，
 * 通常用于创建缓冲区链表中的新节点。
 *
 * @param pool 用于分配内存的内存池指针
 * @return 返回一个指向新分配的ngx_chain_t结构体的指针，如果分配失败则返回NULL
 */
ngx_chain_t *ngx_alloc_chain_link(ngx_pool_t *pool);
/**
 * @brief 释放链表节点并将其添加到内存池的空闲链表中
 *
 * 这个宏定义用于释放一个链表节点，并将其添加到指定内存池的空闲链表中，
 * 以便后续重用。这种做法可以减少内存分配和释放的开销，提高性能。
 *
 * @param pool 内存池指针，用于存储空闲链表
 * @param cl 要释放的链表节点指针
 */
#define ngx_free_chain(pool, cl)                                             \
    (cl)->next = (pool)->chain;                                              \
    (pool)->chain = (cl)



/**
 * @brief 输出缓冲区链
 *
 * 该函数用于处理和输出缓冲区链。它会根据上下文信息处理输入的缓冲区链，
 * 并将处理后的数据输出。这个函数通常用于HTTP响应体的输出过程。
 *
 * @param ctx 输出链上下文，包含输出相关的配置和状态信息
 * @param in 输入的缓冲区链，包含要输出的数据
 * @return 返回NGX_OK表示成功，其他值表示出错
 */
ngx_int_t ngx_output_chain(ngx_output_chain_ctx_t *ctx, ngx_chain_t *in);
/**
 * @brief 链式写入函数
 *
 * 该函数用于将链式缓冲区中的数据写入到指定的上下文中。
 * 通常用于将数据写入文件或网络套接字等输出目标。
 *
 * @param ctx 写入操作的上下文，可能包含文件描述符或其他输出相关信息
 * @param in 输入的缓冲区链，包含要写入的数据
 * @return 返回NGX_OK表示写入成功，其他值表示出错
 */
ngx_int_t ngx_chain_writer(void *ctx, ngx_chain_t *in);

/**
 * @brief 向链表中添加一个副本
 *
 * 该函数用于将输入链表的副本添加到目标链表中。
 * 它会为输入链表中的每个节点创建一个新的节点，并将其添加到目标链表的末尾。
 *
 * @param pool 用于分配新节点的内存池
 * @param chain 指向目标链表指针的指针
 * @param in 要复制的输入链表
 * @return 成功时返回NGX_OK，失败时返回NGX_ERROR
 */
ngx_int_t ngx_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain,
    ngx_chain_t *in);
/**
 * @brief 从空闲链表中获取一个缓冲区
 *
 * 该函数尝试从指定的空闲链表中获取一个缓冲区。如果空闲链表为空，
 * 则会从内存池中分配一个新的缓冲区。这个函数有助于重用缓冲区，
 * 从而减少内存分配和释放的开销。
 *
 * @param p 用于分配新缓冲区的内存池
 * @param free 指向空闲链表的指针的指针
 * @return 返回获取到的缓冲区链节点，如果分配失败则返回NULL
 */
ngx_chain_t *ngx_chain_get_free_buf(ngx_pool_t *p, ngx_chain_t **free);
/**
 * @brief 更新缓冲区链的状态
 *
 * 该函数用于更新缓冲区链的状态，包括空闲链、忙碌链和输出链。
 * 它会重新组织这些链，以便更有效地管理和重用缓冲区。
 *
 * @param p 内存池，用于分配新的链节点
 * @param free 指向空闲链表的指针的指针
 */
void ngx_chain_update_chains(ngx_pool_t *p, ngx_chain_t **free,
    ngx_chain_t **busy, ngx_chain_t **out, ngx_buf_tag_t tag);

/**
 * @brief 合并文件缓冲区链
 *
 * 该函数用于合并输入链中的连续文件缓冲区，以优化文件读取操作。
 * 它会尝试将连续的文件缓冲区合并成一个更大的缓冲区，直到达到指定的限制大小。
 *
 * @param in 指向输入链表指针的指针，函数可能会修改这个指针
 * @param limit 合并的最大大小限制
 * @return 返回合并后的总字节数
 */
off_t ngx_chain_coalesce_file(ngx_chain_t **in, off_t limit);

/**
 * @brief 更新链表中已发送的数据量
 *
 * 该函数用于更新链表中已经发送的数据量，并返回下一个待发送的节点。
 * 它会遍历输入链表，根据已发送的字节数调整每个缓冲区的位置指针，
 * 并移除已完全发送的节点。
 *
 * @param in 输入链表的头节点
 * @param sent 已发送的字节数
 * @return 返回下一个待发送的节点，如果所有数据都已发送则返回NULL
 */
ngx_chain_t *ngx_chain_update_sent(ngx_chain_t *in, off_t sent);

#endif /* _NGX_BUF_H_INCLUDED_ */
