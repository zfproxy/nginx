
/*
 * Copyright (C) Nginx, Inc.
 */


/*
 * ngx_bpf.c
 *
 * 该文件实现了Nginx的BPF (Berkeley Packet Filter) 相关功能。
 *
 * 支持的功能:
 * 1. BPF程序加载
 * 2. BPF程序链接
 * 3. BPF系统调用封装
 * 4. BPF程序重定位
 * 5. BPF程序验证
 * 6. BPF日志缓冲区管理
 * 7. BPF程序类型设置
 * 8. BPF许可证管理
 * 9. BPF指令操作
 * 10. BPF错误处理和日志记录
 *
 * 使用注意点:
 * 1. 确保系统内核支持BPF功能
 * 2. 正确设置BPF程序类型和许可证
 * 3. 谨慎处理BPF程序的重定位
 * 4. 注意BPF验证器的错误信息，用于调试
 * 5. 合理设置日志缓冲区大小，避免溢出
 * 6. 注意BPF程序的安全性，避免潜在的系统风险
 * 7. 在加载大型BPF程序时注意内存使用
 * 8. 正确处理BPF系统调用的返回值和错误
 * 9. 在多线程环境中使用时需要考虑同步问题
 * 10. 定期检查和更新BPF程序以适应内核变化
 */


#include <ngx_config.h>
#include <ngx_core.h>

#define NGX_BPF_LOGBUF_SIZE  (16 * 1024)


static ngx_inline int
ngx_bpf(enum bpf_cmd cmd, union bpf_attr *attr, unsigned int size)
{
    return syscall(__NR_bpf, cmd, attr, size);
}


void
ngx_bpf_program_link(ngx_bpf_program_t *program, const char *symbol, int fd)
{
    ngx_uint_t        i;
    ngx_bpf_reloc_t  *rl;

    rl = program->relocs;

    for (i = 0; i < program->nrelocs; i++) {
        if (ngx_strcmp(rl[i].name, symbol) == 0) {
            program->ins[rl[i].offset].src_reg = 1;
            program->ins[rl[i].offset].imm = fd;
        }
    }
}


int
ngx_bpf_load_program(ngx_log_t *log, ngx_bpf_program_t *program)
{
    int             fd;
    union bpf_attr  attr;
#if (NGX_DEBUG)
    char            buf[NGX_BPF_LOGBUF_SIZE];
#endif

    ngx_memzero(&attr, sizeof(union bpf_attr));

    attr.license = (uintptr_t) program->license;
    attr.prog_type = program->type;
    attr.insns = (uintptr_t) program->ins;
    attr.insn_cnt = program->nins;

#if (NGX_DEBUG)
    /* for verifier errors */
    attr.log_buf = (uintptr_t) buf;
    attr.log_size = NGX_BPF_LOGBUF_SIZE;
    attr.log_level = 1;
#endif

    fd = ngx_bpf(BPF_PROG_LOAD, &attr, sizeof(attr));
    if (fd < 0) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      "failed to load BPF program");

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
                       "bpf verifier: %s", buf);

        return -1;
    }

    return fd;
}


int
ngx_bpf_map_create(ngx_log_t *log, enum bpf_map_type type, int key_size,
    int value_size, int max_entries, uint32_t map_flags)
{
    int             fd;
    union bpf_attr  attr;

    ngx_memzero(&attr, sizeof(union bpf_attr));

    attr.map_type = type;
    attr.key_size = key_size;
    attr.value_size = value_size;
    attr.max_entries = max_entries;
    attr.map_flags = map_flags;

    fd = ngx_bpf(BPF_MAP_CREATE, &attr, sizeof(attr));
    if (fd < 0) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      "failed to create BPF map");
        return NGX_ERROR;
    }

    return fd;
}


int
ngx_bpf_map_update(int fd, const void *key, const void *value, uint64_t flags)
{
    union bpf_attr attr;

    ngx_memzero(&attr, sizeof(union bpf_attr));

    attr.map_fd = fd;
    attr.key = (uintptr_t) key;
    attr.value = (uintptr_t) value;
    attr.flags = flags;

    return ngx_bpf(BPF_MAP_UPDATE_ELEM, &attr, sizeof(attr));
}


int
ngx_bpf_map_delete(int fd, const void *key)
{
    union bpf_attr attr;

    ngx_memzero(&attr, sizeof(union bpf_attr));

    attr.map_fd = fd;
    attr.key = (uintptr_t) key;

    return ngx_bpf(BPF_MAP_DELETE_ELEM, &attr, sizeof(attr));
}


int
ngx_bpf_map_lookup(int fd, const void *key, void *value)
{
    union bpf_attr attr;

    ngx_memzero(&attr, sizeof(union bpf_attr));

    attr.map_fd = fd;
    attr.key = (uintptr_t) key;
    attr.value = (uintptr_t) value;

    return ngx_bpf(BPF_MAP_LOOKUP_ELEM, &attr, sizeof(attr));
}
