#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>

static void list_directory(const char *path, bool show_all, bool show_long) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "ls: cannot open directory '%s'\n", path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') {
            continue;
        }

        if (show_long) {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s/%s", strcmp(path, "/") == 0 ? "" : path, entry->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0) {
                char type_ch = '-';
                if (S_ISDIR(st.st_mode))  type_ch = 'd';
                else if (S_ISCHR(st.st_mode)) type_ch = 'c';
                else if (S_ISBLK(st.st_mode)) type_ch = 'b';
                else if (S_ISFIFO(st.st_mode)) type_ch = 'p';

                printf("%c%c%c%c%c%c%c%c%c%c  %4u %4u  %8ld  %s\n",
                       type_ch,
                       (st.st_mode & 0400) ? 'r' : '-',
                       (st.st_mode & 0200) ? 'w' : '-',
                       (st.st_mode & 0100) ? 'x' : '-',
                       (st.st_mode & 0040) ? 'r' : '-',
                       (st.st_mode & 0020) ? 'w' : '-',
                       (st.st_mode & 0010) ? 'x' : '-',
                       (st.st_mode & 0004) ? 'r' : '-',
                       (st.st_mode & 0002) ? 'w' : '-',
                       (st.st_mode & 0001) ? 'x' : '-',
                       (unsigned int)st.st_uid,
                       (unsigned int)st.st_gid,
                       st.st_size,
                       entry->d_name);
            } else {
                printf("%s\n", entry->d_name);
            }
        } else {
            printf("%s  ", entry->d_name);
        }
    }

    if (!show_long) {
        printf("\n");
    }

    closedir(dir);
}

int main(int argc, char **argv) {
    bool show_all = false;
    bool show_long = false;
    const char *paths[16];
    int num_paths = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (size_t j = 1; j < strlen(argv[i]); j++) {
                if (argv[i][j] == 'a') show_all = true;
                if (argv[i][j] == 'l') show_long = true;
            }
        } else {
            if (num_paths < 16) {
                paths[num_paths++] = argv[i];
            }
        }
    }

    if (num_paths == 0) {
        paths[0] = ".";
        num_paths = 1;
    }

    for (int i = 0; i < num_paths; i++) {
        if (num_paths > 1) {
            printf("%s:\n", paths[i]);
        }
        list_directory(paths[i], show_all, show_long);
        if (num_paths > 1 && i < num_paths - 1) {
            printf("\n");
        }
    }

    return 0;
}
