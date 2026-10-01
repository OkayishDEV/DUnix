#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, char **argv) {
    bool opt_silent = false;
    bool opt_list = false;
    const char *path1 = NULL;
    const char *path2 = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--silent") == 0) {
            opt_silent = true;
        } else if (strcmp(argv[i], "-l") == 0) {
            opt_list = true;
        } else if (argv[i][0] != '-') {
            if (!path1) path1 = argv[i];
            else if (!path2) path2 = argv[i];
        }
    }

    if (!path1 || !path2) {
        fprintf(stderr, "Usage: cmp [-s|-l] <file1> <file2>\n");
        return 2;
    }

    FILE *f1 = fopen(path1, "rb");
    if (!f1) {
        if (!opt_silent) fprintf(stderr, "cmp: %s: No such file or directory\n", path1);
        return 2;
    }

    FILE *f2 = fopen(path2, "rb");
    if (!f2) {
        if (!opt_silent) fprintf(stderr, "cmp: %s: No such file or directory\n", path2);
        fclose(f1);
        return 2;
    }

    long byte_pos = 1;
    long line_num = 1;
    bool differ = false;

    for (;;) {
        int c1 = fgetc(f1);
        int c2 = fgetc(f2);

        if (c1 == EOF && c2 == EOF) {
            break;
        }

        if (c1 != c2) {
            differ = true;
            if (opt_silent) {
                break;
            }
            if (opt_list) {
                if (c1 == EOF) {
                    printf("%ld EOF %03o\n", byte_pos, c2);
                    break;
                } else if (c2 == EOF) {
                    printf("%ld %03o EOF\n", byte_pos, c1);
                    break;
                } else {
                    printf("%ld %03o %03o\n", byte_pos, c1, c2);
                }
            } else {
                if (c1 == EOF) {
                    fprintf(stderr, "cmp: EOF on %s which is empty\n", path1);
                } else if (c2 == EOF) {
                    fprintf(stderr, "cmp: EOF on %s which is empty\n", path2);
                } else {
                    printf("%s %s differ: byte %ld, line %ld\n", path1, path2, byte_pos, line_num);
                }
                break;
            }
        }

        if (c1 == '\n') {
            line_num++;
        }
        byte_pos++;
    }

    fclose(f1);
    fclose(f2);

    return differ ? 1 : 0;
}
