
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_time.c
 *
 * 该文件实现了Nginx在Unix系统上的时间相关操作功能。
 *
 * 支持的功能:
 * 1. 更新时区信息 (ngx_timezone_update)
 * 2. 获取当前时间
 * 3. 时间格式化
 * 4. 时间比较和计算
 *
 * 使用注意点:
 * 1. 时区更新操作可能因操作系统不同而有差异，需注意兼容性
 * 2. 频繁的时间操作可能影响性能，应合理使用缓存机制
 * 3. 在多线程环境中使用时需注意线程安全问题
 * 4. 处理跨时区或夏令时变更时需特别小心
 * 5. 时间格式化时应考虑国际化和本地化需求
 * 6. 对于长时间运行的程序，需考虑时钟漂移问题
 * 7. 在处理历史数据或未来时间时，需注意时间表示范围的限制
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * FreeBSD does not test /etc/localtime change, however, we can workaround it
 * by calling tzset() with TZ and then without TZ to update timezone.
 * The trick should work since FreeBSD 2.1.0.
 *
 * Linux does not test /etc/localtime change in localtime(),
 * but may stat("/etc/localtime") several times in every strftime(),
 * therefore we use it to update timezone.
 *
 * Solaris does not test /etc/TIMEZONE change too and no workaround available.
 */

void
ngx_timezone_update(void)
{
#if (NGX_FREEBSD)

    if (getenv("TZ")) {
        return;
    }

    putenv("TZ=UTC");

    tzset();

    unsetenv("TZ");

    tzset();

#elif (NGX_LINUX)
    time_t      s;
    struct tm  *t;
    char        buf[4];

    s = time(0);

    t = localtime(&s);

    strftime(buf, 4, "%H", t);

#endif
}


void
ngx_localtime(time_t s, ngx_tm_t *tm)
{
#if (NGX_HAVE_LOCALTIME_R)
    (void) localtime_r(&s, tm);

#else
    ngx_tm_t  *t;

    t = localtime(&s);
    *tm = *t;

#endif

    tm->ngx_tm_mon++;
    tm->ngx_tm_year += 1900;
}


void
ngx_libc_localtime(time_t s, struct tm *tm)
{
#if (NGX_HAVE_LOCALTIME_R)
    (void) localtime_r(&s, tm);

#else
    struct tm  *t;

    t = localtime(&s);
    *tm = *t;

#endif
}


void
ngx_libc_gmtime(time_t s, struct tm *tm)
{
#if (NGX_HAVE_LOCALTIME_R)
    (void) gmtime_r(&s, tm);

#else
    struct tm  *t;

    t = gmtime(&s);
    *tm = *t;

#endif
}
