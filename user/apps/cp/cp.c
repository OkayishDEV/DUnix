#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: cp <source> <destination>\n");
        return 1;
    }

    int src_fd = open(argv[1], O_RDONLY);
    if (src_fd < 0) {
        fprintf(stderr, "cp: cannot open source '%s'\n", argv[1]);
        return 1;
    }

    int dst_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        fprintf(stderr, "cp: cannot create destination '%s'\n", argv[2]);
        close(src_fd);
        return 1;
    }

    char buf[1024];
    ssize_t n;
    int ret = 0;

    while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
        if (write(dst_fd, buf, (size_t)n) != n) {
            fprintf(stderr, "cp: write error to '%s'\n", argv[2]);
            ret = 1;
            break;
        }
    }

    close(src_fd);
    close(dst_fd);
    return ret;
}
