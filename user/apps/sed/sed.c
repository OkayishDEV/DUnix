#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LINE_LEN 4096

enum cmd_type {
    CMD_SUBST,
    CMD_DELETE,
    CMD_PRINT
};

struct sed_cmd {
    enum cmd_type type;
    char find[256];
    char replace[256];
    bool global;
    bool ignore_case;
};

static bool opt_silent = false;
static struct sed_cmd commands[32];
static int num_commands = 0;

static int parse_command(const char *cmd_str) {
    if (!cmd_str || !*cmd_str) return 0;
    if (num_commands >= 32) return -1;

    struct sed_cmd *cmd = &commands[num_commands];
    memset(cmd, 0, sizeof(*cmd));

    if (cmd_str[0] == 's') {
        char delim = cmd_str[1];
        if (!delim) return -1;

        const char *p1 = &cmd_str[2];
        const char *p2 = strchr(p1, delim);
        if (!p2) return -1;

        size_t find_len = (size_t)(p2 - p1);
        if (find_len >= sizeof(cmd->find)) find_len = sizeof(cmd->find) - 1;
        strncpy(cmd->find, p1, find_len);
        cmd->find[find_len] = '\0';

        const char *p3 = p2 + 1;
        const char *p4 = strchr(p3, delim);
        if (!p4) {
            strncpy(cmd->replace, p3, sizeof(cmd->replace) - 1);
            p4 = p3 + strlen(p3);
        } else {
            size_t rep_len = (size_t)(p4 - p3);
            if (rep_len >= sizeof(cmd->replace)) rep_len = sizeof(cmd->replace) - 1;
            strncpy(cmd->replace, p3, rep_len);
            cmd->replace[rep_len] = '\0';
        }

        /* Flags */
        const char *flags = (*p4 == delim) ? p4 + 1 : p4;
        while (*flags) {
            if (*flags == 'g') cmd->global = true;
            if (*flags == 'i') cmd->ignore_case = true;
            flags++;
        }

        cmd->type = CMD_SUBST;
        num_commands++;
        return 0;
    } else if (cmd_str[0] == '/') {
        char delim = '/';
        const char *p1 = &cmd_str[1];
        const char *p2 = strchr(p1, delim);
        if (!p2) return -1;

        size_t pat_len = (size_t)(p2 - p1);
        if (pat_len >= sizeof(cmd->find)) pat_len = sizeof(cmd->find) - 1;
        strncpy(cmd->find, p1, pat_len);
        cmd->find[pat_len] = '\0';

        char action = p2[1];
        if (action == 'd') {
            cmd->type = CMD_DELETE;
            num_commands++;
            return 0;
        } else if (action == 'p') {
            cmd->type = CMD_PRINT;
            num_commands++;
            return 0;
        }
    }

    return -1;
}

static char *case_str_search(char *haystack, const char *needle, bool ignore_case) {
    if (!ignore_case) return strstr(haystack, needle);
    size_t nlen = strlen(needle);
    for (char *h = haystack; *h; h++) {
        if (strncasecmp(h, needle, nlen) == 0) return h;
    }
    return NULL;
}

static void apply_commands(char *line, size_t max_len) {
    bool deleted = false;
    bool printed = false;

    for (int c = 0; c < num_commands; c++) {
        struct sed_cmd *cmd = &commands[c];

        if (cmd->type == CMD_DELETE) {
            if (case_str_search(line, cmd->find, cmd->ignore_case) != NULL) {
                deleted = true;
                break;
            }
        } else if (cmd->type == CMD_PRINT) {
            if (case_str_search(line, cmd->find, cmd->ignore_case) != NULL) {
                printf("%s\n", line);
                printed = true;
            }
        } else if (cmd->type == CMD_SUBST) {
            if (cmd->find[0] == '\0') continue;
            size_t find_len = strlen(cmd->find);
            size_t rep_len = strlen(cmd->replace);

            char temp[MAX_LINE_LEN];
            char *src = line;
            char *dst = temp;
            size_t dst_rem = sizeof(temp) - 1;

            char *match;
            while ((match = case_str_search(src, cmd->find, cmd->ignore_case)) != NULL) {
                size_t prefix_len = (size_t)(match - src);
                if (prefix_len > dst_rem) prefix_len = dst_rem;
                memcpy(dst, src, prefix_len);
                dst += prefix_len;
                dst_rem -= prefix_len;

                size_t to_copy = (rep_len > dst_rem) ? dst_rem : rep_len;
                memcpy(dst, cmd->replace, to_copy);
                dst += to_copy;
                dst_rem -= to_copy;

                src = match + find_len;
                if (!cmd->global) break;
            }

            size_t tail_len = strlen(src);
            if (tail_len > dst_rem) tail_len = dst_rem;
            memcpy(dst, src, tail_len);
            dst += tail_len;
            *dst = '\0';

            strncpy(line, temp, max_len - 1);
            line[max_len - 1] = '\0';
        }
    }

    if (!deleted && (!opt_silent || printed)) {
        if (!opt_silent) {
            printf("%s\n", line);
        }
    }
}

static void process_file(FILE *fp) {
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        apply_commands(line, sizeof(line));
    }
}

int main(int argc, char **argv) {
    const char *files[64];
    int num_files = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "--silent") == 0) {
            opt_silent = true;
        } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            parse_command(argv[++i]);
        } else if (argv[i][0] != '-' && num_commands == 0) {
            parse_command(argv[i]);
        } else if (argv[i][0] != '-') {
            if (num_files < 64) {
                files[num_files++] = argv[i];
            }
        }
    }

    if (num_files == 0) {
        process_file(stdin);
    } else {
        for (int i = 0; i < num_files; i++) {
            FILE *fp = fopen(files[i], "r");
            if (!fp) {
                fprintf(stderr, "sed: cannot read '%s'\n", files[i]);
                continue;
            }
            process_file(fp);
            fclose(fp);
        }
    }

    return 0;
}
