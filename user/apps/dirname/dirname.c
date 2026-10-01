#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: dirname <path>\n");
        return 1;
    }

    const char *path = argv[1];
    const char *last_slash = strrchr(path, '/');

    if (!last_slash) {
        printf(".\n");
    } else if (last_slash == path) {
        printf("/\n");
    } else {
        char dir[256];
        size_t len = last_slash - path;
        strncpy(dir, path, len);
        dir[len] = '\0';
        printf("%s\n", dir);
    }

    return 0;
}
