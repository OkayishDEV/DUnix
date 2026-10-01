#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define INITIAL_CAPACITY 256
#define MAX_LINE_LEN     1024

static bool g_reverse = false;
static bool g_numeric = false;
static bool g_unique  = false;

static int compare_lines(const void *a, const void *b) {
    const char *s1 = *(const char *const *)a;
    const char *s2 = *(const char *const *)b;

    int cmp = 0;
    if (g_numeric) {
        long n1 = strtol(s1, NULL, 10);
        long n2 = strtol(s2, NULL, 10);
        if (n1 < n2) cmp = -1;
        else if (n1 > n2) cmp = 1;
        else cmp = 0;
    } else {
        cmp = strcmp(s1, s2);
    }

    return g_reverse ? -cmp : cmp;
}

int main(int argc, char **argv) {
    char *files[64];
    int num_files = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '-' && argv[i][1] != '\0') {
            for (const char *c = argv[i] + 1; *c; c++) {
                if (*c == 'r') g_reverse = true;
                else if (*c == 'n') g_numeric = true;
                else if (*c == 'u') g_unique = true;
            }
        } else if (strcmp(argv[i], "--reverse") == 0) {
            g_reverse = true;
        } else if (strcmp(argv[i], "--numeric-sort") == 0) {
            g_numeric = true;
        } else if (strcmp(argv[i], "--unique") == 0) {
            g_unique = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
            printf("Write sorted concatenation of all FILE(s) to standard output.\n\n");
            printf("  -n, --numeric-sort      compare according to string numerical value\n");
            printf("  -r, --reverse           reverse the result of comparisons\n");
            printf("  -u, --unique            output only the first of an equal run\n");
            printf("      --help              display this help and exit\n");
            return 0;
        } else if (argv[i][0] != '-') {
            if (num_files < 64) {
                files[num_files++] = argv[i];
            }
        }
    }

    size_t capacity = INITIAL_CAPACITY;
    size_t count = 0;
    char **lines = (char **)malloc(capacity * sizeof(char *));
    if (!lines) {
        fprintf(stderr, "%s: out of memory\n", argv[0]);
        return 1;
    }

    char line_buf[MAX_LINE_LEN];

    if (num_files == 0) {
        while (fgets(line_buf, sizeof(line_buf), stdin)) {
            size_t len = strlen(line_buf);
            if (len > 0 && line_buf[len - 1] == '\n') line_buf[len - 1] = '\0';

            if (count >= capacity) {
                capacity *= 2;
                char **new_lines = (char **)realloc(lines, capacity * sizeof(char *));
                if (!new_lines) {
                    fprintf(stderr, "%s: out of memory\n", argv[0]);
                    break;
                }
                lines = new_lines;
            }
            lines[count++] = strdup(line_buf);
        }
    } else {
        for (int i = 0; i < num_files; i++) {
            FILE *fp = fopen(files[i], "r");
            if (!fp) {
                fprintf(stderr, "%s: cannot open '%s'\n", argv[0], files[i]);
                continue;
            }
            while (fgets(line_buf, sizeof(line_buf), fp)) {
                size_t len = strlen(line_buf);
                if (len > 0 && line_buf[len - 1] == '\n') line_buf[len - 1] = '\0';

                if (count >= capacity) {
                    capacity *= 2;
                    char **new_lines = (char **)realloc(lines, capacity * sizeof(char *));
                    if (!new_lines) {
                        fprintf(stderr, "%s: out of memory\n", argv[0]);
                        break;
                    }
                    lines = new_lines;
                }
                lines[count++] = strdup(line_buf);
            }
            fclose(fp);
        }
    }

    if (count > 0) {
        qsort(lines, count, sizeof(char *), compare_lines);

        const char *prev = NULL;
        for (size_t i = 0; i < count; i++) {
            if (g_unique && prev && strcmp(lines[i], prev) == 0) {
                continue;
            }
            printf("%s\n", lines[i]);
            prev = lines[i];
        }
    }

    for (size_t i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);

    return 0;
}
