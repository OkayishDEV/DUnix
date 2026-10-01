#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_DIFF_LINES 2048
#define MAX_LINE_LEN   1024

static bool opt_unified = false;
static bool opt_brief = false;
static bool opt_ignore_case = false;
static bool opt_ignore_ws = false;

static char *lines1[MAX_DIFF_LINES];
static int num_lines1 = 0;
static char *lines2[MAX_DIFF_LINES];
static int num_lines2 = 0;

static int load_lines(const char *path, char **lines, int *num_lines) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "diff: %s: No such file or directory\n", path);
        return -1;
    }

    char buf[MAX_LINE_LEN];
    int count = 0;
    while (fgets(buf, sizeof(buf), fp) && count < MAX_DIFF_LINES) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        lines[count] = strdup(buf);
        if (!lines[count]) break;
        count++;
    }

    fclose(fp);
    *num_lines = count;
    return 0;
}

static bool lines_equal(const char *s1, const char *s2) {
    if (!s1 || !s2) return s1 == s2;

    if (opt_ignore_ws) {
        while (*s1 || *s2) {
            while (isspace((unsigned char)*s1)) s1++;
            while (isspace((unsigned char)*s2)) s2++;
            if (*s1 == '\0' && *s2 == '\0') return true;
            if (opt_ignore_case) {
                if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) return false;
            } else {
                if (*s1 != *s2) return false;
            }
            if (*s1) s1++;
            if (*s2) s2++;
        }
        return true;
    }

    if (opt_ignore_case) {
        return strcasecmp(s1, s2) == 0;
    }

    return strcmp(s1, s2) == 0;
}

int main(int argc, char **argv) {
    const char *path1 = NULL;
    const char *path2 = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-u") == 0) {
            opt_unified = true;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--brief") == 0) {
            opt_brief = true;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ignore-case") == 0) {
            opt_ignore_case = true;
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--ignore-all-space") == 0) {
            opt_ignore_ws = true;
        } else if (argv[i][0] != '-') {
            if (!path1) path1 = argv[i];
            else if (!path2) path2 = argv[i];
        }
    }

    if (!path1 || !path2) {
        fprintf(stderr, "Usage: diff [options] <file1> <file2>\n");
        return 2;
    }

    if (load_lines(path1, lines1, &num_lines1) != 0 ||
        load_lines(path2, lines2, &num_lines2) != 0) {
        return 2;
    }

    bool differs = false;
    int max_l = (num_lines1 > num_lines2) ? num_lines1 : num_lines2;

    for (int i = 0; i < max_l; i++) {
        const char *l1 = (i < num_lines1) ? lines1[i] : NULL;
        const char *l2 = (i < num_lines2) ? lines2[i] : NULL;

        if (!lines_equal(l1, l2)) {
            differs = true;
            break;
        }
    }

    if (!differs) {
        return 0; /* Files are identical */
    }

    if (opt_brief) {
        printf("Files %s and %s differ\n", path1, path2);
        return 1;
    }

    if (opt_unified) {
        printf("--- %s\n", path1);
        printf("+++ %s\n", path2);
        printf("@@ -1,%d +1,%d @@\n", num_lines1, num_lines2);
    }

    for (int i = 0; i < max_l; i++) {
        const char *l1 = (i < num_lines1) ? lines1[i] : NULL;
        const char *l2 = (i < num_lines2) ? lines2[i] : NULL;

        if (!lines_equal(l1, l2)) {
            if (opt_unified) {
                if (l1) printf("-%s\n", l1);
                if (l2) printf("+%s\n", l2);
            } else {
                printf("%dc%d\n", i + 1, i + 1);
                if (l1) printf("< %s\n", l1);
                printf("---\n");
                if (l2) printf("> %s\n", l2);
            }
        } else if (opt_unified) {
            printf(" %s\n", l1);
        }
    }

    return 1;
}
