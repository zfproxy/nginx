
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_FILES_H_INCLUDED_
#define _NGX_FILES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/**
 * @brief 文件描述符类型
 */
typedef int                      ngx_fd_t;

/**
 * @brief 文件信息结构体类型
 */
typedef struct stat              ngx_file_info_t;

/**
 * @brief 文件唯一标识符类型
 */
typedef ino_t                    ngx_file_uniq_t;


/**
 * @brief 文件映射结构体
 */
typedef struct {
    u_char                      *name;  /**< 文件名 */
    size_t                       size;  /**< 文件大小 */
    void                        *addr;  /**< 映射地址 */
    ngx_fd_t                     fd;    /**< 文件描述符 */
    ngx_log_t                   *log;   /**< 日志对象 */
} ngx_file_mapping_t;


/**
 * @brief 目录结构体
 */
typedef struct {
    DIR                         *dir;   /**< 目录流 */
    struct dirent               *de;    /**< 目录项 */
    struct stat                  info;  /**< 文件状态信息 */

    unsigned                     type:8;       /**< 文件类型 */
    unsigned                     valid_info:1; /**< 信息是否有效 */
} ngx_dir_t;


/**
 * @brief 全局匹配结构体
 */
typedef struct {
    size_t                       n;       /**< 匹配数量 */
    glob_t                       pglob;   /**< glob结构 */
    u_char                      *pattern; /**< 匹配模式 */
    ngx_log_t                   *log;     /**< 日志对象 */
    ngx_uint_t                   test;    /**< 测试标志 */
} ngx_glob_t;


/**
 * @brief 无效文件描述符
 */
#define NGX_INVALID_FILE         -1

/**
 * @brief 文件操作错误
 */
#define NGX_FILE_ERROR           -1



#ifdef __CYGWIN__

#ifndef NGX_HAVE_CASELESS_FILESYSTEM
#define NGX_HAVE_CASELESS_FILESYSTEM  1
#endif

#define ngx_open_file(name, mode, create, access)                            \
    open((const char *) name, mode|create|O_BINARY, access)

#else

/* 打开文件的宏定义 */
#define ngx_open_file(name, mode, create, access)                            \
    open((const char *) name, mode|create, access)

#endif

/* 打开文件的函数名字符串 */
#define ngx_open_file_n          "open()"

/* 文件打开模式的宏定义 */
#define NGX_FILE_RDONLY          O_RDONLY    /* 只读模式 */
#define NGX_FILE_WRONLY          O_WRONLY    /* 只写模式 */
#define NGX_FILE_RDWR            O_RDWR      /* 读写模式 */
#define NGX_FILE_CREATE_OR_OPEN  O_CREAT     /* 创建或打开 */
#define NGX_FILE_OPEN            0           /* 仅打开 */
#define NGX_FILE_TRUNCATE        (O_CREAT|O_TRUNC)  /* 创建并截断 */
#define NGX_FILE_APPEND          (O_WRONLY|O_APPEND)  /* 追加模式 */
#define NGX_FILE_NONBLOCK        O_NONBLOCK  /* 非阻塞模式 */

/* 如果支持openat系统调用 */
#if (NGX_HAVE_OPENAT)
#define NGX_FILE_NOFOLLOW        O_NOFOLLOW  /* 不跟随符号链接 */

/* 如果定义了O_DIRECTORY */
#if defined(O_DIRECTORY)
#define NGX_FILE_DIRECTORY       O_DIRECTORY  /* 目录模式 */
#else
#define NGX_FILE_DIRECTORY       0
#endif

/* 搜索模式的定义 */
#if defined(O_SEARCH)
#define NGX_FILE_SEARCH          (O_SEARCH|NGX_FILE_DIRECTORY)

#elif defined(O_EXEC)
#define NGX_FILE_SEARCH          (O_EXEC|NGX_FILE_DIRECTORY)

#elif (NGX_HAVE_O_PATH)
#define NGX_FILE_SEARCH          (O_PATH|O_RDONLY|NGX_FILE_DIRECTORY)

#else
#define NGX_FILE_SEARCH          (O_RDONLY|NGX_FILE_DIRECTORY)
#endif

#endif /* NGX_HAVE_OPENAT */

/* 默认文件访问权限 */
#define NGX_FILE_DEFAULT_ACCESS  0644
/* 文件所有者访问权限 */
#define NGX_FILE_OWNER_ACCESS    0600

/* 关闭文件的宏定义 */
#define ngx_close_file           close
#define ngx_close_file_n         "close()"

/* 删除文件的宏定义 */
#define ngx_delete_file(name)    unlink((const char *) name)
#define ngx_delete_file_n        "unlink()"

/* 打开临时文件的函数声明 */
ngx_fd_t ngx_open_tempfile(u_char *name, ngx_uint_t persistent,
    ngx_uint_t access);
#define ngx_open_tempfile_n      "open()"

/* 读取文件的函数声明 */
ssize_t ngx_read_file(ngx_file_t *file, u_char *buf, size_t size, off_t offset);
/* 如果支持pread系统调用 */
#if (NGX_HAVE_PREAD)
#define ngx_read_file_n          "pread()"
#else
#define ngx_read_file_n          "read()"
#endif

/* 
 * 将数据写入文件
 * @param file 文件对象
 * @param buf 要写入的数据缓冲区
 * @param size 要写入的数据大小
 * @param offset 写入的起始偏移量
 * @return 实际写入的字节数
 */
ssize_t ngx_write_file(ngx_file_t *file, u_char *buf, size_t size,
    off_t offset);

/*
 * 将链表中的数据写入文件
 * @param file 文件对象
 * @param ce 数据链表
 * @param offset 写入的起始偏移量
 * @param pool 内存池
 * @return 实际写入的字节数
 */
ssize_t ngx_write_chain_to_file(ngx_file_t *file, ngx_chain_t *ce,
    off_t offset, ngx_pool_t *pool);


#define ngx_read_fd              read
#define ngx_read_fd_n            "read()"

/*
 * we use inlined function instead of simple #define
 * because glibc 2.3 sets warn_unused_result attribute for write()
 * and in this case gcc 4.3 ignores (void) cast
 */
static ngx_inline ssize_t
ngx_write_fd(ngx_fd_t fd, void *buf, size_t n)
{
    return write(fd, buf, n);
}

#define ngx_write_fd_n           "write()"


#define ngx_write_console        ngx_write_fd


#define ngx_linefeed(p)          *p++ = LF;
#define NGX_LINEFEED_SIZE        1
#define NGX_LINEFEED             "\x0a"


#define ngx_rename_file(o, n)    rename((const char *) o, (const char *) n)
#define ngx_rename_file_n        "rename()"


#define ngx_change_file_access(n, a) chmod((const char *) n, a)
#define ngx_change_file_access_n "chmod()"


ngx_int_t ngx_set_file_time(u_char *name, ngx_fd_t fd, time_t s);
#define ngx_set_file_time_n      "utimes()"


#define ngx_file_info(file, sb)  stat((const char *) file, sb)
#define ngx_file_info_n          "stat()"

#define ngx_fd_info(fd, sb)      fstat(fd, sb)
#define ngx_fd_info_n            "fstat()"

#define ngx_link_info(file, sb)  lstat((const char *) file, sb)
#define ngx_link_info_n          "lstat()"

#define ngx_is_dir(sb)           (S_ISDIR((sb)->st_mode))
#define ngx_is_file(sb)          (S_ISREG((sb)->st_mode))
#define ngx_is_link(sb)          (S_ISLNK((sb)->st_mode))
#define ngx_is_exec(sb)          (((sb)->st_mode & S_IXUSR) == S_IXUSR)
#define ngx_file_access(sb)      ((sb)->st_mode & 0777)
#define ngx_file_size(sb)        (sb)->st_size
#define ngx_file_fs_size(sb)                                                 \
    (((sb)->st_blocks * 512 > (sb)->st_size                                  \
     && (sb)->st_blocks * 512 < (sb)->st_size + 8 * (sb)->st_blksize)        \
     ? (sb)->st_blocks * 512 : (sb)->st_size)
#define ngx_file_mtime(sb)       (sb)->st_mtime
#define ngx_file_uniq(sb)        (sb)->st_ino


/**
 * @brief 创建文件映射
 * @param fm 文件映射结构体指针
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_create_file_mapping(ngx_file_mapping_t *fm);

/**
 * @brief 关闭文件映射
 * @param fm 文件映射结构体指针
 */
void ngx_close_file_mapping(ngx_file_mapping_t *fm);


/**
 * @brief 获取文件或目录的绝对路径
 * @param p 原始路径
 * @param r 存储结果的缓冲区
 * @return 成功时返回绝对路径，失败时返回NULL
 */
#define ngx_realpath(p, r)       (u_char *) realpath((char *) p, (char *) r)

/**
 * @brief realpath()函数的名称字符串，用于日志记录
 */
#define ngx_realpath_n           "realpath()"

/**
 * @brief 获取当前工作目录
 * @param buf 存储结果的缓冲区
 * @param size 缓冲区大小
 * @return 成功时返回非零值，失败时返回0
 */
#define ngx_getcwd(buf, size)    (getcwd((char *) buf, size) != NULL)

/**
 * @brief getcwd()函数的名称字符串，用于日志记录
 */
#define ngx_getcwd_n             "getcwd()"

/**
 * @brief 检查字符是否为路径分隔符
 * @param c 要检查的字符
 * @return 如果是路径分隔符('/')则返回非零值，否则返回0
 */
#define ngx_path_separator(c)    ((c) == '/')


#if defined(PATH_MAX)

#define NGX_HAVE_MAX_PATH        1
#define NGX_MAX_PATH             PATH_MAX

#else

#define NGX_MAX_PATH             4096

#endif


/**
 * @brief 打开一个目录
 * @param name 目录名称
 * @param dir 用于存储目录信息的结构体指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_open_dir(ngx_str_t *name, ngx_dir_t *dir);

/**
 * @brief opendir()函数的名称字符串，用于日志记录
 */
#define ngx_open_dir_n           "opendir()"


/**
 * @brief 关闭一个目录
 * @param d 目录结构体指针
 */
#define ngx_close_dir(d)         closedir((d)->dir)

/**
 * @brief closedir()函数的名称字符串，用于日志记录
 */
#define ngx_close_dir_n          "closedir()"


/**
 * @brief 读取目录中的下一个条目
 * @param dir 目录结构体指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR，到达目录末尾返回NGX_DONE
 */
ngx_int_t ngx_read_dir(ngx_dir_t *dir);

/**
 * @brief readdir()函数的名称字符串，用于日志记录
 */
#define ngx_read_dir_n           "readdir()"


/**
 * @brief 创建一个新目录
 * @param name 目录名称
 * @param access 目录访问权限
 * @return 成功返回0，失败返回-1
 */
#define ngx_create_dir(name, access) mkdir((const char *) name, access)

/**
 * @brief mkdir()函数的名称字符串，用于日志记录
 */
#define ngx_create_dir_n         "mkdir()"


/**
 * @brief 删除一个空目录
 * @param name 目录名称
 * @return 成功返回0，失败返回-1
 */
#define ngx_delete_dir(name)     rmdir((const char *) name)

/**
 * @brief rmdir()函数的名称字符串，用于日志记录
 */
#define ngx_delete_dir_n         "rmdir()"


/**
 * @brief 计算目录的访问权限
 * @param a 原始访问权限
 * @return 计算后的访问权限
 * @note 这个宏将原始权限中的读权限复制到组和其他用户，确保至少有读取权限
 */
#define ngx_dir_access(a)        (a | (a & 0444) >> 2)


/**
 * @brief 获取目录条目的名称
 * @param dir 目录结构体指针
 * @return 目录条目的名称（无符号字符指针）
 */
#define ngx_de_name(dir)         ((u_char *) (dir)->de->d_name)

#if (NGX_HAVE_D_NAMLEN)
/**
 * @brief 获取目录条目名称的长度（当系统支持d_namlen时使用）
 * @param dir 目录结构体指针
 * @return 目录条目名称的长度
 */
#define ngx_de_namelen(dir)      (dir)->de->d_namlen
#else
/**
 * @brief 获取目录条目名称的长度（当系统不支持d_namlen时使用）
 * @param dir 目录结构体指针
 * @return 目录条目名称的长度
 */
#define ngx_de_namelen(dir)      ngx_strlen((dir)->de->d_name)
#endif

/**
 * @brief 获取目录条目的详细信息
 * @param name 目录条目的名称
 * @param dir 目录结构体指针
 * @return 成功返回0，失败返回-1
 */
static ngx_inline ngx_int_t
ngx_de_info(u_char *name, ngx_dir_t *dir)
{
    dir->type = 0;
    return stat((const char *) name, &dir->info);
}

/**
 * @brief 获取目录条目详细信息的函数名称，用于日志记录
 */
#define ngx_de_info_n            "stat()"

/**
 * @brief 获取符号链接的详细信息
 * @param name 符号链接的名称
 * @param dir 目录结构体指针
 * @return 成功返回0，失败返回-1
 */
#define ngx_de_link_info(name, dir)  lstat((const char *) name, &(dir)->info)

/**
 * @brief 获取符号链接详细信息的函数名称，用于日志记录
 */
#define ngx_de_link_info_n       "lstat()"

#if (NGX_HAVE_D_TYPE)

/*
 * some file systems (e.g. XFS on Linux and CD9660 on FreeBSD)
 * do not set dirent.d_type
 */

/**
 * @brief 判断目录条目是否为目录
 * @param dir 目录结构体指针
 * @return 如果是目录返回非零值，否则返回0
 */
#define ngx_de_is_dir(dir)                                                   \
    (((dir)->type) ? ((dir)->type == DT_DIR) : (S_ISDIR((dir)->info.st_mode)))

/**
 * @brief 判断目录条目是否为普通文件
 * @param dir 目录结构体指针
 * @return 如果是普通文件返回非零值，否则返回0
 */
#define ngx_de_is_file(dir)                                                  \
    (((dir)->type) ? ((dir)->type == DT_REG) : (S_ISREG((dir)->info.st_mode)))

/**
 * @brief 判断目录条目是否为符号链接
 * @param dir 目录结构体指针
 * @return 如果是符号链接返回非零值，否则返回0
 */
#define ngx_de_is_link(dir)                                                  \
    (((dir)->type) ? ((dir)->type == DT_LNK) : (S_ISLNK((dir)->info.st_mode)))

#else

/**
 * @brief 判断目录条目是否为目录（不使用d_type）
 * @param dir 目录结构体指针
 * @return 如果是目录返回非零值，否则返回0
 */
#define ngx_de_is_dir(dir)       (S_ISDIR((dir)->info.st_mode))

/**
 * @brief 判断目录条目是否为普通文件（不使用d_type）
 * @param dir 目录结构体指针
 * @return 如果是普通文件返回非零值，否则返回0
 */
#define ngx_de_is_file(dir)      (S_ISREG((dir)->info.st_mode))

/**
 * @brief 判断目录条目是否为符号链接（不使用d_type）
 * @param dir 目录结构体指针
 * @return 如果是符号链接返回非零值，否则返回0
 */
#define ngx_de_is_link(dir)      (S_ISLNK((dir)->info.st_mode))

#endif

/**
 * @brief 获取目录条目的访问权限
 * @param dir 目录结构体指针
 * @return 返回目录条目的访问权限（八进制表示）
 */
#define ngx_de_access(dir)       (((dir)->info.st_mode) & 0777)

/**
 * @brief 获取目录条目的文件大小
 * @param dir 目录结构体指针
 * @return 返回目录条目的文件大小（字节数）
 */
#define ngx_de_size(dir)         (dir)->info.st_size

/**
 * @brief 获取目录条目的文件系统大小
 * @param dir 目录结构体指针
 * @return 返回目录条目的文件系统大小（字节数），取文件大小和块大小*块数中的较大值
 */
#define ngx_de_fs_size(dir)                                                  \
    ngx_max((dir)->info.st_size, (dir)->info.st_blocks * 512)

/**
 * @brief 获取目录条目的最后修改时间
 * @param dir 目录结构体指针
 * @return 返回目录条目的最后修改时间（时间戳）
 */
#define ngx_de_mtime(dir)        (dir)->info.st_mtime

/**
 * @brief 打开全局匹配
 * @param gl 全局匹配结构体指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_open_glob(ngx_glob_t *gl);

/**
 * @brief 全局匹配函数名称
 */
#define ngx_open_glob_n          "glob()"

/**
 * @brief 读取全局匹配结果
 * @param gl 全局匹配结构体指针
 * @param name 用于存储匹配结果的字符串指针
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_read_glob(ngx_glob_t *gl, ngx_str_t *name);

/**
 * @brief 关闭全局匹配
 * @param gl 全局匹配结构体指针
 */
void ngx_close_glob(ngx_glob_t *gl);

/**
 * @brief 尝试对文件描述符加锁（非阻塞）
 * @param fd 文件描述符
 * @return 成功返回0，失败返回错误码
 */
ngx_err_t ngx_trylock_fd(ngx_fd_t fd);

/**
 * @brief 对文件描述符加锁（阻塞）
 * @param fd 文件描述符
 * @return 成功返回0，失败返回错误码
 */
ngx_err_t ngx_lock_fd(ngx_fd_t fd);

/**
 * @brief 解锁文件描述符
 * @param fd 文件描述符
 * @return 成功返回0，失败返回错误码
 */
ngx_err_t ngx_unlock_fd(ngx_fd_t fd);

/**
 * @brief 尝试加锁的系统调用名称
 */
#define ngx_trylock_fd_n         "fcntl(F_SETLK, F_WRLCK)"

/**
 * @brief 加锁的系统调用名称
 */
#define ngx_lock_fd_n            "fcntl(F_SETLKW, F_WRLCK)"

/**
 * @brief 解锁的系统调用名称
 */
#define ngx_unlock_fd_n          "fcntl(F_SETLK, F_UNLCK)"


/**
 * @brief 预读取相关定义和函数
 */
#if (NGX_HAVE_F_READAHEAD)

#define NGX_HAVE_READ_AHEAD      1

/**
 * @brief 使用fcntl的F_READAHEAD命令进行预读取
 * @param fd 文件描述符
 * @param n 预读取的字节数
 */
#define ngx_read_ahead(fd, n)    fcntl(fd, F_READAHEAD, (int) n)
#define ngx_read_ahead_n         "fcntl(fd, F_READAHEAD)"

#elif (NGX_HAVE_POSIX_FADVISE)

#define NGX_HAVE_READ_AHEAD      1

/**
 * @brief 使用posix_fadvise进行预读取
 * @param fd 文件描述符
 * @param n 预读取的字节数
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_read_ahead(ngx_fd_t fd, size_t n);
#define ngx_read_ahead_n         "posix_fadvise(POSIX_FADV_SEQUENTIAL)"

#else

/**
 * @brief 不支持预读取时的空操作
 */
#define ngx_read_ahead(fd, n)    0
#define ngx_read_ahead_n         "ngx_read_ahead_n"

#endif


/**
 * @brief 直接I/O相关定义和函数
 */
#if (NGX_HAVE_O_DIRECT)

/**
 * @brief 启用直接I/O
 * @param fd 文件描述符
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_directio_on(ngx_fd_t fd);
#define ngx_directio_on_n        "fcntl(O_DIRECT)"

/**
 * @brief 关闭直接I/O
 * @param fd 文件描述符
 * @return 成功返回NGX_OK，失败返回NGX_ERROR
 */
ngx_int_t ngx_directio_off(ngx_fd_t fd);
#define ngx_directio_off_n       "fcntl(!O_DIRECT)"

#elif (NGX_HAVE_F_NOCACHE)

/**
 * @brief 使用F_NOCACHE标志启用直接I/O
 */
#define ngx_directio_on(fd)      fcntl(fd, F_NOCACHE, 1)
#define ngx_directio_on_n        "fcntl(F_NOCACHE, 1)"

#elif (NGX_HAVE_DIRECTIO)

/**
 * @brief 使用directio函数启用直接I/O
 */
#define ngx_directio_on(fd)      directio(fd, DIRECTIO_ON)
#define ngx_directio_on_n        "directio(DIRECTIO_ON)"

#else

/**
 * @brief 不支持直接I/O时的空操作
 */
#define ngx_directio_on(fd)      0
#define ngx_directio_on_n        "ngx_directio_on_n"

#endif

/**
 * @brief 获取文件系统的块大小
 * @param name 文件系统路径
 * @return 文件系统的块大小
 */
size_t ngx_fs_bsize(u_char *name);

/**
 * @brief 获取文件系统的可用空间
 * @param name 文件系统路径
 * @return 文件系统的可用空间大小
 */
off_t ngx_fs_available(u_char *name);


#if (NGX_HAVE_OPENAT)

/**
 * @brief 在指定目录描述符上打开文件
 * @param fd 目录描述符
 * @param name 文件名
 * @param mode 打开模式
 * @param create 创建标志
 * @param access 访问权限
 * @return 打开的文件描述符
 */
#define ngx_openat_file(fd, name, mode, create, access)                      \
    openat(fd, (const char *) name, mode|create, access)

#define ngx_openat_file_n        "openat()"

/**
 * @brief 获取指定目录描述符上文件的信息
 * @param fd 目录描述符
 * @param name 文件名
 * @param sb 存储文件信息的结构体
 * @param flag 标志位
 * @return 操作结果
 */
#define ngx_file_at_info(fd, name, sb, flag)                                 \
    fstatat(fd, (const char *) name, sb, flag)

#define ngx_file_at_info_n       "fstatat()"

/**
 * @brief 当前工作目录的文件描述符
 */
#define NGX_AT_FDCWD             (ngx_fd_t) AT_FDCWD

#endif


/**
 * @brief 标准输出文件描述符
 */
#define ngx_stdout               STDOUT_FILENO

/**
 * @brief 标准错误文件描述符
 */
#define ngx_stderr               STDERR_FILENO

/**
 * @brief 重定向标准错误输出
 * @param fd 新的文件描述符
 */
#define ngx_set_stderr(fd)       dup2(fd, STDERR_FILENO)

/**
 * @brief 重定向标准错误输出的函数名
 */
#define ngx_set_stderr_n         "dup2(STDERR_FILENO)"


#if (NGX_HAVE_FILE_AIO)

/**
 * @brief 初始化异步I/O文件
 * @param file 文件对象
 * @param pool 内存池
 * @return NGX_OK 成功，NGX_ERROR 失败
 */
ngx_int_t ngx_file_aio_init(ngx_file_t *file, ngx_pool_t *pool);

/**
 * @brief 异步读取文件
 * @param file 文件对象
 * @param buf 读取缓冲区
 * @param size 读取大小
 * @param offset 文件偏移量
 * @param pool 内存池
 * @return 读取的字节数，错误时返回负值
 */
ssize_t ngx_file_aio_read(ngx_file_t *file, u_char *buf, size_t size,
    off_t offset, ngx_pool_t *pool);

/**
 * @brief 异步I/O标志
 */
extern ngx_uint_t  ngx_file_aio;

#endif

#if (NGX_THREADS)
/**
 * @brief 多线程读取文件
 * @param file 文件对象
 * @param buf 读取缓冲区
 * @param size 读取大小
 * @param offset 文件偏移量
 * @param pool 内存池
 * @return 读取的字节数，错误时返回负值
 */
ssize_t ngx_thread_read(ngx_file_t *file, u_char *buf, size_t size,
    off_t offset, ngx_pool_t *pool);

/**
 * @brief 多线程将链式缓冲区写入文件
 * @param file 文件对象
 * @param cl 链式缓冲区
 * @param offset 文件偏移量
 * @param pool 内存池
 * @return 写入的字节数，错误时返回负值
 */
ssize_t ngx_thread_write_chain_to_file(ngx_file_t *file, ngx_chain_t *cl,
    off_t offset, ngx_pool_t *pool);
#endif


#endif /* _NGX_FILES_H_INCLUDED_ */
