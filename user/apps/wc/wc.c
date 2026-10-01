#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

struct wc_count {
    unsigned long lines;
    unsigned long words;
    unsigned long bytes;
};

static void count_stream(FILE *fp, struct wc_count *cnt) {
    cnt->lines = 0;
    cnt->words = 0;
    cnt->bytes = 0;

    int c;
    bool in_word = false;
    while ((c = fgetc(fp)) != EOF) {
        cnt->bytes++;
        if (c == '\n') {
            cnt->lines++;
        }
        if (isspace((unsigned char)c)) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            cnt->words++;
        }
    }
}

int main(int argc, char **argv) {
    bool count_lines = false;
    bool count_words = false;
    bool count_bytes = false;
    int file_start = 1;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (char *p = argv[i] + 1; *p; p++) {
                if (*p == 'l') count_lines = true;
                else if (*p == 'w') count_words = true;
                else if (*p == 'c') count_bytes = true;
                else {
                    fprintf(stderr, "wc: invalid option -- '%c'\n", *p);
                    return 1;
                }
            }
            file_start = i + 1;
        } else {
            break;
        }
    }

    if (!count_lines && !count_words && !count_bytes) {
        count_lines = true;
        count_words = true;
        count_bytes = true;
    }

    if (file_start >= argc) {
        struct wc_count cnt;
        count_stream(stdin, &cnt);
        if (count_lines) printf("%7lu ", cnt.lines);
        if (count_words) printf("%7lu ", cnt.words);
        if (count_bytes) printf("%7lu ", cnt.bytes);
        printf("\n");
        return 0;
    }

    struct wc_count total = {0, 0, 0};
    int file_count = 0;

    for (int i = file_start; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");
        if (!fp) {
            fprintf(stderr, "wc: %s: No such file or directory\n", argv[i]);
            continue;
        }

        struct wc_count cnt;
        count_stream(fp, &cnt);
        fclose(fp);

        if (count_lines) printf("%7lu ", cnt.lines);
        if (count_words) printf("%7lu ", cnt.words);
        if (count_bytes) printf("%7lu ", cnt.bytes);
        printf("%s\n", argv[i]);

        total.lines += cnt.lines;
        total.words += cnt.words;
        total.bytes += cnt.bytes;
        file_count++;
    }

    if (file_count > 1) {
        if (count_lines) printf("%7lu ", total.lines);
        if (count_words) printf("%7lu ", total.words);
        if (count_bytes) printf("%7lu ", total.bytes);
        printf("total\n");
    }

    return 0;
}
