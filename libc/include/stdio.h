#ifndef _LIBC_STDIO_H
#define _LIBC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)

typedef struct FILE {
    int fd;
    int flags;
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int   printf(const char *format, ...);
int   sprintf(char *str, const char *format, ...);
int   snprintf(char *str, size_t size, const char *format, ...);
int   vprintf(const char *format, va_list ap);
int   vsnprintf(char *str, size_t size, const char *format, va_list ap);

int   puts(const char *s);
int   putchar(int c);
int   getchar(void);

FILE *fopen(const char *pathname, const char *mode);
int   fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int   fputs(const char *s, FILE *stream);
int   fputc(int c, FILE *stream);
int   fgetc(FILE *stream);
int   fprintf(FILE *stream, const char *format, ...);
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

int   fseek(FILE *stream, long offset, int whence);
long  ftell(FILE *stream);
void  rewind(FILE *stream);
int   fflush(FILE *stream);
void  perror(const char *s);

#endif /* _LIBC_STDIO_H */
