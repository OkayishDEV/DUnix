#ifndef _DUNIX_KHEAP_H
#define _DUNIX_KHEAP_H

#include <dunix/types.h>
#include <dunix/stddef.h>

void *kmalloc(size_t size);
void *kzalloc(size_t size);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t new_size);
void  kfree(void *ptr);

void  kheap_init(void);
size_t kheap_get_used_bytes(void);
size_t kheap_get_free_bytes(void);

#endif /* _DUNIX_KHEAP_H */
