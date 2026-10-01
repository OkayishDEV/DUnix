#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>

static bool opt_human = false;
static bool opt_summary = false;
static bool opt_all = false;
static int  opt_max_depth = -1;

static void format_size(uint64_t kb, char *out, size_t out_len) {
    if (!opt_human) {
        snprintf(out, out_len, "%lu", (unsigned long)kb);
        return;
    }

    if (kb < 1024) {
        snprintf(out, out_len, "%luK", (unsigned long)kb);
    } else if (kb < 1024 * 1024) {
        snprintf(out, out_len, "%.1fM", (double)kb / 1024.0);
    } else {
        snprintf(out, out_len, "%.1fG", (double)kb / (1024.0 * 1024.0));
    }
}

static uint64_t du_path(const char *path, int depth) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "du: cannot access '%s'\n", path);
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
        uint64_t kb = (st.st_size + 1023) / 1024;
        if (kb == 0 && st.st_size > 0) kb = 1;
        if (opt_all && (opt_max_depth < 0 || depth <= opt_max_depth)) {
            char sbuf[32];
            format_size(kb, sbuf, sizeof(sbuf));
            printf("%s\t%s\n", sbuf, path);
        }
        return kb;
    }

    /* Directory */
    uint64_t total_kb = (st.st_size + 1023) / 1024;
    if (total_kb == 0) total_kb = 1;

    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "du: cannot read directory '%s'\n", path);
        return total_kb;
    }

    struct dirent *de;
    char subpath[512];
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        size_t plen = strlen(path);
        if (plen > 0 && path[plen - 1] == '/') {
            snprintf(subpath, sizeof(subpath), "%s%s", path, de->d_name);
        } else {
            snprintf(subpath, sizeof(subpath), "%s/%s", path, de->d_name);
        }

        total_kb += du_path(subpath, depth + 1);
    }

    closedir(dir);

    if (!opt_summary || depth == 0) {
        if (opt_max_depth < 0 || depth <= opt_max_depth) {
            char sbuf[32];
            format_size(total_kb, sbuf, sizeof(sbuf));
            printf("%s\t%s\n", sbuf, path);
        }
    }

    return total_kb;
}

int main(int argc, char **argv) {
    const char *targets[64];
    int num_targets = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--human-readable") == 0) {
            opt_human = true;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--summarize") == 0) {
            opt_summary = true;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            opt_all = true;
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            opt_max_depth = atoi(argv[++i]);
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int k = 1; argv[i][k]; k++) {
                if (argv[i][k] == 'h') opt_human = true;
                else if (argv[i][k] == 's') opt_summary = true;
                else if (argv[i][k] == 'a') opt_all = true;
            }
        } else {
            if (num_targets < 64) {
                targets[num_targets++] = argv[i];
            }
        }
    }

    if (num_targets == 0) {
        targets[num_targets++] = ".";
    }

    for (int i = 0; i < num_targets; i++) {
        du_path(targets[i], 0);
    }

    return 0;
}
