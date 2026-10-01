#ifndef _FS_VFS_H
#define _FS_VFS_H

#include <dunix/types.h>
#include <dunix/stdbool.h>

#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEVICE  0x03
#define VFS_BLOCKDEVICE 0x04
#define VFS_PIPE        0x05
#define VFS_SYMLINK     0x06
#define VFS_SOCKET      0x07
#define VFS_MOUNTPOINT  0x08

/* POSIX open flags */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800

/* POSIX fcntl commands */
#define F_DUPFD     0
#define F_GETFD     1
#define F_SETFD     2
#define F_GETFL     3
#define F_SETFL     4

/* POSIX lseek whence */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* POSIX Terminal IOCTLs */
#define TIOCGWINSZ  0x5413
#define TIOCSWINSZ  0x5414
#define TCGETS      0x5401
#define TCSETS      0x5402
#define TCSETSW     0x5403
#define TCSETSF     0x5404

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/* POSIX Poll */
#define POLLIN      0x0001
#define POLLPRI     0x0002
#define POLLOUT     0x0004
#define POLLERR     0x0008
#define POLLHUP     0x0010
#define POLLNVAL    0x0020

typedef unsigned long nfds_t;

struct pollfd {
    int   fd;
    short events;
    short revents;
};

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct vfs_node;
struct dirent;

struct stat {
    dev_t     st_dev;
    ino_t     st_ino;
    mode_t    st_mode;
    nlink_t   st_nlink;
    uid_t     st_uid;
    gid_t     st_gid;
    dev_t     st_rdev;
    off_t     st_size;
    time_t    st_atime;
    time_t    st_mtime;
    time_t    st_ctime;
};

typedef ssize_t           (*vfs_read_t)(struct vfs_node *node, uint64_t offset, size_t size, void *buffer);
typedef ssize_t           (*vfs_write_t)(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer);
typedef int               (*vfs_open_t)(struct vfs_node *node, uint32_t flags);
typedef int               (*vfs_close_t)(struct vfs_node *node);
typedef struct dirent *   (*vfs_readdir_t)(struct vfs_node *node, uint32_t index);
typedef struct vfs_node * (*vfs_finddir_t)(struct vfs_node *node, const char *name);
typedef int               (*vfs_mkdir_t)(struct vfs_node *node, const char *name, mode_t mode);
typedef int               (*vfs_create_t)(struct vfs_node *node, const char *name, mode_t mode);
typedef int               (*vfs_unlink_t)(struct vfs_node *node, const char *name);
typedef int               (*vfs_ioctl_t)(struct vfs_node *node, unsigned long request, void *arg);

struct vfs_ops {
    vfs_read_t    read;
    vfs_write_t   write;
    vfs_open_t    open;
    vfs_close_t   close;
    vfs_readdir_t readdir;
    vfs_finddir_t finddir;
    vfs_mkdir_t   mkdir;
    vfs_create_t  create;
    vfs_unlink_t  unlink;
    vfs_ioctl_t   ioctl;
};

struct vfs_node {
    char             name[128];
    uint32_t         flags;
    uint32_t         inode;
    uint32_t         length;
    uint32_t         mask;       /* Standard Unix permissions (e.g. 0755, 0644) */
    uid_t            uid;
    gid_t            gid;
    struct vfs_ops  *ops;
    void            *device;     /* Private filesystem/driver data */
    struct vfs_node *ptr;        /* Mountpoint link or symlink */
};

struct dirent {
    char     d_name[128];
    uint32_t d_ino;
    uint32_t d_type;
};

struct file {
    struct vfs_node *node;
    uint64_t         offset;
    uint32_t         flags;
    uint32_t         ref_count;
};

void             vfs_init(void);
void             vfs_mount(const char *path, struct vfs_node *fs_root);
struct vfs_node *vfs_lookup(const char *path);
struct vfs_node *vfs_lookup_parent(const char *path, char *out_name);

ssize_t          vfs_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer);
ssize_t          vfs_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer);
int              vfs_open(struct vfs_node *node, uint32_t flags);
int              vfs_close(struct vfs_node *node);
struct dirent   *vfs_readdir(struct vfs_node *node, uint32_t index);
struct vfs_node *vfs_finddir(struct vfs_node *node, const char *name);
int              vfs_mkdir(const char *path, mode_t mode);
int              vfs_create(const char *path, mode_t mode);
int              vfs_unlink(const char *path);
int              vfs_rename(const char *oldpath, const char *newpath);
int              vfs_ioctl(struct vfs_node *node, unsigned long request, void *arg);

struct file     *vfs_file_open(struct vfs_node *node, uint32_t flags);
void             vfs_close_fd(struct file *f);
struct block_device;
void             vfs_register_block_device(struct block_device *bdev);

/* POSIX File & System Syscalls */
int64_t sys_open(const char *pathname, int flags, mode_t mode);
int64_t sys_close(int fd);
int64_t sys_read(int fd, void *buf, size_t count);
int64_t sys_write(int fd, const void *buf, size_t count);
int64_t sys_lseek(int fd, off_t offset, int whence);
int64_t sys_dup(int oldfd);
int64_t sys_dup2(int oldfd, int newfd);
int64_t sys_stat(const char *pathname, struct stat *statbuf);
int64_t sys_fstat(int fd, struct stat *statbuf);
int64_t sys_mkdir(const char *pathname, mode_t mode);
int64_t sys_unlink(const char *pathname);
int64_t sys_rename(const char *oldpath, const char *newpath);
int64_t sys_getdents(int fd, void *dirp, size_t count);
int64_t sys_ioctl(int fd, unsigned long request, void *arg);
int64_t sys_poll(struct pollfd *fds, nfds_t nfds, int timeout);
int64_t sys_nanosleep(const struct timespec *req, struct timespec *rem);
int64_t sys_mount(const char *source, const char *target, const char *fstype, unsigned long flags, const void *data);
int64_t sys_umount(const char *target);
int64_t sys_chmod(const char *pathname, mode_t mode);
int64_t sys_chown(const char *pathname, uid_t owner, gid_t group);
int64_t sys_fcntl(int fd, int cmd, uint64_t arg);

#endif /* _FS_VFS_H */
