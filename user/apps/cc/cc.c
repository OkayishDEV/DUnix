#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_SRC_SIZE 65536
#define MAX_TOKENS   8192
#define MAX_VARS     256
#define STACK_SIZE   4096

enum token_type {
    TOK_EOF = 0,
    TOK_INT,
    TOK_CHAR,
    TOK_VOID,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_RETURN,
    TOK_NUM,
    TOK_IDENT,
    TOK_STRING,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_ASSIGN,
    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_GT,
    TOK_LTE,
    TOK_GTE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMI,
    TOK_COMMA
};

struct token {
    enum token_type type;
    int64_t val;
    char str[128];
};

static struct token tokens[MAX_TOKENS];
static int num_tokens = 0;
static int tok_idx = 0;

struct var_entry {
    char name[64];
    int64_t val;
};

static struct var_entry vars[MAX_VARS];
static int num_vars = 0;

static int find_var(const char *name) {
    for (int i = 0; i < num_vars; i++) {
        if (strcmp(vars[i].name, name) == 0) return i;
    }
    if (num_vars < MAX_VARS) {
        strncpy(vars[num_vars].name, name, sizeof(vars[num_vars].name) - 1);
        vars[num_vars].val = 0;
        return num_vars++;
    }
    return -1;
}

static void tokenize(const char *src) {
    const char *p = src;
    num_tokens = 0;

    while (*p && num_tokens < MAX_TOKENS - 1) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;

        /* Skip preprocessor directives */
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* Skip comments */
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (isdigit((unsigned char)*p)) {
            int64_t val = 0;
            while (isdigit((unsigned char)*p)) {
                val = val * 10 + (*p - '0');
                p++;
            }
            tokens[num_tokens].type = TOK_NUM;
            tokens[num_tokens].val = val;
            num_tokens++;
            continue;
        }

        if (isalpha((unsigned char)*p) || *p == '_') {
            char ident[128];
            int len = 0;
            while ((isalnum((unsigned char)*p) || *p == '_') && len < 127) {
                ident[len++] = *p++;
            }
            ident[len] = '\0';

            enum token_type t = TOK_IDENT;
            if (strcmp(ident, "int") == 0) t = TOK_INT;
            else if (strcmp(ident, "char") == 0) t = TOK_CHAR;
            else if (strcmp(ident, "void") == 0) t = TOK_VOID;
            else if (strcmp(ident, "if") == 0) t = TOK_IF;
            else if (strcmp(ident, "else") == 0) t = TOK_ELSE;
            else if (strcmp(ident, "while") == 0) t = TOK_WHILE;
            else if (strcmp(ident, "for") == 0) t = TOK_FOR;
            else if (strcmp(ident, "return") == 0) t = TOK_RETURN;

            tokens[num_tokens].type = t;
            strncpy(tokens[num_tokens].str, ident, sizeof(tokens[num_tokens].str) - 1);
            num_tokens++;
            continue;
        }

        if (*p == '"') {
            p++;
            char str_buf[128];
            int len = 0;
            while (*p && *p != '"' && len < 127) {
                if (*p == '\\' && p[1] == 'n') {
                    str_buf[len++] = '\n';
                    p += 2;
                } else {
                    str_buf[len++] = *p++;
                }
            }
            if (*p == '"') p++;
            str_buf[len] = '\0';
            tokens[num_tokens].type = TOK_STRING;
            strncpy(tokens[num_tokens].str, str_buf, sizeof(tokens[num_tokens].str) - 1);
            num_tokens++;
            continue;
        }

        if (p[0] == '=' && p[1] == '=') { tokens[num_tokens++].type = TOK_EQ; p += 2; continue; }
        if (p[0] == '!' && p[1] == '=') { tokens[num_tokens++].type = TOK_NEQ; p += 2; continue; }
        if (p[0] == '<' && p[1] == '=') { tokens[num_tokens++].type = TOK_LTE; p += 2; continue; }
        if (p[0] == '>' && p[1] == '=') { tokens[num_tokens++].type = TOK_GTE; p += 2; continue; }

        switch (*p) {
            case '+': tokens[num_tokens++].type = TOK_PLUS; break;
            case '-': tokens[num_tokens++].type = TOK_MINUS; break;
            case '*': tokens[num_tokens++].type = TOK_STAR; break;
            case '/': tokens[num_tokens++].type = TOK_SLASH; break;
            case '=': tokens[num_tokens++].type = TOK_ASSIGN; break;
            case '<': tokens[num_tokens++].type = TOK_LT; break;
            case '>': tokens[num_tokens++].type = TOK_GT; break;
            case '(': tokens[num_tokens++].type = TOK_LPAREN; break;
            case ')': tokens[num_tokens++].type = TOK_RPAREN; break;
            case '{': tokens[num_tokens++].type = TOK_LBRACE; break;
            case '}': tokens[num_tokens++].type = TOK_RBRACE; break;
            case ';': tokens[num_tokens++].type = TOK_SEMI; break;
            case ',': tokens[num_tokens++].type = TOK_COMMA; break;
            default: break;
        }
        p++;
    }
    tokens[num_tokens].type = TOK_EOF;
}

static int64_t eval_expr(void);

static int64_t eval_primary(void) {
    if (tokens[tok_idx].type == TOK_NUM) {
        return tokens[tok_idx++].val;
    } else if (tokens[tok_idx].type == TOK_IDENT) {
        char name[128];
        strcpy(name, tokens[tok_idx++].str);

        if (tokens[tok_idx].type == TOK_LPAREN) {
            /* Function call */
            tok_idx++; /* Skip '(' */
            if (strcmp(name, "printf") == 0) {
                if (tokens[tok_idx].type == TOK_STRING) {
                    char fmt[128];
                    strcpy(fmt, tokens[tok_idx++].str);
                    int64_t arg = 0;
                    if (tokens[tok_idx].type == TOK_COMMA) {
                        tok_idx++;
                        arg = eval_expr();
                    }
                    if (tokens[tok_idx].type == TOK_RPAREN) tok_idx++;
                    printf(fmt, arg);
                    return 0;
                }
            }
            while (tok_idx < num_tokens && tokens[tok_idx].type != TOK_RPAREN) tok_idx++;
            if (tokens[tok_idx].type == TOK_RPAREN) tok_idx++;
            return 0;
        }

        int var_id = find_var(name);
        return (var_id >= 0) ? vars[var_id].val : 0;
    } else if (tokens[tok_idx].type == TOK_LPAREN) {
        tok_idx++;
        int64_t v = eval_expr();
        if (tokens[tok_idx].type == TOK_RPAREN) tok_idx++;
        return v;
    }
    return 0;
}

static int64_t eval_factor(void) {
    int64_t val = eval_primary();
    while (tokens[tok_idx].type == TOK_STAR || tokens[tok_idx].type == TOK_SLASH) {
        enum token_type op = tokens[tok_idx++].type;
        int64_t rhs = eval_primary();
        if (op == TOK_STAR) val *= rhs;
        else if (rhs != 0) val /= rhs;
    }
    return val;
}

static int64_t eval_arith(void) {
    int64_t val = eval_factor();
    while (tokens[tok_idx].type == TOK_PLUS || tokens[tok_idx].type == TOK_MINUS) {
        enum token_type op = tokens[tok_idx++].type;
        int64_t rhs = eval_factor();
        if (op == TOK_PLUS) val += rhs;
        else val -= rhs;
    }
    return val;
}

static int64_t eval_relational(void) {
    int64_t val = eval_arith();
    while (tokens[tok_idx].type >= TOK_EQ && tokens[tok_idx].type <= TOK_GTE) {
        enum token_type op = tokens[tok_idx++].type;
        int64_t rhs = eval_arith();
        switch (op) {
            case TOK_EQ:  val = (val == rhs); break;
            case TOK_NEQ: val = (val != rhs); break;
            case TOK_LT:  val = (val < rhs);  break;
            case TOK_GT:  val = (val > rhs);  break;
            case TOK_LTE: val = (val <= rhs); break;
            case TOK_GTE: val = (val >= rhs); break;
            default: break;
        }
    }
    return val;
}

static int64_t eval_expr(void) {
    if (tokens[tok_idx].type == TOK_IDENT && tokens[tok_idx + 1].type == TOK_ASSIGN) {
        char name[128];
        strcpy(name, tokens[tok_idx].str);
        tok_idx += 2; /* Skip IDENT and '=' */
        int64_t val = eval_expr();
        int id = find_var(name);
        if (id >= 0) vars[id].val = val;
        return val;
    }
    return eval_relational();
}

static void exec_stmt(void);

static void exec_block(void) {
    if (tokens[tok_idx].type == TOK_LBRACE) tok_idx++;
    while (tok_idx < num_tokens && tokens[tok_idx].type != TOK_RBRACE && tokens[tok_idx].type != TOK_EOF) {
        exec_stmt();
    }
    if (tokens[tok_idx].type == TOK_RBRACE) tok_idx++;
}

static void exec_stmt(void) {
    if (tokens[tok_idx].type == TOK_INT || tokens[tok_idx].type == TOK_CHAR || tokens[tok_idx].type == TOK_VOID) {
        tok_idx++;
        if (tokens[tok_idx].type == TOK_IDENT) {
            char name[128];
            strcpy(name, tokens[tok_idx++].str);
            find_var(name);
            if (tokens[tok_idx].type == TOK_ASSIGN) {
                tok_idx++;
                int64_t val = eval_expr();
                int id = find_var(name);
                if (id >= 0) vars[id].val = val;
            }
        }
        if (tokens[tok_idx].type == TOK_SEMI) tok_idx++;
    } else if (tokens[tok_idx].type == TOK_IF) {
        tok_idx++; /* Skip 'if' */
        if (tokens[tok_idx].type == TOK_LPAREN) tok_idx++;
        int64_t cond = eval_expr();
        if (tokens[tok_idx].type == TOK_RPAREN) tok_idx++;

        if (cond) {
            exec_stmt();
            if (tokens[tok_idx].type == TOK_ELSE) {
                tok_idx++;
                /* Skip else body */
                int depth = 0;
                if (tokens[tok_idx].type == TOK_LBRACE) {
                    tok_idx++; depth = 1;
                    while (tok_idx < num_tokens && depth > 0) {
                        if (tokens[tok_idx].type == TOK_LBRACE) depth++;
                        else if (tokens[tok_idx].type == TOK_RBRACE) depth--;
                        tok_idx++;
                    }
                } else {
                    while (tok_idx < num_tokens && tokens[tok_idx].type != TOK_SEMI) tok_idx++;
                    if (tokens[tok_idx].type == TOK_SEMI) tok_idx++;
                }
            }
        } else {
            /* Skip then body */
            int depth = 0;
            if (tokens[tok_idx].type == TOK_LBRACE) {
                tok_idx++; depth = 1;
                while (tok_idx < num_tokens && depth > 0) {
                    if (tokens[tok_idx].type == TOK_LBRACE) depth++;
                    else if (tokens[tok_idx].type == TOK_RBRACE) depth--;
                    tok_idx++;
                }
            } else {
                while (tok_idx < num_tokens && tokens[tok_idx].type != TOK_SEMI) tok_idx++;
                if (tokens[tok_idx].type == TOK_SEMI) tok_idx++;
            }
            if (tokens[tok_idx].type == TOK_ELSE) {
                tok_idx++;
                exec_stmt();
            }
        }
    } else if (tokens[tok_idx].type == TOK_WHILE) {
        tok_idx++;
        int cond_pos = tok_idx;
        for (;;) {
            tok_idx = cond_pos;
            if (tokens[tok_idx].type == TOK_LPAREN) tok_idx++;
            int64_t cond = eval_expr();
            if (tokens[tok_idx].type == TOK_RPAREN) tok_idx++;
            if (!cond) break;
            exec_stmt();
        }
        /* Skip loop body */
        int depth = 0;
        if (tokens[tok_idx].type == TOK_LBRACE) {
            tok_idx++; depth = 1;
            while (tok_idx < num_tokens && depth > 0) {
                if (tokens[tok_idx].type == TOK_LBRACE) depth++;
                else if (tokens[tok_idx].type == TOK_RBRACE) depth--;
                tok_idx++;
            }
        } else {
            while (tok_idx < num_tokens && tokens[tok_idx].type != TOK_SEMI) tok_idx++;
            if (tokens[tok_idx].type == TOK_SEMI) tok_idx++;
        }
    } else if (tokens[tok_idx].type == TOK_FOR) {
        tok_idx++;
        if (tokens[tok_idx].type == TOK_LPAREN) tok_idx++;
        if (tokens[tok_idx].type != TOK_SEMI) eval_expr();
        if (tokens[tok_idx].type == TOK_SEMI) tok_idx++;

        int cond_pos = tok_idx;
        int step_pos = 0;

        /* Find step pos */
        int p = tok_idx;
        while (p < num_tokens && tokens[p].type != TOK_SEMI) p++;
        if (tokens[p].type == TOK_SEMI) step_pos = p + 1;

        /* Find body pos */
        int body_pos = step_pos;
        while (body_pos < num_tokens && tokens[body_pos].type != TOK_RPAREN) body_pos++;
        if (tokens[body_pos].type == TOK_RPAREN) body_pos++;

        for (;;) {
            tok_idx = cond_pos;
            int64_t cond = 1;
            if (tokens[tok_idx].type != TOK_SEMI) cond = eval_expr();
            if (!cond) break;

            tok_idx = body_pos;
            exec_stmt();

            tok_idx = step_pos;
            if (tokens[tok_idx].type != TOK_RPAREN) eval_expr();
        }

        tok_idx = body_pos;
        /* Skip body once finished */
        int depth = 0;
        if (tokens[tok_idx].type == TOK_LBRACE) {
            tok_idx++; depth = 1;
            while (tok_idx < num_tokens && depth > 0) {
                if (tokens[tok_idx].type == TOK_LBRACE) depth++;
                else if (tokens[tok_idx].type == TOK_RBRACE) depth--;
                tok_idx++;
            }
        } else {
            while (tok_idx < num_tokens && tokens[tok_idx].type != TOK_SEMI) tok_idx++;
            if (tokens[tok_idx].type == TOK_SEMI) tok_idx++;
        }
    } else if (tokens[tok_idx].type == TOK_LBRACE) {
        exec_block();
    } else if (tokens[tok_idx].type == TOK_RETURN) {
        tok_idx++;
        eval_expr();
        if (tokens[tok_idx].type == TOK_SEMI) tok_idx++;
    } else {
        eval_expr();
        if (tokens[tok_idx].type == TOK_SEMI) tok_idx++;
    }
}

static void run_c_program(const char *src) {
    tokenize(src);
    num_vars = 0;
    tok_idx = 0;

    /* Scan for main() */
    while (tok_idx < num_tokens) {
        if (tokens[tok_idx].type == TOK_IDENT && strcmp(tokens[tok_idx].str, "main") == 0) {
            tok_idx++;
            if (tokens[tok_idx].type == TOK_LPAREN) {
                while (tok_idx < num_tokens && tokens[tok_idx].type != TOK_RPAREN) tok_idx++;
                if (tokens[tok_idx].type == TOK_RPAREN) tok_idx++;
                exec_block();
                return;
            }
        }
        tok_idx++;
    }

    /* Fallback execute all statements */
    tok_idx = 0;
    while (tok_idx < num_tokens && tokens[tok_idx].type != TOK_EOF) {
        exec_stmt();
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("DUnix Native C Compiler / Runner (cc)\n");
        printf("Usage: cc <source_file.c> [-o output]\n");
        return 1;
    }

    const char *src_file = argv[1];
    FILE *fp = fopen(src_file, "r");
    if (!fp) {
        fprintf(stderr, "cc: error: unable to open input file '%s'\n", src_file);
        return 1;
    }

    char *src_code = (char *)malloc(MAX_SRC_SIZE);
    if (!src_code) {
        fclose(fp);
        return 1;
    }

    size_t n = fread(src_code, 1, MAX_SRC_SIZE - 1, fp);
    src_code[n] = '\0';
    fclose(fp);

    printf("[cc] Compiling and executing '%s' natively on DUnix...\n\n", src_file);
    run_c_program(src_code);
    free(src_code);

    return 0;
}
