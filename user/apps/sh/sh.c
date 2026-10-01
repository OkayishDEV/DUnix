#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <dirent.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_LINE_LEN 1024
#define MAX_ARGS     64
#define MAX_STAGES   16

static int g_last_status = 0;

#define MAX_HISTORY 64
static char g_history[MAX_HISTORY][MAX_LINE_LEN];
static int  g_history_count = 0;
static int  g_history_idx = 0;
static char g_scratch[MAX_LINE_LEN];

static void trim(char *s) {
    if (!s) return;
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static void expand_variables(const char *src, char *dest, size_t max_dest) {
    size_t d = 0;
    bool in_single_quote = false;
    bool in_double_quote = false;

    for (size_t i = 0; src[i] != '\0' && d < max_dest - 1; ) {
        if (src[i] == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            dest[d++] = src[i++];
            continue;
        }
        if (src[i] == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            dest[d++] = src[i++];
            continue;
        }

        if (src[i] == '$' && !in_single_quote) {
            i++; /* skip '$' */
            if (src[i] == '?') {
                i++;
                char num_buf[16];
                snprintf(num_buf, sizeof(num_buf), "%d", g_last_status);
                for (size_t k = 0; num_buf[k] && d < max_dest - 1; k++) {
                    dest[d++] = num_buf[k];
                }
                continue;
            } else if (src[i] == '$') {
                i++;
                char num_buf[16];
                snprintf(num_buf, sizeof(num_buf), "%d", getpid());
                for (size_t k = 0; num_buf[k] && d < max_dest - 1; k++) {
                    dest[d++] = num_buf[k];
                }
                continue;
            } else if (src[i] == '0') {
                i++;
                const char *val = "sh";
                for (size_t k = 0; val[k] && d < max_dest - 1; k++) {
                    dest[d++] = val[k];
                }
                continue;
            }

            /* Extract variable name */
            char var_name[64];
            size_t v = 0;
            bool braced = false;
            if (src[i] == '{') {
                braced = true;
                i++;
            }

            while (src[i] && (isalnum((unsigned char)src[i]) || src[i] == '_') && v < sizeof(var_name) - 1) {
                var_name[v++] = src[i++];
            }
            var_name[v] = '\0';

            if (braced && src[i] == '}') {
                i++;
            }

            const char *val = NULL;
            if (v > 0) {
                if (strcmp(var_name, "PWD") == 0) {
                    static char cwd_buf[256];
                    val = getcwd(cwd_buf, sizeof(cwd_buf));
                } else if (strcmp(var_name, "USER") == 0) {
                    val = (getuid() == 0) ? "root" : "user";
                } else if (strcmp(var_name, "HOME") == 0) {
                    val = (getuid() == 0) ? "/root" : "/";
                } else {
                    val = getenv(var_name);
                }
            }

            if (val) {
                for (size_t k = 0; val[k] && d < max_dest - 1; k++) {
                    dest[d++] = val[k];
                }
            }
            continue;
        }

        dest[d++] = src[i++];
    }

    dest[d] = '\0';
}

static int parse_args(char *cmd, char **argv, char **in_file, char **out_file, bool *append_out) {
    int argc = 0;
    *in_file = NULL;
    *out_file = NULL;
    *append_out = false;

    char *p = cmd;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;

        /* Check redirection */
        if (*p == '<') {
            p++;
            while (isspace((unsigned char)*p)) p++;
            *in_file = p;
            while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '<') p++;
            if (*p) *p++ = '\0';
            continue;
        } else if (*p == '>') {
            p++;
            if (*p == '>') {
                *append_out = true;
                p++;
            }
            while (isspace((unsigned char)*p)) p++;
            *out_file = p;
            while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '<') p++;
            if (*p) *p++ = '\0';
            continue;
        }

        /* Argument parsing with full quote support */
        char *arg_start = p;
        char *dst = p;
        bool in_q = false;
        char q_char = 0;

        while (*p) {
            if (!in_q && (isspace((unsigned char)*p) || *p == '<' || *p == '>')) {
                break;
            }
            if (!in_q && (*p == '\'' || *p == '"')) {
                in_q = true;
                q_char = *p++;
            } else if (in_q && *p == q_char) {
                in_q = false;
                p++;
            } else {
                *dst++ = *p++;
            }
        }

        char end_ch = *p;
        if (dst == p && (end_ch == '<' || end_ch == '>')) {
            memmove(p + 1, p, strlen(p) + 1);
            *dst = '\0';
            p++;
        } else {
            *dst = '\0';
            if (end_ch != '\0' && end_ch != '<' && end_ch != '>') {
                p++;
            }
        }

        if (argc < MAX_ARGS - 1) {
            argv[argc++] = arg_start;
        }
    }
    argv[argc] = NULL;
    return argc;
}

static int execute_script_file(const char *path);

static int handle_builtin(int argc, char **argv) {
    if (strcmp(argv[0], "cd") == 0) {
        const char *target = (argc > 1) ? argv[1] : "/";
        if (chdir(target) != 0) {
            fprintf(stderr, "cd: cannot change directory to '%s'\n", target);
            return 1;
        }
        return 0;
    } else if (strcmp(argv[0], "pwd") == 0) {
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd))) {
            printf("%s\n", cwd);
        }
        return 0;
    } else if (strcmp(argv[0], "exit") == 0) {
        int code = (argc > 1) ? atoi(argv[1]) : g_last_status;
        exit(code);
    } else if (strcmp(argv[0], "clear") == 0) {
        printf("\033[2J\033[H");
        fflush(stdout);
        return 0;
    } else if (strcmp(argv[0], "sync") == 0) {
        sync();
        return 0;
    } else if (strcmp(argv[0], "export") == 0) {
        if (argc <= 1) {
            if (environ) {
                for (char **ep = environ; *ep; ep++) {
                    printf("export %s\n", *ep);
                }
            }
            return 0;
        }
        char *eq = strchr(argv[1], '=');
        if (eq) {
            *eq = '\0';
            setenv(argv[1], eq + 1, 1);
        } else {
            setenv(argv[1], "", 1);
        }
        return 0;
    } else if (strcmp(argv[0], "unset") == 0) {
        if (argc > 1) {
            unsetenv(argv[1]);
        }
        return 0;
    } else if (strcmp(argv[0], "source") == 0 || strcmp(argv[0], ".") == 0) {
        if (argc < 2) {
            fprintf(stderr, "%s: filename argument required\n", argv[0]);
            return 1;
        }
        return execute_script_file(argv[1]);
    } else if (strcmp(argv[0], "history") == 0) {
        int start = 0;
        if (g_history_count > MAX_HISTORY) start = g_history_count - MAX_HISTORY;
        for (int i = start; i < g_history_count; i++) {
            printf("%4d  %s\n", i + 1, g_history[i % MAX_HISTORY]);
        }
        return 0;
    } else if (strcmp(argv[0], "help") == 0) {
        printf("DUnix 64-Bit Interactive Unix Shell (/bin/sh)\n\n");
        printf("Built-in Commands:\n");
        printf("  cd <dir>         Change current working directory\n");
        printf("  pwd              Print working directory\n");
        printf("  export [NAME=V]  Set or display environment variables\n");
        printf("  unset <NAME>     Remove environment variable\n");
        printf("  source, . <file> Execute commands from script file in current shell\n");
        printf("  history          Show shell command history\n");
        printf("  sync             Synchronize cached data to persistent storage\n");
        printf("  clear            Clear terminal screen\n");
        printf("  exit [code]      Exit current shell session\n");
        printf("  help             Display this help summary\n\n");
        printf("Shell Features:\n");
        printf("  - Variable Expansion: $VAR, ${VAR}, $?, $$, $PWD, $USER, $HOME\n");
        printf("  - Operators: Chaining (;), Conditional AND (&&), Conditional OR (||)\n");
        printf("  - Pipelines: Arbitrary multi-stage pipe cascades (cmd1 | cmd2 | cmd3 ...)\n");
        printf("  - I/O Redirection: < (input), > (truncate output), >> (append output)\n");
        printf("  - Background Jobs: Command suffix &\n");
        return 0;
    }
    return -1; /* Not a builtin */
}

static int execute_single_command(char *cmd) {
    char *argv[MAX_ARGS];
    char *in_file = NULL;
    char *out_file = NULL;
    bool append_out = false;

    int argc = parse_args(cmd, argv, &in_file, &out_file, &append_out);
    if (argc == 0) return 0;

    bool background = false;
    if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        background = true;
        argv[argc - 1] = NULL;
        argc--;
        if (argc == 0) return 0;
    }

    /* Check built-in commands */
    int builtin_res = handle_builtin(argc, argv);
    if (builtin_res != -1) {
        return builtin_res;
    }

    /* Fork and execute external binary */
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "sh: fork failed\n");
        return 1;
    }

    if (pid == 0) {
        /* Child process: handle I/O redirections */
        if (in_file) {
            int fd = open(in_file, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "sh: cannot open input '%s'\n", in_file);
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (out_file) {
            int flags = O_WRONLY | O_CREAT | (append_out ? O_APPEND : O_TRUNC);
            int fd = open(out_file, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "sh: cannot open output '%s'\n", out_file);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(argv[0], argv);
        fprintf(stderr, "sh: %s: command not found\n", argv[0]);
        exit(127);
    }

    if (background) {
        printf("[%d]\n", pid);
        return 0;
    }

    /* Parent process: wait for child */
    int status = 0;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

static int execute_pipeline_multi(char **stages, int nstages) {
    if (nstages == 1) {
        return execute_single_command(stages[0]);
    }

    int pipefds[MAX_STAGES - 1][2];
    for (int i = 0; i < nstages - 1; i++) {
        if (pipe(pipefds[i]) != 0) {
            fprintf(stderr, "sh: pipe creation failed\n");
            return 1;
        }
    }

    pid_t pids[MAX_STAGES];

    for (int i = 0; i < nstages; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            /* Child process */
            if (i > 0) {
                dup2(pipefds[i - 1][0], STDIN_FILENO);
            }
            if (i < nstages - 1) {
                dup2(pipefds[i][1], STDOUT_FILENO);
            }

            /* Close all pipe descriptors in child */
            for (int k = 0; k < nstages - 1; k++) {
                close(pipefds[k][0]);
                close(pipefds[k][1]);
            }

            char *argv[MAX_ARGS];
            char *in_file = NULL, *out_file = NULL;
            bool append_out = false;
            int argc = parse_args(stages[i], argv, &in_file, &out_file, &append_out);
            if (argc > 0) {
                if (in_file) {
                    int fd = open(in_file, O_RDONLY);
                    if (fd >= 0) { dup2(fd, STDIN_FILENO); close(fd); }
                }
                if (out_file) {
                    int flags = O_WRONLY | O_CREAT | (append_out ? O_APPEND : O_TRUNC);
                    int fd = open(out_file, flags, 0644);
                    if (fd >= 0) { dup2(fd, STDOUT_FILENO); close(fd); }
                }

                int builtin_res = handle_builtin(argc, argv);
                if (builtin_res != -1) {
                    exit(builtin_res);
                }

                execvp(argv[0], argv);
                fprintf(stderr, "sh: %s: command not found\n", argv[0]);
            }
            exit(127);
        }
    }

    /* Parent closes all pipe fds */
    for (int i = 0; i < nstages - 1; i++) {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }

    /* Wait for all children and return status of the last command in pipeline */
    int last_status = 0;
    for (int i = 0; i < nstages; i++) {
        int status = 0;
        waitpid(pids[i], &status, 0);
        if (i == nstages - 1) {
            last_status = WEXITSTATUS(status);
        }
    }

    return last_status;
}

static int execute_pipeline_string(char *pipeline_cmd) {
    char *stages[MAX_STAGES];
    int nstages = 0;

    char *p = pipeline_cmd;
    bool in_single = false, in_double = false;
    char *start = p;

    while (*p) {
        if (*p == '\'' && !in_double) in_single = !in_single;
        else if (*p == '"' && !in_single) in_double = !in_double;
        else if (*p == '|' && !in_single && !in_double) {
            *p = '\0';
            trim(start);
            if (start[0] != '\0' && nstages < MAX_STAGES) {
                stages[nstages++] = start;
            }
            start = p + 1;
        }
        p++;
    }

    trim(start);
    if (start[0] != '\0' && nstages < MAX_STAGES) {
        stages[nstages++] = start;
    }

    if (nstages == 0) return 0;
    return execute_pipeline_multi(stages, nstages);
}

enum op_type { OP_NONE, OP_SEQ, OP_AND, OP_OR };

static int execute_line(char *raw_line) {
    trim(raw_line);
    if (raw_line[0] == '\0' || raw_line[0] == '#') {
        return g_last_status;
    }

    /* Tokenize by ;, &&, || taking quotes into account */
    char *p = raw_line;
    char *start = p;
    bool in_single = false, in_double = false;

    enum op_type next_op = OP_NONE;
    bool skip_next = false;

    while (*p) {
        if (*p == '\'' && !in_double) in_single = !in_single;
        else if (*p == '"' && !in_single) in_double = !in_double;
        else if (!in_single && !in_double) {
            enum op_type cur_op = OP_NONE;
            int op_len = 0;

            if (*p == ';') {
                cur_op = OP_SEQ;
                op_len = 1;
            } else if (*p == '&' && *(p + 1) == '&') {
                cur_op = OP_AND;
                op_len = 2;
            } else if (*p == '|' && *(p + 1) == '|') {
                cur_op = OP_OR;
                op_len = 2;
            }

            if (cur_op != OP_NONE) {
                *p = '\0';
                trim(start);

                if (start[0] != '\0') {
                    if (!skip_next) {
                        char expanded[MAX_LINE_LEN];
                        expand_variables(start, expanded, sizeof(expanded));
                        trim(expanded);
                        if (expanded[0] != '\0') {
                            g_last_status = execute_pipeline_string(expanded);
                        }
                    }
                }

                if (cur_op == OP_AND) {
                    skip_next = (g_last_status != 0);
                } else if (cur_op == OP_OR) {
                    skip_next = (g_last_status == 0);
                } else {
                    skip_next = false;
                }

                p += op_len;
                start = p;
                next_op = cur_op;
                continue;
            }
        }
        p++;
    }

    trim(start);
    if (start[0] != '\0') {
        if (!skip_next) {
            char expanded[MAX_LINE_LEN];
            expand_variables(start, expanded, sizeof(expanded));
            trim(expanded);
            if (expanded[0] != '\0') {
                g_last_status = execute_pipeline_string(expanded);
            }
        }
    }

    (void)next_op;
    return g_last_status;
}

static int execute_script_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "sh: cannot open script '%s'\n", path);
        return 1;
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        execute_line(line);
    }

    fclose(fp);
    return g_last_status;
}

static const char *g_builtins[] = {
    "cd", "pwd", "exit", "clear", "sync", "export", "unset", "source", "history", "help", NULL
};

static void print_prompt(void) {
    char cwd[256];
    uid_t uid = getuid();
    char prompt_char = (uid == 0) ? '#' : '$';

    if (getcwd(cwd, sizeof(cwd))) {
        printf("dunix:%s%c ", cwd, prompt_char);
    } else {
        printf("dunix%c ", prompt_char);
    }
    fflush(stdout);
}

static void handle_tab_completion(char *line, size_t *pos, size_t *len, size_t max_len) {
    if (*pos == 0) return;

    size_t word_start = *pos;
    while (word_start > 0 && !isspace((unsigned char)line[word_start - 1])) {
        word_start--;
    }

    size_t prefix_len = *pos - word_start;
    if (prefix_len == 0) return;

    char prefix[256];
    if (prefix_len >= sizeof(prefix)) prefix_len = sizeof(prefix) - 1;
    memcpy(prefix, &line[word_start], prefix_len);
    prefix[prefix_len] = '\0';

    bool is_command = true;
    for (size_t i = 0; i < word_start; i++) {
        if (!isspace((unsigned char)line[i])) {
            if (line[i] == ';' || line[i] == '|' || line[i] == '&') {
                is_command = true;
            } else {
                is_command = false;
            }
        }
    }

    char matches[64][128];
    bool is_dir[64];
    int num_matches = 0;

    if (is_command && strchr(prefix, '/') == NULL) {
        for (int i = 0; g_builtins[i]; i++) {
            if (strncmp(g_builtins[i], prefix, prefix_len) == 0) {
                if (num_matches < 64) {
                    strncpy(matches[num_matches], g_builtins[i], sizeof(matches[0]) - 1);
                    matches[num_matches][sizeof(matches[0]) - 1] = '\0';
                    is_dir[num_matches] = false;
                    num_matches++;
                }
            }
        }

        DIR *dir = opendir("/bin");
        if (dir) {
            struct dirent *de;
            while ((de = readdir(dir)) != NULL && num_matches < 64) {
                if (strncmp(de->d_name, prefix, prefix_len) == 0) {
                    bool dup = false;
                    for (int m = 0; m < num_matches; m++) {
                        if (strcmp(matches[m], de->d_name) == 0) { dup = true; break; }
                    }
                    if (!dup) {
                        strncpy(matches[num_matches], de->d_name, sizeof(matches[0]) - 1);
                        matches[num_matches][sizeof(matches[0]) - 1] = '\0';
                        is_dir[num_matches] = false;
                        num_matches++;
                    }
                }
            }
            closedir(dir);
        }
    } else {
        char dir_path[256] = ".";
        const char *file_prefix = prefix;
        char *last_slash = strrchr(prefix, '/');
        if (last_slash) {
            size_t dlen = (size_t)(last_slash - prefix);
            if (dlen == 0) {
                strcpy(dir_path, "/");
            } else {
                strncpy(dir_path, prefix, dlen);
                dir_path[dlen] = '\0';
            }
            file_prefix = last_slash + 1;
        }
        size_t fprefix_len = strlen(file_prefix);

        DIR *dir = opendir(dir_path);
        if (dir) {
            struct dirent *de;
            while ((de = readdir(dir)) != NULL && num_matches < 64) {
                if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
                    continue;
                }
                if (strncmp(de->d_name, file_prefix, fprefix_len) == 0) {
                    char full_check[512];
                    if (strcmp(dir_path, "/") == 0) {
                        snprintf(full_check, sizeof(full_check), "/%s", de->d_name);
                    } else if (strcmp(dir_path, ".") == 0) {
                        snprintf(full_check, sizeof(full_check), "%s", de->d_name);
                    } else {
                        snprintf(full_check, sizeof(full_check), "%s/%s", dir_path, de->d_name);
                    }

                    struct stat st;
                    bool isd = false;
                    if (stat(full_check, &st) == 0 && S_ISDIR(st.st_mode)) {
                        isd = true;
                    }

                    strncpy(matches[num_matches], de->d_name, sizeof(matches[0]) - 1);
                    matches[num_matches][sizeof(matches[0]) - 1] = '\0';
                    is_dir[num_matches] = isd;
                    num_matches++;
                }
            }
            closedir(dir);
        }
    }

    if (num_matches == 0) {
        putchar('\a');
        fflush(stdout);
        return;
    }

    if (num_matches == 1) {
        const char *m = matches[0];
        const char *to_add;
        char add_buf[128];

        if (is_command && strchr(prefix, '/') == NULL) {
            to_add = m + prefix_len;
            snprintf(add_buf, sizeof(add_buf), "%s ", to_add);
        } else {
            char *last_slash = strrchr(prefix, '/');
            const char *file_prefix = last_slash ? (last_slash + 1) : prefix;
            to_add = m + strlen(file_prefix);
            if (is_dir[0]) {
                snprintf(add_buf, sizeof(add_buf), "%s/", to_add);
            } else {
                snprintf(add_buf, sizeof(add_buf), "%s ", to_add);
            }
        }

        size_t add_len = strlen(add_buf);
        if (*len + add_len < max_len - 1) {
            memmove(&line[*pos + add_len], &line[*pos], *len - *pos + 1);
            memcpy(&line[*pos], add_buf, add_len);
            *pos += add_len;
            *len += add_len;
            printf("%s", add_buf);
            if (*pos < *len) {
                printf("%s", &line[*pos]);
                for (size_t i = *pos; i < *len; i++) printf("\033[D");
            }
            fflush(stdout);
        }
        return;
    }

    char lcp[128];
    strncpy(lcp, matches[0], sizeof(lcp) - 1);
    lcp[sizeof(lcp) - 1] = '\0';
    for (int i = 1; i < num_matches; i++) {
        size_t k = 0;
        while (lcp[k] && matches[i][k] && lcp[k] == matches[i][k]) k++;
        lcp[k] = '\0';
    }

    size_t base_len;
    if (is_command && strchr(prefix, '/') == NULL) {
        base_len = prefix_len;
    } else {
        char *last_slash = strrchr(prefix, '/');
        const char *file_prefix = last_slash ? (last_slash + 1) : prefix;
        base_len = strlen(file_prefix);
    }

    size_t lcp_len = strlen(lcp);
    if (lcp_len > base_len) {
        const char *to_add = lcp + base_len;
        size_t add_len = strlen(to_add);
        if (*len + add_len < max_len - 1) {
            memmove(&line[*pos + add_len], &line[*pos], *len - *pos + 1);
            memcpy(&line[*pos], to_add, add_len);
            *pos += add_len;
            *len += add_len;
            printf("%s", to_add);
            if (*pos < *len) {
                printf("%s", &line[*pos]);
                for (size_t i = *pos; i < *len; i++) printf("\033[D");
            }
            fflush(stdout);
        }
    } else {
        printf("\n");
        for (int i = 0; i < num_matches; i++) {
            printf("%s%s  ", matches[i], is_dir[i] ? "/" : "");
        }
        printf("\n");
        print_prompt();
        printf("%s", line);
        for (size_t i = *len; i > *pos; i--) printf("\033[D");
        fflush(stdout);
    }
}

static bool read_line_interactive(char *line, size_t max_len) {
    struct termios orig_term;
    bool has_term = (tcgetattr(STDIN_FILENO, &orig_term) == 0);

    if (has_term) {
        struct termios raw = orig_term;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    size_t pos = 0;
    size_t len = 0;
    line[0] = '\0';
    g_history_idx = g_history_count;

    for (;;) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            if (has_term) tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
            return false;
        }

        if (c == '\r' || c == '\n') {
            printf("\n");
            fflush(stdout);
            line[len] = '\0';
            break;
        }

        if (c == 0x04) { /* Ctrl+D */
            if (len == 0) {
                if (has_term) tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
                return false;
            }
            continue;
        }

        if (c == 0x03) { /* Ctrl+C */
            printf("^C\n");
            line[0] = '\0';
            len = 0;
            pos = 0;
            print_prompt();
            continue;
        }

        if (c == 0x0C) { /* Ctrl+L: Clear screen */
            printf("\033[2J\033[H");
            print_prompt();
            printf("%s", line);
            for (size_t i = len; i > pos; i--) printf("\033[D");
            fflush(stdout);
            continue;
        }

        if (c == 0x15) { /* Ctrl+U: Erase line before cursor */
            memmove(line, &line[pos], len - pos + 1);
            len -= pos;
            pos = 0;
            printf("\r\033[K");
            print_prompt();
            printf("%s", line);
            for (size_t i = len; i > pos; i--) printf("\033[D");
            fflush(stdout);
            continue;
        }

        if (c == 0x0B) { /* Ctrl+K: Erase line after cursor */
            line[pos] = '\0';
            len = pos;
            printf("\033[K");
            fflush(stdout);
            continue;
        }

        if (c == 0x01) { /* Ctrl+A: Home */
            while (pos > 0) {
                printf("\033[D");
                pos--;
            }
            fflush(stdout);
            continue;
        }

        if (c == 0x05) { /* Ctrl+E: End */
            while (pos < len) {
                printf("\033[C");
                pos++;
            }
            fflush(stdout);
            continue;
        }

        if (c == 0x7F || c == 0x08) { /* Backspace */
            if (pos > 0) {
                memmove(&line[pos - 1], &line[pos], len - pos + 1);
                pos--;
                len--;
                printf("\b\033[K%s", &line[pos]);
                for (size_t i = pos; i < len; i++) printf("\033[D");
                fflush(stdout);
            }
            continue;
        }

        if (c == '\t') { /* Tab completion */
            handle_tab_completion(line, &pos, &len, max_len);
            continue;
        }

        if (c == '\033') { /* ANSI Escape Sequence */
            char seq[3] = {0};
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
                if (seq[1] == 'A') { /* Up Arrow: History Previous */
                    if (g_history_idx > 0) {
                        if (g_history_idx == g_history_count) {
                            strncpy(g_scratch, line, sizeof(g_scratch) - 1);
                            g_scratch[sizeof(g_scratch) - 1] = '\0';
                        }
                        g_history_idx--;
                        strncpy(line, g_history[g_history_idx % MAX_HISTORY], max_len - 1);
                        line[max_len - 1] = '\0';
                        len = strlen(line);
                        pos = len;
                        printf("\r\033[K");
                        print_prompt();
                        printf("%s", line);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'B') { /* Down Arrow: History Next */
                    if (g_history_idx < g_history_count) {
                        g_history_idx++;
                        if (g_history_idx == g_history_count) {
                            strncpy(line, g_scratch, max_len - 1);
                        } else {
                            strncpy(line, g_history[g_history_idx % MAX_HISTORY], max_len - 1);
                        }
                        line[max_len - 1] = '\0';
                        len = strlen(line);
                        pos = len;
                        printf("\r\033[K");
                        print_prompt();
                        printf("%s", line);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'C') { /* Right Arrow */
                    if (pos < len) {
                        pos++;
                        printf("\033[C");
                        fflush(stdout);
                    }
                } else if (seq[1] == 'D') { /* Left Arrow */
                    if (pos > 0) {
                        pos--;
                        printf("\033[D");
                        fflush(stdout);
                    }
                } else if (seq[1] == 'H') { /* Home */
                    while (pos > 0) {
                        printf("\033[D");
                        pos--;
                    }
                    fflush(stdout);
                } else if (seq[1] == 'F') { /* End */
                    while (pos < len) {
                        printf("\033[C");
                        pos++;
                    }
                    fflush(stdout);
                } else if (seq[1] >= '1' && seq[1] <= '6') {
                    if (read(STDIN_FILENO, &seq[2], 1) > 0 && seq[2] == '~') {
                        if (seq[1] == '1' || seq[1] == '7') { /* Home */
                            while (pos > 0) {
                                printf("\033[D");
                                pos--;
                            }
                            fflush(stdout);
                        } else if (seq[1] == '4' || seq[1] == '8') { /* End */
                            while (pos < len) {
                                printf("\033[C");
                                pos++;
                            }
                            fflush(stdout);
                        } else if (seq[1] == '3') { /* Delete */
                            if (pos < len) {
                                memmove(&line[pos], &line[pos + 1], len - pos);
                                len--;
                                printf("\033[K%s", &line[pos]);
                                for (size_t i = pos; i < len; i++) printf("\033[D");
                                fflush(stdout);
                            }
                        }
                    }
                }
            }
            continue;
        }

        /* Printable ASCII characters */
        if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
            if (len < max_len - 1) {
                memmove(&line[pos + 1], &line[pos], len - pos + 1);
                line[pos] = c;
                pos++;
                len++;
                line[len] = '\0';
                printf("%s", &line[pos - 1]);
                for (size_t i = pos; i < len; i++) printf("\033[D");
                fflush(stdout);
            }
            continue;
        }
    }

    if (has_term) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
    }

    if (len > 0) {
        if (g_history_count == 0 || strcmp(g_history[(g_history_count - 1) % MAX_HISTORY], line) != 0) {
            strncpy(g_history[g_history_count % MAX_HISTORY], line, MAX_LINE_LEN - 1);
            g_history[g_history_count % MAX_HISTORY][MAX_LINE_LEN - 1] = '\0';
            g_history_count++;
        }
    }

    return true;
}

int main(int argc, char **argv) {
    char line[MAX_LINE_LEN];

    /* Check if executing a command string (sh -c "command") */
    if (argc > 2 && strcmp(argv[1], "-c") == 0) {
        strncpy(line, argv[2], sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
        return execute_line(line);
    }

    /* Check if executing a script file (e.g. /bin/sh script.sh or ./script.sh) */
    if (argc > 1 && argv[1][0] != '-') {
        return execute_script_file(argv[1]);
    }

    printf("Welcome to DUnix Shell (/bin/sh)\n");
    printf("Type 'help' for built-in commands and core utilities\n\n");

    for (;;) {
        print_prompt();

        if (isatty(STDIN_FILENO)) {
            if (!read_line_interactive(line, sizeof(line))) {
                printf("\nexit\n");
                break;
            }
        } else {
            if (!fgets(line, sizeof(line), stdin)) {
                printf("\nexit\n");
                break;
            }
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        }

        execute_line(line);
    }

    return g_last_status;
}
