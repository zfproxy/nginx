
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGINX_H_INCLUDED_
#define _NGINX_H_INCLUDED_


/**
 * @brief Nginx版本号（数字表示）
 */
#define nginx_version      1027002

/**
 * @brief Nginx版本号（字符串表示）
 */
#define NGINX_VERSION      "1.27.2"

/**
 * @brief Nginx完整版本字符串
 */
#define NGINX_VER          "nginx/" NGINX_VERSION

#ifdef NGX_BUILD
/**
 * @brief 包含构建信息的Nginx版本字符串
 */
#define NGINX_VER_BUILD    NGINX_VER " (" NGX_BUILD ")"
#else
/**
 * @brief 不包含构建信息的Nginx版本字符串
 */
#define NGINX_VER_BUILD    NGINX_VER
#endif

/**
 * @brief Nginx环境变量名
 */
#define NGINX_VAR          "NGINX"

/**
 * @brief 旧进程ID文件扩展名
 */
#define NGX_OLDPID_EXT     ".oldbin"


#endif /* _NGINX_H_INCLUDED_ */
