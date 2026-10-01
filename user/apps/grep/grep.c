#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

static bool match_pattern(const char *line, const char *pattern, bool ignore_case) {
    if (!line || !pattern) return false;
    if (!*pattern) return true;

    size_t pat_len = strlen(pattern);
    size_t line_len = strlen(line);
    if (line_len < pat_len) return false;

    for (size_t i = 0; i <= line_len - pat_len; i++) {
        bool match = true;
        for (size_t j = 0; j < pat_len; j++) {
            char c1 = line[i + j];
            char c2 = pattern[j];
            if (ignore_case) {
                c1 = (char)tolower((unsigned char)c1);
                c2 = (char)tolower((unsigned char)c2);
            }
            if (c1 != c2) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static int grep_stream(FILE *fp, const char *pattern, bool ignore_case, bool invert, bool line_num, bool count_only, const char *filename, bool print_filename) {
    char line[1024];
    int match_count = 0;
    int line_index = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_index++;
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        bool matched = match_pattern(line, pattern, ignore_case);
        if (invert) matched = !matched;

        if (matched) {
            match_count++;
            if (!count_only) {
                if (print_filename) {
                    printf("%s:", filename);
                }
                if (line_num) {
                    printf("%d:", line_index);
                }
                printf("%s\n", line);
            }
        }
    }

    if (count_only) {
        if (print_filename) {
            printf("%s:", filename);
        }
        printf("%d\n", match_count);
    }

    return (match_count > 0) ? 0 : 1;
}

int main(int argc, char **argv) {
    bool ignore_case = false;
    bool invert = false;
    bool line_num = false;
    bool count_only = false;
    const char *pattern = NULL;
    int file_start = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0' && !pattern) {
            for (char *p = argv[i] + 1; *p; p++) {
                if (*p == 'i') ignore_case = true;
                else if (*p == 'v') invert = true;
                else if (*p == 'n') line_num = true;
                else if (*p == 'c') count_only = true;
                else {
                    fprintf(stderr, "grep: invalid option -- '%c'\n", *p);
                    return 2;
                }
            }
        } else if (!pattern) {
            pattern = argv[i];
            file_start = i + 1;
        }
    }

    if (!pattern) {
        fprintf(stderr, "Usage: grep [-ivnc] <pattern> [file...]\n");
        return 2;
    }

    if (file_start >= argc) {
        return grep_stream(stdin, pattern, ignore_case, invert, line_num, count_only, "(standard input)", false);
    }

    bool multiple_files = (argc - file_start > 1);
    int status = 1;

    for (int i = file_start; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");
        if (!fp) {
            fprintf(stderr, "grep: %s: No such file or directory\n", argv[i]);
            continue;
        }

        int res = grep_stream(fp, pattern, ignore_case, invert, line_num, count_only, argv[i], multiple_files);
        if (res == 0) status = 0;
        fclose(fp);
    }

    return status;
}
