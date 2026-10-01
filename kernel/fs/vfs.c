#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <fs/block.h>
#include <fs/ext2.h>
#include <fs/ramdisk.h>
#include <fs/initrd_bins.h>
#include <boot/boot_info.h>
#include <process/process.h>
#include <sched/sched.h>
#include <mm/heap.h>
#include <arch/x86_64/drivers/ata.h>
#include <arch/x86_64/drivers/ahci.h>
#include <arch/x86_64/drivers/serial.h>
#include <arch/x86_64/drivers/pit.h>
#include <arch/x86_64/drivers/console.h>
#include <arch/x86_64/drivers/dmi.h>
#include <net/socket.h>
#include <net/e1000.h>
#include <ipc/pipe.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct vfs_node *vfs_root = NULL;

#define MAX_MOUNTS 16

struct vfs_mount_entry {
    char path[128];
    struct vfs_node *fs_root;
    bool active;
};

static struct vfs_mount_entry mount_table[MAX_MOUNTS];

static struct vfs_node *follow_mount(struct vfs_node *node) {
    int max_depth = 16;
    while (node && (node->flags & VFS_MOUNTPOINT) && node->ptr && node->ptr != node && max_depth-- > 0) {
        node = node->ptr;
    }
    return node;
}

static struct vfs_node *vfs_lookup_internal(const char *path, bool follow_final) {
    if (!path || !vfs_root) return NULL;

    if (strcmp(path, "/") == 0) {
        return follow_final ? follow_mount(vfs_root) : vfs_root;
    }

    struct vfs_node *curr = NULL;
    const char *start_path = path;

    if (path[0] == '/') {
        /* Check mount table for longest matching prefix */
        int best_match = -1;
        size_t best_len = 0;

        for (int i = 0; i < MAX_MOUNTS; i++) {
            if (!mount_table[i].active) continue;
            size_t mlen = strlen(mount_table[i].path);
            if (strncmp(path, mount_table[i].path, mlen) == 0) {
                if (path[mlen] == '\0' || path[mlen] == '/') {
                    if (mlen > best_len) {
                        best_len = mlen;
                        best_match = i;
                    }
                }
            }
        }

        if (best_match >= 0) {
            const char *rem = path + best_len;
            while (*rem == '/') rem++;

            if (*rem == '\0') {
                /* Exact match on mount point */
                if (!follow_final) {
                    curr = follow_mount(vfs_root);
                    start_path = path;
                } else {
                    return mount_table[best_match].fs_root;
                }
            } else {
                /* Subpath within the mounted filesystem */
                curr = mount_table[best_match].fs_root;
                start_path = rem;
            }
        } else {
            curr = follow_mount(vfs_root);
            start_path = path;
        }
    } else {
        struct process *proc = process_get_current();
        curr = vfs_lookup(proc ? proc->cwd : "/");
        start_path = path;
    }

    if (!curr) return NULL;

    char path_copy[256];
    strncpy(path_copy, start_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *token = path_copy;
    if (*token == '/') token++;

    char *next_slash = NULL;
    while (*token) {
        next_slash = strchr(token, '/');
        if (next_slash) {
            *next_slash = '\0';
        }

        if (strcmp(token, ".") == 0 || strlen(token) == 0) {
            /* Current directory */
        } else if (strcmp(token, "..") == 0) {
            /* Parent directory */
        } else {
            curr = follow_mount(curr);
            if (!curr->ops || !curr->ops->finddir) {
                return NULL;
            }
            curr = curr->ops->finddir(curr, token);
            if (!curr) {
                return NULL;
            }
        }

        if (!next_slash) {
            break;
        }
        token = next_slash + 1;
    }

    return follow_final ? follow_mount(curr) : curr;
}

struct vfs_node *vfs_lookup(const char *path) {
    return vfs_lookup_internal(path, true);
}

struct vfs_node *vfs_lookup_mountpoint(const char *path) {
    return vfs_lookup_internal(path, false);
}

void vfs_mount(const char *path, struct vfs_node *fs_root) {
    if (!path || !fs_root) return;

    if (strcmp(path, "/") == 0) {
        vfs_root = fs_root;
        return;
    }

    char norm_path[128];
    strncpy(norm_path, path, sizeof(norm_path) - 1);
    norm_path[sizeof(norm_path) - 1] = '\0';
    size_t len = strlen(norm_path);
    while (len > 1 && norm_path[len - 1] == '/') {
        norm_path[--len] = '\0';
    }

    /* Check if already in table, update if so */
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].active && strcmp(mount_table[i].path, norm_path) == 0) {
            mount_table[i].fs_root = fs_root;
            return;
        }
    }

    /* Add new entry */
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mount_table[i].active) {
            strncpy(mount_table[i].path, norm_path, sizeof(mount_table[i].path) - 1);
            mount_table[i].fs_root = fs_root;
            mount_table[i].active = true;
            break;
        }
    }

    /* Also set on in-memory node if present (for RamFS compatibility) */
    struct vfs_node *mount_point = vfs_lookup_mountpoint(norm_path);
    if (mount_point && mount_point != fs_root) {
        mount_point->flags |= VFS_MOUNTPOINT;
        mount_point->ptr = fs_root;
    }
}

struct vfs_node *vfs_lookup_parent(const char *path, char *out_name) {
    if (!path || !out_name) return NULL;

    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash) {
        /* In current directory */
        strcpy(out_name, path);
        struct process *proc = process_get_current();
        return vfs_lookup(proc ? proc->cwd : "/");
    }

    if (last_slash == path_copy) {
        /* In root directory */
        strcpy(out_name, last_slash + 1);
        return vfs_root;
    }

    *last_slash = '\0';
    strcpy(out_name, last_slash + 1);
    return vfs_lookup(path_copy);
}

ssize_t vfs_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    node = follow_mount(node);
    if (!node || !node->ops || !node->ops->read) return -1;
    return node->ops->read(node, offset, size, buffer);
}

ssize_t vfs_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    node = follow_mount(node);
    if (!node || !node->ops || !node->ops->write) return -1;
    return node->ops->write(node, offset, size, buffer);
}

struct dirent *vfs_readdir(struct vfs_node *node, uint32_t index) {
    node = follow_mount(node);
    if (!node || !node->ops || !node->ops->readdir) return NULL;
    return node->ops->readdir(node, index);
}

struct vfs_node *vfs_finddir(struct vfs_node *node, const char *name) {
    node = follow_mount(node);
    if (!node || !node->ops || !node->ops->finddir) return NULL;
    return follow_mount(node->ops->finddir(node, name));
}

int vfs_mkdir(const char *path, mode_t mode) {
    char name[128];
    struct vfs_node *parent = vfs_lookup_parent(path, name);
    if (!parent) return -2; /* -ENOENT */

    parent = follow_mount(parent);
    if (!parent->ops || !parent->ops->mkdir) return -1;
    return parent->ops->mkdir(parent, name, mode);
}

int vfs_create(const char *path, mode_t mode) {
    char name[128];
    struct vfs_node *parent = vfs_lookup_parent(path, name);
    if (!parent) return -2; /* -ENOENT */

    parent = follow_mount(parent);
    if (!parent->ops || !parent->ops->create) return -1;
    return parent->ops->create(parent, name, mode);
}

int vfs_unlink(const char *path) {
    char name[128];
    struct vfs_node *parent = vfs_lookup_parent(path, name);
    if (!parent) return -2; /* -ENOENT */

    parent = follow_mount(parent);
    if (!parent->ops || !parent->ops->unlink) return -1;
    return parent->ops->unlink(parent, name);
}

int vfs_rename(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath) return -14;

    char old_name[128];
    struct vfs_node *old_parent = vfs_lookup_parent(oldpath, old_name);
    if (!old_parent) return -2;

    char new_name[128];
    struct vfs_node *new_parent = vfs_lookup_parent(newpath, new_name);
    if (!new_parent) return -2;

    old_parent = follow_mount(old_parent);
    new_parent = follow_mount(new_parent);

    return ramfs_rename(old_parent, old_name, new_parent, new_name);
}

int vfs_ioctl(struct vfs_node *node, unsigned long request, void *arg) {
    node = follow_mount(node);
    if (node && node->ops && node->ops->ioctl) {
        return node->ops->ioctl(node, request, arg);
    }

    if (request == TIOCGWINSZ) {
        struct winsize *ws = (struct winsize *)arg;
        if (ws) {
            ws->ws_row = 25;
            ws->ws_col = 80;
            ws->ws_xpixel = 640;
            ws->ws_ypixel = 400;
            return 0;
        }
    } else if (request == TIOCSWINSZ || request == TCGETS ||
               request == TCSETS || request == TCSETSW || request == TCSETSF) {
        return 0;
    }

    return -25; /* -ENOTTY */
}

struct file *vfs_file_open(struct vfs_node *node, uint32_t flags) {
    if (!node) return NULL;

    struct file *f = (struct file *)kzalloc(sizeof(struct file));
    if (!f) return NULL;

    f->node = node;
    f->flags = flags;
    if ((flags & O_TRUNC) && (flags & (O_WRONLY | O_RDWR))) {
        node->length = 0;
        f->offset = 0;
    } else if (flags & O_APPEND) {
        f->offset = node->length;
    } else {
        f->offset = 0;
    }
    f->ref_count = 1;

    if (node->ops && node->ops->open) {
        node->ops->open(node, flags);
    }

    return f;
}

void vfs_close_fd(struct file *f) {
    if (!f) return;
    if (--f->ref_count == 0) {
        if (f->node && f->node->ops && f->node->ops->close) {
            f->node->ops->close(f->node);
        }
        kfree(f);
    }
}

/* POSIX File Syscalls */
int64_t sys_open(const char *pathname, int flags, mode_t mode) {
    if (!pathname) return -14; /* -EFAULT */

    struct process *proc = process_get_current();
    if (!proc) return -1;

    int fd = -1;
    for (int i = 0; i < MAX_FD; i++) {
        if (!proc->files[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -24; /* -EMFILE */

    struct vfs_node *node = vfs_lookup(pathname);
    if (!node) {
        if (flags & O_CREAT) {
            if (vfs_create(pathname, mode) != 0) {
                return -2; /* -ENOENT */
            }
            node = vfs_lookup(pathname);
            if (!node) return -2;
        } else {
            return -2; /* -ENOENT */
        }
    }

    if (!node) return -2;

    struct file *f = vfs_file_open(node, (uint32_t)flags);
    if (!f) return -12; /* -ENOMEM */

    proc->files[fd] = f;
    return fd;
}

int64_t sys_close(int fd) {
    struct process *proc = process_get_current();
    if (!proc || fd < 0 || fd >= MAX_FD || !proc->files[fd]) {
        return -9; /* -EBADF */
    }

    vfs_close_fd(proc->files[fd]);
    proc->files[fd] = NULL;
    return 0;
}

int64_t sys_read(int fd, void *buf, size_t count) {
    if (!buf) return -14; /* -EFAULT */
    struct process *proc = process_get_current();
    if (!proc || fd < 0 || fd >= MAX_FD || !proc->files[fd]) {
        return -9; /* -EBADF */
    }

    struct file *f = proc->files[fd];
    if (f->flags & O_NONBLOCK) {
        if (f->node && (strcmp(f->node->name, "console") == 0 || strcmp(f->node->name, "tty") == 0)) {
            if (!console_has_input()) {
                return -11; /* -EAGAIN */
            }
        }
        if (f->node && (f->node->flags & VFS_SOCKET) == VFS_SOCKET) {
            struct socket_handle *sock = (struct socket_handle *)f->node->device;
            if (sock && sock->type == SOCK_STREAM && sock->tcp) {
                if (sock->tcp->rx_head == sock->tcp->rx_tail) {
                    if (sock->tcp->state == TCP_STATE_CLOSED || sock->tcp->state == TCP_STATE_CLOSE_WAIT) {
                        return 0; /* EOF */
                    }
                    return -11; /* -EAGAIN */
                }
            } else if (sock && sock->type == SOCK_DGRAM && sock->udp) {
                if (sock->udp->queue_head == sock->udp->queue_tail) {
                    return -11; /* -EAGAIN */
                }
            }
        }
        if (f->node && f->node->flags == VFS_PIPE) {
            struct pipe_buffer *pipe = (struct pipe_buffer *)f->node->device;
            if (pipe && pipe->count == 0) {
                if (pipe->writers == 0) {
                    return 0; /* EOF */
                }
                return -11; /* -EAGAIN */
            }
        }
    }

    ssize_t bytes = vfs_read(f->node, f->offset, count, buf);
    if (bytes > 0 && f->node->flags != VFS_PIPE) {
        f->offset += (uint64_t)bytes;
    }
    return bytes;
}

int64_t sys_write(int fd, const void *buf, size_t count) {
    if (!buf) return -14; /* -EFAULT */
    struct process *proc = process_get_current();
    if (!proc || fd < 0 || fd >= MAX_FD || !proc->files[fd]) {
        return -9; /* -EBADF */
    }

    struct file *f = proc->files[fd];
    if (f->flags & O_NONBLOCK) {
        if (f->node && f->node->flags == VFS_PIPE) {
            struct pipe_buffer *pipe = (struct pipe_buffer *)f->node->device;
            if (pipe && pipe->count >= PIPE_BUFFER_SIZE) {
                if (pipe->readers == 0) {
                    return -32; /* -EPIPE */
                }
                return -11; /* -EAGAIN */
            }
        }
    }
    if (f->flags & O_APPEND) {
        f->offset = f->node->length;
    }
    ssize_t bytes = vfs_write(f->node, f->offset, count, buf);
    if (bytes > 0 && f->node->flags != VFS_PIPE) {
        f->offset += (uint64_t)bytes;
    }
    return bytes;
}

int64_t sys_dup(int oldfd) {
    struct process *proc = process_get_current();
    if (!proc || oldfd < 0 || oldfd >= MAX_FD || !proc->files[oldfd]) {
        return -9; /* -EBADF */
    }

    int newfd = -1;
    for (int i = 0; i < MAX_FD; i++) {
        if (!proc->files[i]) {
            newfd = i;
            break;
        }
    }
    if (newfd == -1) return -24; /* -EMFILE */

    struct file *f = proc->files[oldfd];
    f->ref_count++;
    proc->files[newfd] = f;
    return newfd;
}

int64_t sys_dup2(int oldfd, int newfd) {
    struct process *proc = process_get_current();
    if (!proc || oldfd < 0 || oldfd >= MAX_FD || !proc->files[oldfd] ||
        newfd < 0 || newfd >= MAX_FD) {
        return -9; /* -EBADF */
    }

    if (oldfd == newfd) {
        return newfd;
    }

    if (proc->files[newfd]) {
        sys_close(newfd);
    }

    struct file *f = proc->files[oldfd];
    f->ref_count++;
    proc->files[newfd] = f;
    return newfd;
}

int64_t sys_lseek(int fd, off_t offset, int whence) {
    struct process *proc = process_get_current();
    if (!proc || fd < 0 || fd >= MAX_FD || !proc->files[fd]) {
        return -9; /* -EBADF */
    }

    struct file *f = proc->files[fd];
    off_t new_offset = 0;

    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = (off_t)f->offset + offset;
            break;
        case SEEK_END:
            new_offset = (off_t)f->node->length + offset;
            break;
        default:
            return -22; /* -EINVAL */
    }

    if (new_offset < 0) {
        return -22; /* -EINVAL */
    }

    f->offset = (uint64_t)new_offset;
    return new_offset;
}

static void fill_stat_buf(struct vfs_node *node, struct stat *st) {
    memset(st, 0, sizeof(struct stat));
    st->st_ino = node->inode;
    st->st_size = (off_t)node->length;
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    st->st_nlink = 1;

    mode_t type_bits = 0100000; /* Regular file S_IFREG */
    if (node->flags == VFS_DIRECTORY)   type_bits = 0040000; /* S_IFDIR */
    else if (node->flags == VFS_CHARDEVICE)  type_bits = 0020000; /* S_IFCHR */
    else if (node->flags == VFS_BLOCKDEVICE) type_bits = 0060000; /* S_IFBLK */
    else if (node->flags == VFS_PIPE)        type_bits = 0010000; /* S_IFIFO */
    else if (node->flags == VFS_SYMLINK)     type_bits = 0120000; /* S_IFLNK */

    st->st_mode = type_bits | (node->mask & 07777);
}

int64_t sys_stat(const char *pathname, struct stat *statbuf) {
    if (!pathname || !statbuf) return -14; /* -EFAULT */

    struct vfs_node *node = vfs_lookup(pathname);
    if (!node) return -2; /* -ENOENT */

    fill_stat_buf(node, statbuf);
    return 0;
}

int64_t sys_fstat(int fd, struct stat *statbuf) {
    if (!statbuf) return -14; /* -EFAULT */
    struct process *proc = process_get_current();
    if (!proc || fd < 0 || fd >= MAX_FD || !proc->files[fd]) {
        return -9; /* -EBADF */
    }

    fill_stat_buf(proc->files[fd]->node, statbuf);
    return 0;
}

int64_t sys_mkdir(const char *pathname, mode_t mode) {
    if (!pathname) return -14;
    return vfs_mkdir(pathname, mode);
}

int64_t sys_unlink(const char *pathname) {
    if (!pathname) return -14;
    return vfs_unlink(pathname);
}

int64_t sys_rename(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath) return -14;
    return vfs_rename(oldpath, newpath);
}

int64_t sys_getdents(int fd, void *dirp, size_t count) {
    if (!dirp || count < sizeof(struct dirent)) return -14; /* -EFAULT */
    struct process *proc = process_get_current();
    if (!proc || fd < 0 || fd >= MAX_FD || !proc->files[fd]) {
        return -9; /* -EBADF */
    }

    struct file *f = proc->files[fd];
    if (!f->node || (f->node->flags & VFS_DIRECTORY) != VFS_DIRECTORY) {
        return -20; /* -ENOTDIR */
    }

    struct dirent *d = vfs_readdir(f->node, (uint32_t)f->offset);
    if (!d) {
        return 0; /* End of directory */
    }

    struct dirent *out = (struct dirent *)dirp;
    memcpy(out->d_name, d->d_name, sizeof(out->d_name));
    out->d_ino = d->d_ino;
    out->d_type = d->d_type;
    f->offset++;

    return sizeof(struct dirent);
}

int64_t sys_ioctl(int fd, unsigned long request, void *arg) {
    struct process *proc = process_get_current();
    if (!proc || fd < 0 || fd >= MAX_FD || !proc->files[fd]) {
        return -9; /* -EBADF */
    }

    struct file *f = proc->files[fd];
    return vfs_ioctl(f->node, request, arg);
}

int64_t sys_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    if (!fds && nfds > 0) return -14; /* -EFAULT */
    struct process *proc = process_get_current();
    if (!proc) return -1;

    uint64_t start_ms = pit_get_uptime_ms();

    for (;;) {
        /* Service network interface so packet arrivals wake up pollers */
        e1000_poll_rx();

        int ready_count = 0;
        for (nfds_t i = 0; i < nfds; i++) {
            fds[i].revents = 0;
            int fd = fds[i].fd;
            if (fd < 0) {
                continue;
            }
            if (fd >= MAX_FD || !proc->files[fd]) {
                fds[i].revents = POLLNVAL;
                ready_count++;
                continue;
            }

            struct file *f = proc->files[fd];
            short revents = 0;

            if (f->node && (f->node->flags & VFS_SOCKET) == VFS_SOCKET) {
                struct socket_handle *sock = (struct socket_handle *)f->node->device;
                if (sock) {
                    if (sock->type == SOCK_STREAM && sock->tcp) {
                        if (sock->tcp->state == TCP_STATE_ESTABLISHED) {
                            if (fds[i].events & POLLOUT) {
                                revents |= POLLOUT;
                            }
                            if (sock->tcp->rx_head != sock->tcp->rx_tail) {
                                if (fds[i].events & POLLIN) {
                                    revents |= POLLIN;
                                }
                            }
                        } else if (sock->tcp->state == TCP_STATE_CLOSE_WAIT ||
                                   sock->tcp->state == TCP_STATE_CLOSED ||
                                   sock->tcp->state == TCP_STATE_TIME_WAIT) {
                            if (sock->tcp->rx_head != sock->tcp->rx_tail) {
                                if (fds[i].events & POLLIN) {
                                    revents |= POLLIN;
                                }
                            }
                            revents |= POLLHUP;
                        } else if (sock->tcp->state == TCP_STATE_LISTEN) {
                            if (sock->tcp->pending_count > 0) {
                                if (fds[i].events & POLLIN) {
                                    revents |= POLLIN;
                                }
                            }
                        }
                    } else if (sock->type == SOCK_DGRAM && sock->udp) {
                        if (fds[i].events & POLLOUT) {
                            revents |= POLLOUT;
                        }
                        if (sock->udp->queue_head != sock->udp->queue_tail) {
                            if (fds[i].events & POLLIN) {
                                revents |= POLLIN;
                            }
                        }
                    }
                }
            } else if (f->node && f->node->flags == VFS_PIPE) {
                struct pipe_buffer *pipe = (struct pipe_buffer *)f->node->device;
                if (pipe) {
                    if (fds[i].events & POLLIN) {
                        if (pipe->count > 0 || pipe->writers == 0) {
                            revents |= POLLIN;
                        }
                    }
                    if (fds[i].events & POLLOUT) {
                        if (pipe->count < PIPE_BUFFER_SIZE) {
                            revents |= POLLOUT;
                        }
                    }
                    if (pipe->writers == 0) {
                        revents |= POLLHUP;
                    }
                }
            } else {
                /* Regular file or device */
                if (fds[i].events & POLLIN)  revents |= POLLIN;
                if (fds[i].events & POLLOUT) revents |= POLLOUT;
            }

            fds[i].revents = revents;
            if (revents) {
                ready_count++;
            }
        }

        if (ready_count > 0) {
            return ready_count;
        }

        if (timeout == 0) {
            return 0;
        }

        if (timeout > 0) {
            uint64_t elapsed = pit_get_uptime_ms() - start_ms;
            if (elapsed >= (uint64_t)timeout) {
                return 0;
            }
        }

        sched_sleep(5);
    }
}

int64_t sys_nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)rem;
    if (!req) return -14; /* -EFAULT */

    uint64_t ms = (uint64_t)req->tv_sec * 1000 + (uint64_t)req->tv_nsec / 1000000;
    if (ms > 0) {
        sched_sleep(ms);
    }
    return 0;
}

int64_t sys_mount(const char *source, const char *target, const char *fstype, unsigned long flags, const void *data) {
    (void)flags; (void)data;
    if (!target || !fstype) return -14;

    struct vfs_node *fs_root = NULL;

    if (strcmp(fstype, "ext2") == 0) {
        if (!source) return -14;
        const char *dev_name = source;
        if (strncmp(dev_name, "/dev/", 5) == 0) dev_name += 5;

        struct block_device *bdev = block_device_get_by_name(dev_name);
        if (!bdev) return -19; /* -ENODEV */

        fs_root = ext2_mount(bdev);
        if (!fs_root) return -22; /* -EINVAL */
    } else if (strcmp(fstype, "procfs") == 0 || strcmp(fstype, "proc") == 0) {
        fs_root = procfs_mount();
    } else if (strcmp(fstype, "ramfs") == 0) {
        fs_root = ramfs_create_root();
    } else {
        return -19; /* -ENODEV */
    }

    vfs_mount(target, fs_root);
    return 0;
}

int64_t sys_umount(const char *target) {
    if (!target) return -14;

    char norm_path[128];
    strncpy(norm_path, target, sizeof(norm_path) - 1);
    norm_path[sizeof(norm_path) - 1] = '\0';
    size_t len = strlen(norm_path);
    while (len > 1 && norm_path[len - 1] == '/') {
        norm_path[--len] = '\0';
    }

    struct vfs_node *fs_root = NULL;
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].active && strcmp(mount_table[i].path, norm_path) == 0) {
            fs_root = mount_table[i].fs_root;
            mount_table[i].active = false;
            break;
        }
    }

    struct vfs_node *mp = vfs_lookup_mountpoint(norm_path);
    if (mp) {
        if (!fs_root && (mp->flags & VFS_MOUNTPOINT)) {
            fs_root = mp->ptr;
        }
        mp->flags &= ~VFS_MOUNTPOINT;
        mp->ptr = NULL;
    }

    if (!fs_root && !mp) return -2; /* -ENOENT */

    if (fs_root && fs_root->device) {
        struct ext2_fs *fs = (struct ext2_fs *)fs_root->device;
        if (fs->sb.s_magic == EXT2_SUPER_MAGIC) {
            ext2_sync(fs);
        }
    }

    return 0;
}

int64_t sys_chmod(const char *pathname, mode_t mode) {
    if (!pathname) return -14;
    struct vfs_node *node = vfs_lookup(pathname);
    if (!node) return -2; /* -ENOENT */

    node->mask = mode & 07777;
    return 0;
}

int64_t sys_chown(const char *pathname, uid_t owner, gid_t group) {
    if (!pathname) return -14;
    struct vfs_node *node = vfs_lookup(pathname);
    if (!node) return -2; /* -ENOENT */

    if ((int)owner != -1) node->uid = owner;
    if ((int)group != -1) node->gid = group;
    return 0;
}

int64_t sys_fcntl(int fd, int cmd, uint64_t arg) {
    struct process *proc = process_get_current();
    if (!proc || fd < 0 || fd >= MAX_FD || !proc->files[fd]) {
        return -9; /* -EBADF */
    }

    struct file *f = proc->files[fd];
    switch (cmd) {
        case F_DUPFD:
            return sys_dup(fd);
        case F_GETFD:
            return 0;
        case F_SETFD:
            return 0;
        case F_GETFL:
            return (int64_t)f->flags;
        case F_SETFL:
            f->flags = (f->flags & ~O_NONBLOCK) | ((uint32_t)arg & O_NONBLOCK);
            return 0;
        default:
            return 0;
    }
}

#define BLKGETSIZE    0x1260
#define BLKFORMAT     0x1261
#define BLKGETSIZE64  0x1268

static ssize_t bdev_vfs_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    struct block_device *bdev = (struct block_device *)node->device;
    if (!bdev || !bdev->read || size == 0) return 0;

    uint32_t bsz = bdev->block_size ? bdev->block_size : 512;
    uint64_t start_lba = offset / bsz;
    uint64_t end_lba = (offset + size + bsz - 1) / bsz;
    if (end_lba > bdev->total_blocks) end_lba = bdev->total_blocks;
    if (start_lba >= end_lba) return 0;

    size_t lba_count = (size_t)(end_lba - start_lba);
    uint8_t *tmp = (uint8_t *)kmalloc(lba_count * bsz);
    if (!tmp) return -12;

    if (bdev->read(bdev, start_lba, lba_count, tmp) != 0) {
        kfree(tmp);
        return -5;
    }

    size_t off_in_first = (size_t)(offset % bsz);
    size_t copy_bytes = size;
    if (copy_bytes > (lba_count * bsz - off_in_first)) {
        copy_bytes = lba_count * bsz - off_in_first;
    }

    memcpy(buffer, tmp + off_in_first, copy_bytes);
    kfree(tmp);
    return (ssize_t)copy_bytes;
}

static ssize_t bdev_vfs_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    struct block_device *bdev = (struct block_device *)node->device;
    if (!bdev || !bdev->write || size == 0) return 0;

    uint32_t bsz = bdev->block_size ? bdev->block_size : 512;
    uint64_t start_lba = offset / bsz;
    uint64_t end_lba = (offset + size + bsz - 1) / bsz;
    if (end_lba > bdev->total_blocks) {
        klog(KLOG_ERROR, "bdev_vfs_write: ENOSPC on %s (end_lba %lu > total %lu)\n", bdev->name, end_lba, bdev->total_blocks);
        return -28; /* -ENOSPC */
    }

    size_t lba_count = (size_t)(end_lba - start_lba);
    uint8_t *tmp = (uint8_t *)kmalloc(lba_count * bsz);
    if (!tmp) {
        klog(KLOG_ERROR, "bdev_vfs_write: kmalloc failed for %zu bytes on %s\n", lba_count * bsz, bdev->name);
        return -12;
    }

    size_t off_in_first = (size_t)(offset % bsz);
    if (off_in_first != 0 || (size % bsz) != 0) {
        bdev->read(bdev, start_lba, lba_count, tmp);
    }

    memcpy(tmp + off_in_first, buffer, size);
    if (bdev->write(bdev, start_lba, lba_count, tmp) != 0) {
        klog(KLOG_ERROR, "bdev_vfs_write: bdev->write failed on %s (lba %lu count %zu)\n", bdev->name, start_lba, lba_count);
        kfree(tmp);
        return -5;
    }

    if (bdev->flush) bdev->flush(bdev);
    kfree(tmp);
    return (ssize_t)size;
}

static int bdev_vfs_ioctl(struct vfs_node *node, unsigned long request, void *arg) {
    struct block_device *bdev = (struct block_device *)node->device;
    if (!bdev) return -19;

    if (request == BLKGETSIZE) {
        if (!arg) return -14;
        *(uint64_t *)arg = bdev->total_blocks;
        return 0;
    } else if (request == BLKGETSIZE64) {
        if (!arg) return -14;
        *(uint64_t *)arg = (uint64_t)bdev->total_blocks * (bdev->block_size ? bdev->block_size : 512);
        return 0;
    } else if (request == BLKFORMAT) {
        return ext2_format(bdev);
    } else if (request == BLKRRPART) {
        return block_device_rescan_partitions(bdev);
    }

    return -22;
}

static struct vfs_ops bdev_vfs_ops = {
    .read = bdev_vfs_read,
    .write = bdev_vfs_write,
    .ioctl = bdev_vfs_ioctl,
    .finddir = NULL,
    .readdir = NULL,
    .create = NULL,
    .mkdir = NULL,
    .unlink = NULL
};

static struct vfs_node *g_dev_dir = NULL;

void vfs_register_block_device(struct block_device *bdev) {
    if (!g_dev_dir || !bdev) return;
    if (vfs_finddir(g_dev_dir, bdev->name)) return;

    struct vfs_node *bnode = ramfs_create_file(g_dev_dir, bdev->name, NULL, 0, 0660);
    if (bnode) {
        bnode->flags = VFS_BLOCKDEVICE;
        bnode->device = bdev;
        bnode->ops = &bdev_vfs_ops;
    }
}

static void vfs_populate_sysfs(struct vfs_node *sys_root) {
    if (!sys_root) return;
    struct vfs_node *class_dir = ramfs_create_dir(sys_root, "class", 0755);
    if (!class_dir) return;
    struct vfs_node *dmi_dir = ramfs_create_dir(class_dir, "dmi", 0755);
    if (!dmi_dir) return;
    struct vfs_node *id_dir = ramfs_create_dir(dmi_dir, "id", 0755);
    if (!id_dir) return;

    const struct dmi_system_info *dmi = dmi_get_info();
    char val[128];

    ksnprintf(val, sizeof(val), "%s\n", dmi->product_name[0] ? dmi->product_name : "x86_64 Machine");
    ramfs_create_file(id_dir, "product_name", val, strlen(val), 0444);

    ksnprintf(val, sizeof(val), "%s\n", dmi->sys_vendor[0] ? dmi->sys_vendor : "PC Compatible");
    ramfs_create_file(id_dir, "sys_vendor", val, strlen(val), 0444);

    ksnprintf(val, sizeof(val), "%s\n", dmi->product_version[0] ? dmi->product_version : "1.0");
    ramfs_create_file(id_dir, "product_version", val, strlen(val), 0444);

    ksnprintf(val, sizeof(val), "%s\n", dmi->bios_vendor[0] ? dmi->bios_vendor : "Unknown");
    ramfs_create_file(id_dir, "bios_vendor", val, strlen(val), 0444);

    ksnprintf(val, sizeof(val), "%s\n", dmi->bios_version[0] ? dmi->bios_version : "Unknown");
    ramfs_create_file(id_dir, "bios_version", val, strlen(val), 0444);

    ksnprintf(val, sizeof(val), "%s\n", dmi->bios_date[0] ? dmi->bios_date : "Unknown");
    ramfs_create_file(id_dir, "bios_date", val, strlen(val), 0444);

    ksnprintf(val, sizeof(val), "%s\n", dmi->board_name[0] ? dmi->board_name : "Unknown");
    ramfs_create_file(id_dir, "board_name", val, strlen(val), 0444);

    ksnprintf(val, sizeof(val), "%s\n", dmi->chassis_type[0] ? dmi->chassis_type : "Desktop");
    ramfs_create_file(id_dir, "chassis_type", val, strlen(val), 0444);
}

void vfs_init(void) {
    /* Step 1: Create RamFS Root Filesystem */
    vfs_root = ramfs_create_root();

    /* Step 2: Populate Standard Unix Hierarchy */
    struct vfs_node *bin_dir  = ramfs_create_dir(vfs_root, "bin", 0755);
    struct vfs_node *sbin_dir = ramfs_create_dir(vfs_root, "sbin", 0755);
    struct vfs_node *dev_dir  = ramfs_create_dir(vfs_root, "dev", 0755);
    struct vfs_node *etc_dir  = ramfs_create_dir(vfs_root, "etc", 0755);
    struct vfs_node *home_dir = ramfs_create_dir(vfs_root, "home", 0755);
    struct vfs_node *proc_dir = ramfs_create_dir(vfs_root, "proc", 0555);
    struct vfs_node *sys_dir  = ramfs_create_dir(vfs_root, "sys", 0755);
    struct vfs_node *tmp_dir  = ramfs_create_dir(vfs_root, "tmp", 0777);
    struct vfs_node *usr_dir  = ramfs_create_dir(vfs_root, "usr", 0755);
    struct vfs_node *var_dir  = ramfs_create_dir(vfs_root, "var", 0755);
    struct vfs_node *mnt_dir  = ramfs_create_dir(vfs_root, "mnt", 0755);
    struct vfs_node *root_dir = ramfs_create_dir(vfs_root, "root", 0755);

    (void)bin_dir; (void)sbin_dir; (void)home_dir; (void)tmp_dir; (void)mnt_dir; (void)root_dir;

    vfs_populate_sysfs(sys_dir);

    ramfs_create_dir(usr_dir, "bin", 0755);
    ramfs_create_dir(usr_dir, "lib", 0755);
    ramfs_create_dir(usr_dir, "include", 0755);

    ramfs_create_dir(var_dir, "log", 0755);
    ramfs_create_dir(var_dir, "run", 0755);

    /* FreeBSD / NetBSD Style Linuxulator root (/compat/linux) */
    struct vfs_node *compat_dir = ramfs_create_dir(vfs_root, "compat", 0755);
    if (compat_dir) {
        struct vfs_node *compat_linux = ramfs_create_dir(compat_dir, "linux", 0755);
        if (compat_linux) {
            ramfs_create_dir(compat_linux, "bin", 0755);
            ramfs_create_dir(compat_linux, "lib", 0755);
            ramfs_create_dir(compat_linux, "lib64", 0755);
            struct vfs_node *clinux_etc = ramfs_create_dir(compat_linux, "etc", 0755);
            ramfs_create_dir(compat_linux, "usr", 0755);
            ramfs_create_dir(compat_linux, "proc", 0555);

            if (clinux_etc) {
                const char *linux_release =
                    "NAME=\"Ubuntu\"\n"
                    "VERSION=\"22.04.3 LTS (Jammy Jellyfish)\"\n"
                    "ID=ubuntu\n"
                    "ID_LIKE=debian\n"
                    "PRETTY_NAME=\"Ubuntu 22.04.3 LTS (DUnix Linuxulator)\"\n"
                    "VERSION_ID=\"22.04\"\n";
                ramfs_create_file(clinux_etc, "os-release", linux_release, strlen(linux_release), 0644);
            }
        }
    }

    /* Step 3: Populate Standard Configuration Files */
    const char *passwd_content =
        "root:x:0:0:root:/root:/bin/sh\n";
    ramfs_create_file(etc_dir, "passwd", passwd_content, strlen(passwd_content), 0644);

    const char *group_content =
        "root:x:0:root\n"
        "wheel:x:10:root\n"
        "sudo:x:27:root\n"
        "users:x:100:\n";
    ramfs_create_file(etc_dir, "group", group_content, strlen(group_content), 0644);

    const char *sudoers_content =
        "# /etc/sudoers - DUnix Privileged Access Configuration\n"
        "root ALL=(ALL) ALL\n"
        "%wheel ALL=(ALL) ALL\n"
        "%sudo ALL=(ALL) ALL\n";
    ramfs_create_file(etc_dir, "sudoers", sudoers_content, strlen(sudoers_content), 0440);

    const char *hostname_content = "dunix\n";
    ramfs_create_file(etc_dir, "hostname", hostname_content, strlen(hostname_content), 0644);

    const char *os_release = "NAME=\"DUnix\"\nVERSION=\"0.8.0-milestone15\"\nID=dunix\n";
    ramfs_create_file(etc_dir, "os-release", os_release, strlen(os_release), 0644);

    const char *hosts_content =
        "127.0.0.1\tlocalhost\n"
        "10.0.2.15\tdunix\n"
        "10.0.2.2\tgateway\n"
        "142.251.34.238\tgoogle.com\n"
        "142.251.34.238\twww.google.com\n";
    ramfs_create_file(etc_dir, "hosts", hosts_content, strlen(hosts_content), 0644);

    const char *resolv_content =
        "nameserver 10.0.2.3\n"
        "nameserver 8.8.8.8\n";
    ramfs_create_file(etc_dir, "resolv.conf", resolv_content, strlen(resolv_content), 0644);

    /* Step 4: Initialize DevFS on /dev */
    g_dev_dir = dev_dir;
    devfs_init(dev_dir);
    vfs_mount("/dev", dev_dir);

    /* Step 5: Initialize Block Subsystem, ATA & AHCI SATA Controller Drivers */
    block_init();
    ata_init();
    ahci_init();
    ramdisk_create("ram0", 8 * 1024 * 1024); /* 8 MB default installation/scratch RAM disk */

    /* Register any pending Block Devices in /dev */
    struct block_device *bdev = block_device_get_first();
    while (bdev) {
        vfs_register_block_device(bdev);
        bdev = bdev->next;
    }

    /* Step 6: Mount Dynamic /proc virtual filesystem */
    struct vfs_node *proc_root = procfs_mount();
    if (proc_root) {
        proc_dir->flags |= VFS_MOUNTPOINT;
        proc_dir->ptr = proc_root;
        vfs_mount("/proc", proc_root);
    }

    /* Step 7: Populate /bin and /sbin with native userspace binaries */
    vfs_populate_binaries();

    /* Step 8: Check for root= mount from boot command line */
    const char *cmd = g_boot_info.cmdline;
    char root_param[64] = {0};
    if (cmd && cmd[0]) {
        const char *p = cmd;
        while (*p) {
            while (*p == ' ') p++;
            if (strncmp(p, "root=", 5) == 0) {
                p += 5;
                size_t idx = 0;
                while (*p && *p != ' ' && idx < sizeof(root_param) - 1) {
                    root_param[idx++] = *p++;
                }
                root_param[idx] = '\0';
                break;
            }
            while (*p && *p != ' ') p++;
        }
    }

    if (root_param[0] && strcmp(root_param, "/dev/ram0") != 0 && strcmp(root_param, "ram0") != 0) {
        const char *dev_name = root_param;
        if (strncmp(dev_name, "/dev/", 5) == 0) {
            dev_name += 5;
        }

        struct block_device *root_bdev = block_device_get_by_name(dev_name);

        /* Cross-controller fallback: sda1 <-> hda1 */
        if (!root_bdev && (strncmp(dev_name, "sd", 2) == 0 || strncmp(dev_name, "hd", 2) == 0)) {
            char alt_name[32];
            ksnprintf(alt_name, sizeof(alt_name), "%s%s",
                      (dev_name[0] == 's') ? "hd" : "sd",
                      dev_name + 2);
            root_bdev = block_device_get_by_name(alt_name);
        }

        /* Whole-disk fallback: if root=sda, check sda1 */
        if (!root_bdev && strlen(dev_name) == 3) {
            char part_name[32];
            ksnprintf(part_name, sizeof(part_name), "%s1", dev_name);
            root_bdev = block_device_get_by_name(part_name);
            if (!root_bdev) {
                ksnprintf(part_name, sizeof(part_name), "%s1", (dev_name[0] == 's') ? "hda" : "sda");
                root_bdev = block_device_get_by_name(part_name);
            }
        }

        /* Generic fallback: look for first partitioned fixed drive */
        if (!root_bdev) {
            struct block_device *curr = block_device_get_first();
            while (curr) {
                size_t clen = strlen(curr->name);
                if ((strncmp(curr->name, "sd", 2) == 0 || strncmp(curr->name, "hd", 2) == 0) &&
                    clen >= 4 && curr->name[clen - 1] >= '1' && curr->name[clen - 1] <= '9') {
                    root_bdev = curr;
                    break;
                }
                curr = curr->next;
            }
        }

        if (root_bdev) {
            klog(KLOG_INFO, "VFS: Mounting persistent root filesystem from /dev/%s (Ext2)...\n", root_bdev->name);
            struct vfs_node *ext2_root = ext2_mount(root_bdev);
            if (ext2_root) {
                vfs_root = ext2_root;

                /* Ensure /dev and /proc mountpoints exist on disk root */
                struct vfs_node *new_dev = vfs_lookup_mountpoint("/dev");
                if (!new_dev) {
                    vfs_mkdir("/dev", 0755);
                    new_dev = vfs_lookup_mountpoint("/dev");
                }
                struct vfs_node *new_dev_root = ramfs_create_root();
                if (new_dev_root) {
                    if (new_dev) {
                        new_dev->flags |= VFS_MOUNTPOINT;
                        new_dev->ptr = new_dev_root;
                    }
                    g_dev_dir = new_dev_root;
                    devfs_init(new_dev_root);
                    vfs_mount("/dev", new_dev_root);
                }

                struct vfs_node *new_proc = vfs_lookup_mountpoint("/proc");
                if (!new_proc) {
                    vfs_mkdir("/proc", 0755);
                    new_proc = vfs_lookup_mountpoint("/proc");
                }
                struct vfs_node *new_proc_root = procfs_mount();
                if (new_proc_root) {
                    if (new_proc) {
                        new_proc->flags |= VFS_MOUNTPOINT;
                        new_proc->ptr = new_proc_root;
                    }
                    vfs_mount("/proc", new_proc_root);
                }

                struct vfs_node *new_sys = vfs_lookup_mountpoint("/sys");
                if (!new_sys) {
                    vfs_mkdir("/sys", 0755);
                    new_sys = vfs_lookup_mountpoint("/sys");
                }
                struct vfs_node *new_sys_root = ramfs_create_root();
                if (new_sys_root) {
                    if (new_sys) {
                        new_sys->flags |= VFS_MOUNTPOINT;
                        new_sys->ptr = new_sys_root;
                    }
                    vfs_populate_sysfs(new_sys_root);
                    vfs_mount("/sys", new_sys_root);
                }

                /* Re-register all block devices into the new /dev */
                struct block_device *b = block_device_get_first();
                while (b) {
                    vfs_register_block_device(b);
                    b = b->next;
                }

                klog(KLOG_INFO, "VFS: Successfully mounted root on /dev/%s (Ext2)\n", root_bdev->name);
            } else {
                klog(KLOG_WARN, "VFS: ext2_mount failed on %s; keeping RamFS root\n", root_bdev->name);
            }
        } else {
            klog(KLOG_WARN, "VFS: Target root block device '%s' not found; keeping RamFS root\n", root_param);
        }
    }

    klog(KLOG_INFO, "VFS initialized with /dev, /proc, and block storage devices online\n");
}
