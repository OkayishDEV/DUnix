#ifndef _LIBC_STDLIB_H
#define _LIBC_STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX     32767

int           rand(void);
void          srand(unsigned int seed);
int           atexit(void (*func)(void));
int           abs(int j);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

__attribute__((noreturn)) void exit(int status);
__attribute__((noreturn)) void abort(void);

int           atoi(const char *nptr);
long          atol(const char *nptr);
double        atof(const char *nptr);
long          strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
extern char **environ;

int           setenv(const char *name, const char *value, int overwrite);
int           unsetenv(const char *name);
char         *getenv(const char *name);
void          qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif /* _LIBC_STDLIB_H */
