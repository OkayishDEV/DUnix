#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

static int make_parents(const char *path) {
    char tmp[256];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: mkdir [-p] <directory...>\n");
        return 1;
    }

    bool parents = false;
    int idx = 1;
    while (idx < argc && strcmp(argv[idx], "-p") == 0) {
        parents = true;
        idx++;
    }

    if (idx >= argc) {
        fprintf(stderr, "usage: mkdir [-p] <directory...>\n");
        return 1;
    }

    int ret = 0;
    for (int i = idx; i < argc; i++) {
        int r = parents ? make_parents(argv[i]) : mkdir(argv[i], 0755);
        if (r != 0) {
            struct stat st;
            if (parents && stat(argv[i], &st) == 0 && S_ISDIR(st.st_mode)) {
                continue;
            }
            fprintf(stderr, "mkdir: cannot create directory '%s'\n", argv[i]);
            ret = 1;
        }
    }

    return ret;
}
