#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TAIL_LINES 256
#define LINE_BUFFER_SIZE 1024

static void tail_stream(FILE *fp, int lines) {
    if (lines > MAX_TAIL_LINES) lines = MAX_TAIL_LINES;
    if (lines <= 0) return;

    char ring[MAX_TAIL_LINES][LINE_BUFFER_SIZE];
    int count = 0;
    int head = 0;

    char buf[LINE_BUFFER_SIZE];
    while (fgets(buf, sizeof(buf), fp)) {
        strncpy(ring[head], buf, LINE_BUFFER_SIZE - 1);
        ring[head][LINE_BUFFER_SIZE - 1] = '\0';
        head = (head + 1) % lines;
        if (count < lines) count++;
    }

    int start = (count < lines) ? 0 : head;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % lines;
        fputs(ring[idx], stdout);
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
        tail_stream(stdin, lines);
        return 0;
    }

    for (int i = file_idx; i < argc; i++) {
        if (argc - file_idx > 1) {
            printf("==> %s <==\n", argv[i]);
        }
        FILE *fp = fopen(argv[i], "r");
        if (!fp) {
            fprintf(stderr, "tail: cannot open '%s'\n", argv[i]);
            continue;
        }
        tail_stream(fp, lines);
        fclose(fp);
    }

    return 0;
}
