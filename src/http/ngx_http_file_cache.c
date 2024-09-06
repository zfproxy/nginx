
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_http_file_cache.c
 *
 * 该文件实现了Nginx的HTTP文件缓存功能。
 *
 * 支持的功能:
 * 1. 文件缓存的创建、读取和更新
 * 2. 缓存键的生成和管理
 * 3. 缓存锁机制，防止并发访问冲突
 * 4. 缓存元数据的处理
 * 5. 缓存清理和过期处理
 * 6. 缓存状态的统计和报告
 * 7. 支持条件性缓存和部分内容缓存
 * 8. 缓存预热和后台更新
 *
 * 使用注意点:
 * 1. 合理配置缓存路径和大小，避免磁盘空间耗尽
 * 2. 注意缓存键的唯一性，防止缓存碰撞
 * 3. 适当设置缓存锁超时时间，避免长时间阻塞
 * 4. 定期进行缓存清理，保持缓存效率
 * 5. 监控缓存使用情况，及时调整缓存策略
 * 6. 考虑使用缓存分片来提高并发性能
 * 7. 注意处理缓存文件的权限问题
 * 8. 在高并发场景下，可能需要调整文件描述符限制
 * 9. 谨慎使用缓存预热功能，避免过度消耗系统资源
 * 10. 定期检查和更新缓存配置，以适应变化的访问模式
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_md5.h>


/*
 * 函数: ngx_http_file_cache_lock
 * 功能: 尝试为文件缓存获取锁
 * 参数:
 *   r: HTTP请求结构体指针
 *   c: 缓存结构体指针（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t类型，表示锁定操作的结果
 */
static ngx_int_t ngx_http_file_cache_lock(ngx_http_request_t *r,
    ngx_http_cache_t *c);
/*
 * 函数: ngx_http_file_cache_lock_wait_handler
 * 功能: 处理文件缓存锁等待事件
 * 参数:
 *   ev: 事件结构体指针，包含了等待锁的相关信息
 * 描述:
 *   当文件缓存锁无法立即获取时，此函数作为回调被调用。
 *   它负责处理等待锁的过程，可能包括重试获取锁或超时处理。
 */
static void ngx_http_file_cache_lock_wait_handler(ngx_event_t *ev);
/*
 * 函数: ngx_http_file_cache_lock_wait
 * 功能: 等待文件缓存锁
 * 参数:
 *   r: HTTP请求结构体指针
 *   c: 缓存结构体指针（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t类型，表示等待操作的结果
 * 描述:
 *   当文件缓存锁无法立即获取时，此函数被调用来处理等待过程。
 *   它可能会设置定时器或执行其他等待逻辑，直到锁可用或超时。
 */
static ngx_int_t ngx_http_file_cache_lock_wait(ngx_http_request_t *r,
    ngx_http_cache_t *c);
/*
 * 函数: ngx_http_file_cache_read
 * 功能: 从文件缓存中读取数据
 * 参数:
 *   r: HTTP请求结构体指针
 *   c: 缓存结构体指针（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t类型，表示读取操作的结果
 * 描述:
 *   此函数负责从文件缓存中读取数据。它可能会处理缓存命中、
 *   缓存未命中或需要更新缓存等情况。函数可能会执行实际的
 *   文件读取操作，或者设置相应的标志以指示后续处理。
 */
static ngx_int_t ngx_http_file_cache_read(ngx_http_request_t *r,
    ngx_http_cache_t *c);
/*
 * 函数: ngx_http_file_cache_aio_read
 * 功能: 异步读取文件缓存
 * 参数:
 *   r: HTTP请求结构体指针
 *   c: 缓存结构体指针（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: ssize_t类型，表示异步读取的字节数或错误码
 * 描述:
 *   此函数用于异步读取文件缓存数据。它可能会启动一个异步I/O操作，
 *   并立即返回，而不等待I/O完成。实际的数据读取可能在后台进行，
 *   完成后通过回调或事件通知机制通知主程序。
 */
static ssize_t ngx_http_file_cache_aio_read(ngx_http_request_t *r,
    ngx_http_cache_t *c);
#if (NGX_HAVE_FILE_AIO)
/*
 * 函数: ngx_http_cache_aio_event_handler
 * 功能: 处理异步I/O缓存事件
 * 参数:
 *   ev: 事件结构体指针，包含了异步I/O操作的相关信息
 * 描述:
 *   此函数作为异步I/O操作的回调函数，在异步读取文件缓存完成时被调用。
 *   它负责处理异步I/O完成后的逻辑，可能包括更新缓存状态、处理读取的数据
 *   或触发后续的请求处理流程。
 */
static void ngx_http_cache_aio_event_handler(ngx_event_t *ev);
#endif
#if (NGX_THREADS)
/*
 * 函数: ngx_http_cache_thread_handler
 * 功能: 处理缓存相关的线程任务
 * 参数:
 *   task: 线程任务结构体指针，包含任务相关信息
 *   file: 文件结构体指针，可能用于文件操作（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t类型，表示线程任务处理的结果
 * 描述:
 *   此函数作为线程任务的处理函数，用于在单独的线程中执行缓存相关的操作。
 *   它可能会处理如读取缓存文件、更新缓存数据等任务，以提高主线程的性能。
 */
static ngx_int_t ngx_http_cache_thread_handler(ngx_thread_task_t *task,
    ngx_file_t *file);
/*
 * 函数: ngx_http_cache_thread_event_handler
 * 功能: 处理缓存线程事件
 * 参数:
 *   ev: 事件结构体指针，包含了线程事件的相关信息
 * 描述:
 *   此函数作为缓存线程事件的处理函数。当缓存相关的线程任务完成时，
 *   会触发此事件处理器。它可能负责处理线程任务的结果，更新缓存状态，
 *   或者触发后续的请求处理流程。这个函数在多线程环境下用于协调
 *   主线程和工作线程之间的交互，确保缓存操作的正确性和效率。
 */
static void ngx_http_cache_thread_event_handler(ngx_event_t *ev);
#endif
/*
 * 函数: ngx_http_file_cache_exists
 * 功能: 检查文件缓存是否存在
 * 参数:
 *   cache: 文件缓存结构体指针，包含缓存的配置和状态信息
 *   c: 缓存结构体指针（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t类型，表示缓存是否存在或操作结果
 * 描述:
 *   此函数用于检查指定的文件缓存是否存在。它可能会检查缓存文件的存在性，
 *   验证缓存的有效性，或者执行其他相关的检查操作。这个函数在处理HTTP请求时
 *   用于决定是否可以使用现有的缓存数据来响应请求。
 */
static ngx_int_t ngx_http_file_cache_exists(ngx_http_file_cache_t *cache,
    ngx_http_cache_t *c);
/*
 * 函数: ngx_http_file_cache_name
 * 功能: 生成文件缓存的名称
 * 参数:
 *   r: HTTP请求结构体指针，包含请求的相关信息
 *   path: 路径结构体指针，用于存储生成的缓存文件路径（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t类型，表示生成缓存名称的结果
 * 描述:
 *   此函数用于为HTTP请求生成对应的文件缓存名称。它可能会基于请求的URL、
 *   头部信息或其他相关参数来构造一个唯一的缓存文件名。这个函数在
 *   创建新的缓存文件或查找现有缓存时使用，确保每个缓存项都有一个
 *   唯一的标识符。
 */
static ngx_int_t ngx_http_file_cache_name(ngx_http_request_t *r,
    ngx_path_t *path);
/*
* 函数: ngx_http_file_cache_lookup
* 功能: 在文件缓存中查找指定的键
* 参数:
*   cache: 文件缓存结构体指针，包含缓存的配置和状态信息
*   key: 要查找的缓存键
* 返回值: ngx_http_file_cache_node_t 指针，指向找到的缓存节点，如果未找到则返回 NULL
* 描述:
*   此函数在给定的文件缓存中查找与指定键匹配的缓存节点。
*   它可能会使用红黑树或其他数据结构来快速定位缓存项。
*   这个函数在处理HTTP请求时用于检查是否存在可用的缓存数据。
*/
static ngx_http_file_cache_node_t *
    ngx_http_file_cache_lookup(ngx_http_file_cache_t *cache, u_char *key);
/*
 * 函数: ngx_http_file_cache_rbtree_insert_value
 * 功能: 在文件缓存的红黑树中插入新节点
 * 参数:
 *   temp: 临时节点，用于比较和定位插入位置
 *   node: 要插入的新节点
 *   sentinel: 哨兵节点，用于标识树的边界
 * 返回值: 无
 * 描述:
 *   此函数用于在文件缓存的红黑树中插入新的缓存节点。它实现了红黑树的插入算法，
 *   确保树的平衡性和查找效率。这个函数在添加新的缓存项到缓存系统时被调用，
 *   有助于维护缓存的快速查找结构。
 */
static void ngx_http_file_cache_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
/*
 * 函数: ngx_http_file_cache_vary
 * 功能: 处理文件缓存的Vary头部
 * 参数:
 *   r: HTTP请求结构体指针，包含请求的相关信息
 *   vary: Vary头部的值
 *   len: Vary头部值的长度（在函数定义中未显示，但可能在函数体中使用）
 *   hash: 用于存储计算得到的哈希值（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: 无
 * 描述:
 *   此函数用于处理HTTP响应中的Vary头部，这对于缓存变体很重要。
 *   它可能会解析Vary头部的值，计算相应的哈希值，并更新缓存键。
 *   这有助于确保针对不同的Vary条件正确地存储和检索缓存内容。
 */
static void ngx_http_file_cache_vary(ngx_http_request_t *r, u_char *vary,
    size_t len, u_char *hash);
/*
 * 函数: ngx_http_file_cache_vary_header
 * 功能: 处理文件缓存的Vary头部
 * 参数:
 *   r: HTTP请求结构体指针，包含请求的相关信息
 *   md5: MD5上下文指针，用于计算Vary头部的哈希值
 *   name: 头部名称的字符串指针
 * 返回值: 无
 * 描述:
 *   此函数用于处理特定的Vary头部。它可能会从请求中提取相应的头部值，
 *   并将其添加到MD5哈希计算中。这有助于为不同的Vary条件生成唯一的缓存键，
 *   确保正确地存储和检索缓存内容的变体。
 */
static void ngx_http_file_cache_vary_header(ngx_http_request_t *r,
    ngx_md5_t *md5, ngx_str_t *name);
/*
 * 函数: ngx_http_file_cache_reopen
 * 功能: 重新打开文件缓存
 * 参数:
 *   r: HTTP请求结构体指针，包含请求的相关信息
 *   c: 缓存结构体指针，包含缓存的相关信息（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t 类型，表示操作的结果
 * 描述:
 *   此函数用于重新打开文件缓存。在某些情况下，可能需要重新打开缓存文件，
 *   例如当缓存文件被修改或需要更新时。这个函数可能会检查缓存文件的状态，
 *   并在必要时重新打开或更新缓存。
 */
static ngx_int_t ngx_http_file_cache_reopen(ngx_http_request_t *r,
    ngx_http_cache_t *c);
/*
 * 函数: ngx_http_file_cache_update_variant
 * 功能: 更新文件缓存的变体
 * 参数:
 *   r: HTTP请求结构体指针，包含请求的相关信息
 *   c: 缓存结构体指针，包含缓存的相关信息（在函数定义中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t 类型，表示操作的结果
 * 描述:
 *   此函数用于更新文件缓存中的变体。当请求的内容有多个变体（例如，由于Vary头部）时，
 *   这个函数可能会被调用来更新或创建特定变体的缓存。它可能涉及更新缓存元数据、
 *   写入新的缓存文件，或者更新现有的缓存文件。
 */
static ngx_int_t ngx_http_file_cache_update_variant(ngx_http_request_t *r,
    ngx_http_cache_t *c);
/*
 * 函数: ngx_http_file_cache_cleanup
 * 功能: 清理文件缓存
 * 参数:
 *   data: 指向需要清理的数据的指针
 * 返回值: 无
 * 描述:
 *   此函数用于清理文件缓存相关的资源。它可能会被调用来释放内存、
 *   关闭文件句柄或执行其他必要的清理操作，以确保在不再需要缓存时
 *   正确地释放所有相关资源。这有助于防止内存泄漏和其他资源问题。
 */
static void ngx_http_file_cache_cleanup(void *data);
/*
 * 函数: ngx_http_file_cache_forced_expire
 * 功能: 强制过期文件缓存中的条目
 * 参数:
 *   cache: 指向 ngx_http_file_cache_t 结构的指针，表示要操作的文件缓存
 * 返回值: time_t 类型，表示下一次需要执行强制过期的时间
 * 描述:
 *   此函数用于强制使文件缓存中的某些条目过期。
 *   它可能会遍历缓存中的条目，根据特定的规则（如缓存大小限制或时间限制）
 *   来决定哪些条目需要被强制标记为过期。
 *   这有助于维护缓存的新鲜度和控制缓存的大小。
 *   函数返回下一次需要执行此操作的时间，以便调度后续的强制过期操作。
 */
static time_t ngx_http_file_cache_forced_expire(ngx_http_file_cache_t *cache);
/*
 * 函数: ngx_http_file_cache_expire
 * 功能: 使文件缓存中的条目过期
 * 参数:
 *   cache: 指向 ngx_http_file_cache_t 结构的指针，表示要操作的文件缓存
 * 返回值: time_t 类型，表示下一次需要执行过期操作的时间
 * 描述:
 *   此函数用于处理文件缓存中条目的过期。它可能会遍历缓存中的条目，
 *   检查它们的过期时间，并将过期的条目标记为无效或从缓存中移除。
 *   这有助于维护缓存的新鲜度，确保缓存中的内容是最新的。
 *   函数返回下一次需要执行此操作的时间，以便调度后续的过期检查。
 */
static time_t ngx_http_file_cache_expire(ngx_http_file_cache_t *cache);
/*
 * 函数: ngx_http_file_cache_delete
 * 功能: 从文件缓存中删除指定的缓存项
 * 参数:
 *   cache: 指向 ngx_http_file_cache_t 结构的指针，表示要操作的文件缓存
 *   q: 指向 ngx_queue_t 结构的指针，可能表示要删除的缓存项在队列中的位置
 *   name: 指向 u_char 类型的指针，可能表示要删除的缓存项的名称或标识符
 * 返回值: 无
 * 描述:
 *   此函数用于从文件缓存中删除特定的缓存项。它可能会执行以下操作：
 *   1. 从缓存数据结构中移除相关条目
 *   2. 删除对应的缓存文件
 *   3. 更新缓存统计信息
 *   4. 释放相关的内存资源
 *   这个函数对于维护缓存的大小和保持缓存的新鲜度非常重要。
 */
static void ngx_http_file_cache_delete(ngx_http_file_cache_t *cache,
    ngx_queue_t *q, u_char *name);
/*
 * 函数: ngx_http_file_cache_loader_sleep
 * 功能: 使文件缓存加载器进入睡眠状态
 * 参数:
 *   cache: 指向 ngx_http_file_cache_t 结构的指针，表示要操作的文件缓存
 * 返回值: 无
 * 描述:
 *   此函数用于控制文件缓存加载器的睡眠行为。它可能会执行以下操作：
 *   1. 根据缓存的当前状态和配置，计算适当的睡眠时间
 *   2. 使加载器进程进入睡眠状态，以避免过度消耗系统资源
 *   3. 在睡眠结束后，可能会更新一些统计信息或状态标志
 *   这个函数对于优化缓存加载过程和控制系统资源使用非常重要。
 */
static void ngx_http_file_cache_loader_sleep(ngx_http_file_cache_t *cache);
/*
 * 函数: ngx_http_file_cache_noop
 * 功能: 文件缓存的空操作函数
 * 参数:
 *   ctx: 指向 ngx_tree_ctx_t 结构的指针，表示树遍历的上下文
 *   path: 指向 ngx_str_t 结构的指针，表示当前处理的路径
 * 返回值: ngx_int_t 类型，表示操作的结果
 * 描述:
 *   这是一个空操作函数，用于文件缓存系统中可能需要但不执行实际操作的场景。
 *   它可能被用作占位符或默认处理器，以保持接口的一致性。
 *   函数通常会直接返回一个表示成功的值，而不进行任何实际操作。
 */
static ngx_int_t ngx_http_file_cache_noop(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);
/*
 * 函数: ngx_http_file_cache_manage_file
 * 功能: 管理文件缓存中的单个文件
 * 参数:
 *   ctx: 指向 ngx_tree_ctx_t 结构的指针，表示树遍历的上下文
 *   path: 指向 ngx_str_t 结构的指针，表示当前处理的文件路径（在函数签名中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t 类型，表示操作的结果
 * 描述:
 *   此函数用于管理文件缓存中的单个文件。它可能执行以下操作：
 *   1. 检查文件的状态（如修改时间、大小等）
 *   2. 决定是否需要更新或删除文件
 *   3. 更新缓存元数据
 *   4. 处理过期的缓存文件
 *   这个函数在维护缓存系统的整体状态和性能方面起着关键作用。
 */
static ngx_int_t ngx_http_file_cache_manage_file(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);
/*
 * 函数: ngx_http_file_cache_manage_directory
 * 功能: 管理文件缓存中的目录
 * 参数:
 *   ctx: 指向 ngx_tree_ctx_t 结构的指针，表示树遍历的上下文
 *   path: 指向 ngx_str_t 结构的指针，表示当前处理的目录路径（在函数签名中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t 类型，表示操作的结果
 * 描述:
 *   此函数用于管理文件缓存中的目录。它可能执行以下操作：
 *   1. 遍历目录中的文件和子目录
 *   2. 对目录中的文件进行清理或整理
 *   3. 更新目录的元数据
 *   4. 处理过期的缓存目录
 *   这个函数在维护缓存系统的目录结构和整体组织方面起着重要作用。
 */
static ngx_int_t ngx_http_file_cache_manage_directory(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);
/*
 * 函数: ngx_http_file_cache_add_file
 * 功能: 向文件缓存中添加新文件
 * 参数:
 *   ctx: 指向 ngx_tree_ctx_t 结构的指针，表示树遍历的上下文
 *   path: 指向 ngx_str_t 结构的指针，表示要添加的文件路径（在函数签名中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t 类型，表示操作的结果
 * 描述:
 *   此函数用于向文件缓存系统中添加新的文件。它可能执行以下操作：
 *   1. 验证文件的有效性
 *   2. 创建文件的缓存条目
 *   3. 更新缓存元数据
 *   4. 处理可能的冲突或重复
 *   这个函数在扩展和维护缓存内容方面起着重要作用。
 */
static ngx_int_t ngx_http_file_cache_add_file(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);
/*
 * 函数: ngx_http_file_cache_add
 * 功能: 向文件缓存中添加新的缓存项
 * 参数:
 *   cache: 指向 ngx_http_file_cache_t 结构的指针，表示文件缓存
 *   c: 指向 ngx_http_cache_t 结构的指针，表示要添加的缓存项（在函数签名中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t 类型，表示操作的结果
 * 描述:
 *   此函数用于向文件缓存系统中添加新的缓存项。它可能执行以下操作：
 *   1. 为新的缓存项分配空间
 *   2. 初始化缓存项的元数据
 *   3. 将缓存项添加到缓存数据结构中
 *   4. 更新缓存统计信息
 *   这个函数在扩展缓存内容和管理缓存系统的容量方面起着关键作用。
 */
static ngx_int_t ngx_http_file_cache_add(ngx_http_file_cache_t *cache,
    ngx_http_cache_t *c);
/*
 * 函数: ngx_http_file_cache_delete_file
 * 功能: 从文件缓存中删除指定文件
 * 参数:
 *   ctx: 指向 ngx_tree_ctx_t 结构的指针，表示树遍历的上下文
 *   path: 指向 ngx_str_t 结构的指针，表示要删除的文件路径（在函数签名中未显示，但可能在函数体中使用）
 * 返回值: ngx_int_t 类型，表示操作的结果
 * 描述:
 *   此函数用于从文件缓存系统中删除指定的文件。它可能执行以下操作：
 *   1. 验证文件的存在性和可删除性
 *   2. 从缓存数据结构中移除文件的引用
 *   3. 实际删除文件系统中的文件
 *   4. 更新缓存统计信息
 *   这个函数在维护缓存系统的清洁度和管理存储空间方面起着重要作用。
 */
static ngx_int_t ngx_http_file_cache_delete_file(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);
/*
 * 函数: ngx_http_file_cache_set_watermark
 * 功能: 设置文件缓存的水位线
 * 参数:
 *   cache: 指向 ngx_http_file_cache_t 结构的指针，表示文件缓存
 * 返回值: 无
 * 描述:
 *   此函数用于设置文件缓存系统的水位线。水位线通常用于:
 *   1. 控制缓存的使用量
 *   2. 触发缓存清理或扩展操作
 *   3. 优化缓存性能和资源利用
 *   设置适当的水位线对于维护缓存系统的效率和可靠性非常重要。
 */
static void ngx_http_file_cache_set_watermark(ngx_http_file_cache_t *cache);


/*
 * HTTP缓存状态数组
 * 此数组定义了不同的HTTP缓存状态，每个元素都是一个ngx_str_t类型，表示一个状态字符串
 * 数组索引对应于不同的缓存状态，如MISS、BYPASS等
 * 这些状态用于指示缓存操作的结果，对于调试和监控缓存行为非常有用
 */
ngx_str_t  ngx_http_cache_status[] = {
    ngx_string("MISS"),
    ngx_string("BYPASS"),
    ngx_string("EXPIRED"),
    ngx_string("STALE"),
    ngx_string("UPDATING"),
    ngx_string("REVALIDATED"),
    ngx_string("HIT")
};


/*
 * HTTP文件缓存键前缀
 * 这个静态数组定义了HTTP文件缓存键的前缀
 * 包含以下字符:
 * - LF (换行符)
 * - "KEY: " (字符串 "KEY: ")
 * 这个前缀用于在缓存文件中标识和分隔缓存键信息
 */
static u_char  ngx_http_file_cache_key[] = { LF, 'K', 'E', 'Y', ':', ' ' };


static ngx_int_t
ngx_http_file_cache_init(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_http_file_cache_t  *ocache = data;

    size_t                  len;
    ngx_uint_t              n;
    ngx_http_file_cache_t  *cache;

    cache = shm_zone->data;

    if (ocache) {
        if (ngx_strcmp(cache->path->name.data, ocache->path->name.data) != 0) {
            ngx_log_error(NGX_LOG_EMERG, shm_zone->shm.log, 0,
                          "cache \"%V\" uses the \"%V\" cache path "
                          "while previously it used the \"%V\" cache path",
                          &shm_zone->shm.name, &cache->path->name,
                          &ocache->path->name);

            return NGX_ERROR;
        }

        for (n = 0; n < NGX_MAX_PATH_LEVEL; n++) {
            if (cache->path->level[n] != ocache->path->level[n]) {
                ngx_log_error(NGX_LOG_EMERG, shm_zone->shm.log, 0,
                              "cache \"%V\" had previously different levels",
                              &shm_zone->shm.name);
                return NGX_ERROR;
            }
        }

        cache->sh = ocache->sh;

        cache->shpool = ocache->shpool;
        cache->bsize = ocache->bsize;

        cache->max_size /= cache->bsize;

        if (!cache->sh->cold || cache->sh->loading) {
            cache->path->loader = NULL;
        }

        return NGX_OK;
    }

    cache->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (shm_zone->shm.exists) {
        cache->sh = cache->shpool->data;
        cache->bsize = ngx_fs_bsize(cache->path->name.data);
        cache->max_size /= cache->bsize;

        return NGX_OK;
    }

    cache->sh = ngx_slab_alloc(cache->shpool, sizeof(ngx_http_file_cache_sh_t));
    if (cache->sh == NULL) {
        return NGX_ERROR;
    }

    cache->shpool->data = cache->sh;

    ngx_rbtree_init(&cache->sh->rbtree, &cache->sh->sentinel,
                    ngx_http_file_cache_rbtree_insert_value);

    ngx_queue_init(&cache->sh->queue);

    cache->sh->cold = 1;
    cache->sh->loading = 0;
    cache->sh->size = 0;
    cache->sh->count = 0;
    cache->sh->watermark = (ngx_uint_t) -1;

    cache->bsize = ngx_fs_bsize(cache->path->name.data);

    cache->max_size /= cache->bsize;

    len = sizeof(" in cache keys zone \"\"") + shm_zone->shm.name.len;

    cache->shpool->log_ctx = ngx_slab_alloc(cache->shpool, len);
    if (cache->shpool->log_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_sprintf(cache->shpool->log_ctx, " in cache keys zone \"%V\"%Z",
                &shm_zone->shm.name);

    cache->shpool->log_nomem = 0;

    return NGX_OK;
}


ngx_int_t
ngx_http_file_cache_new(ngx_http_request_t *r)
{
    ngx_http_cache_t  *c;

    c = ngx_pcalloc(r->pool, sizeof(ngx_http_cache_t));
    if (c == NULL) {
        return NGX_ERROR;
    }

    if (ngx_array_init(&c->keys, r->pool, 4, sizeof(ngx_str_t)) != NGX_OK) {
        return NGX_ERROR;
    }

    r->cache = c;
    c->file.log = r->connection->log;
    c->file.fd = NGX_INVALID_FILE;

    return NGX_OK;
}


ngx_int_t
ngx_http_file_cache_create(ngx_http_request_t *r)
{
    ngx_http_cache_t       *c;
    ngx_pool_cleanup_t     *cln;
    ngx_http_file_cache_t  *cache;

    c = r->cache;
    cache = c->file_cache;

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        return NGX_ERROR;
    }

    cln->handler = ngx_http_file_cache_cleanup;
    cln->data = c;

    if (ngx_http_file_cache_exists(cache, c) == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (ngx_http_file_cache_name(r, cache->path) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


void
ngx_http_file_cache_create_key(ngx_http_request_t *r)
{
    size_t             len;
    ngx_str_t         *key;
    ngx_uint_t         i;
    ngx_md5_t          md5;
    ngx_http_cache_t  *c;

    c = r->cache;

    len = 0;

    ngx_crc32_init(c->crc32);
    ngx_md5_init(&md5);

    key = c->keys.elts;
    for (i = 0; i < c->keys.nelts; i++) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http cache key: \"%V\"", &key[i]);

        len += key[i].len;

        ngx_crc32_update(&c->crc32, key[i].data, key[i].len);
        ngx_md5_update(&md5, key[i].data, key[i].len);
    }

    c->header_start = sizeof(ngx_http_file_cache_header_t)
                      + sizeof(ngx_http_file_cache_key) + len + 1;

    ngx_crc32_final(c->crc32);
    ngx_md5_final(c->key, &md5);

    ngx_memcpy(c->main, c->key, NGX_HTTP_CACHE_KEY_LEN);
}


ngx_int_t
ngx_http_file_cache_open(ngx_http_request_t *r)
{
    ngx_int_t                  rc, rv;
    ngx_uint_t                 test;
    ngx_http_cache_t          *c;
    ngx_pool_cleanup_t        *cln;
    ngx_open_file_info_t       of;
    ngx_http_file_cache_t     *cache;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->cache;

    if (c->waiting) {
        return NGX_AGAIN;
    }

    if (c->reading) {
        return ngx_http_file_cache_read(r, c);
    }

    cache = c->file_cache;

    if (c->node == NULL) {
        cln = ngx_pool_cleanup_add(r->pool, 0);
        if (cln == NULL) {
            return NGX_ERROR;
        }

        cln->handler = ngx_http_file_cache_cleanup;
        cln->data = c;
    }

    c->buffer_size = c->body_start;

    rc = ngx_http_file_cache_exists(cache, c);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache exists: %i e:%d", rc, c->exists);

    if (rc == NGX_ERROR) {
        return rc;
    }

    if (rc == NGX_AGAIN) {
        return NGX_HTTP_CACHE_SCARCE;
    }

    if (rc == NGX_OK) {

        if (c->error) {
            return c->error;
        }

        c->temp_file = 1;
        test = c->exists ? 1 : 0;
        rv = NGX_DECLINED;

    } else { /* rc == NGX_DECLINED */

        test = cache->sh->cold ? 1 : 0;

        if (c->min_uses > 1) {

            if (!test) {
                return NGX_HTTP_CACHE_SCARCE;
            }

            rv = NGX_HTTP_CACHE_SCARCE;

        } else {
            c->temp_file = 1;
            rv = NGX_DECLINED;
        }
    }

    if (ngx_http_file_cache_name(r, cache->path) != NGX_OK) {
        return NGX_ERROR;
    }

    if (!test) {
        goto done;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.uniq = c->uniq;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.events = clcf->open_file_cache_events;
    of.directio = NGX_OPEN_FILE_DIRECTIO_OFF;
    of.read_ahead = clcf->read_ahead;

    if (ngx_open_cached_file(clcf->open_file_cache, &c->file.name, &of, r->pool)
        != NGX_OK)
    {
        switch (of.err) {

        case 0:
            return NGX_ERROR;

        case NGX_ENOENT:
        case NGX_ENOTDIR:
            goto done;

        default:
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, of.err,
                          ngx_open_file_n " \"%s\" failed", c->file.name.data);
            return NGX_ERROR;
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache fd: %d", of.fd);

    c->file.fd = of.fd;
    c->file.log = r->connection->log;
    c->uniq = of.uniq;
    c->length = of.size;
    c->fs_size = (of.fs_size + cache->bsize - 1) / cache->bsize;

    c->buf = ngx_create_temp_buf(r->pool, c->body_start);
    if (c->buf == NULL) {
        return NGX_ERROR;
    }

    return ngx_http_file_cache_read(r, c);

done:

    if (rv == NGX_DECLINED) {
        return ngx_http_file_cache_lock(r, c);
    }

    return rv;
}


static ngx_int_t
ngx_http_file_cache_lock(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    ngx_msec_t                 now, timer;
    ngx_http_file_cache_t     *cache;

    if (!c->lock) {
        return NGX_DECLINED;
    }

    now = ngx_current_msec;

    cache = c->file_cache;

    ngx_shmtx_lock(&cache->shpool->mutex);

    timer = c->node->lock_time - now;

    if (!c->node->updating || (ngx_msec_int_t) timer <= 0) {
        c->node->updating = 1;
        c->node->lock_time = now + c->lock_age;
        c->updating = 1;
        c->lock_time = c->node->lock_time;
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache lock u:%d wt:%M",
                   c->updating, c->wait_time);

    if (c->updating) {
        return NGX_DECLINED;
    }

    if (c->lock_timeout == 0) {
        return NGX_HTTP_CACHE_SCARCE;
    }

    c->waiting = 1;

    if (c->wait_time == 0) {
        c->wait_time = now + c->lock_timeout;

        c->wait_event.handler = ngx_http_file_cache_lock_wait_handler;
        c->wait_event.data = r;
        c->wait_event.log = r->connection->log;
    }

    timer = c->wait_time - now;

    ngx_add_timer(&c->wait_event, (timer > 500) ? 500 : timer);

    r->main->blocked++;

    return NGX_AGAIN;
}


static void
ngx_http_file_cache_lock_wait_handler(ngx_event_t *ev)
{
    ngx_int_t            rc;
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    r = ev->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http file cache wait: \"%V?%V\"", &r->uri, &r->args);

    rc = ngx_http_file_cache_lock_wait(r, r->cache);

    if (rc == NGX_AGAIN) {
        return;
    }

    r->cache->waiting = 0;
    r->main->blocked--;

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


static ngx_int_t
ngx_http_file_cache_lock_wait(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    ngx_uint_t              wait;
    ngx_msec_t              now, timer;
    ngx_http_file_cache_t  *cache;

    now = ngx_current_msec;

    timer = c->wait_time - now;

    if ((ngx_msec_int_t) timer <= 0) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "cache lock timeout");
        c->lock_timeout = 0;
        return NGX_OK;
    }

    cache = c->file_cache;
    wait = 0;

    ngx_shmtx_lock(&cache->shpool->mutex);

    timer = c->node->lock_time - now;

    if (c->node->updating && (ngx_msec_int_t) timer > 0) {
        wait = 1;
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    if (wait) {
        ngx_add_timer(&c->wait_event, (timer > 500) ? 500 : timer);
        return NGX_AGAIN;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_file_cache_read(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    u_char                        *p;
    time_t                         now;
    ssize_t                        n;
    ngx_str_t                     *key;
    ngx_int_t                      rc;
    ngx_uint_t                     i;
    ngx_http_file_cache_t         *cache;
    ngx_http_file_cache_header_t  *h;

    n = ngx_http_file_cache_aio_read(r, c);

    if (n < 0) {
        return n;
    }

    if ((size_t) n < c->header_start) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" is too small", c->file.name.data);
        return NGX_DECLINED;
    }

    h = (ngx_http_file_cache_header_t *) c->buf->pos;

    if (h->version != NGX_HTTP_CACHE_VERSION) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "cache file \"%s\" version mismatch", c->file.name.data);
        return NGX_DECLINED;
    }

    if (h->crc32 != c->crc32 || (size_t) h->header_start != c->header_start) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" has md5 collision", c->file.name.data);
        return NGX_DECLINED;
    }

    p = c->buf->pos + sizeof(ngx_http_file_cache_header_t)
        + sizeof(ngx_http_file_cache_key);

    key = c->keys.elts;
    for (i = 0; i < c->keys.nelts; i++) {
        if (ngx_memcmp(p, key[i].data, key[i].len) != 0) {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                          "cache file \"%s\" has md5 collision",
                          c->file.name.data);
            return NGX_DECLINED;
        }

        p += key[i].len;
    }

    if ((size_t) h->body_start > c->body_start) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" has too long header",
                      c->file.name.data);
        return NGX_DECLINED;
    }

    if (h->vary_len > NGX_HTTP_CACHE_VARY_LEN) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" has incorrect vary length",
                      c->file.name.data);
        return NGX_DECLINED;
    }

    if (h->vary_len) {
        ngx_http_file_cache_vary(r, h->vary, h->vary_len, c->variant);

        if (ngx_memcmp(c->variant, h->variant, NGX_HTTP_CACHE_KEY_LEN) != 0) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http file cache vary mismatch");
            return ngx_http_file_cache_reopen(r, c);
        }
    }

    c->buf->last += n;

    c->valid_sec = h->valid_sec;
    c->updating_sec = h->updating_sec;
    c->error_sec = h->error_sec;
    c->last_modified = h->last_modified;
    c->date = h->date;
    c->valid_msec = h->valid_msec;
    c->body_start = h->body_start;
    c->etag.len = h->etag_len;
    c->etag.data = h->etag;

    r->cached = 1;

    cache = c->file_cache;

    if (cache->sh->cold) {

        ngx_shmtx_lock(&cache->shpool->mutex);

        if (!c->node->exists) {
            c->node->uses = 1;
            c->node->body_start = c->body_start;
            c->node->exists = 1;
            c->node->uniq = c->uniq;
            c->node->fs_size = c->fs_size;

            cache->sh->size += c->fs_size;
        }

        ngx_shmtx_unlock(&cache->shpool->mutex);
    }

    now = ngx_time();

    if (c->valid_sec < now) {
        c->stale_updating = c->valid_sec + c->updating_sec >= now;
        c->stale_error = c->valid_sec + c->error_sec >= now;

        ngx_shmtx_lock(&cache->shpool->mutex);

        if (c->node->updating) {
            rc = NGX_HTTP_CACHE_UPDATING;

        } else {
            c->node->updating = 1;
            c->updating = 1;
            c->lock_time = c->node->lock_time;
            rc = NGX_HTTP_CACHE_STALE;
        }

        ngx_shmtx_unlock(&cache->shpool->mutex);

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http file cache expired: %i %T %T",
                       rc, c->valid_sec, now);

        return rc;
    }

    return NGX_OK;
}


static ssize_t
ngx_http_file_cache_aio_read(ngx_http_request_t *r, ngx_http_cache_t *c)
{
#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
    ssize_t                    n;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
#endif

#if (NGX_HAVE_FILE_AIO)

    if (clcf->aio == NGX_HTTP_AIO_ON && ngx_file_aio) {
        n = ngx_file_aio_read(&c->file, c->buf->pos, c->body_start, 0, r->pool);

        if (n != NGX_AGAIN) {
            c->reading = 0;
            return n;
        }

        c->reading = 1;

        c->file.aio->data = r;
        c->file.aio->handler = ngx_http_cache_aio_event_handler;

        ngx_add_timer(&c->file.aio->event, 60000);

        r->main->blocked++;
        r->aio = 1;

        return NGX_AGAIN;
    }

#endif

#if (NGX_THREADS)

    if (clcf->aio == NGX_HTTP_AIO_THREADS) {
        c->file.thread_task = c->thread_task;
        c->file.thread_handler = ngx_http_cache_thread_handler;
        c->file.thread_ctx = r;

        n = ngx_thread_read(&c->file, c->buf->pos, c->body_start, 0, r->pool);

        c->thread_task = c->file.thread_task;
        c->reading = (n == NGX_AGAIN);

        return n;
    }

#endif

    return ngx_read_file(&c->file, c->buf->pos, c->body_start, 0);
}


#if (NGX_HAVE_FILE_AIO)

static void
ngx_http_cache_aio_event_handler(ngx_event_t *ev)
{
    ngx_event_aio_t     *aio;
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    aio = ev->data;
    r = aio->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http file cache aio: \"%V?%V\"", &r->uri, &r->args);

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

static ngx_int_t
ngx_http_cache_thread_handler(ngx_thread_task_t *task, ngx_file_t *file)
{
    ngx_str_t                  name;
    ngx_thread_pool_t         *tp;
    ngx_http_request_t        *r;
    ngx_http_core_loc_conf_t  *clcf;

    r = file->thread_ctx;

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
    task->event.handler = ngx_http_cache_thread_event_handler;

    if (ngx_thread_task_post(tp, task) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_add_timer(&task->event, 60000);

    r->main->blocked++;
    r->aio = 1;

    return NGX_OK;
}


static void
ngx_http_cache_thread_event_handler(ngx_event_t *ev)
{
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    r = ev->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http file cache thread: \"%V?%V\"", &r->uri, &r->args);

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


static ngx_int_t
ngx_http_file_cache_exists(ngx_http_file_cache_t *cache, ngx_http_cache_t *c)
{
    ngx_int_t                    rc;
    ngx_http_file_cache_node_t  *fcn;

    ngx_shmtx_lock(&cache->shpool->mutex);

    fcn = c->node;

    if (fcn == NULL) {
        fcn = ngx_http_file_cache_lookup(cache, c->key);
    }

    if (fcn) {
        ngx_queue_remove(&fcn->queue);

        if (c->node == NULL) {
            fcn->uses++;
            fcn->count++;
        }

        if (fcn->error) {

            if (fcn->valid_sec < ngx_time()) {
                goto renew;
            }

            rc = NGX_OK;

            goto done;
        }

        if (fcn->exists || fcn->uses >= c->min_uses) {

            c->exists = fcn->exists;
            if (fcn->body_start && !c->update_variant) {
                c->body_start = fcn->body_start;
            }

            rc = NGX_OK;

            goto done;
        }

        rc = NGX_AGAIN;

        goto done;
    }

    fcn = ngx_slab_calloc_locked(cache->shpool,
                                 sizeof(ngx_http_file_cache_node_t));
    if (fcn == NULL) {
        ngx_http_file_cache_set_watermark(cache);

        ngx_shmtx_unlock(&cache->shpool->mutex);

        (void) ngx_http_file_cache_forced_expire(cache);

        ngx_shmtx_lock(&cache->shpool->mutex);

        fcn = ngx_slab_calloc_locked(cache->shpool,
                                     sizeof(ngx_http_file_cache_node_t));
        if (fcn == NULL) {
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                          "could not allocate node%s", cache->shpool->log_ctx);
            rc = NGX_ERROR;
            goto failed;
        }
    }

    cache->sh->count++;

    ngx_memcpy((u_char *) &fcn->node.key, c->key, sizeof(ngx_rbtree_key_t));

    ngx_memcpy(fcn->key, &c->key[sizeof(ngx_rbtree_key_t)],
               NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t));

    ngx_rbtree_insert(&cache->sh->rbtree, &fcn->node);

    fcn->uses = 1;
    fcn->count = 1;

renew:

    rc = NGX_DECLINED;

    fcn->valid_msec = 0;
    fcn->error = 0;
    fcn->exists = 0;
    fcn->valid_sec = 0;
    fcn->uniq = 0;
    fcn->body_start = 0;
    fcn->fs_size = 0;

done:

    fcn->expire = ngx_time() + cache->inactive;

    ngx_queue_insert_head(&cache->sh->queue, &fcn->queue);

    c->uniq = fcn->uniq;
    c->error = fcn->error;
    c->node = fcn;

failed:

    ngx_shmtx_unlock(&cache->shpool->mutex);

    return rc;
}


static ngx_int_t
ngx_http_file_cache_name(ngx_http_request_t *r, ngx_path_t *path)
{
    u_char            *p;
    ngx_http_cache_t  *c;

    c = r->cache;

    if (c->file.name.len) {
        return NGX_OK;
    }

    c->file.name.len = path->name.len + 1 + path->len
                       + 2 * NGX_HTTP_CACHE_KEY_LEN;

    c->file.name.data = ngx_pnalloc(r->pool, c->file.name.len + 1);
    if (c->file.name.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(c->file.name.data, path->name.data, path->name.len);

    p = c->file.name.data + path->name.len + 1 + path->len;
    p = ngx_hex_dump(p, c->key, NGX_HTTP_CACHE_KEY_LEN);
    *p = '\0';

    ngx_create_hashed_filename(path, c->file.name.data, c->file.name.len);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "cache file: \"%s\"", c->file.name.data);

    return NGX_OK;
}


static ngx_http_file_cache_node_t *
ngx_http_file_cache_lookup(ngx_http_file_cache_t *cache, u_char *key)
{
    ngx_int_t                    rc;
    ngx_rbtree_key_t             node_key;
    ngx_rbtree_node_t           *node, *sentinel;
    ngx_http_file_cache_node_t  *fcn;

    ngx_memcpy((u_char *) &node_key, key, sizeof(ngx_rbtree_key_t));

    node = cache->sh->rbtree.root;
    sentinel = cache->sh->rbtree.sentinel;

    while (node != sentinel) {

        if (node_key < node->key) {
            node = node->left;
            continue;
        }

        if (node_key > node->key) {
            node = node->right;
            continue;
        }

        /* node_key == node->key */

        fcn = (ngx_http_file_cache_node_t *) node;

        rc = ngx_memcmp(&key[sizeof(ngx_rbtree_key_t)], fcn->key,
                        NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t));

        if (rc == 0) {
            return fcn;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    /* not found */

    return NULL;
}


static void
ngx_http_file_cache_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t           **p;
    ngx_http_file_cache_node_t   *cn, *cnt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            cn = (ngx_http_file_cache_node_t *) node;
            cnt = (ngx_http_file_cache_node_t *) temp;

            p = (ngx_memcmp(cn->key, cnt->key,
                            NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t))
                 < 0)
                    ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static void
ngx_http_file_cache_vary(ngx_http_request_t *r, u_char *vary, size_t len,
    u_char *hash)
{
    u_char     *p, *last;
    ngx_str_t   name;
    ngx_md5_t   md5;
    u_char      buf[NGX_HTTP_CACHE_VARY_LEN];

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache vary: \"%*s\"", len, vary);

    ngx_md5_init(&md5);
    ngx_md5_update(&md5, r->cache->main, NGX_HTTP_CACHE_KEY_LEN);

    ngx_strlow(buf, vary, len);

    p = buf;
    last = buf + len;

    while (p < last) {

        while (p < last && (*p == ' ' || *p == ',')) { p++; }

        name.data = p;

        while (p < last && *p != ',' && *p != ' ') { p++; }

        name.len = p - name.data;

        if (name.len == 0) {
            break;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http file cache vary: %V", &name);

        ngx_md5_update(&md5, name.data, name.len);
        ngx_md5_update(&md5, (u_char *) ":", sizeof(":") - 1);

        ngx_http_file_cache_vary_header(r, &md5, &name);

        ngx_md5_update(&md5, (u_char *) CRLF, sizeof(CRLF) - 1);
    }

    ngx_md5_final(hash, &md5);
}


static void
ngx_http_file_cache_vary_header(ngx_http_request_t *r, ngx_md5_t *md5,
    ngx_str_t *name)
{
    size_t            len;
    u_char           *p, *start, *last;
    ngx_uint_t        i, multiple, normalize;
    ngx_list_part_t  *part;
    ngx_table_elt_t  *header;

    multiple = 0;
    normalize = 0;

    if (name->len == sizeof("Accept-Charset") - 1
        && ngx_strncasecmp(name->data, (u_char *) "Accept-Charset",
                           sizeof("Accept-Charset") - 1) == 0)
    {
        normalize = 1;

    } else if (name->len == sizeof("Accept-Encoding") - 1
        && ngx_strncasecmp(name->data, (u_char *) "Accept-Encoding",
                           sizeof("Accept-Encoding") - 1) == 0)
    {
        normalize = 1;

    } else if (name->len == sizeof("Accept-Language") - 1
        && ngx_strncasecmp(name->data, (u_char *) "Accept-Language",
                           sizeof("Accept-Language") - 1) == 0)
    {
        normalize = 1;
    }

    part = &r->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (header[i].hash == 0) {
            continue;
        }

        if (header[i].key.len != name->len) {
            continue;
        }

        if (ngx_strncasecmp(header[i].key.data, name->data, name->len) != 0) {
            continue;
        }

        if (!normalize) {

            if (multiple) {
                ngx_md5_update(md5, (u_char *) ",", sizeof(",") - 1);
            }

            ngx_md5_update(md5, header[i].value.data, header[i].value.len);

            multiple = 1;

            continue;
        }

        /* normalize spaces */

        p = header[i].value.data;
        last = p + header[i].value.len;

        while (p < last) {

            while (p < last && (*p == ' ' || *p == ',')) { p++; }

            start = p;

            while (p < last && *p != ',' && *p != ' ') { p++; }

            len = p - start;

            if (len == 0) {
                break;
            }

            if (multiple) {
                ngx_md5_update(md5, (u_char *) ",", sizeof(",") - 1);
            }

            ngx_md5_update(md5, start, len);

            multiple = 1;
        }
    }
}


static ngx_int_t
ngx_http_file_cache_reopen(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    ngx_http_file_cache_t  *cache;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                   "http file cache reopen");

    if (c->secondary) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" has incorrect vary hash",
                      c->file.name.data);
        return NGX_DECLINED;
    }

    cache = c->file_cache;

    ngx_shmtx_lock(&cache->shpool->mutex);

    c->node->count--;
    c->node = NULL;

    ngx_shmtx_unlock(&cache->shpool->mutex);

    c->secondary = 1;
    c->file.name.len = 0;
    c->body_start = c->buffer_size;

    ngx_memcpy(c->key, c->variant, NGX_HTTP_CACHE_KEY_LEN);

    return ngx_http_file_cache_open(r);
}


ngx_int_t
ngx_http_file_cache_set_header(ngx_http_request_t *r, u_char *buf)
{
    ngx_http_file_cache_header_t  *h = (ngx_http_file_cache_header_t *) buf;

    u_char            *p;
    ngx_str_t         *key;
    ngx_uint_t         i;
    ngx_http_cache_t  *c;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache set header");

    c = r->cache;

    ngx_memzero(h, sizeof(ngx_http_file_cache_header_t));

    h->version = NGX_HTTP_CACHE_VERSION;
    h->valid_sec = c->valid_sec;
    h->updating_sec = c->updating_sec;
    h->error_sec = c->error_sec;
    h->last_modified = c->last_modified;
    h->date = c->date;
    h->crc32 = c->crc32;
    h->valid_msec = (u_short) c->valid_msec;
    h->header_start = (u_short) c->header_start;
    h->body_start = (u_short) c->body_start;

    if (c->etag.len <= NGX_HTTP_CACHE_ETAG_LEN) {
        h->etag_len = (u_char) c->etag.len;
        ngx_memcpy(h->etag, c->etag.data, c->etag.len);
    }

    if (c->vary.len) {
        if (c->vary.len > NGX_HTTP_CACHE_VARY_LEN) {
            /* should not happen */
            c->vary.len = NGX_HTTP_CACHE_VARY_LEN;
        }

        h->vary_len = (u_char) c->vary.len;
        ngx_memcpy(h->vary, c->vary.data, c->vary.len);

        ngx_http_file_cache_vary(r, c->vary.data, c->vary.len, c->variant);
        ngx_memcpy(h->variant, c->variant, NGX_HTTP_CACHE_KEY_LEN);
    }

    if (ngx_http_file_cache_update_variant(r, c) != NGX_OK) {
        return NGX_ERROR;
    }

    p = buf + sizeof(ngx_http_file_cache_header_t);

    p = ngx_cpymem(p, ngx_http_file_cache_key, sizeof(ngx_http_file_cache_key));

    key = c->keys.elts;
    for (i = 0; i < c->keys.nelts; i++) {
        p = ngx_copy(p, key[i].data, key[i].len);
    }

    *p = LF;

    return NGX_OK;
}


static ngx_int_t
ngx_http_file_cache_update_variant(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    ngx_http_file_cache_t  *cache;

    if (!c->secondary) {
        return NGX_OK;
    }

    if (c->vary.len
        && ngx_memcmp(c->variant, c->key, NGX_HTTP_CACHE_KEY_LEN) == 0)
    {
        return NGX_OK;
    }

    /*
     * if the variant hash doesn't match one we used as a secondary
     * cache key, switch back to the original key
     */

    cache = c->file_cache;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache main key");

    ngx_shmtx_lock(&cache->shpool->mutex);

    c->node->count--;
    c->node->updating = 0;
    c->node = NULL;

    ngx_shmtx_unlock(&cache->shpool->mutex);

    c->file.name.len = 0;
    c->update_variant = 1;

    ngx_memcpy(c->key, c->main, NGX_HTTP_CACHE_KEY_LEN);

    if (ngx_http_file_cache_exists(cache, c) == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (ngx_http_file_cache_name(r, cache->path) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


void
ngx_http_file_cache_update(ngx_http_request_t *r, ngx_temp_file_t *tf)
{
    off_t                   fs_size;
    ngx_int_t               rc;
    ngx_file_uniq_t         uniq;
    ngx_file_info_t         fi;
    ngx_http_cache_t        *c;
    ngx_ext_rename_file_t   ext;
    ngx_http_file_cache_t  *cache;

    c = r->cache;

    if (c->updated) {
        return;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache update");

    cache = c->file_cache;

    c->updated = 1;
    c->updating = 0;

    uniq = 0;
    fs_size = 0;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache rename: \"%s\" to \"%s\"",
                   tf->file.name.data, c->file.name.data);

    ext.access = NGX_FILE_OWNER_ACCESS;
    ext.path_access = NGX_FILE_OWNER_ACCESS;
    ext.time = -1;
    ext.create_path = 1;
    ext.delete_file = 1;
    ext.log = r->connection->log;

    rc = ngx_ext_rename_file(&tf->file.name, &c->file.name, &ext);

    if (rc == NGX_OK) {

        if (ngx_fd_info(tf->file.fd, &fi) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                          ngx_fd_info_n " \"%s\" failed", tf->file.name.data);

            rc = NGX_ERROR;

        } else {
            uniq = ngx_file_uniq(&fi);
            fs_size = (ngx_file_fs_size(&fi) + cache->bsize - 1) / cache->bsize;
        }
    }

    ngx_shmtx_lock(&cache->shpool->mutex);

    c->node->count--;
    c->node->error = 0;
    c->node->uniq = uniq;
    c->node->body_start = c->body_start;

    cache->sh->size += fs_size - c->node->fs_size;
    c->node->fs_size = fs_size;

    if (rc == NGX_OK) {
        c->node->exists = 1;
    }

    c->node->updating = 0;

    ngx_shmtx_unlock(&cache->shpool->mutex);
}


void
ngx_http_file_cache_update_header(ngx_http_request_t *r)
{
    ssize_t                        n;
    ngx_err_t                      err;
    ngx_file_t                     file;
    ngx_file_info_t                fi;
    ngx_http_cache_t              *c;
    ngx_http_file_cache_header_t   h;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache update header");

    c = r->cache;

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.name = c->file.name;
    file.log = r->connection->log;
    file.fd = ngx_open_file(file.name.data, NGX_FILE_RDWR, NGX_FILE_OPEN, 0);

    if (file.fd == NGX_INVALID_FILE) {
        err = ngx_errno;

        /* cache file may have been deleted */

        if (err == NGX_ENOENT) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http file cache \"%s\" not found",
                           file.name.data);
            return;
        }

        ngx_log_error(NGX_LOG_CRIT, r->connection->log, err,
                      ngx_open_file_n " \"%s\" failed", file.name.data);
        return;
    }

    /*
     * make sure cache file wasn't replaced;
     * if it was, do nothing
     */

    if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                      ngx_fd_info_n " \"%s\" failed", file.name.data);
        goto done;
    }

    if (c->uniq != ngx_file_uniq(&fi)
        || c->length != ngx_file_size(&fi))
    {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http file cache \"%s\" changed",
                       file.name.data);
        goto done;
    }

    n = ngx_read_file(&file, (u_char *) &h,
                      sizeof(ngx_http_file_cache_header_t), 0);

    if (n == NGX_ERROR) {
        goto done;
    }

    if ((size_t) n != sizeof(ngx_http_file_cache_header_t)) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      ngx_read_file_n " read only %z of %z from \"%s\"",
                      n, sizeof(ngx_http_file_cache_header_t), file.name.data);
        goto done;
    }

    if (h.version != NGX_HTTP_CACHE_VERSION
        || h.last_modified != c->last_modified
        || h.crc32 != c->crc32
        || (size_t) h.header_start != c->header_start
        || (size_t) h.body_start != c->body_start)
    {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http file cache \"%s\" content changed",
                       file.name.data);
        goto done;
    }

    /*
     * update cache file header with new data,
     * notably h.valid_sec and h.date
     */

    ngx_memzero(&h, sizeof(ngx_http_file_cache_header_t));

    h.version = NGX_HTTP_CACHE_VERSION;
    h.valid_sec = c->valid_sec;
    h.updating_sec = c->updating_sec;
    h.error_sec = c->error_sec;
    h.last_modified = c->last_modified;
    h.date = c->date;
    h.crc32 = c->crc32;
    h.valid_msec = (u_short) c->valid_msec;
    h.header_start = (u_short) c->header_start;
    h.body_start = (u_short) c->body_start;

    if (c->etag.len <= NGX_HTTP_CACHE_ETAG_LEN) {
        h.etag_len = (u_char) c->etag.len;
        ngx_memcpy(h.etag, c->etag.data, c->etag.len);
    }

    if (c->vary.len) {
        if (c->vary.len > NGX_HTTP_CACHE_VARY_LEN) {
            /* should not happen */
            c->vary.len = NGX_HTTP_CACHE_VARY_LEN;
        }

        h.vary_len = (u_char) c->vary.len;
        ngx_memcpy(h.vary, c->vary.data, c->vary.len);

        ngx_http_file_cache_vary(r, c->vary.data, c->vary.len, c->variant);
        ngx_memcpy(h.variant, c->variant, NGX_HTTP_CACHE_KEY_LEN);
    }

    (void) ngx_write_file(&file, (u_char *) &h,
                          sizeof(ngx_http_file_cache_header_t), 0);

done:

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", file.name.data);
    }
}


ngx_int_t
ngx_http_cache_send(ngx_http_request_t *r)
{
    ngx_int_t          rc;
    ngx_buf_t         *b;
    ngx_chain_t        out;
    ngx_http_cache_t  *c;

    c = r->cache;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache send: %s", c->file.name.data);

    /* we need to allocate all before the header would be sent */

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    if (b->file == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    b->file_pos = c->body_start;
    b->file_last = c->length;

    b->in_file = (c->length - c->body_start) ? 1 : 0;
    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;
    b->sync = (b->last_buf || b->in_file) ? 0 : 1;

    b->file->fd = c->file.fd;
    b->file->name = c->file.name;
    b->file->log = r->connection->log;

    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}


void
ngx_http_file_cache_free(ngx_http_cache_t *c, ngx_temp_file_t *tf)
{
    ngx_http_file_cache_t       *cache;
    ngx_http_file_cache_node_t  *fcn;

    if (c->updated || c->node == NULL) {
        return;
    }

    cache = c->file_cache;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                   "http file cache free, fd: %d", c->file.fd);

    ngx_shmtx_lock(&cache->shpool->mutex);

    fcn = c->node;
    fcn->count--;

    if (c->updating && fcn->lock_time == c->lock_time) {
        fcn->updating = 0;
    }

    if (c->error) {
        fcn->error = c->error;

        if (c->valid_sec) {
            fcn->valid_sec = c->valid_sec;
            fcn->valid_msec = c->valid_msec;
        }

    } else if (!fcn->exists && fcn->count == 0 && c->min_uses == 1) {
        ngx_queue_remove(&fcn->queue);
        ngx_rbtree_delete(&cache->sh->rbtree, &fcn->node);
        ngx_slab_free_locked(cache->shpool, fcn);
        cache->sh->count--;
        c->node = NULL;
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    c->updated = 1;
    c->updating = 0;

    if (c->temp_file) {
        if (tf && tf->file.fd != NGX_INVALID_FILE) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                           "http file cache incomplete: \"%s\"",
                           tf->file.name.data);

            if (ngx_delete_file(tf->file.name.data) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_CRIT, c->file.log, ngx_errno,
                              ngx_delete_file_n " \"%s\" failed",
                              tf->file.name.data);
            }
        }
    }

    if (c->wait_event.timer_set) {
        ngx_del_timer(&c->wait_event);
    }
}


static void
ngx_http_file_cache_cleanup(void *data)
{
    ngx_http_cache_t  *c = data;

    if (c->updated) {
        return;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                   "http file cache cleanup");

    if (c->updating && !c->background) {
        ngx_log_error(NGX_LOG_ALERT, c->file.log, 0,
                      "stalled cache updating, error:%ui", c->error);
    }

    ngx_http_file_cache_free(c, NULL);
}


static time_t
ngx_http_file_cache_forced_expire(ngx_http_file_cache_t *cache)
{
    u_char                      *name, *p;
    size_t                       len;
    time_t                       wait;
    ngx_uint_t                   tries;
    ngx_path_t                  *path;
    ngx_queue_t                 *q, *sentinel;
    ngx_http_file_cache_node_t  *fcn;
    u_char                       key[2 * NGX_HTTP_CACHE_KEY_LEN];

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "http file cache forced expire");

    path = cache->path;
    len = path->name.len + 1 + path->len + 2 * NGX_HTTP_CACHE_KEY_LEN;

    name = ngx_alloc(len + 1, ngx_cycle->log);
    if (name == NULL) {
        return 10;
    }

    ngx_memcpy(name, path->name.data, path->name.len);

    wait = 10;
    tries = 20;
    sentinel = NULL;

    ngx_shmtx_lock(&cache->shpool->mutex);

    for ( ;; ) {
        if (ngx_queue_empty(&cache->sh->queue)) {
            break;
        }

        q = ngx_queue_last(&cache->sh->queue);

        if (q == sentinel) {
            break;
        }

        fcn = ngx_queue_data(q, ngx_http_file_cache_node_t, queue);

        ngx_log_debug6(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                  "http file cache forced expire: #%d %d %02xd%02xd%02xd%02xd",
                  fcn->count, fcn->exists,
                  fcn->key[0], fcn->key[1], fcn->key[2], fcn->key[3]);

        if (fcn->count == 0) {
            ngx_http_file_cache_delete(cache, q, name);
            wait = 0;
            break;
        }

        if (fcn->deleting) {
            wait = 1;
            break;
        }

        p = ngx_hex_dump(key, (u_char *) &fcn->node.key,
                         sizeof(ngx_rbtree_key_t));
        len = NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t);
        (void) ngx_hex_dump(p, fcn->key, len);

        /*
         * abnormally exited workers may leave locked cache entries,
         * and although it may be safe to remove them completely,
         * we prefer to just move them to the top of the inactive queue
         */

        ngx_queue_remove(q);
        fcn->expire = ngx_time() + cache->inactive;
        ngx_queue_insert_head(&cache->sh->queue, &fcn->queue);

        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                      "ignore long locked inactive cache entry %*s, count:%d",
                      (size_t) 2 * NGX_HTTP_CACHE_KEY_LEN, key, fcn->count);

        if (sentinel == NULL) {
            sentinel = q;
        }

        if (--tries) {
            continue;
        }

        wait = 1;
        break;
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    ngx_free(name);

    return wait;
}


static time_t
ngx_http_file_cache_expire(ngx_http_file_cache_t *cache)
{
    u_char                      *name, *p;
    size_t                       len;
    time_t                       now, wait;
    ngx_path_t                  *path;
    ngx_msec_t                   elapsed;
    ngx_queue_t                 *q;
    ngx_http_file_cache_node_t  *fcn;
    u_char                       key[2 * NGX_HTTP_CACHE_KEY_LEN];

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "http file cache expire");

    path = cache->path;
    len = path->name.len + 1 + path->len + 2 * NGX_HTTP_CACHE_KEY_LEN;

    name = ngx_alloc(len + 1, ngx_cycle->log);
    if (name == NULL) {
        return 10;
    }

    ngx_memcpy(name, path->name.data, path->name.len);

    now = ngx_time();

    ngx_shmtx_lock(&cache->shpool->mutex);

    for ( ;; ) {

        if (ngx_quit || ngx_terminate) {
            wait = 1;
            break;
        }

        if (ngx_queue_empty(&cache->sh->queue)) {
            wait = 10;
            break;
        }

        q = ngx_queue_last(&cache->sh->queue);

        fcn = ngx_queue_data(q, ngx_http_file_cache_node_t, queue);

        wait = fcn->expire - now;

        if (wait > 0) {
            wait = wait > 10 ? 10 : wait;
            break;
        }

        ngx_log_debug6(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "http file cache expire: #%d %d %02xd%02xd%02xd%02xd",
                       fcn->count, fcn->exists,
                       fcn->key[0], fcn->key[1], fcn->key[2], fcn->key[3]);

        if (fcn->count == 0) {
            ngx_http_file_cache_delete(cache, q, name);
            goto next;
        }

        if (fcn->deleting) {
            wait = 1;
            break;
        }

        p = ngx_hex_dump(key, (u_char *) &fcn->node.key,
                         sizeof(ngx_rbtree_key_t));
        len = NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t);
        (void) ngx_hex_dump(p, fcn->key, len);

        /*
         * abnormally exited workers may leave locked cache entries,
         * and although it may be safe to remove them completely,
         * we prefer to just move them to the top of the inactive queue
         */

        ngx_queue_remove(q);
        fcn->expire = ngx_time() + cache->inactive;
        ngx_queue_insert_head(&cache->sh->queue, &fcn->queue);

        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                      "ignore long locked inactive cache entry %*s, count:%d",
                      (size_t) 2 * NGX_HTTP_CACHE_KEY_LEN, key, fcn->count);

next:

        if (++cache->files >= cache->manager_files) {
            wait = 0;
            break;
        }

        ngx_time_update();

        elapsed = ngx_abs((ngx_msec_int_t) (ngx_current_msec - cache->last));

        if (elapsed >= cache->manager_threshold) {
            wait = 0;
            break;
        }
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    ngx_free(name);

    return wait;
}


static void
ngx_http_file_cache_delete(ngx_http_file_cache_t *cache, ngx_queue_t *q,
    u_char *name)
{
    u_char                      *p;
    size_t                       len;
    ngx_path_t                  *path;
    ngx_http_file_cache_node_t  *fcn;

    fcn = ngx_queue_data(q, ngx_http_file_cache_node_t, queue);

    if (fcn->exists) {
        cache->sh->size -= fcn->fs_size;

        path = cache->path;
        p = name + path->name.len + 1 + path->len;
        p = ngx_hex_dump(p, (u_char *) &fcn->node.key,
                         sizeof(ngx_rbtree_key_t));
        len = NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t);
        p = ngx_hex_dump(p, fcn->key, len);
        *p = '\0';

        fcn->count++;
        fcn->deleting = 1;
        ngx_shmtx_unlock(&cache->shpool->mutex);

        len = path->name.len + 1 + path->len + 2 * NGX_HTTP_CACHE_KEY_LEN;
        ngx_create_hashed_filename(path, name, len);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "http file cache expire: \"%s\"", name);

        if (ngx_delete_file(name) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, ngx_errno,
                          ngx_delete_file_n " \"%s\" failed", name);
        }

        ngx_shmtx_lock(&cache->shpool->mutex);
        fcn->count--;
        fcn->deleting = 0;
    }

    if (fcn->count == 0) {
        ngx_queue_remove(q);
        ngx_rbtree_delete(&cache->sh->rbtree, &fcn->node);
        ngx_slab_free_locked(cache->shpool, fcn);
        cache->sh->count--;
    }
}


static ngx_msec_t
ngx_http_file_cache_manager(void *data)
{
    ngx_http_file_cache_t  *cache = data;

    off_t       size, free;
    time_t      wait;
    ngx_msec_t  elapsed, next;
    ngx_uint_t  count, watermark;

    cache->last = ngx_current_msec;
    cache->files = 0;

    next = (ngx_msec_t) ngx_http_file_cache_expire(cache) * 1000;

    if (next == 0) {
        next = cache->manager_sleep;
        goto done;
    }

    for ( ;; ) {
        ngx_shmtx_lock(&cache->shpool->mutex);

        size = cache->sh->size;
        count = cache->sh->count;
        watermark = cache->sh->watermark;

        ngx_shmtx_unlock(&cache->shpool->mutex);

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "http file cache size: %O c:%ui w:%i",
                       size, count, (ngx_int_t) watermark);

        if (size < cache->max_size && count < watermark) {

            if (!cache->min_free) {
                break;
            }

            free = ngx_fs_available(cache->path->name.data);

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                           "http file cache free: %O", free);

            if (free > cache->min_free) {
                break;
            }
        }

        wait = ngx_http_file_cache_forced_expire(cache);

        if (wait > 0) {
            next = (ngx_msec_t) wait * 1000;
            break;
        }

        if (ngx_quit || ngx_terminate) {
            break;
        }

        if (++cache->files >= cache->manager_files) {
            next = cache->manager_sleep;
            break;
        }

        ngx_time_update();

        elapsed = ngx_abs((ngx_msec_int_t) (ngx_current_msec - cache->last));

        if (elapsed >= cache->manager_threshold) {
            next = cache->manager_sleep;
            break;
        }
    }

done:

    elapsed = ngx_abs((ngx_msec_int_t) (ngx_current_msec - cache->last));

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "http file cache manager: %ui e:%M n:%M",
                   cache->files, elapsed, next);

    return next;
}


static void
ngx_http_file_cache_loader(void *data)
{
    ngx_http_file_cache_t  *cache = data;

    ngx_tree_ctx_t  tree;

    if (!cache->sh->cold || cache->sh->loading) {
        return;
    }

    if (!ngx_atomic_cmp_set(&cache->sh->loading, 0, ngx_pid)) {
        return;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "http file cache loader");

    tree.init_handler = NULL;
    tree.file_handler = ngx_http_file_cache_manage_file;
    tree.pre_tree_handler = ngx_http_file_cache_manage_directory;
    tree.post_tree_handler = ngx_http_file_cache_noop;
    tree.spec_handler = ngx_http_file_cache_delete_file;
    tree.data = cache;
    tree.alloc = 0;
    tree.log = ngx_cycle->log;

    cache->last = ngx_current_msec;
    cache->files = 0;

    if (ngx_walk_tree(&tree, &cache->path->name) == NGX_ABORT) {
        cache->sh->loading = 0;
        return;
    }

    cache->sh->cold = 0;
    cache->sh->loading = 0;

    ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                  "http file cache: %V %.3fM, bsize: %uz",
                  &cache->path->name,
                  ((double) cache->sh->size * cache->bsize) / (1024 * 1024),
                  cache->bsize);
}


static ngx_int_t
ngx_http_file_cache_noop(ngx_tree_ctx_t *ctx, ngx_str_t *path)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_file_cache_manage_file(ngx_tree_ctx_t *ctx, ngx_str_t *path)
{
    ngx_msec_t              elapsed;
    ngx_http_file_cache_t  *cache;

    cache = ctx->data;

    if (ngx_http_file_cache_add_file(ctx, path) != NGX_OK) {
        (void) ngx_http_file_cache_delete_file(ctx, path);
    }

    if (++cache->files >= cache->loader_files) {
        ngx_http_file_cache_loader_sleep(cache);

    } else {
        ngx_time_update();

        elapsed = ngx_abs((ngx_msec_int_t) (ngx_current_msec - cache->last));

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "http file cache loader time elapsed: %M", elapsed);

        if (elapsed >= cache->loader_threshold) {
            ngx_http_file_cache_loader_sleep(cache);
        }
    }

    return (ngx_quit || ngx_terminate) ? NGX_ABORT : NGX_OK;
}


static ngx_int_t
ngx_http_file_cache_manage_directory(ngx_tree_ctx_t *ctx, ngx_str_t *path)
{
    if (path->len >= 5
        && ngx_strncmp(path->data + path->len - 5, "/temp", 5) == 0)
    {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static void
ngx_http_file_cache_loader_sleep(ngx_http_file_cache_t *cache)
{
    ngx_msleep(cache->loader_sleep);

    ngx_time_update();

    cache->last = ngx_current_msec;
    cache->files = 0;
}


static ngx_int_t
ngx_http_file_cache_add_file(ngx_tree_ctx_t *ctx, ngx_str_t *name)
{
    u_char                 *p;
    ngx_int_t               n;
    ngx_uint_t              i;
    ngx_http_cache_t        c;
    ngx_http_file_cache_t  *cache;

    if (name->len < 2 * NGX_HTTP_CACHE_KEY_LEN) {
        return NGX_ERROR;
    }

    /*
     * Temporary files in cache have a suffix consisting of a dot
     * followed by 10 digits.
     */

    if (name->len >= 2 * NGX_HTTP_CACHE_KEY_LEN + 1 + 10
        && name->data[name->len - 10 - 1] == '.')
    {
        return NGX_OK;
    }

    if (ctx->size < (off_t) sizeof(ngx_http_file_cache_header_t)) {
        ngx_log_error(NGX_LOG_CRIT, ctx->log, 0,
                      "cache file \"%s\" is too small", name->data);
        return NGX_ERROR;
    }

    ngx_memzero(&c, sizeof(ngx_http_cache_t));
    cache = ctx->data;

    c.length = ctx->size;
    c.fs_size = (ctx->fs_size + cache->bsize - 1) / cache->bsize;

    p = &name->data[name->len - 2 * NGX_HTTP_CACHE_KEY_LEN];

    for (i = 0; i < NGX_HTTP_CACHE_KEY_LEN; i++) {
        n = ngx_hextoi(p, 2);

        if (n == NGX_ERROR) {
            return NGX_ERROR;
        }

        p += 2;

        c.key[i] = (u_char) n;
    }

    return ngx_http_file_cache_add(cache, &c);
}


static ngx_int_t
ngx_http_file_cache_add(ngx_http_file_cache_t *cache, ngx_http_cache_t *c)
{
    ngx_http_file_cache_node_t  *fcn;

    ngx_shmtx_lock(&cache->shpool->mutex);

    fcn = ngx_http_file_cache_lookup(cache, c->key);

    if (fcn == NULL) {

        fcn = ngx_slab_calloc_locked(cache->shpool,
                                     sizeof(ngx_http_file_cache_node_t));
        if (fcn == NULL) {
            ngx_http_file_cache_set_watermark(cache);

            if (cache->fail_time != ngx_time()) {
                cache->fail_time = ngx_time();
                ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                           "could not allocate node%s", cache->shpool->log_ctx);
            }

            ngx_shmtx_unlock(&cache->shpool->mutex);
            return NGX_ERROR;
        }

        cache->sh->count++;

        ngx_memcpy((u_char *) &fcn->node.key, c->key, sizeof(ngx_rbtree_key_t));

        ngx_memcpy(fcn->key, &c->key[sizeof(ngx_rbtree_key_t)],
                   NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t));

        ngx_rbtree_insert(&cache->sh->rbtree, &fcn->node);

        fcn->uses = 1;
        fcn->exists = 1;
        fcn->fs_size = c->fs_size;

        cache->sh->size += c->fs_size;

    } else {
        ngx_queue_remove(&fcn->queue);
    }

    fcn->expire = ngx_time() + cache->inactive;

    ngx_queue_insert_head(&cache->sh->queue, &fcn->queue);

    ngx_shmtx_unlock(&cache->shpool->mutex);

    return NGX_OK;
}


static ngx_int_t
ngx_http_file_cache_delete_file(ngx_tree_ctx_t *ctx, ngx_str_t *path)
{
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                   "http file cache delete: \"%s\"", path->data);

    if (ngx_delete_file(path->data) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, ctx->log, ngx_errno,
                      ngx_delete_file_n " \"%s\" failed", path->data);
    }

    return NGX_OK;
}


static void
ngx_http_file_cache_set_watermark(ngx_http_file_cache_t *cache)
{
    cache->sh->watermark = cache->sh->count - cache->sh->count / 8;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "http file cache watermark: %ui", cache->sh->watermark);
}


time_t
ngx_http_file_cache_valid(ngx_array_t *cache_valid, ngx_uint_t status)
{
    ngx_uint_t               i;
    ngx_http_cache_valid_t  *valid;

    if (cache_valid == NULL) {
        return 0;
    }

    valid = cache_valid->elts;
    for (i = 0; i < cache_valid->nelts; i++) {

        if (valid[i].status == 0) {
            return valid[i].valid;
        }

        if (valid[i].status == status) {
            return valid[i].valid;
        }
    }

    return 0;
}


char *
ngx_http_file_cache_set_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *confp = conf;

    off_t                   max_size, min_free;
    u_char                 *last, *p;
    time_t                  inactive;
    ssize_t                 size;
    ngx_str_t               s, name, *value;
    ngx_int_t               loader_files, manager_files;
    ngx_msec_t              loader_sleep, manager_sleep, loader_threshold,
                            manager_threshold;
    ngx_uint_t              i, n, use_temp_path;
    ngx_array_t            *caches;
    ngx_http_file_cache_t  *cache, **ce;

    cache = ngx_pcalloc(cf->pool, sizeof(ngx_http_file_cache_t));
    if (cache == NULL) {
        return NGX_CONF_ERROR;
    }

    cache->path = ngx_pcalloc(cf->pool, sizeof(ngx_path_t));
    if (cache->path == NULL) {
        return NGX_CONF_ERROR;
    }

    use_temp_path = 1;

    inactive = 600;

    loader_files = 100;
    loader_sleep = 50;
    loader_threshold = 200;

    manager_files = 100;
    manager_sleep = 50;
    manager_threshold = 200;

    name.len = 0;
    size = 0;
    max_size = NGX_MAX_OFF_T_VALUE;
    min_free = 0;

    value = cf->args->elts;

    cache->path->name = value[1];

    if (cache->path->name.data[cache->path->name.len - 1] == '/') {
        cache->path->name.len--;
    }

    if (ngx_conf_full_name(cf->cycle, &cache->path->name, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    for (i = 2; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "levels=", 7) == 0) {

            p = value[i].data + 7;
            last = value[i].data + value[i].len;

            for (n = 0; n < NGX_MAX_PATH_LEVEL && p < last; n++) {

                if (*p > '0' && *p < '3') {

                    cache->path->level[n] = *p++ - '0';
                    cache->path->len += cache->path->level[n] + 1;

                    if (p == last) {
                        break;
                    }

                    if (*p++ == ':' && n < NGX_MAX_PATH_LEVEL - 1 && p < last) {
                        continue;
                    }

                    goto invalid_levels;
                }

                goto invalid_levels;
            }

            if (cache->path->len < 10 + NGX_MAX_PATH_LEVEL) {
                continue;
            }

        invalid_levels:

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid \"levels\" \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (ngx_strncmp(value[i].data, "use_temp_path=", 14) == 0) {

            if (ngx_strcmp(&value[i].data[14], "on") == 0) {
                use_temp_path = 1;

            } else if (ngx_strcmp(&value[i].data[14], "off") == 0) {
                use_temp_path = 0;

            } else {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid use_temp_path value \"%V\", "
                                   "it must be \"on\" or \"off\"",
                                   &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "keys_zone=", 10) == 0) {

            name.data = value[i].data + 10;

            p = (u_char *) ngx_strchr(name.data, ':');

            if (p == NULL) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid keys zone size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            name.len = p - name.data;

            s.data = p + 1;
            s.len = value[i].data + value[i].len - s.data;

            size = ngx_parse_size(&s);

            if (size == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid keys zone size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            if (size < (ssize_t) (2 * ngx_pagesize)) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "keys zone \"%V\" is too small", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "inactive=", 9) == 0) {

            s.len = value[i].len - 9;
            s.data = value[i].data + 9;

            inactive = ngx_parse_time(&s, 1);
            if (inactive == (time_t) NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid inactive value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "max_size=", 9) == 0) {

            s.len = value[i].len - 9;
            s.data = value[i].data + 9;

            max_size = ngx_parse_offset(&s);
            if (max_size < 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid max_size value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "min_free=", 9) == 0) {

#if (NGX_WIN32 || NGX_HAVE_STATFS || NGX_HAVE_STATVFS)

            s.len = value[i].len - 9;
            s.data = value[i].data + 9;

            min_free = ngx_parse_offset(&s);
            if (min_free < 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid min_free value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

#else
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "min_free is not supported "
                               "on this platform, ignored");
#endif

            continue;
        }

        if (ngx_strncmp(value[i].data, "loader_files=", 13) == 0) {

            loader_files = ngx_atoi(value[i].data + 13, value[i].len - 13);
            if (loader_files == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid loader_files value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "loader_sleep=", 13) == 0) {

            s.len = value[i].len - 13;
            s.data = value[i].data + 13;

            loader_sleep = ngx_parse_time(&s, 0);
            if (loader_sleep == (ngx_msec_t) NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid loader_sleep value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "loader_threshold=", 17) == 0) {

            s.len = value[i].len - 17;
            s.data = value[i].data + 17;

            loader_threshold = ngx_parse_time(&s, 0);
            if (loader_threshold == (ngx_msec_t) NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid loader_threshold value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "manager_files=", 14) == 0) {

            manager_files = ngx_atoi(value[i].data + 14, value[i].len - 14);
            if (manager_files == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid manager_files value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "manager_sleep=", 14) == 0) {

            s.len = value[i].len - 14;
            s.data = value[i].data + 14;

            manager_sleep = ngx_parse_time(&s, 0);
            if (manager_sleep == (ngx_msec_t) NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid manager_sleep value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "manager_threshold=", 18) == 0) {

            s.len = value[i].len - 18;
            s.data = value[i].data + 18;

            manager_threshold = ngx_parse_time(&s, 0);
            if (manager_threshold == (ngx_msec_t) NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid manager_threshold value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    if (name.len == 0 || size == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"keys_zone\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    cache->path->manager = ngx_http_file_cache_manager;
    cache->path->loader = ngx_http_file_cache_loader;
    cache->path->data = cache;
    cache->path->conf_file = cf->conf_file->file.name.data;
    cache->path->line = cf->conf_file->line;
    cache->loader_files = loader_files;
    cache->loader_sleep = loader_sleep;
    cache->loader_threshold = loader_threshold;
    cache->manager_files = manager_files;
    cache->manager_sleep = manager_sleep;
    cache->manager_threshold = manager_threshold;

    if (ngx_add_path(cf, &cache->path) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    cache->shm_zone = ngx_shared_memory_add(cf, &name, size, cmd->post);
    if (cache->shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (cache->shm_zone->data) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate zone \"%V\"", &name);
        return NGX_CONF_ERROR;
    }


    cache->shm_zone->init = ngx_http_file_cache_init;
    cache->shm_zone->data = cache;

    cache->use_temp_path = use_temp_path;

    cache->inactive = inactive;
    cache->max_size = max_size;
    cache->min_free = min_free;

    caches = (ngx_array_t *) (confp + cmd->offset);

    ce = ngx_array_push(caches);
    if (ce == NULL) {
        return NGX_CONF_ERROR;
    }

    *ce = cache;

    return NGX_CONF_OK;
}

/*
 * 设置文件缓存有效期的配置处理函数
 * 
 * @param cf 配置结构体指针
 * @param cmd 命令结构体指针
 * @param conf 配置指针
 * @return 成功返回NGX_CONF_OK，失败返回NGX_CONF_ERROR
 */
char *
ngx_http_file_cache_valid_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    char  *p = conf;

    time_t                    valid;
    ngx_str_t                *value;
    ngx_int_t                 status;
    ngx_uint_t                i, n;
    ngx_array_t             **a;
    ngx_http_cache_valid_t   *v;
    static ngx_uint_t         statuses[] = { 200, 301, 302 };

    a = (ngx_array_t **) (p + cmd->offset);

    if (*a == NGX_CONF_UNSET_PTR) {
        *a = ngx_array_create(cf->pool, 1, sizeof(ngx_http_cache_valid_t));
        if (*a == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;
    n = cf->args->nelts - 1;

    valid = ngx_parse_time(&value[n], 1);
    if (valid == (time_t) NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid time value \"%V\"", &value[n]);
        return NGX_CONF_ERROR;
    }

    if (n == 1) {

        for (i = 0; i < 3; i++) {
            v = ngx_array_push(*a);
            if (v == NULL) {
                return NGX_CONF_ERROR;
            }

            v->status = statuses[i];
            v->valid = valid;
        }

        return NGX_CONF_OK;
    }

    for (i = 1; i < n; i++) {

        if (ngx_strcmp(value[i].data, "any") == 0) {

            status = 0;

        } else {

            status = ngx_atoi(value[i].data, value[i].len);
            if (status < 100 || status > 599) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid status \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }
        }

        v = ngx_array_push(*a);
        if (v == NULL) {
            return NGX_CONF_ERROR;
        }

        v->status = status;
        v->valid = valid;
    }

    return NGX_CONF_OK;
}
