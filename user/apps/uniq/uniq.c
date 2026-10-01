#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LINE_LEN 1024

int main(int argc, char **argv) {
    bool count_mode = false;
    bool duplicates_only = false;
    bool unique_only = false;
    const char *in_filename = NULL;
    const char *out_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--count") == 0) {
            count_mode = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--repeated") == 0) {
            duplicates_only = true;
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--unique") == 0) {
            unique_only = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTION]... [INPUT [OUTPUT]]\n", argv[0]);
            printf("Filter adjacent matching lines from INPUT (or standard input) to OUTPUT.\n\n");
            printf("  -c, --count     prefix lines by the number of occurrences\n");
            printf("  -d, --repeated  only print duplicate lines, one for each group\n");
            printf("  -u, --unique    only print unique lines\n");
            printf("      --help      display this help and exit\n");
            return 0;
        } else if (argv[i][0] != '-') {
            if (!in_filename) in_filename = argv[i];
            else if (!out_filename) out_filename = argv[i];
        }
    }

    FILE *in_fp = stdin;
    if (in_filename && strcmp(in_filename, "-") != 0) {
        in_fp = fopen(in_filename, "r");
        if (!in_fp) {
            fprintf(stderr, "%s: cannot open '%s'\n", argv[0], in_filename);
            return 1;
        }
    }

    FILE *out_fp = stdout;
    if (out_filename) {
        out_fp = fopen(out_filename, "w");
        if (!out_fp) {
            fprintf(stderr, "%s: cannot create '%s'\n", argv[0], out_filename);
            if (in_fp != stdin) fclose(in_fp);
            return 1;
        }
    }

    char prev_line[MAX_LINE_LEN];
    char curr_line[MAX_LINE_LEN];
    bool have_prev = false;
    int group_count = 0;

    while (fgets(curr_line, sizeof(curr_line), in_fp)) {
        size_t len = strlen(curr_line);
        if (len > 0 && curr_line[len - 1] == '\n') curr_line[len - 1] = '\0';

        if (have_prev) {
            if (strcmp(curr_line, prev_line) == 0) {
                group_count++;
                continue;
            } else {
                /* Flush previous group */
                bool print_it = true;
                if (duplicates_only && group_count == 1) print_it = false;
                if (unique_only && group_count > 1) print_it = false;

                if (print_it) {
                    if (count_mode) {
                        fprintf(out_fp, "%7d %s\n", group_count, prev_line);
                    } else {
                        fprintf(out_fp, "%s\n", prev_line);
                    }
                }

                strncpy(prev_line, curr_line, sizeof(prev_line) - 1);
                prev_line[sizeof(prev_line) - 1] = '\0';
                group_count = 1;
            }
        } else {
            strncpy(prev_line, curr_line, sizeof(prev_line) - 1);
            prev_line[sizeof(prev_line) - 1] = '\0';
            group_count = 1;
            have_prev = true;
        }
    }

    if (have_prev) {
        bool print_it = true;
        if (duplicates_only && group_count == 1) print_it = false;
        if (unique_only && group_count > 1) print_it = false;

        if (print_it) {
            if (count_mode) {
                fprintf(out_fp, "%7d %s\n", group_count, prev_line);
            } else {
                fprintf(out_fp, "%s\n", prev_line);
            }
        }
    }

    if (in_fp != stdin) fclose(in_fp);
    if (out_fp != stdout) fclose(out_fp);

    return 0;
}
