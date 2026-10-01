#ifndef _COMPAT_LINUX_H
#define _COMPAT_LINUX_H

#include <dunix/types.h>
#include <dunix/stdbool.h>
#include <fs/vfs.h>

/* Linux System Call Numbers (x86_64 ABI) */
#define LINUX_SYS_read              0
#define LINUX_SYS_write             1
#define LINUX_SYS_open              2
#define LINUX_SYS_close             3
#define LINUX_SYS_stat              4
#define LINUX_SYS_fstat             5
#define LINUX_SYS_poll              7
#define LINUX_SYS_lseek             8
#define LINUX_SYS_mmap              9
#define LINUX_SYS_mprotect          10
#define LINUX_SYS_munmap            11
#define LINUX_SYS_brk               12
#define LINUX_SYS_rt_sigaction      13
#define LINUX_SYS_rt_sigprocmask    14
#define LINUX_SYS_ioctl             16
#define LINUX_SYS_readv             19
#define LINUX_SYS_writev            20
#define LINUX_SYS_access            21
#define LINUX_SYS_pipe              22
#define LINUX_SYS_dup               32
#define LINUX_SYS_dup2              33
#define LINUX_SYS_nanosleep         35
#define LINUX_SYS_getpid            39
#define LINUX_SYS_socket            41
#define LINUX_SYS_connect           42
#define LINUX_SYS_accept            43
#define LINUX_SYS_sendto            44
#define LINUX_SYS_recvfrom          45
#define LINUX_SYS_shutdown          48
#define LINUX_SYS_bind              49
#define LINUX_SYS_listen            50
#define LINUX_SYS_fork              57
#define LINUX_SYS_execve            59
#define LINUX_SYS_exit              60
#define LINUX_SYS_wait4             61
#define LINUX_SYS_kill              62
#define LINUX_SYS_uname             63
#define LINUX_SYS_fcntl             72
#define LINUX_SYS_getdents          78
#define LINUX_SYS_getcwd            79
#define LINUX_SYS_chdir             80
#define LINUX_SYS_mkdir             83
#define LINUX_SYS_unlink            87
#define LINUX_SYS_readlink          89
#define LINUX_SYS_chmod             90
#define LINUX_SYS_fchmod            91
#define LINUX_SYS_chown             92
#define LINUX_SYS_fchown            93
#define LINUX_SYS_gettimeofday      96
#define LINUX_SYS_sysinfo           99
#define LINUX_SYS_getuid            102
#define LINUX_SYS_getgid            104
#define LINUX_SYS_setuid            105
#define LINUX_SYS_setgid            106
#define LINUX_SYS_geteuid           107
#define LINUX_SYS_getegid           108
#define LINUX_SYS_seteuid           109
#define LINUX_SYS_setegid           110
#define LINUX_SYS_getppid           110
#define LINUX_SYS_arch_prctl        158
#define LINUX_SYS_mount             165
#define LINUX_SYS_umount            166
#define LINUX_SYS_gettid            186
#define LINUX_SYS_futex             202
#define LINUX_SYS_getdents64        217
#define LINUX_SYS_set_tid_address   218
#define LINUX_SYS_clock_gettime     228
#define LINUX_SYS_exit_group        231
#define LINUX_SYS_openat            257
#define LINUX_SYS_newfstatat        262
#define LINUX_SYS_readlinkat        267
#define LINUX_SYS_pipe2             293
#define LINUX_SYS_prlimit64         302
#define LINUX_SYS_getrandom         318
#define LINUX_SYS_statx             332
#define LINUX_SYS_rseq              334

/* arch_prctl operations */
#define ARCH_SET_GS                 0x1001
#define ARCH_SET_FS                 0x1002
#define ARCH_GET_FS                 0x1003
#define ARCH_GET_GS                 0x1004

/* Linux Auxiliary Vector Constants */
#define LINUX_AT_NULL               0
#define LINUX_AT_IGNORE             1
#define LINUX_AT_EXECFD             2
#define LINUX_AT_PHDR               3
#define LINUX_AT_PHENT              4
#define LINUX_AT_PHNUM              5
#define LINUX_AT_PAGESZ             6
#define LINUX_AT_BASE               7
#define LINUX_AT_FLAGS              8
#define LINUX_AT_ENTRY              9
#define LINUX_AT_NOTELF             10
#define LINUX_AT_UID                11
#define LINUX_AT_EUID               12
#define LINUX_AT_GID                13
#define LINUX_AT_EGID               14
#define LINUX_AT_PLATFORM           15
#define LINUX_AT_HWCAP              16
#define LINUX_AT_CLKTCK             17
#define LINUX_AT_SECURE             23
#define LINUX_AT_BASE_PLATFORM      24
#define LINUX_AT_RANDOM             25
#define LINUX_AT_HWCAP2             26
#define LINUX_AT_EXECFN             31
#define LINUX_AT_SYSINFO_EHDR       33

/* openat constants */
#define LINUX_AT_FDCWD              -100
#define LINUX_AT_SYMLINK_NOFOLLOW   0x100
#define LINUX_AT_REMOVEDIR          0x200
#define LINUX_AT_SYMLINK_FOLLOW     0x400
#define LINUX_AT_NO_AUTOMOUNT       0x800
#define LINUX_AT_EMPTY_PATH         0x1000

/* Linux struct stat (x86_64 layout) */
struct linux_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    int64_t  st_atime;
    uint64_t st_atime_nsec;
    int64_t  st_mtime;
    uint64_t st_mtime_nsec;
    int64_t  st_ctime;
    uint64_t st_ctime_nsec;
    int64_t  __unused[3];
};

/* Linux struct linux_dirent64 */
struct linux_dirent64 {
    uint64_t       d_ino;
    int64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

/* Linux struct iovec */
struct linux_iovec {
    void  *iov_base;
    size_t iov_len;
};

/* Linux struct timeval */
struct linux_timeval {
    time_t tv_sec;
    long   tv_usec;
};

/* Linux struct sysinfo */
struct linux_sysinfo {
    int64_t  uptime;
    uint64_t loads[3];
    uint64_t totalram;
    uint64_t freeram;
    uint64_t sharedram;
    uint64_t bufferram;
    uint64_t totalswap;
    uint64_t freeswap;
    uint16_t procs;
    uint16_t pad;
    uint64_t totalhigh;
    uint64_t freehigh;
    uint32_t mem_unit;
    char     _f[8];
};

/* Linux rlimit */
struct linux_rlimit64 {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

/* Function Prototypes */
void        linux_compat_init(void);
void        linux_fill_stat(struct vfs_node *node, struct linux_stat *lst);
const char *linux_resolve_path(const char *path, char *out_buf, size_t buf_sz);

int64_t linux_sys_arch_prctl(int code, unsigned long addr);
int64_t linux_sys_set_tid_address(int *tidptr);
int64_t linux_sys_openat(int dfd, const char *filename, int flags, mode_t mode);
int64_t linux_sys_mkdirat(int dfd, const char *pathname, mode_t mode);
int64_t linux_sys_unlinkat(int dfd, const char *pathname, int flags);
int64_t linux_sys_rename(const char *oldpath, const char *newpath);
int64_t linux_sys_renameat(int olddfd, const char *oldpath, int newdfd, const char *newpath);
int64_t linux_sys_faccessat(int dfd, const char *pathname, int mode, int flags);
int64_t linux_sys_newfstatat(int dfd, const char *filename, void *statbuf, int flag);
int64_t linux_sys_getdents64(unsigned int fd, void *dirp, unsigned int count);
int64_t linux_sys_writev(int fd, const struct linux_iovec *iov, int iovcnt);
int64_t linux_sys_readv(int fd, const struct linux_iovec *iov, int iovcnt);
int64_t linux_sys_clock_gettime(int which_clock, struct timespec *tp);
int64_t linux_sys_futex(uint32_t *uaddr, int op, uint32_t val, const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3);
int64_t linux_sys_prlimit64(pid_t pid, int resource, const void *new_limit, void *old_limit);
int64_t linux_sys_getrandom(void *buf, size_t buflen, unsigned int flags);
int64_t linux_sys_access(const char *pathname, int mode);
int64_t linux_sys_readlink(const char *path, char *buf, size_t bufsiz);
int64_t linux_sys_readlinkat(int dfd, const char *path, char *buf, size_t bufsiz);
int64_t linux_sys_gettimeofday(struct linux_timeval *tv, void *tz);
int64_t linux_sys_sysinfo(struct linux_sysinfo *info);
int64_t linux_sys_mprotect(void *addr, size_t len, int prot);
int64_t linux_sys_rt_sigaction(int signum, const void *act, void *oldact, size_t sigsetsize);
int64_t linux_sys_rt_sigprocmask(int how, const void *set, void *oldset, size_t sigsetsize);
int64_t linux_sys_gettid(void);
int64_t linux_sys_pipe2(int pipefd[2], int flags);
int64_t linux_sys_uname(struct utsname *buf);
int64_t linux_sys_select(int nfds, void *readfds, void *writefds, void *exceptfds, struct linux_timeval *timeout);
int64_t linux_sys_pselect6(int nfds, void *readfds, void *writefds, void *exceptfds, const struct timespec *timeout, const void *sigmask);

#endif /* _COMPAT_LINUX_H */
