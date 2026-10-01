#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

static const char *expr_ptr = NULL;

static int64_t parse_expr(void);

static int64_t parse_primary(void) {
    while (isspace((unsigned char)*expr_ptr)) expr_ptr++;
    if (*expr_ptr == '(') {
        expr_ptr++;
        int64_t val = parse_expr();
        while (isspace((unsigned char)*expr_ptr)) expr_ptr++;
        if (*expr_ptr == ')') expr_ptr++;
        return val;
    }

    int64_t val = 0;
    int sign = 1;
    if (*expr_ptr == '-') {
        sign = -1;
        expr_ptr++;
    }
    while (isdigit((unsigned char)*expr_ptr)) {
        val = val * 10 + (*expr_ptr - '0');
        expr_ptr++;
    }
    return val * sign;
}

static int64_t parse_factor(void) {
    int64_t val = parse_primary();
    while (1) {
        while (isspace((unsigned char)*expr_ptr)) expr_ptr++;
        if (*expr_ptr == '*') {
            expr_ptr++;
            val *= parse_primary();
        } else if (*expr_ptr == '/') {
            expr_ptr++;
            int64_t d = parse_primary();
            if (d != 0) val /= d;
        } else if (*expr_ptr == '%') {
            expr_ptr++;
            int64_t d = parse_primary();
            if (d != 0) val %= d;
        } else {
            break;
        }
    }
    return val;
}

static int64_t parse_expr(void) {
    int64_t val = parse_factor();
    while (1) {
        while (isspace((unsigned char)*expr_ptr)) expr_ptr++;
        if (*expr_ptr == '+') {
            expr_ptr++;
            val += parse_factor();
        } else if (*expr_ptr == '-') {
            expr_ptr++;
            val -= parse_factor();
        } else {
            break;
        }
    }
    return val;
}

int main(int argc, char **argv) {
    if (argc > 1) {
        char full_expr[256] = "";
        for (int i = 1; i < argc; i++) {
            strcat(full_expr, argv[i]);
            strcat(full_expr, " ");
        }
        expr_ptr = full_expr;
        printf("%ld\n", parse_expr());
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) break;
        if (line[0] == '\0') continue;

        expr_ptr = line;
        printf("%ld\n", parse_expr());
    }

    return 0;
}
