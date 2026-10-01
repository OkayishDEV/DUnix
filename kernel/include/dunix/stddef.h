#ifndef _DUNIX_STDDEF_H
#define _DUNIX_STDDEF_H

#include <dunix/types.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define ALIGN_UP(addr, align) (((addr) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))
#define IS_ALIGNED(addr, align) (((addr) & ((align) - 1)) == 0)

#endif /* _DUNIX_STDDEF_H */
