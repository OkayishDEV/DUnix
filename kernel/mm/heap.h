#ifndef _MM_HEAP_H
#define _MM_HEAP_H

#include <dunix/kheap.h>
#include <dunix/stdbool.h>

#define KHEAP_START_VADDR 0xFFFFFFFF90000000ULL
#define KHEAP_INITIAL_SIZE (4 * 1024 * 1024ULL) /* 4 MB initial heap */
#define KHEAP_MAX_SIZE     (256 * 1024 * 1024ULL) /* 256 MB max heap */

#define HEAP_BLOCK_MAGIC  0xDEADBEEFCAFEBABEULL
#define HEAP_ALIGNMENT    16

struct heap_block {
    uint64_t           magic;
    size_t             size;       /* Data payload size */
    bool               is_free;
    struct heap_block *prev;
    struct heap_block *next;
} __attribute__((aligned(16)));

#endif /* _MM_HEAP_H */
