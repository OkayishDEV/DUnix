#ifndef _DUNIX_KPRINTF_H
#define _DUNIX_KPRINTF_H

#include <dunix/types.h>
#include <dunix/stddef.h>
#include <dunix/stdarg.h>

/* Kernel Log Levels */
#define KLOG_DEBUG 0
#define KLOG_INFO  1
#define KLOG_WARN  2
#define KLOG_ERROR 3
#define KLOG_PANIC 4

int  kprintf(const char *fmt, ...);
int  kvprintf(const char *fmt, va_list ap);
int  ksnprintf(char *buf, size_t size, const char *fmt, ...);
int  kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

void klog(int level, const char *fmt, ...);
__attribute__((noreturn)) void kpanic(const char *fmt, ...);

#endif /* _DUNIX_KPRINTF_H */
