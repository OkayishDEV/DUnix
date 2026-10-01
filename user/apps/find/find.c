#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static void find_recurse(const char *path, const char *name_pattern) {
    DIR *dir = opendir(path);
    if (!dir) {
        return;
    }

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];
        if (strcmp(path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", de->d_name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, de->d_name);
        }

        if (!name_pattern || strstr(de->d_name, name_pattern) != NULL) {
            printf("%s\n", full_path);
        }

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            find_recurse(full_path, name_pattern);
        }
    }

    closedir(dir);
}

int main(int argc, char **argv) {
    const char *start_path = ".";
    const char *name_pattern = NULL;

    int idx = 1;
    if (idx < argc && argv[idx][0] != '-') {
        start_path = argv[idx++];
    }

    while (idx < argc) {
        if (strcmp(argv[idx], "-name") == 0 && idx + 1 < argc) {
            name_pattern = argv[idx + 1];
            idx += 2;
        } else {
            idx++;
        }
    }

    printf("%s\n", start_path);
    find_recurse(start_path, name_pattern);
    return 0;
}
