#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

static FILE _stdin_struct  = { .fd = STDIN_FILENO,  .flags = O_RDONLY };
static FILE _stdout_struct = { .fd = STDOUT_FILENO, .flags = O_WRONLY };
static FILE _stderr_struct = { .fd = STDERR_FILENO, .flags = O_WRONLY };

FILE *stdin  = &_stdin_struct;
FILE *stdout = &_stdout_struct;
FILE *stderr = &_stderr_struct;

static int format_uint(char *buf, size_t size, uint64_t val, int base, int uppercase, int min_width, char pad_char) {
    char tmp[65];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = digits[val % (unsigned int)base];
            val /= (unsigned int)base;
        }
    }

    int len = i;
    int pad_len = (min_width > len) ? (min_width - len) : 0;
    int written = 0;

    for (int p = 0; p < pad_len; p++) {
        if ((size_t)written + 1 < size && buf) buf[written] = pad_char;
        written++;
    }

    for (int j = i - 1; j >= 0; j--) {
        if ((size_t)written + 1 < size && buf) buf[written] = tmp[j];
        written++;
    }

    return written;
}

static int format_int(char *buf, size_t size, int64_t val, int min_width, char pad_char) {
    int written = 0;
    uint64_t uval;

    if (val < 0) {
        if ((size_t)written + 1 < size && buf) buf[written] = '-';
        written++;
        uval = (uint64_t)(-val);
        if (min_width > 0) min_width--;
    } else {
        uval = (uint64_t)val;
    }

    written += format_uint(buf ? buf + written : NULL,
                           (buf && (size_t)written < size) ? (size - written) : 0,
                           uval, 10, 0, min_width, pad_char);
    return written;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t written = 0;

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            if (written + 1 < size && buf) buf[written] = *p;
            written++;
            continue;
        }

        p++;
        if (*p == '\0') break;
        if (*p == '%') {
            if (written + 1 < size && buf) buf[written] = '%';
            written++;
            continue;
        }

        int left_align = 0;
        char pad_char = ' ';
        while (*p == '-' || *p == '0') {
            if (*p == '-') {
                left_align = 1;
                p++;
            } else if (*p == '0') {
                pad_char = '0';
                p++;
            }
        }

        int min_width = 0;
        while (*p >= '0' && *p <= '9') {
            min_width = min_width * 10 + (*p - '0');
            p++;
        }

        /* Precision */
        int precision = -1;
        if (*p == '.') {
            p++;
            precision = 0;
            while (*p >= '0' && *p <= '9') {
                precision = precision * 10 + (*p - '0');
                p++;
            }
        }

        int is_long = 0;
        int is_long_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
            if (*p == 'l') {
                is_long_long = 1;
                p++;
            }
        } else if (*p == 'z') {
            is_long = 1;
            p++;
        }

        switch (*p) {
            case 'd':
            case 'i': {
                int64_t val = (is_long || is_long_long) ? va_arg(ap, int64_t) : va_arg(ap, int32_t);
                char num_buf[64];
                int nlen = format_int(num_buf, sizeof(num_buf), val, min_width, pad_char);
                for (int i = 0; i < nlen; i++) {
                    if (written + 1 < size && buf) buf[written] = num_buf[i];
                    written++;
                }
                break;
            }
            case 'u': {
                uint64_t val = (is_long || is_long_long) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
                char num_buf[64];
                int nlen = format_uint(num_buf, sizeof(num_buf), val, 10, 0, min_width, pad_char);
                for (int i = 0; i < nlen; i++) {
                    if (written + 1 < size && buf) buf[written] = num_buf[i];
                    written++;
                }
                break;
            }
            case 'x':
            case 'X': {
                uint64_t val = (is_long || is_long_long) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
                char num_buf[64];
                int nlen = format_uint(num_buf, sizeof(num_buf), val, 16, (*p == 'X'), min_width, pad_char);
                for (int i = 0; i < nlen; i++) {
                    if (written + 1 < size && buf) buf[written] = num_buf[i];
                    written++;
                }
                break;
            }
            case 'p': {
                uintptr_t val = (uintptr_t)va_arg(ap, void *);
                if (written + 1 < size && buf) buf[written] = '0';
                written++;
                if (written + 1 < size && buf) buf[written] = 'x';
                written++;
                char num_buf[64];
                int nlen = format_uint(num_buf, sizeof(num_buf), val, 16, 0, 16, '0');
                for (int i = 0; i < nlen; i++) {
                    if (written + 1 < size && buf) buf[written] = num_buf[i];
                    written++;
                }
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                size_t slen = strlen(s);
                if (precision >= 0 && (size_t)precision < slen) {
                    slen = (size_t)precision;
                }
                int pad = (min_width > (int)slen) ? (min_width - (int)slen) : 0;
                if (!left_align) {
                    while (pad-- > 0) {
                        if (written + 1 < size && buf) buf[written] = ' ';
                        written++;
                    }
                }
                for (size_t i = 0; i < slen; i++) {
                    if (written + 1 < size && buf) buf[written] = s[i];
                    written++;
                }
                if (left_align) {
                    while (pad-- > 0) {
                        if (written + 1 < size && buf) buf[written] = ' ';
                        written++;
                    }
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                if (written + 1 < size && buf) buf[written] = c;
                written++;
                break;
            }
            default:
                if (written + 1 < size && buf) buf[written] = *p;
                written++;
                break;
        }
    }

    if (buf && size > 0) {
        if (written < size) buf[written] = '\0';
        else buf[size - 1] = '\0';
    }

    return (int)written;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, 65536, format, ap);
    va_end(ap);
    return ret;
}

int vprintf(const char *format, va_list ap) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    write(STDOUT_FILENO, buf, (size_t)len);
    return len;
}

int printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}

int puts(const char *s) {
    size_t len = strlen(s);
    write(STDOUT_FILENO, s, len);
    write(STDOUT_FILENO, "\n", 1);
    return (int)len + 1;
}

int putchar(int c) {
    char ch = (char)c;
    write(STDOUT_FILENO, &ch, 1);
    return c;
}

int getchar(void) {
    char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    return (n == 1) ? (unsigned char)ch : EOF;
}

FILE *fopen(const char *pathname, const char *mode) {
    int flags = 0;
    if (strcmp(mode, "r") == 0)        flags = O_RDONLY;
    else if (strcmp(mode, "w") == 0)   flags = O_WRONLY | O_CREAT | O_TRUNC;
    else if (strcmp(mode, "a") == 0)   flags = O_WRONLY | O_CREAT | O_APPEND;
    else if (strcmp(mode, "r+") == 0)  flags = O_RDWR;
    else if (strcmp(mode, "w+") == 0)  flags = O_RDWR | O_CREAT | O_TRUNC;

    int fd = open(pathname, flags, 0644);
    if (fd < 0) return NULL;

    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (!f) {
        close(fd);
        return NULL;
    }
    f->fd = fd;
    f->flags = flags;
    return f;
}

int fclose(FILE *stream) {
    if (!stream) return EOF;
    int res = close(stream->fd);
    if (stream != stdin && stream != stdout && stream != stderr) {
        free(stream);
    }
    return (res == 0) ? 0 : EOF;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || size == 0 || nmemb == 0) return 0;
    ssize_t n = read(stream->fd, ptr, size * nmemb);
    return (n > 0) ? (size_t)n / size : 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || size == 0 || nmemb == 0) return 0;
    ssize_t n = write(stream->fd, ptr, size * nmemb);
    return (n > 0) ? (size_t)n / size : 0;
}

char *fgets(char *s, int size, FILE *stream) {
    if (!s || size <= 0 || !stream) return NULL;
    int i = 0;
    while (i < size - 1) {
        char ch;
        ssize_t n = read(stream->fd, &ch, 1);
        if (n <= 0) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = ch;
        if (ch == '\n') break;
    }
    s[i] = '\0';
    return s;
}

int fputs(const char *s, FILE *stream) {
    if (!s || !stream) return EOF;
    size_t len = strlen(s);
    ssize_t n = write(stream->fd, s, len);
    return (n == (ssize_t)len) ? 0 : EOF;
}

int fputc(int c, FILE *stream) {
    if (!stream) return EOF;
    char ch = (char)c;
    ssize_t n = write(stream->fd, &ch, 1);
    return (n == 1) ? c : EOF;
}

int fgetc(FILE *stream) {
    if (!stream) return EOF;
    char ch;
    ssize_t n = read(stream->fd, &ch, 1);
    return (n == 1) ? (unsigned char)ch : EOF;
}

int fprintf(FILE *stream, const char *format, ...) {
    if (!stream) return -1;
    char buf[1024];
    va_list ap;
    va_start(ap, format);
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    write(stream->fd, buf, (size_t)len);
    return len;
}

int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    off_t res = lseek(stream->fd, (off_t)offset, whence);
    return (res >= 0) ? 0 : -1;
}

long ftell(FILE *stream) {
    if (!stream) return -1L;
    off_t res = lseek(stream->fd, 0, SEEK_CUR);
    return (long)res;
}

void rewind(FILE *stream) {
    if (stream) fseek(stream, 0L, SEEK_SET);
}

void perror(const char *s) {
    if (s && *s) {
        write(STDERR_FILENO, s, strlen(s));
        write(STDERR_FILENO, ": error\n", 8);
    } else {
        write(STDERR_FILENO, "error\n", 6);
    }
}

