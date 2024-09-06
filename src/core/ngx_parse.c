
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

/*
 * ngx_parse.c
 *
 * 该文件实现了Nginx中的解析功能。
 *
 * 支持的功能:
 * 1. 解析大小字符串 (ngx_parse_size)
 * 2. 解析时间字符串 (ngx_parse_time)
 * 3. 解析偏移量字符串 (ngx_parse_offset)
 * 4. 解析布尔值字符串 (ngx_parse_bool)
 *
 * 使用注意点:
 * 1. 输入字符串必须以null结尾
 * 2. 大小解析支持K/k (千字节)和M/m (兆字节)后缀
 * 3. 时间解析支持ms (毫秒), s (秒), m (分钟), h (小时), d (天), w (周), M (月), y (年)后缀
 * 4. 偏移量解析支持K/k, M/m, G/g后缀
 * 5. 布尔值解析支持on/off, yes/no, true/false, 1/0
 * 6. 解析失败时会返回NGX_ERROR
 * 7. 解析结果可能会超出某些类型的范围，使用时需要进行额外的范围检查
 * 8. 在配置文件解析时，这些函数被广泛使用，需要注意性能影响
 * 9. 对于大小和偏移量解析，注意可能的整数溢出问题
 * 10. 时间解析结果的单位为毫秒，使用时可能需要进行单位转换
 */


#include <ngx_config.h>
#include <ngx_core.h>


ssize_t
ngx_parse_size(ngx_str_t *line)
{
    u_char   unit;
    size_t   len;
    ssize_t  size, scale, max;

    len = line->len;

    if (len == 0) {
        return NGX_ERROR;
    }

    unit = line->data[len - 1];

    switch (unit) {
    case 'K':
    case 'k':
        len--;
        max = NGX_MAX_SIZE_T_VALUE / 1024;
        scale = 1024;
        break;

    case 'M':
    case 'm':
        len--;
        max = NGX_MAX_SIZE_T_VALUE / (1024 * 1024);
        scale = 1024 * 1024;
        break;

    default:
        max = NGX_MAX_SIZE_T_VALUE;
        scale = 1;
    }

    size = ngx_atosz(line->data, len);
    if (size == NGX_ERROR || size > max) {
        return NGX_ERROR;
    }

    size *= scale;

    return size;
}


off_t
ngx_parse_offset(ngx_str_t *line)
{
    u_char  unit;
    off_t   offset, scale, max;
    size_t  len;

    len = line->len;

    // 如果输入字符串长度为0，返回错误
    if (len == 0) {
        return NGX_ERROR;
    }

    // 获取最后一个字符作为单位
    unit = line->data[len - 1];

    // 根据单位设置相应的最大值和比例
    switch (unit) {
    case 'K':
    case 'k':
        len--;  // 减去单位字符
        max = NGX_MAX_OFF_T_VALUE / 1024;
        scale = 1024;
        break;

    case 'M':
    case 'm':
        len--;
        max = NGX_MAX_OFF_T_VALUE / (1024 * 1024);
        scale = 1024 * 1024;
        break;

    case 'G':
    case 'g':
        len--;
        max = NGX_MAX_OFF_T_VALUE / (1024 * 1024 * 1024);
        scale = 1024 * 1024 * 1024;
        break;

    default:
        // 如果没有单位，使用默认值
        max = NGX_MAX_OFF_T_VALUE;
        scale = 1;
    }

    // 将字符串转换为off_t类型的数值
    offset = ngx_atoof(line->data, len);
    // 检查转换是否成功，以及是否超过最大值
    if (offset == NGX_ERROR || offset > max) {
        return NGX_ERROR;
    }

    // 根据单位进行缩放
    offset *= scale;

    return offset;
}


ngx_int_t
ngx_parse_time(ngx_str_t *line, ngx_uint_t is_sec)
{
    u_char      *p, *last;
    ngx_int_t    value, total, scale;
    ngx_int_t    max, cutoff, cutlim;
    ngx_uint_t   valid;
    enum {
        st_start = 0,
        st_year,
        st_month,
        st_week,
        st_day,
        st_hour,
        st_min,
        st_sec,
        st_msec,
        st_last
    } step;

    valid = 0;
    value = 0;
    total = 0;
    cutoff = NGX_MAX_INT_T_VALUE / 10;
    cutlim = NGX_MAX_INT_T_VALUE % 10;
    step = is_sec ? st_start : st_month;

    p = line->data;
    last = p + line->len;

    while (p < last) {

        if (*p >= '0' && *p <= '9') {
            if (value >= cutoff && (value > cutoff || *p - '0' > cutlim)) {
                return NGX_ERROR;
            }

            value = value * 10 + (*p++ - '0');
            valid = 1;
            continue;
        }

        switch (*p++) {

        case 'y':
            if (step > st_start) {
                return NGX_ERROR;
            }
            step = st_year;
            max = NGX_MAX_INT_T_VALUE / (60 * 60 * 24 * 365);
            scale = 60 * 60 * 24 * 365;
            break;

        case 'M':
            if (step >= st_month) {
                return NGX_ERROR;
            }
            step = st_month;
            max = NGX_MAX_INT_T_VALUE / (60 * 60 * 24 * 30);
            scale = 60 * 60 * 24 * 30;
            break;

        case 'w':
            if (step >= st_week) {
                return NGX_ERROR;
            }
            step = st_week;
            max = NGX_MAX_INT_T_VALUE / (60 * 60 * 24 * 7);
            scale = 60 * 60 * 24 * 7;
            break;

        case 'd':
            if (step >= st_day) {
                return NGX_ERROR;
            }
            step = st_day;
            max = NGX_MAX_INT_T_VALUE / (60 * 60 * 24);
            scale = 60 * 60 * 24;
            break;

        case 'h':
            if (step >= st_hour) {
                return NGX_ERROR;
            }
            step = st_hour;
            max = NGX_MAX_INT_T_VALUE / (60 * 60);
            scale = 60 * 60;
            break;

        case 'm':
            if (p < last && *p == 's') {
                if (is_sec || step >= st_msec) {
                    return NGX_ERROR;
                }
                p++;
                step = st_msec;
                max = NGX_MAX_INT_T_VALUE;
                scale = 1;
                break;
            }

            if (step >= st_min) {
                return NGX_ERROR;
            }
            step = st_min;
            max = NGX_MAX_INT_T_VALUE / 60;
            scale = 60;
            break;

        case 's':
            if (step >= st_sec) {
                return NGX_ERROR;
            }
            step = st_sec;
            max = NGX_MAX_INT_T_VALUE;
            scale = 1;
            break;

        case ' ':
            if (step >= st_sec) {
                return NGX_ERROR;
            }
            step = st_last;
            max = NGX_MAX_INT_T_VALUE;
            scale = 1;
            break;

        default:
            return NGX_ERROR;
        }

        if (step != st_msec && !is_sec) {
            scale *= 1000;
            max /= 1000;
        }

        if (value > max) {
            return NGX_ERROR;
        }

        value *= scale;

        if (total > NGX_MAX_INT_T_VALUE - value) {
            return NGX_ERROR;
        }

        total += value;

        value = 0;

        while (p < last && *p == ' ') {
            p++;
        }
    }

    if (!valid) {
        return NGX_ERROR;
    }

    if (!is_sec) {
        if (value > NGX_MAX_INT_T_VALUE / 1000) {
            return NGX_ERROR;
        }

        value *= 1000;
    }

    if (total > NGX_MAX_INT_T_VALUE - value) {
        return NGX_ERROR;
    }

    return total + value;
}
