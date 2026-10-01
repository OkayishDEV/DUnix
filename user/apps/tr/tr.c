#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int expand_set(const char *src, unsigned char *dest, int max_dest) {
    int out_len = 0;
    size_t len = strlen(src);

    for (size_t i = 0; i < len; i++) {
        if (i + 2 < len && src[i + 1] == '-' && src[i] <= src[i + 2]) {
            unsigned char start = (unsigned char)src[i];
            unsigned char end = (unsigned char)src[i + 2];
            for (unsigned char c = start; c <= end && out_len < max_dest; c++) {
                dest[out_len++] = c;
            }
            i += 2;
        } else {
            if (out_len < max_dest) {
                dest[out_len++] = (unsigned char)src[i];
            }
        }
    }
    return out_len;
}

int main(int argc, char **argv) {
    bool delete_mode = false;
    const char *set1_str = NULL;
    const char *set2_str = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--delete") == 0) {
            delete_mode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTION]... SET1 [SET2]\n", argv[0]);
            printf("Translate, squeeze, and/or delete characters from standard input.\n\n");
            printf("  -d, --delete    delete characters in SET1, do not translate\n");
            printf("      --help      display this help and exit\n");
            return 0;
        } else if (argv[i][0] != '-') {
            if (!set1_str) set1_str = argv[i];
            else if (!set2_str) set2_str = argv[i];
        }
    }

    if (!set1_str) {
        fprintf(stderr, "%s: missing operand\n", argv[0]);
        return 1;
    }

    unsigned char set1[256];
    int len1 = expand_set(set1_str, set1, sizeof(set1));

    unsigned char map[256];
    bool del_table[256];

    for (int i = 0; i < 256; i++) {
        map[i] = (unsigned char)i;
        del_table[i] = false;
    }

    if (delete_mode) {
        for (int i = 0; i < len1; i++) {
            del_table[set1[i]] = true;
        }
    } else {
        if (!set2_str) {
            fprintf(stderr, "%s: missing SET2 operand for translation\n", argv[0]);
            return 1;
        }
        unsigned char set2[256];
        int len2 = expand_set(set2_str, set2, sizeof(set2));

        for (int i = 0; i < len1; i++) {
            unsigned char target = (i < len2) ? set2[i] : (len2 > 0 ? set2[len2 - 1] : set1[i]);
            map[set1[i]] = target;
        }
    }

    int c;
    while ((c = getchar()) != EOF) {
        unsigned char uc = (unsigned char)c;
        if (delete_mode) {
            if (!del_table[uc]) {
                putchar(uc);
            }
        } else {
            putchar(map[uc]);
        }
    }

    return 0;
}
