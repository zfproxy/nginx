
/*
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_BPF_H_INCLUDED_
#define _NGX_BPF_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

#include <linux/bpf.h>


/**
 * @brief BPF重定位结构体
 */
typedef struct {
    char                *name;    /**< 符号名称 */
    int                  offset;  /**< 指令偏移量 */
} ngx_bpf_reloc_t;

/**
 * @brief BPF程序结构体
 */
typedef struct {
    char                *license;  /**< 许可证字符串 */
    enum bpf_prog_type   type;     /**< BPF程序类型 */
    struct bpf_insn     *ins;      /**< BPF指令数组 */
    size_t               nins;     /**< 指令数量 */
    ngx_bpf_reloc_t     *relocs;   /**< 重定位数组 */
    size_t               nrelocs;  /**< 重定位数量 */
} ngx_bpf_program_t;


/**
 * @brief 链接BPF程序中的符号
 * @param program BPF程序结构体指针
 * @param symbol 要链接的符号名
 * @param fd 文件描述符
 */
void ngx_bpf_program_link(ngx_bpf_program_t *program, const char *symbol,
    int fd);

/**
 * @brief 加载BPF程序
 * @param log 日志对象指针
 * @param program BPF程序结构体指针
 * @return 成功返回文件描述符，失败返回-1
 */
int ngx_bpf_load_program(ngx_log_t *log, ngx_bpf_program_t *program);

/**
 * @brief 创建BPF映射
 * @param log 日志对象指针
 * @param type BPF映射类型
 * @param key_size 键大小
 * @param value_size 值大小
 * @param max_entries 最大条目数
 * @param map_flags 映射标志
 * @return 成功返回文件描述符，失败返回NGX_ERROR
 */
int ngx_bpf_map_create(ngx_log_t *log, enum bpf_map_type type, int key_size,
    int value_size, int max_entries, uint32_t map_flags);

/**
 * @brief 更新BPF映射中的元素
 * @param fd BPF映射文件描述符
 * @param key 键指针
 * @param value 值指针
 * @param flags 更新标志
 * @return 成功返回0，失败返回负值
 */
int ngx_bpf_map_update(int fd, const void *key, const void *value,
    uint64_t flags);

/**
 * @brief 从BPF映射中删除元素
 * @param fd BPF映射文件描述符
 * @param key 要删除的元素的键指针
 * @return 成功返回0，失败返回负值
 */
int ngx_bpf_map_delete(int fd, const void *key);

/**
 * @brief 查找BPF映射中的元素
 * @param fd BPF映射文件描述符
 * @param key 要查找的元素的键指针
 * @param value 用于存储查找结果的值指针
 * @return 成功返回0，失败返回负值
 */
int ngx_bpf_map_lookup(int fd, const void *key, void *value);

#endif /* _NGX_BPF_H_INCLUDED_ */
