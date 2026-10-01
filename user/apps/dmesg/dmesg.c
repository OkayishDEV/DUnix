#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int fd = open("/proc/version", O_RDONLY);
    if (fd >= 0) {
        char buf[512];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[    0.000000] %s", buf);
        }
        close(fd);
    }

    int fd_cpu = open("/proc/cpuinfo", O_RDONLY);
    if (fd_cpu >= 0) {
        char buf[1024];
        ssize_t n = read(fd_cpu, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[    0.001000] Initialized CPU topology:\n%s", buf);
        }
        close(fd_cpu);
    }

    int fd_mem = open("/proc/meminfo", O_RDONLY);
    if (fd_mem >= 0) {
        char buf[512];
        ssize_t n = read(fd_mem, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[    0.002000] Memory layout:\n%s", buf);
        }
        close(fd_mem);
    }

    return 0;
}
