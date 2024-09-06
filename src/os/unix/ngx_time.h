
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_TIME_H_INCLUDED_
#define _NGX_TIME_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/* 定义 ngx_msec_t 类型为 ngx_rbtree_key_t 的别名
 * 这通常用于表示毫秒级的时间戳或时间间隔
 * 使用红黑树键类型可能是为了在时间相关的数据结构中优化性能
 */
typedef ngx_rbtree_key_t      ngx_msec_t;
/* 定义 ngx_msec_int_t 类型为 ngx_rbtree_key_int_t 的别名
 * 这可能用于表示毫秒级的整数时间戳或时间间隔
 * 使用红黑树整数键类型可能是为了在时间相关的数据结构中进一步优化性能和内存使用
 */
typedef ngx_rbtree_key_int_t  ngx_msec_int_t;

/**
 * @brief 定义 ngx_tm_t 类型为 struct tm 的别名
 *
 * 这个类型定义将标准C库中的 struct tm 结构重命名为 ngx_tm_t。
 * struct tm 用于表示分解后的日期和时间信息，包含年、月、日、时、分、秒等字段。
 * 使用这个别名可以使Nginx代码更加一致，并且在需要时可以更容易地替换底层的时间结构。
 */
typedef struct tm             ngx_tm_t;

/**
 * @brief 定义 ngx_tm_sec 为 tm_sec 的别名
 *
 * 这个宏定义将标准 struct tm 结构中的 tm_sec 字段重命名为 ngx_tm_sec。
 * tm_sec 表示秒数，范围从 0 到 59。
 * 使用这个别名可以保持 Nginx 代码的一致性，并在需要时更容易地替换底层时间结构。
 */
#define ngx_tm_sec            tm_sec
/**
 * @brief 定义 ngx_tm_min 为 tm_min 的别名
 *
 * 这个宏定义将标准 struct tm 结构中的 tm_min 字段重命名为 ngx_tm_min。
 * tm_min 表示分钟数，范围从 0 到 59。
 * 使用这个别名可以保持 Nginx 代码的一致性，并在需要时更容易地替换底层时间结构。
 */
#define ngx_tm_min            tm_min
#define ngx_tm_hour           tm_hour
#define ngx_tm_mday           tm_mday
#define ngx_tm_mon            tm_mon
#define ngx_tm_year           tm_year
#define ngx_tm_wday           tm_wday
#define ngx_tm_isdst          tm_isdst

#define ngx_tm_sec_t          int
#define ngx_tm_min_t          int
#define ngx_tm_hour_t         int
#define ngx_tm_mday_t         int
#define ngx_tm_mon_t          int
#define ngx_tm_year_t         int
#define ngx_tm_wday_t         int


#if (NGX_HAVE_GMTOFF)
/**
 * @brief 定义 ngx_tm_gmtoff 为 tm_gmtoff 的别名
 *
 * 这个宏定义将标准 struct tm 结构中的 tm_gmtoff 字段重命名为 ngx_tm_gmtoff。
 * tm_gmtoff 表示与格林威治标准时间（GMT）的偏移量，以秒为单位。
 * 使用这个别名可以保持 Nginx 代码的一致性，并在需要时更容易地替换底层时间结构。
 * 注意：这个字段并不是在所有系统上都可用，因此这个定义可能是条件编译的一部分。
 */
#define ngx_tm_gmtoff         tm_gmtoff
/**
 * @brief 定义 ngx_tm_zone 为 tm_zone 的别名
 *
 * 这个宏定义将标准 struct tm 结构中的 tm_zone 字段重命名为 ngx_tm_zone。
 * tm_zone 通常用于表示时区的名称或缩写（例如，"GMT"、"EST"等）。
 * 使用这个别名可以保持 Nginx 代码的一致性，并在需要时更容易地替换底层时间结构。
 * 注意：这个字段可能不是在所有系统上都可用，因此这个定义可能是条件编译的一部分。
 */
#define ngx_tm_zone           tm_zone
#endif


#if (NGX_SOLARIS)

#define ngx_timezone(isdst) (- (isdst ? altzone : timezone) / 60)

#else

/**
 * @brief 计算时区偏移量（以分钟为单位）
 *
 * @param isdst 是否为夏令时
 * @return 时区偏移量（分钟）
 *
 * 这个宏定义用于计算当前时区相对于UTC的偏移量（以分钟为单位）。
 * - 如果 isdst 为真（夏令时），则使用 timezone + 3600 秒（1小时）
 * - 如果 isdst 为假（标准时间），则直接使用 timezone
 * 计算结果取反并除以60，将秒转换为分钟
 */
#define ngx_timezone(isdst) (- (isdst ? timezone + 3600 : timezone) / 60)

#endif


/**
 * @brief 更新时区信息
 *
 * 这个函数用于更新Nginx的时区信息。它通常在以下情况下被调用：
 * 1. 系统时区发生变化时
 * 2. 夏令时开始或结束时
 * 3. Nginx重新加载配置时
 *
 * 函数会重新计算当前的时区偏移量，并更新相关的内部时间结构。
 * 这确保了Nginx在处理时间相关操作时始终使用最新的时区信息。
 *
 * @note 这个函数不接受参数，也不返回值。它直接修改Nginx的内部时间状态。
 */
void ngx_timezone_update(void);
/**
 * @brief 将时间戳转换为本地时间
 *
 * 这个函数将给定的 UNIX 时间戳转换为本地时间，并将结果存储在提供的 ngx_tm_t 结构中。
 * 它考虑了当前的时区设置和夏令时调整。
 *
 * @param s 要转换的 UNIX 时间戳（自 1970 年 1 月 1 日以来的秒数）
 * @param tm 指向 ngx_tm_t 结构的指针，用于存储转换后的本地时间
 *
 * @note ngx_tm_t 是 Nginx 定义的时间结构，类似于标准 C 库的 struct tm
 * @note 这个函数是线程安全的，可以在多线程环境中使用
 */
void ngx_localtime(time_t s, ngx_tm_t *tm);
/**
 * @brief 使用系统库函数将时间戳转换为本地时间
 *
 * 这个函数是对系统标准库 localtime 函数的封装。它将给定的 UNIX 时间戳
 * 转换为本地时间，并将结果存储在提供的 struct tm 结构中。
 *
 * @param s 要转换的 UNIX 时间戳（自 1970 年 1 月 1 日以来的秒数）
 * @param tm 指向 struct tm 结构的指针，用于存储转换后的本地时间
 *
 * @note 这个函数使用系统的 localtime 函数，可能不是线程安全的
 * @note 与 ngx_localtime 不同，这个函数使用标准 C 库的 struct tm 结构
 */
void ngx_libc_localtime(time_t s, struct tm *tm);
/**
 * @brief 使用系统库函数将时间戳转换为GMT时间
 *
 * 这个函数是对系统标准库 gmtime 函数的封装。它将给定的 UNIX 时间戳
 * 转换为GMT（格林威治标准时间），并将结果存储在提供的 struct tm 结构中。
 *
 * @param s 要转换的 UNIX 时间戳（自 1970 年 1 月 1 日以来的秒数）
 * @param tm 指向 struct tm 结构的指针，用于存储转换后的GMT时间
 *
 * @note 这个函数使用系统的 gmtime 函数，可能不是线程安全的
 * @note 与 ngx_localtime 不同，这个函数返回的是GMT时间，不考虑本地时区
 */
void ngx_libc_gmtime(time_t s, struct tm *tm);

/**
 * @brief 获取当前系统时间
 *
 * 这个宏定义封装了系统的 gettimeofday 函数，用于获取当前的系统时间。
 * 它将结果存储在提供的 timeval 结构中。
 *
 * @param tp 指向 struct timeval 结构的指针，用于存储获取的时间
 *
 * @note 这个宏忽略了 gettimeofday 的返回值，因为在大多数情况下不需要检查错误
 * @note 第二个参数（时区）被设置为 NULL，因为现代系统不再使用这个参数
 */
#define ngx_gettimeofday(tp)  (void) gettimeofday(tp, NULL);
/**
 * @brief 使进程休眠指定的毫秒数
 *
 * 这个宏定义封装了系统的 usleep 函数，用于使当前进程休眠指定的毫秒数。
 * 它将毫秒转换为微秒（1毫秒 = 1000微秒），然后调用 usleep 函数。
 *
 * @param ms 要休眠的毫秒数
 *
 * @note 这个宏使用 (void) 来忽略 usleep 的返回值，因为在大多数情况下不需要检查错误
 * @note usleep 函数的参数是微秒，所以这里将毫秒乘以1000进行转换
 */
#define ngx_msleep(ms)        (void) usleep(ms * 1000)
/**
 * @brief 使进程休眠指定的秒数
 *
 * 这个宏定义封装了系统的 sleep 函数，用于使当前进程休眠指定的秒数。
 *
 * @param s 要休眠的秒数
 *
 * @note 这个宏使用 (void) 来忽略 sleep 的返回值，因为在大多数情况下不需要检查错误
 * @note 与 ngx_msleep 不同，这个宏使用秒作为单位，而不是毫秒
 */
#define ngx_sleep(s)          (void) sleep(s)


#endif /* _NGX_TIME_H_INCLUDED_ */
