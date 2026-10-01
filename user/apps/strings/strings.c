#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>

static int opt_min_len = 4;
static char opt_radix = 0; /* 'd', 'o', 'x', or 0 */

static void print_offset(long offset) {
    if (opt_radix == 'o') {
        printf("%7lo ", (unsigned long)offset);
    } else if (opt_radix == 'd') {
        printf("%7ld ", offset);
    } else if (opt_radix == 'x') {
        printf("%7lx ", (unsigned long)offset);
    }
}

static void process_stream(FILE *fp) {
    char buf[1024];
    int len = 0;
    long file_pos = 0;
    long str_start_pos = 0;
    int c;

    while ((c = fgetc(fp)) != EOF) {
        if ((c >= 32 && c <= 126) || c == '\t') {
            if (len == 0) {
                str_start_pos = file_pos;
            }
            if (len < (int)sizeof(buf) - 1) {
                buf[len++] = (char)c;
            }
        } else {
            if (len >= opt_min_len) {
                buf[len] = '\0';
                if (opt_radix != 0) {
                    print_offset(str_start_pos);
                }
                printf("%s\n", buf);
            }
            len = 0;
        }
        file_pos++;
    }

    if (len >= opt_min_len) {
        buf[len] = '\0';
        if (opt_radix != 0) {
            print_offset(str_start_pos);
        }
        printf("%s\n", buf);
    }
}

int main(int argc, char **argv) {
    const char *files[64];
    int num_files = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            opt_min_len = atoi(argv[++i]);
            if (opt_min_len < 1) opt_min_len = 1;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            opt_radix = argv[++i][0];
        } else if (strcmp(argv[i], "-a") == 0) {
            /* Scan entire file */
        } else if (argv[i][0] == '-' && isdigit((unsigned char)argv[i][1])) {
            opt_min_len = atoi(&argv[i][1]);
            if (opt_min_len < 1) opt_min_len = 1;
        } else if (argv[i][0] != '-') {
            if (num_files < 64) {
                files[num_files++] = argv[i];
            }
        }
    }

    if (num_files == 0) {
        process_stream(stdin);
    } else {
        for (int i = 0; i < num_files; i++) {
            FILE *fp = fopen(files[i], "rb");
            if (!fp) {
                fprintf(stderr, "strings: cannot open '%s'\n", files[i]);
                continue;
            }
            process_stream(fp);
            fclose(fp);
        }
    }

    return 0;
}
