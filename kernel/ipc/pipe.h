#ifndef _IPC_PIPE_H
#define _IPC_PIPE_H

#include <fs/vfs.h>

#define PIPE_BUFFER_SIZE 4096

struct pipe_buffer {
    uint8_t  buffer[PIPE_BUFFER_SIZE];
    uint32_t read_idx;
    uint32_t write_idx;
    uint32_t count;
    uint32_t readers;
    uint32_t writers;
};

int pipe_create(struct vfs_node **read_node_out, struct vfs_node **write_node_out);
int64_t sys_pipe(int pipefd[2]);

#endif /* _IPC_PIPE_H */
