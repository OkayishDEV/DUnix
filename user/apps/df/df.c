#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Filesystem     Type    1K-blocks     Used Available Use%% Mounted on\n");

    int fd = open("/proc/meminfo", O_RDONLY);
    unsigned long total_kb = 131072;
    unsigned long free_kb = 128500;
    if (fd >= 0) {
        char buf[512];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            char *p = strstr(buf, "MemTotal:");
            if (p) total_kb = strtoul(p + 9, NULL, 10);
            p = strstr(buf, "MemFree:");
            if (p) free_kb = strtoul(p + 8, NULL, 10);
        }
        close(fd);
    }

    unsigned long used_kb = total_kb - free_kb;
    int use_pct = (int)((used_kb * 100) / (total_kb ? total_kb : 1));

    printf("%-14s %-7s %10lu %8lu %9lu %3d%% /\n", "rootfs", "ramfs", total_kb, used_kb, free_kb, use_pct);
    printf("%-14s %-7s %10lu %8lu %9lu %3d%% /dev\n", "devfs", "devfs", 1024UL, 64UL, 960UL, 6);
    printf("%-14s %-7s %10lu %8lu %9lu %3d%% /proc\n", "procfs", "procfs", 0UL, 0UL, 0UL, 0);

    return 0;
}
