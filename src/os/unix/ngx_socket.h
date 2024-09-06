
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SOCKET_H_INCLUDED_
#define _NGX_SOCKET_H_INCLUDED_


#include <ngx_config.h>


#define NGX_WRITE_SHUTDOWN SHUT_WR
#define NGX_READ_SHUTDOWN  SHUT_RD
#define NGX_RDWR_SHUTDOWN  SHUT_RDWR

/**
 * @brief 定义套接字类型
 *
 * 在Unix系统中，将套接字类型定义为整数。
 * 这是因为在Unix/Linux系统中，套接字描述符实际上就是一个整数值。
 * 这个类型定义使得在Nginx代码中可以使用ngx_socket_t来表示套接字，
 * 提高了代码的可读性和可移植性。
 */
typedef int  ngx_socket_t;

/**
 * @brief 定义套接字创建函数
 *
 * 这个宏定义将Nginx中的ngx_socket函数映射到系统的socket函数。
 * 使用这种方式可以在需要时轻松替换底层的套接字创建函数，
 * 提高了代码的可移植性和可维护性。
 * 
 * 在Unix/Linux系统中，socket函数用于创建一个新的套接字描述符。
 */
#define ngx_socket          socket
/**
 * @brief 定义套接字创建函数的字符串表示
 *
 * 这个宏定义了套接字创建函数的字符串表示。
 * 在错误日志或调试输出中使用时，它提供了一个人类可读的描述，
 * 指明正在调用的是"socket()"函数。
 * 这有助于在日志分析和调试过程中快速识别相关的系统调用。
 */
#define ngx_socket_n        "socket()"


#if (NGX_HAVE_FIONBIO)

/**
 * @brief 将套接字设置为非阻塞模式
 *
 * 该函数用于将指定的套接字设置为非阻塞模式。在非阻塞模式下，
 * 套接字的操作（如 send、recv、accept 等）不会阻塞进程，
 * 而是立即返回，即使操作无法立即完成。
 *
 * @param s 要设置为非阻塞模式的套接字描述符
 * @return 成功时返回 0，失败时返回 -1
 */
int ngx_nonblocking(ngx_socket_t s);
/**
 * @brief 将套接字设置为阻塞模式
 *
 * 该函数用于将指定的套接字设置为阻塞模式。在阻塞模式下，
 * 套接字的操作（如 send、recv、accept 等）会阻塞进程，
 * 直到操作完成或出错。这是套接字的默认行为。
 *
 * @param s 要设置为阻塞模式的套接字描述符
 * @return 成功时返回 0，失败时返回 -1
 */
int ngx_blocking(ngx_socket_t s);

/**
 * @brief 定义非阻塞套接字操作的字符串表示
 *
 * 这个宏定义了将套接字设置为非阻塞模式的操作的字符串表示。
 * 在使用 ioctl 系统调用和 FIONBIO 标志来设置非阻塞模式时，
 * 这个字符串可以用于日志记录或错误报告。
 * 
 * "ioctl(FIONBIO)" 表示使用 ioctl 调用和 FIONBIO 标志
 * 来设置套接字的非阻塞属性。这种方法在某些系统上比使用 fcntl 更高效。
 */
#define ngx_nonblocking_n   "ioctl(FIONBIO)"

/**
 * @brief 定义阻塞套接字操作的字符串表示
 *
 * 这个宏定义了将套接字设置为阻塞模式的操作的字符串表示。
 * 使用 ioctl 系统调用和 !FIONBIO 标志来设置阻塞模式时，
 * 这个字符串可以用于日志记录或错误报告。
 * 
 * "ioctl(!FIONBIO)" 表示使用 ioctl 调用和 !FIONBIO 标志
 * 来设置套接字的阻塞属性。这是套接字的默认行为。
 */
#define ngx_blocking_n      "ioctl(!FIONBIO)"

#else

/**
 * @brief 将套接字设置为非阻塞模式
 *
 * 该宏使用fcntl函数将指定的套接字设置为非阻塞模式。
 * 它首先获取套接字的当前标志，然后添加O_NONBLOCK标志。
 *
 * @param s 要设置为非阻塞模式的套接字描述符
 * @return 成功时返回fcntl调用的结果，失败时返回-1
 */
#define ngx_nonblocking(s)  fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK)
/**
 * @brief 定义非阻塞套接字操作的字符串表示
 *
 * 这个宏定义了将套接字设置为非阻塞模式的操作的字符串表示。
 * 在使用 fcntl 系统调用和 O_NONBLOCK 标志来设置非阻塞模式时，
 * 这个字符串可以用于日志记录或错误报告。
 * 
 * "fcntl(O_NONBLOCK)" 表示使用 fcntl 调用和 O_NONBLOCK 标志
 * 来设置套接字的非阻塞属性。这种方法是跨平台的标准做法。
 */
#define ngx_nonblocking_n   "fcntl(O_NONBLOCK)"

/**
 * @brief 将套接字设置为阻塞模式
 *
 * 该宏使用fcntl函数将指定的套接字设置为阻塞模式。
 * 它首先获取套接字的当前标志，然后移除O_NONBLOCK标志。
 *
 * @param s 要设置为阻塞模式的套接字描述符
 * @return 成功时返回fcntl调用的结果，失败时返回-1
 */
#define ngx_blocking(s)     fcntl(s, F_SETFL, fcntl(s, F_GETFL) & ~O_NONBLOCK)
/**
 * @brief 定义阻塞套接字操作的字符串表示
 *
 * 这个宏定义了将套接字设置为阻塞模式的操作的字符串表示。
 * 在使用 fcntl 系统调用和 !O_NONBLOCK 标志来设置阻塞模式时，
 * 这个字符串可以用于日志记录或错误报告。
 * 
 * "fcntl(!O_NONBLOCK)" 表示使用 fcntl 调用并清除 O_NONBLOCK 标志
 * 来设置套接字的阻塞属性。这是将非阻塞套接字恢复为阻塞模式的标准方法。
 */
#define ngx_blocking_n      "fcntl(!O_NONBLOCK)"

#endif

#if (NGX_HAVE_FIONREAD)

/**
 * @brief 获取套接字接收缓冲区中可读数据的字节数
 *
 * 该宏使用ioctl系统调用和FIONREAD命令来获取指定套接字的接收缓冲区中
 * 当前可读数据的字节数。这对于非阻塞I/O操作特别有用，可以在实际读取
 * 数据之前检查有多少数据可用。
 *
 * @param s 要查询的套接字描述符
 * @param n 指向整数的指针，用于存储可读数据的字节数
 * @return 成功时返回ioctl调用的结果（通常为0），失败时返回-1
 */
#define ngx_socket_nread(s, n)  ioctl(s, FIONREAD, n)
/**
 * @brief 定义获取套接字可读数据量操作的字符串表示
 *
 * 这个宏定义了使用ioctl系统调用和FIONREAD命令来获取套接字接收缓冲区中
 * 可读数据字节数的操作的字符串表示。
 * 
 * "ioctl(FIONREAD)" 表示使用ioctl调用和FIONREAD命令来查询套接字的
 * 可读数据量。这个字符串可以用于日志记录、错误报告或调试输出，
 * 以指示正在执行的是获取套接字可读数据量的操作。
 */
#define ngx_socket_nread_n      "ioctl(FIONREAD)"

#endif

/**
 * @brief 设置TCP套接字的NOPUSH选项
 *
 * 该函数用于设置指定TCP套接字的NOPUSH（或CORK）选项。
 * NOPUSH选项可以延迟发送小数据包，直到缓冲区填满或显式刷新，
 * 从而优化网络传输效率。
 *
 * 在Linux系统上，这通常对应于TCP_CORK选项。
 * 在其他Unix系统上，可能对应于TCP_NOPUSH选项。
 *
 * @param s 要设置NOPUSH选项的套接字描述符
 * @return 成功时返回0，失败时返回-1
 */
int ngx_tcp_nopush(ngx_socket_t s);
/**
 * @brief 禁用TCP套接字的NOPUSH选项
 *
 * 该函数用于禁用指定TCP套接字的NOPUSH（或CORK）选项。
 * 禁用NOPUSH选项会立即发送缓冲区中的所有数据，不再延迟小数据包的发送。
 * 这通常用于在需要立即发送数据时调用，如在发送最后一个数据包时。
 *
 * 在Linux系统上，这对应于清除TCP_CORK选项。
 * 在其他Unix系统上，可能对应于清除TCP_NOPUSH选项。
 *
 * @param s 要禁用NOPUSH选项的套接字描述符
 * @return 成功时返回0，失败时返回-1
 */
int ngx_tcp_push(ngx_socket_t s);

#if (NGX_LINUX)

/**
 * @brief 定义TCP NOPUSH选项设置操作的字符串表示
 *
 * 这个宏定义了在Linux系统上使用setsockopt系统调用和TCP_CORK选项
 * 来设置TCP NOPUSH功能的操作的字符串表示。
 * 
 * "setsockopt(TCP_CORK)" 表示使用setsockopt调用和TCP_CORK选项
 * 来启用或禁用TCP的NOPUSH（或CORK）功能。这个字符串可以用于
 * 日志记录、错误报告或调试输出，以指示正在执行的是TCP NOPUSH
 * 选项的设置操作。
 *
 * 在Linux系统中，TCP_CORK选项用于实现TCP NOPUSH功能，
 * 它可以延迟发送小数据包，直到缓冲区填满或显式刷新，
 * 从而优化网络传输效率。
 */
#define ngx_tcp_nopush_n   "setsockopt(TCP_CORK)"
/**
 * @brief 定义TCP PUSH选项设置操作的字符串表示
 *
 * 这个宏定义了在Linux系统上使用setsockopt系统调用和TCP_CORK选项
 * 来禁用TCP NOPUSH功能的操作的字符串表示。
 * 
 * "setsockopt(!TCP_CORK)" 表示使用setsockopt调用来清除TCP_CORK选项，
 * 从而禁用TCP的NOPUSH（或CORK）功能。这个字符串可以用于
 * 日志记录、错误报告或调试输出，以指示正在执行的是禁用TCP NOPUSH
 * 选项的操作。
 *
 * 在Linux系统中，清除TCP_CORK选项会立即发送所有缓冲的数据，
 * 不再延迟小数据包的发送。这通常用于在需要立即发送数据时，
 * 如在发送最后一个数据包时。
 */
#define ngx_tcp_push_n     "setsockopt(!TCP_CORK)"

#else

/**
 * @brief 定义TCP NOPUSH选项设置操作的字符串表示
 *
 * 这个宏定义了在非Linux系统上使用setsockopt系统调用和TCP_NOPUSH选项
 * 来设置TCP NOPUSH功能的操作的字符串表示。
 * 
 * "setsockopt(TCP_NOPUSH)" 表示使用setsockopt调用和TCP_NOPUSH选项
 * 来启用或禁用TCP的NOPUSH功能。这个字符串可以用于日志记录、错误报告
 * 或调试输出，以指示正在执行的是TCP NOPUSH选项的设置操作。
 *
 * 在非Linux系统（如FreeBSD或macOS）中，TCP_NOPUSH选项用于实现
 * 类似于Linux的TCP_CORK功能，它可以延迟发送小数据包，直到缓冲区
 * 填满或显式刷新，从而优化网络传输效率。
 */
#define ngx_tcp_nopush_n   "setsockopt(TCP_NOPUSH)"
/**
 * @brief 定义TCP PUSH选项设置操作的字符串表示
 *
 * 这个宏定义了在非Linux系统上使用setsockopt系统调用来禁用TCP NOPUSH功能的操作的字符串表示。
 * 
 * "setsockopt(!TCP_NOPUSH)" 表示使用setsockopt调用来清除TCP_NOPUSH选项，
 * 从而禁用TCP的NOPUSH功能。这个字符串可以用于日志记录、错误报告或调试输出，
 * 以指示正在执行的是禁用TCP NOPUSH选项的操作。
 *
 * 在非Linux系统（如FreeBSD或macOS）中，清除TCP_NOPUSH选项会立即发送所有缓冲的数据，
 * 不再延迟小数据包的发送。这通常用于在需要立即发送数据时，如在发送最后一个数据包时。
 */
#define ngx_tcp_push_n     "setsockopt(!TCP_NOPUSH)"

#endif


/**
 * @brief 定义套接字关闭操作的宏
 *
 * 这个宏将ngx_shutdown_socket定义为系统调用shutdown的别名。
 * shutdown函数用于关闭套接字的全部或部分连接。
 *
 * 使用这个宏可以在Nginx代码中统一使用ngx_shutdown_socket，
 * 而不是直接调用shutdown，这样可以提高代码的可读性和可维护性，
 * 特别是在需要针对不同平台进行适配时。
 */
#define ngx_shutdown_socket    shutdown
/**
 * @brief 定义套接字关闭操作的字符串表示
 *
 * 这个宏定义了套接字关闭操作的字符串表示。
 * "shutdown()" 表示使用 shutdown 系统调用来关闭套接字连接。
 * 
 * 这个字符串可以用于日志记录、错误报告或调试输出，
 * 以指示正在执行的是套接字关闭操作。
 * 
 * 在 Nginx 的错误处理和日志记录中，这个宏通常与 ngx_shutdown_socket 
 * 一起使用，提供了一种统一的方式来表示和记录套接字关闭操作。
 */
#define ngx_shutdown_socket_n  "shutdown()"

/**
 * @brief 定义套接字关闭函数的宏
 *
 * 这个宏将 ngx_close_socket 定义为系统调用 close 的别名。
 * close 函数用于关闭一个文件描述符，在这里特指关闭套接字。
 *
 * 使用这个宏可以在 Nginx 代码中统一使用 ngx_close_socket，
 * 而不是直接调用 close，这样可以提高代码的可读性和可维护性，
 * 特别是在需要针对不同平台进行适配时。
 *
 * 在 Unix/Linux 系统中，套接字被视为文件描述符，
 * 因此可以直接使用 close 函数来关闭套接字。
 */
#define ngx_close_socket    close
/**
 * @brief 定义套接字关闭函数的字符串表示
 *
 * 这个宏定义了套接字关闭操作的字符串表示。
 * "close() socket" 表示使用 close 系统调用来关闭套接字。
 * 
 * 这个字符串可以用于日志记录、错误报告或调试输出，
 * 以指示正在执行的是套接字关闭操作。
 * 
 * 在 Nginx 的错误处理和日志记录中，这个宏通常与 ngx_close_socket 
 * 一起使用，提供了一种统一的方式来表示和记录套接字关闭操作。
 * 使用 "close() socket" 而不是简单的 "close()" 可以更明确地
 * 指出正在关闭的是一个套接字，而不是普通文件。
 */
#define ngx_close_socket_n  "close() socket"


#endif /* _NGX_SOCKET_H_INCLUDED_ */
