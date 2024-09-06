
/*
 * Copyright (C) Maxim Dounin
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_DLOPEN_H_INCLUDED_
#define _NGX_DLOPEN_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


// 定义动态加载库函数,使用dlopen打开共享库
#define ngx_dlopen(path)           dlopen((char *) path, RTLD_NOW | RTLD_GLOBAL)
#define ngx_dlopen_n               "dlopen()"

// 定义获取动态库中符号地址的函数,使用dlsym
#define ngx_dlsym(handle, symbol)  dlsym(handle, symbol)
#define ngx_dlsym_n                "dlsym()"

// 定义关闭动态库的函数,使用dlclose
#define ngx_dlclose(handle)        dlclose(handle)
#define ngx_dlclose_n              "dlclose()"


#if (NGX_HAVE_DLOPEN)
// 如果支持dlopen,定义获取动态加载错误信息的函数
char *ngx_dlerror(void);
#endif


#endif /* _NGX_DLOPEN_H_INCLUDED_ */
