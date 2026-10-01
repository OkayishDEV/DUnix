#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct heap_block *heap_head = NULL;
static vaddr_t heap_current_end = KHEAP_START_VADDR;
static size_t  heap_used_bytes = 0;
static size_t  heap_allocated_capacity = 0;

static bool heap_expand(size_t bytes) {
    size_t pages = (bytes + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    uint64_t *kernel_pml4 = vmm_get_kernel_pml4();

    for (size_t i = 0; i < pages; i++) {
        paddr_t frame = pmm_alloc_frame();
        if (!frame) {
            return false;
        }

        if (!vmm_map_page(kernel_pml4, heap_current_end, frame, VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_GLOBAL)) {
            pmm_free_frame(frame);
            return false;
        }

        heap_current_end += PAGE_SIZE_4K;
        heap_allocated_capacity += PAGE_SIZE_4K;
    }

    return true;
}

void kheap_init(void) {
    heap_head = (struct heap_block *)KHEAP_START_VADDR;
    heap_current_end = KHEAP_START_VADDR;
    heap_used_bytes = 0;
    heap_allocated_capacity = 0;

    if (!heap_expand(KHEAP_INITIAL_SIZE)) {
        kpanic("KHEAP: Failed to initialize kernel heap!\n");
    }

    heap_head->magic = HEAP_BLOCK_MAGIC;
    heap_head->size = KHEAP_INITIAL_SIZE - sizeof(struct heap_block);
    heap_head->is_free = true;
    heap_head->prev = NULL;
    heap_head->next = NULL;

    klog(KLOG_INFO, "Kernel Heap initialized: %lu MB at 0x%016lx\n",
         KHEAP_INITIAL_SIZE / (1024 * 1024), KHEAP_START_VADDR);
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    /* Align size to 16 bytes */
    size = ALIGN_UP(size, HEAP_ALIGNMENT);

    struct heap_block *curr = heap_head;
    while (curr) {
        if (curr->magic != HEAP_BLOCK_MAGIC) {
            kpanic("KHEAP: Heap corruption detected at block 0x%016lx (magic: 0x%016lx)\n",
                   (uint64_t)curr, curr->magic);
        }

        if (curr->is_free && curr->size >= size) {
            /* Can we split this block? */
            if (curr->size >= size + sizeof(struct heap_block) + HEAP_ALIGNMENT) {
                struct heap_block *new_block = (struct heap_block *)((uintptr_t)curr + sizeof(struct heap_block) + size);
                new_block->magic = HEAP_BLOCK_MAGIC;
                new_block->size = curr->size - size - sizeof(struct heap_block);
                new_block->is_free = true;
                new_block->prev = curr;
                new_block->next = curr->next;

                if (curr->next) {
                    curr->next->prev = new_block;
                }
                curr->next = new_block;
                curr->size = size;
            }

            curr->is_free = false;
            heap_used_bytes += curr->size;
            return (void *)((uintptr_t)curr + sizeof(struct heap_block));
        }

        if (!curr->next) {
            break;
        }
        curr = curr->next;
    }

    /* No suitable block found, expand heap */
    size_t needed = size + sizeof(struct heap_block);
    size_t expand_size = ALIGN_UP(needed, PAGE_SIZE_4K * 16); /* Expand at least 64KB */

    vaddr_t old_end = heap_current_end;
    if (!heap_expand(expand_size)) {
        klog(KLOG_ERROR, "KHEAP: Out of memory trying to allocate %lu bytes\n", size);
        return NULL;
    }

    /* Create new free block in expanded area */
    struct heap_block *expanded_block = (struct heap_block *)old_end;
    expanded_block->magic = HEAP_BLOCK_MAGIC;
    expanded_block->size = expand_size - sizeof(struct heap_block);
    expanded_block->is_free = true;
    expanded_block->prev = curr;
    expanded_block->next = NULL;
    if (curr) {
        curr->next = expanded_block;
    }

    return kmalloc(size);
}

void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void *kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    return kzalloc(total);
}

void kfree(void *ptr) {
    if (!ptr) {
        return;
    }

    struct heap_block *block = (struct heap_block *)((uintptr_t)ptr - sizeof(struct heap_block));
    if (block->magic != HEAP_BLOCK_MAGIC) {
        kpanic("KHEAP: Invalid pointer passed to kfree: 0x%016lx (magic: 0x%016lx)\n",
               (uint64_t)ptr, block->magic);
    }

    block->is_free = true;
    if (heap_used_bytes >= block->size) {
        heap_used_bytes -= block->size;
    }

    /* Coalesce with next block if free */
    if (block->next && block->next->is_free) {
        block->size += sizeof(struct heap_block) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    /* Coalesce with previous block if free */
    if (block->prev && block->prev->is_free) {
        block->prev->size += sizeof(struct heap_block) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    struct heap_block *block = (struct heap_block *)((uintptr_t)ptr - sizeof(struct heap_block));
    if (block->magic != HEAP_BLOCK_MAGIC) {
        kpanic("KHEAP: Invalid pointer in krealloc\n");
    }

    if (block->size >= new_size) {
        return ptr;
    }

    void *new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}

size_t kheap_get_used_bytes(void) {
    return heap_used_bytes;
}

size_t kheap_get_free_bytes(void) {
    return (heap_allocated_capacity > heap_used_bytes) ? (heap_allocated_capacity - heap_used_bytes) : 0;
}
