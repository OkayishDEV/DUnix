#ifndef _PROCESS_PROCESS_H
#define _PROCESS_PROCESS_H

#include <dunix/types.h>
#include <sched/thread.h>
#include <fs/vfs.h>

typedef enum {
    PROC_EMBRYO,
    PROC_RUNNING,
    PROC_SLEEPING,
    PROC_ZOMBIE,
    PROC_DEAD
} process_state_t;

#define MAX_PROCESSES   256
#define MAX_FD          256

#define MMAP_BASE_ADDR  0x0000700000000000ULL

#define WNOHANG         1
#define WUNTRACED       2
#define WCONTINUED      8

/* Memory Protection & Mapping Flags */
#define PROT_NONE       0x0
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_FAILED      ((void *)-1)

struct file;

struct process {
    pid_t             pid;
    pid_t             ppid;
    char              name[64];
    process_state_t   state;
    int               exit_code;
    uid_t             uid;
    gid_t             gid;
    uid_t             euid;
    gid_t             egid;
    uint64_t         *pml4;
    uint64_t          brk_start;
    uint64_t          brk_current;
    uint64_t          mmap_current;
    char              cwd[256];
    struct thread    *main_thread;
    struct file      *files[MAX_FD];
    uint64_t          fs_base;
    uint64_t          gs_base;
    int              *clear_child_tid;
    bool              is_linux_compat;
    struct process   *parent;
    struct process   *children;
    struct process   *sibling;
    struct process   *next;
};

void            process_init(void);
struct process *process_get_current(void);
struct process *process_get_by_pid(pid_t pid);
struct process *process_create(const char *name, struct process *parent);

struct syscall_frame;

/* System Call Entry Handlers */
pid_t   sys_getpid(void);
pid_t   sys_getppid(void);
uid_t   sys_getuid(void);
gid_t   sys_getgid(void);
uid_t   sys_geteuid(void);
gid_t   sys_getegid(void);
int64_t sys_setuid(uid_t uid);
int64_t sys_setgid(gid_t gid);
int64_t sys_seteuid(uid_t euid);
int64_t sys_setegid(gid_t egid);

int64_t sys_fork(struct syscall_frame *frame);
int64_t sys_execve(const char *filename, char *const argv[], char *const envp[]);
int64_t sys_exit(int status);
int64_t sys_waitpid(pid_t pid, int *status, int options);
int64_t sys_brk(void *addr);
int64_t sys_getcwd(char *buf, size_t size);
int64_t sys_chdir(const char *path);
int64_t sys_uname(struct utsname *buf);

void   *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int64_t sys_munmap(void *addr, size_t length);

#endif /* _PROCESS_PROCESS_H */
