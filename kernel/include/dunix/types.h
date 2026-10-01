#ifndef _DUNIX_TYPES_H
#define _DUNIX_TYPES_H

#include <dunix/stdbool.h>

/* Exact-width integer types for x86_64 */
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

/* Minimum-width integer types */
typedef int8_t             int_least8_t;
typedef int16_t            int_least16_t;
typedef int32_t            int_least32_t;
typedef int64_t            int_least64_t;
typedef uint8_t            uint_least8_t;
typedef uint16_t           uint_least16_t;
typedef uint32_t           uint_least32_t;
typedef uint64_t           uint_least64_t;

/* Fastest minimum-width integer types */
typedef int8_t             int_fast8_t;
typedef int64_t            int_fast16_t;
typedef int64_t            int_fast32_t;
typedef int64_t            int_fast64_t;
typedef uint8_t            uint_fast8_t;
typedef uint64_t           uint_fast16_t;
typedef uint64_t           uint_fast32_t;
typedef uint64_t           uint_fast64_t;

/* Pointer types and sizes */
typedef unsigned long      size_t;
typedef long               ssize_t;
typedef unsigned long      uintptr_t;
typedef long               intptr_t;
typedef long               ptrdiff_t;

/* POSIX / Unix kernel types */
typedef int64_t            off_t;
typedef int32_t            pid_t;
typedef uint32_t           uid_t;
typedef uint32_t           gid_t;
typedef uint32_t           mode_t;
typedef uint64_t           dev_t;
typedef uint64_t           ino_t;
typedef uint32_t           nlink_t;
typedef int64_t            time_t;
typedef int64_t            suseconds_t;
typedef int32_t            clockid_t;
typedef int32_t            id_t;

/* Physical and virtual addresses */
typedef uint64_t           paddr_t;
typedef uint64_t           vaddr_t;

/* Integer limits */
#define INT8_MIN    (-128)
#define INT8_MAX    127
#define UINT8_MAX   0xFFU
#define INT16_MIN   (-32768)
#define INT16_MAX   32767
#define UINT16_MAX  0xFFFFU
#define INT32_MIN   (-2147483647 - 1)
#define INT32_MAX   2147483647
#define UINT32_MAX  0xFFFFFFFFU
#define INT64_MIN   (-9223372036854775807LL - 1)
#define INT64_MAX   9223372036854775807LL
#define UINT64_MAX  0xFFFFFFFFFFFFFFFFULL

#endif /* _DUNIX_TYPES_H */
