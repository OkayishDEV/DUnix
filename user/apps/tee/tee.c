#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_FILES 64

int main(int argc, char **argv) {
    bool append = false;
    FILE *fps[MAX_FILES];
    int num_files = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--append") == 0) {
            append = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
            printf("Copy standard input to each FILE, and also to standard output.\n\n");
            printf("  -a, --append   append to the given FILEs, do not overwrite\n");
            printf("      --help     display this help and exit\n");
            return 0;
        } else if (argv[i][0] != '-') {
            if (num_files < MAX_FILES) {
                FILE *fp = fopen(argv[i], append ? "a" : "w");
                if (!fp) {
                    fprintf(stderr, "%s: %s: Cannot open file\n", argv[0], argv[i]);
                } else {
                    fps[num_files++] = fp;
                }
            }
        }
    }

    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        fwrite(buf, 1, n, stdout);
        fflush(stdout);

        for (int i = 0; i < num_files; i++) {
            fwrite(buf, 1, n, fps[i]);
            fflush(fps[i]);
        }
    }

    for (int i = 0; i < num_files; i++) {
        fclose(fps[i]);
    }

    return 0;
}
