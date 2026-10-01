#ifndef _ARCH_X86_64_CPUID_H
#define _ARCH_X86_64_CPUID_H

#include <dunix/types.h>
#include <dunix/stdbool.h>

struct cpu_info {
    char     vendor[13];
    char     brand[49];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    bool     has_fpu;
    bool     has_apic;
    bool     has_sse;
    bool     has_sse2;
    bool     has_sse3;
    bool     has_ssse3;
    bool     has_sse4_1;
    bool     has_sse4_2;
    bool     has_avx;
    bool     has_rdrand;
    bool     has_long_mode;
    bool     has_nx;
    bool     has_1gb_pages;
    bool     has_syscall;
    bool     has_x2apic;
};

static inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf));
}

void cpu_detect(struct cpu_info *info);
void cpu_print_info(const struct cpu_info *info);
void fpu_init(void);

#endif /* _ARCH_X86_64_CPUID_H */
