#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void head_stream(FILE *fp, int lines) {
    char buf[1024];
    int count = 0;
    while (count < lines && fgets(buf, sizeof(buf), fp)) {
        fputs(buf, stdout);
        count++;
    }
}

int main(int argc, char **argv) {
    int lines = 10;
    int file_idx = 1;

    if (argc > 1 && strncmp(argv[1], "-n", 2) == 0) {
        if (strlen(argv[1]) > 2) {
            lines = atoi(argv[1] + 2);
            file_idx = 2;
        } else if (argc > 2) {
            lines = atoi(argv[2]);
            file_idx = 3;
        }
    }

    if (file_idx >= argc) {
        head_stream(stdin, lines);
        return 0;
    }

    for (int i = file_idx; i < argc; i++) {
        if (argc - file_idx > 1) {
            printf("==> %s <==\n", argv[i]);
        }
        FILE *fp = fopen(argv[i], "r");
        if (!fp) {
            fprintf(stderr, "head: cannot open '%s'\n", argv[i]);
            continue;
        }
        head_stream(fp, lines);
        fclose(fp);
    }

    return 0;
}
