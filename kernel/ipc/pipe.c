#include <ipc/pipe.h>
#include <mm/heap.h>
#include <process/process.h>
#include <sched/sched.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct vfs_ops pipe_read_ops;
static struct vfs_ops pipe_write_ops;
static uint32_t pipe_inode_counter = 0x80000000;

static ssize_t pipe_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)offset;
    struct pipe_buffer *pipe = (struct pipe_buffer *)node->device;
    if (!pipe || !buffer || size == 0) return 0;

    uint8_t *buf = (uint8_t *)buffer;
    size_t bytes_read = 0;

    while (bytes_read < size) {
        if (pipe->count > 0) {
            buf[bytes_read++] = pipe->buffer[pipe->read_idx];
            pipe->read_idx = (pipe->read_idx + 1) % PIPE_BUFFER_SIZE;
            pipe->count--;
        } else {
            /* Pipe is empty */
            if (pipe->writers == 0) {
                /* No writers left -> EOF */
                break;
            }
            if (bytes_read > 0) {
                /* Return partial read */
                break;
            }
            /* Block/Sleep waiting for writers */
            sched_sleep(5);
        }
    }

    return (ssize_t)bytes_read;
}

static ssize_t pipe_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    (void)offset;
    struct pipe_buffer *pipe = (struct pipe_buffer *)node->device;
    if (!pipe || !buffer || size == 0) return 0;

    if (pipe->readers == 0) {
        /* Broken pipe: No readers */
        return -32; /* -EPIPE */
    }

    const uint8_t *buf = (const uint8_t *)buffer;
    size_t bytes_written = 0;

    while (bytes_written < size) {
        if (pipe->readers == 0) {
            return (bytes_written > 0) ? (ssize_t)bytes_written : -32;
        }

        if (pipe->count < PIPE_BUFFER_SIZE) {
            pipe->buffer[pipe->write_idx] = buf[bytes_written++];
            pipe->write_idx = (pipe->write_idx + 1) % PIPE_BUFFER_SIZE;
            pipe->count++;
        } else {
            /* Pipe full, wait for readers */
            sched_sleep(5);
        }
    }

    return (ssize_t)bytes_written;
}

static int pipe_read_close(struct vfs_node *node) {
    struct pipe_buffer *pipe = (struct pipe_buffer *)node->device;
    if (pipe) {
        if (pipe->readers > 0) pipe->readers--;
        if (pipe->readers == 0 && pipe->writers == 0) {
            kfree(pipe);
            node->device = NULL;
        }
    }
    return 0;
}

static int pipe_write_close(struct vfs_node *node) {
    struct pipe_buffer *pipe = (struct pipe_buffer *)node->device;
    if (pipe) {
        if (pipe->writers > 0) pipe->writers--;
        if (pipe->readers == 0 && pipe->writers == 0) {
            kfree(pipe);
            node->device = NULL;
        }
    }
    return 0;
}

int pipe_create(struct vfs_node **read_node_out, struct vfs_node **write_node_out) {
    pipe_read_ops.read = pipe_read;
    pipe_read_ops.close = pipe_read_close;

    pipe_write_ops.write = pipe_write;
    pipe_write_ops.close = pipe_write_close;

    struct pipe_buffer *pipe = (struct pipe_buffer *)kzalloc(sizeof(struct pipe_buffer));
    if (!pipe) return -12;

    pipe->readers = 1;
    pipe->writers = 1;

    struct vfs_node *rnode = (struct vfs_node *)kzalloc(sizeof(struct vfs_node));
    struct vfs_node *wnode = (struct vfs_node *)kzalloc(sizeof(struct vfs_node));

    if (!rnode || !wnode) {
        if (rnode) kfree(rnode);
        if (wnode) kfree(wnode);
        kfree(pipe);
        return -12;
    }

    strcpy(rnode->name, "pipe:[read]");
    rnode->flags = VFS_PIPE;
    rnode->inode = pipe_inode_counter++;
    rnode->mask = 0600;
    rnode->ops = &pipe_read_ops;
    rnode->device = pipe;

    strcpy(wnode->name, "pipe:[write]");
    wnode->flags = VFS_PIPE;
    wnode->inode = pipe_inode_counter++;
    wnode->mask = 0600;
    wnode->ops = &pipe_write_ops;
    wnode->device = pipe;

    *read_node_out = rnode;
    *write_node_out = wnode;
    return 0;
}

int64_t sys_pipe(int pipefd[2]) {
    if (!pipefd) return -14; /* -EFAULT */

    struct process *proc = process_get_current();
    if (!proc) return -1;

    /* Find two available file descriptors */
    int fd1 = -1, fd2 = -1;
    for (int i = 0; i < MAX_FD; i++) {
        if (!proc->files[i]) {
            if (fd1 == -1) {
                fd1 = i;
            } else if (fd2 == -1) {
                fd2 = i;
                break;
            }
        }
    }

    if (fd1 == -1 || fd2 == -1) return -24; /* -EMFILE */

    struct vfs_node *rnode = NULL;
    struct vfs_node *wnode = NULL;

    int ret = pipe_create(&rnode, &wnode);
    if (ret != 0) return ret;

    struct file *rf = vfs_file_open(rnode, O_RDONLY);
    struct file *wf = vfs_file_open(wnode, O_WRONLY);

    proc->files[fd1] = rf;
    proc->files[fd2] = wf;

    pipefd[0] = fd1;
    pipefd[1] = fd2;

    return 0;
}
