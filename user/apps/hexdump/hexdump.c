#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>

static bool opt_canonical = true;
static long opt_skip = 0;
static long opt_length = -1;

static void dump_canonical(FILE *fp) {
    if (opt_skip > 0) {
        fseek(fp, opt_skip, SEEK_SET);
    }

    unsigned char buf[16];
    long total_read = 0;
    long offset = opt_skip;

    for (;;) {
        size_t to_read = sizeof(buf);
        if (opt_length >= 0) {
            long remaining = opt_length - total_read;
            if (remaining <= 0) break;
            if ((long)to_read > remaining) to_read = (size_t)remaining;
        }

        size_t n = fread(buf, 1, to_read, fp);
        if (n == 0) break;

        printf("%08lx  ", offset);

        /* Print hex bytes */
        for (size_t i = 0; i < 16; i++) {
            if (i == 8) printf(" ");
            if (i < n) {
                printf("%02x ", buf[i]);
            } else {
                printf("   ");
            }
        }

        /* Print ASCII representation */
        printf(" |");
        for (size_t i = 0; i < n; i++) {
            if (buf[i] >= 32 && buf[i] <= 126) {
                putchar(buf[i]);
            } else {
                putchar('.');
            }
        }
        printf("|\n");

        offset += n;
        total_read += n;
    }

    if (total_read > 0) {
        printf("%08lx\n", offset);
    }
}

int main(int argc, char **argv) {
    const char *files[64];
    int num_files = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-C") == 0) {
            opt_canonical = true;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            opt_length = atol(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            opt_skip = atol(argv[++i]);
        } else if (argv[i][0] != '-') {
            if (num_files < 64) {
                files[num_files++] = argv[i];
            }
        }
    }

    if (num_files == 0) {
        dump_canonical(stdin);
    } else {
        for (int i = 0; i < num_files; i++) {
            FILE *fp = fopen(files[i], "rb");
            if (!fp) {
                fprintf(stderr, "hexdump: %s: No such file or directory\n", files[i]);
                continue;
            }
            dump_canonical(fp);
            fclose(fp);
        }
    }

    return 0;
}
