
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ATOMIC_H_INCLUDED_
#define _NGX_ATOMIC_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#if (NGX_HAVE_LIBATOMIC)

#define AO_REQUIRE_CAS
#include <atomic_ops.h>

#define NGX_HAVE_ATOMIC_OPS  1

typedef long                        ngx_atomic_int_t;  // 原子整型
typedef AO_t                        ngx_atomic_uint_t; // 原子无符号整型
typedef volatile ngx_atomic_uint_t  ngx_atomic_t;      // 原子类型

#if (NGX_PTR_SIZE == 8)
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)  // 64位原子类型长度
#else
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)           // 32位原子类型长度
#endif

// 原子比较并设置
#define ngx_atomic_cmp_set(lock, old, new)                                    \
    AO_compare_and_swap(lock, old, new)
// 原子获取并增加    
#define ngx_atomic_fetch_add(value, add)                                      \
    AO_fetch_and_add(value, add)
// 内存屏障    
#define ngx_memory_barrier()        AO_nop()
// CPU暂停
#define ngx_cpu_pause()


#elif (NGX_HAVE_GCC_ATOMIC)

/* GCC 4.1 builtin atomic operations */

#define NGX_HAVE_ATOMIC_OPS  1

typedef long                        ngx_atomic_int_t;   // 原子整型
typedef unsigned long               ngx_atomic_uint_t;  // 原子无符号整型

#if (NGX_PTR_SIZE == 8)
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)  // 64位原子类型长度
#else
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)           // 32位原子类型长度
#endif

typedef volatile ngx_atomic_uint_t  ngx_atomic_t;  // 原子类型

// 原子比较并设置
#define ngx_atomic_cmp_set(lock, old, set)                                    \
    __sync_bool_compare_and_swap(lock, old, set)

// 原子获取并增加
#define ngx_atomic_fetch_add(value, add)                                      \
    __sync_fetch_and_add(value, add)

// 内存屏障
#define ngx_memory_barrier()        __sync_synchronize()

#if ( __i386__ || __i386 || __amd64__ || __amd64 )
#define ngx_cpu_pause()             __asm__ ("pause")  // x86/x64 CPU暂停
#else
#define ngx_cpu_pause()
#endif


#elif (NGX_DARWIN_ATOMIC)

/*
 * use Darwin 8 atomic(3) and barrier(3) operations
 * optimized at run-time for UP and SMP
 */

#include <libkern/OSAtomic.h>

/* "bool" conflicts with perl's CORE/handy.h */
#if 0
#undef bool
#endif


#define NGX_HAVE_ATOMIC_OPS  1

#if (NGX_PTR_SIZE == 8)

typedef int64_t                     ngx_atomic_int_t;   // 64位原子整型
typedef uint64_t                    ngx_atomic_uint_t;  // 64位原子无符号整型
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)  // 64位原子类型长度

// 64位原子比较并设置
#define ngx_atomic_cmp_set(lock, old, new)                                    \
    OSAtomicCompareAndSwap64Barrier(old, new, (int64_t *) lock)

// 64位原子获取并增加
#define ngx_atomic_fetch_add(value, add)                                      \
    (OSAtomicAdd64(add, (int64_t *) value) - add)

#else

typedef int32_t                     ngx_atomic_int_t;   // 32位原子整型
typedef uint32_t                    ngx_atomic_uint_t;  // 32位原子无符号整型
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)  // 32位原子类型长度

// 32位原子比较并设置
#define ngx_atomic_cmp_set(lock, old, new)                                    \
    OSAtomicCompareAndSwap32Barrier(old, new, (int32_t *) lock)

// 32位原子获取并增加
#define ngx_atomic_fetch_add(value, add)                                      \
    (OSAtomicAdd32(add, (int32_t *) value) - add)

#endif

// 内存屏障
#define ngx_memory_barrier()        OSMemoryBarrier()

// CPU暂停
#define ngx_cpu_pause()

typedef volatile ngx_atomic_uint_t  ngx_atomic_t;  // 原子类型


#elif ( __i386__ || __i386 )

typedef int32_t                     ngx_atomic_int_t;   // 32位原子整型
typedef uint32_t                    ngx_atomic_uint_t;  // 32位原子无符号整型
typedef volatile ngx_atomic_uint_t  ngx_atomic_t;       // 原子类型
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)  // 32位原子类型长度


#if ( __SUNPRO_C )

#define NGX_HAVE_ATOMIC_OPS  1

// 原子比较并设置函数声明
ngx_atomic_uint_t
ngx_atomic_cmp_set(ngx_atomic_t *lock, ngx_atomic_uint_t old,
    ngx_atomic_uint_t set);

// 原子获取并增加函数声明
ngx_atomic_int_t
ngx_atomic_fetch_add(ngx_atomic_t *value, ngx_atomic_int_t add);

/*
 * Sun Studio 12 exits with segmentation fault on '__asm ("pause")',
 * so ngx_cpu_pause is declared in src/os/unix/ngx_sunpro_x86.il
 */

// CPU暂停函数声明
void
ngx_cpu_pause(void);

/* the code in src/os/unix/ngx_sunpro_x86.il */

// 内存屏障
#define ngx_memory_barrier()        __asm (".volatile"); __asm (".nonvolatile")


#else /* ( __GNUC__ || __INTEL_COMPILER ) */

#define NGX_HAVE_ATOMIC_OPS  1

#include "ngx_gcc_atomic_x86.h"

#endif


#elif ( __amd64__ || __amd64 )

typedef int64_t                     ngx_atomic_int_t;   // 64位原子整型
typedef uint64_t                    ngx_atomic_uint_t;  // 64位原子无符号整型
typedef volatile ngx_atomic_uint_t  ngx_atomic_t;       // 原子类型
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)  // 64位原子类型长度


#if ( __SUNPRO_C )

#define NGX_HAVE_ATOMIC_OPS  1

// 原子比较并设置函数声明
ngx_atomic_uint_t
ngx_atomic_cmp_set(ngx_atomic_t *lock, ngx_atomic_uint_t old,
    ngx_atomic_uint_t set);

// 原子获取并增加函数声明
ngx_atomic_int_t
ngx_atomic_fetch_add(ngx_atomic_t *value, ngx_atomic_int_t add);

/*
 * Sun Studio 12 exits with segmentation fault on '__asm ("pause")',
 * so ngx_cpu_pause is declared in src/os/unix/ngx_sunpro_amd64.il
 */

// CPU暂停函数声明
void
ngx_cpu_pause(void);

/* the code in src/os/unix/ngx_sunpro_amd64.il */

// 内存屏障
#define ngx_memory_barrier()        __asm (".volatile"); __asm (".nonvolatile")


#else /* ( __GNUC__ || __INTEL_COMPILER ) */

#define NGX_HAVE_ATOMIC_OPS  1

#include "ngx_gcc_atomic_amd64.h"

#endif


#elif ( __sparc__ || __sparc || __sparcv9 )

#if (NGX_PTR_SIZE == 8)

typedef int64_t                     ngx_atomic_int_t;   // 64位原子整型
typedef uint64_t                    ngx_atomic_uint_t;  // 64位原子无符号整型
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)  // 64位原子类型长度

#else

typedef int32_t                     ngx_atomic_int_t;   // 32位原子整型
typedef uint32_t                    ngx_atomic_uint_t;  // 32位原子无符号整型
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)  // 32位原子类型长度

#endif

typedef volatile ngx_atomic_uint_t  ngx_atomic_t;  // 原子类型


#if ( __SUNPRO_C )

#define NGX_HAVE_ATOMIC_OPS  1

#include "ngx_sunpro_atomic_sparc64.h"


#else /* ( __GNUC__ || __INTEL_COMPILER ) */

#define NGX_HAVE_ATOMIC_OPS  1

#include "ngx_gcc_atomic_sparc64.h"

#endif


#elif ( __powerpc__ || __POWERPC__ )

#define NGX_HAVE_ATOMIC_OPS  1

#if (NGX_PTR_SIZE == 8)

typedef int64_t                     ngx_atomic_int_t;   // 64位原子整型
typedef uint64_t                    ngx_atomic_uint_t;  // 64位原子无符号整型
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)  // 64位原子类型长度

#else

typedef int32_t                     ngx_atomic_int_t;   // 32位原子整型
typedef uint32_t                    ngx_atomic_uint_t;  // 32位原子无符号整型
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)  // 32位原子类型长度

#endif

typedef volatile ngx_atomic_uint_t  ngx_atomic_t;  // 原子类型


#include "ngx_gcc_atomic_ppc.h"

#endif


#if !(NGX_HAVE_ATOMIC_OPS)

#define NGX_HAVE_ATOMIC_OPS  0

typedef int32_t                     ngx_atomic_int_t;   // 32位原子整型
typedef uint32_t                    ngx_atomic_uint_t;  // 32位原子无符号整型
typedef volatile ngx_atomic_uint_t  ngx_atomic_t;       // 原子类型
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)  // 32位原子类型长度

// 原子比较并设置函数
static ngx_inline ngx_atomic_uint_t
ngx_atomic_cmp_set(ngx_atomic_t *lock, ngx_atomic_uint_t old,
    ngx_atomic_uint_t set)
{
    if (*lock == old) {
        *lock = set;
        return 1;
    }

    return 0;
}

// 原子获取并增加函数
static ngx_inline ngx_atomic_int_t
ngx_atomic_fetch_add(ngx_atomic_t *value, ngx_atomic_int_t add)
{
    ngx_atomic_int_t  old;

    old = *value;
    *value += add;

    return old;
}

// 内存屏障
#define ngx_memory_barrier()
// CPU暂停
#define ngx_cpu_pause()

#endif

// 自旋锁函数声明
void ngx_spinlock(ngx_atomic_t *lock, ngx_atomic_int_t value, ngx_uint_t spin);

// 尝试加锁宏定义
#define ngx_trylock(lock)  (*(lock) == 0 && ngx_atomic_cmp_set(lock, 0, 1))
// 解锁宏定义
#define ngx_unlock(lock)    *(lock) = 0


#endif /* _NGX_ATOMIC_H_INCLUDED_ */
