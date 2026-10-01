#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: which <command>\n");
        return 1;
    }

    const char *paths[] = { "/bin", "/sbin", "/usr/bin", NULL };
    int found = 0;

    for (int i = 1; i < argc; i++) {
        const char *cmd = argv[i];
        if (cmd[0] == '/' || cmd[0] == '.') {
            struct stat st;
            if (stat(cmd, &st) == 0) {
                printf("%s\n", cmd);
                found = 1;
                continue;
            }
        }

        int item_found = 0;
        for (int p = 0; paths[p]; p++) {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s/%s", paths[p], cmd);
            struct stat st;
            if (stat(full_path, &st) == 0) {
                printf("%s\n", full_path);
                item_found = 1;
                found = 1;
                break;
            }
        }

        if (!item_found) {
            fprintf(stderr, "%s: not found\n", cmd);
        }
    }

    return found ? 0 : 1;
}
