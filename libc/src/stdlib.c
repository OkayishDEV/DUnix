#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>

#define SYS_exit 60
extern int64_t __syscall(uint64_t num, ...);

struct user_block {
    size_t             size;
    int                is_free;
    struct user_block *prev;
    struct user_block *next;
};

static struct user_block *user_heap_head = NULL;

void *malloc(size_t size) {
    if (size == 0) return NULL;

    size = (size + 15) & ~15; /* 16-byte alignment */

    struct user_block *curr = user_heap_head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            curr->is_free = 0;
            return (void *)((uintptr_t)curr + sizeof(struct user_block));
        }
        if (!curr->next) break;
        curr = curr->next;
    }

    /* Allocate from kernel via sbrk */
    size_t total_alloc = size + sizeof(struct user_block);
    struct user_block *new_block = (struct user_block *)sbrk((intptr_t)total_alloc);
    if (new_block == (void *)-1) {
        return NULL;
    }

    new_block->size = size;
    new_block->is_free = 0;
    new_block->prev = curr;
    new_block->next = NULL;

    if (curr) {
        curr->next = new_block;
    } else {
        user_heap_head = new_block;
    }

    return (void *)((uintptr_t)new_block + sizeof(struct user_block));
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void free(void *ptr) {
    if (!ptr) return;

    struct user_block *block = (struct user_block *)((uintptr_t)ptr - sizeof(struct user_block));
    block->is_free = 1;

    /* Coalesce next */
    if (block->next && block->next->is_free) {
        block->size += sizeof(struct user_block) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    /* Coalesce prev */
    if (block->prev && block->prev->is_free) {
        block->prev->size += sizeof(struct user_block) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    struct user_block *block = (struct user_block *)((uintptr_t)ptr - sizeof(struct user_block));
    if (block->size >= size) {
        return ptr;
    }

    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        free(ptr);
    }
    return new_ptr;
}

#define MAX_ATEXIT 32
static void (*atexit_funcs[MAX_ATEXIT])(void);
static int atexit_count = 0;

int atexit(void (*func)(void)) {
    if (!func || atexit_count >= MAX_ATEXIT) return -1;
    atexit_funcs[atexit_count++] = func;
    return 0;
}

int abs(int j) {
    return j < 0 ? -j : j;
}

void exit(int status) {
    for (int i = atexit_count - 1; i >= 0; i--) {
        if (atexit_funcs[i]) {
            atexit_funcs[i]();
        }
    }
    __syscall(SYS_exit, (uint64_t)status);
    for (;;) { }
}

void abort(void) {
    kill(getpid(), SIGABRT);
    exit(134);
}

int atoi(const char *nptr) {
    while (isspace((unsigned char)*nptr)) nptr++;

    int sign = 1;
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    int res = 0;
    while (isdigit((unsigned char)*nptr)) {
        res = res * 10 + (*nptr - '0');
        nptr++;
    }
    return sign * res;
}

long atol(const char *nptr) {
    return strtol(nptr, NULL, 10);
}

double atof(const char *nptr) {
    while (isspace((unsigned char)*nptr)) nptr++;

    double sign = 1.0;
    if (*nptr == '-') {
        sign = -1.0;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    double res = 0.0;
    while (isdigit((unsigned char)*nptr)) {
        res = res * 10.0 + (*nptr - '0');
        nptr++;
    }

    if (*nptr == '.') {
        nptr++;
        double factor = 0.1;
        while (isdigit((unsigned char)*nptr)) {
            res += (*nptr - '0') * factor;
            factor *= 0.1;
            nptr++;
        }
    }

    return sign * res;
}

long strtol(const char *nptr, char **endptr, int base) {
    while (isspace((unsigned char)*nptr)) nptr++;

    int sign = 1;
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    if (base == 0) {
        if (*nptr == '0') {
            if (nptr[1] == 'x' || nptr[1] == 'X') {
                base = 16;
                nptr += 2;
            } else {
                base = 8;
                nptr++;
            }
        } else {
            base = 10;
        }
    }

    long val = 0;
    while (*nptr) {
        int digit;
        if (isdigit((unsigned char)*nptr)) {
            digit = *nptr - '0';
        } else if (isalpha((unsigned char)*nptr)) {
            digit = toupper((unsigned char)*nptr) - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;
        val = val * base + digit;
        nptr++;
    }

    if (endptr) *endptr = (char *)nptr;
    return sign * val;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    while (isspace((unsigned char)*nptr)) nptr++;

    if (*nptr == '+') {
        nptr++;
    }

    if (base == 0) {
        if (*nptr == '0') {
            if (nptr[1] == 'x' || nptr[1] == 'X') {
                base = 16;
                nptr += 2;
            } else {
                base = 8;
                nptr++;
            }
        } else {
            base = 10;
        }
    }

    unsigned long val = 0;
    while (*nptr) {
        int digit;
        if (isdigit((unsigned char)*nptr)) {
            digit = *nptr - '0';
        } else if (isalpha((unsigned char)*nptr)) {
            digit = toupper((unsigned char)*nptr) - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;
        val = val * base + digit;
        nptr++;
    }

    if (endptr) *endptr = (char *)nptr;
    return val;
}

char *getenv(const char *name) {
    if (!name || !environ) return NULL;
    size_t len = strlen(name);
    for (char **ep = environ; *ep; ep++) {
        if (strncmp(*ep, name, len) == 0 && (*ep)[len] == '=') {
            return *ep + len + 1;
        }
    }
    return NULL;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (!name || name[0] == '\0' || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    size_t name_len = strlen(name);
    size_t val_len = value ? strlen(value) : 0;

    /* Check if variable already exists */
    if (environ) {
        for (char **ep = environ; *ep; ep++) {
            if (strncmp(*ep, name, name_len) == 0 && (*ep)[name_len] == '=') {
                if (!overwrite) return 0;

                char *new_entry = (char *)malloc(name_len + 1 + val_len + 1);
                if (!new_entry) {
                    errno = ENOMEM;
                    return -1;
                }
                snprintf(new_entry, name_len + 1 + val_len + 1, "%s=%s", name, value ? value : "");
                *ep = new_entry;
                return 0;
            }
        }
    }

    char *new_entry = (char *)malloc(name_len + 1 + val_len + 1);
    if (!new_entry) {
        errno = ENOMEM;
        return -1;
    }
    snprintf(new_entry, name_len + 1 + val_len + 1, "%s=%s", name, value ? value : "");

    size_t count = 0;
    if (environ) {
        while (environ[count]) count++;
    }

    char **new_environ = (char **)malloc((count + 2) * sizeof(char *));
    if (!new_environ) {
        free(new_entry);
        errno = ENOMEM;
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        new_environ[i] = environ[i];
    }
    new_environ[count] = new_entry;
    new_environ[count + 1] = NULL;
    environ = new_environ;
    return 0;
}

int unsetenv(const char *name) {
    if (!name || name[0] == '\0' || strchr(name, '=') || !environ) {
        errno = EINVAL;
        return -1;
    }

    size_t len = strlen(name);
    char **ep = environ;
    while (*ep) {
        if (strncmp(*ep, name, len) == 0 && (*ep)[len] == '=') {
            char **dp = ep;
            while (*dp) {
                *dp = *(dp + 1);
                dp++;
            }
        } else {
            ep++;
        }
    }
    return 0;
}

static void swap_bytes(char *a, char *b, size_t size) {
    while (size--) {
        char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (!base || nmemb <= 1 || size == 0 || !compar) return;

    char *arr = (char *)base;
    size_t i = 0;
    size_t j = nmemb - 1;
    char *pivot = arr + (nmemb / 2) * size;

    while (i <= j) {
        while (compar(arr + i * size, pivot) < 0) {
            i++;
        }
        while (compar(arr + j * size, pivot) > 0) {
            if (j == 0) break;
            j--;
        }
        if (i <= j) {
            if (i != j) {
                swap_bytes(arr + i * size, arr + j * size, size);
                if (pivot == arr + i * size) pivot = arr + j * size;
                else if (pivot == arr + j * size) pivot = arr + i * size;
            }
            i++;
            if (j == 0) break;
            j--;
        }
    }

    if (j > 0) {
        qsort(arr, j + 1, size, compar);
    }
    if (i < nmemb) {
        qsort(arr + i * size, nmemb - i, size, compar);
    }
}

static unsigned long int next_rand = 1;

int rand(void) {
    next_rand = next_rand * 1103515245ULL + 12345ULL;
    return (int)((next_rand / 65536ULL) % 32768ULL);
}

void srand(unsigned int seed) {
    next_rand = seed;
}


