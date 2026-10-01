#include <arch/x86_64/cpu/cpuid.h>
#include <arch/x86_64/io.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

void cpu_detect(struct cpu_info *info) {
    memset(info, 0, sizeof(struct cpu_info));

    uint32_t eax, ebx, ecx, edx;

    /* Leaf 0: Vendor String */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    *(uint32_t *)&info->vendor[0] = ebx;
    *(uint32_t *)&info->vendor[4] = edx;
    *(uint32_t *)&info->vendor[8] = ecx;
    info->vendor[12] = '\0';

    uint32_t max_leaf = eax;

    /* Leaf 1: Family, Model, Stepping & Features */
    if (max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        info->stepping = eax & 0x0F;
        info->model    = (eax >> 4) & 0x0F;
        info->family   = (eax >> 8) & 0x0F;

        if (info->family == 0x0F) {
            info->family += (eax >> 20) & 0xFF;
        }
        if (info->family == 0x06 || info->family == 0x0F) {
            info->model += ((eax >> 16) & 0x0F) << 4;
        }

        info->has_fpu    = (edx & (1 << 0)) != 0;
        info->has_apic   = (edx & (1 << 9)) != 0;
        info->has_sse    = (edx & (1 << 25)) != 0;
        info->has_sse2   = (edx & (1 << 26)) != 0;
        info->has_sse3   = (ecx & (1 << 0)) != 0;
        info->has_ssse3  = (ecx & (1 << 9)) != 0;
        info->has_sse4_1 = (ecx & (1 << 19)) != 0;
        info->has_sse4_2 = (ecx & (1 << 20)) != 0;
        info->has_x2apic = (ecx & (1 << 21)) != 0;
        info->has_avx    = (ecx & (1 << 28)) != 0;
        info->has_rdrand = (ecx & (1 << 30)) != 0;
    }

    /* Extended Leaf 0x80000000 */
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_ext_leaf = eax;

    /* Extended Leaf 0x80000001: Long Mode, NX, Syscall */
    if (max_ext_leaf >= 0x80000001) {
        cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
        info->has_syscall   = (edx & (1 << 11)) != 0;
        info->has_nx        = (edx & (1 << 20)) != 0;
        info->has_1gb_pages = (edx & (1 << 26)) != 0;
        info->has_long_mode = (edx & (1 << 29)) != 0;
    }

    /* Extended Leaves 0x80000002..0x80000004: Brand String */
    if (max_ext_leaf >= 0x80000004) {
        uint32_t *brand_ptr = (uint32_t *)info->brand;
        cpuid(0x80000002, 0, &brand_ptr[0], &brand_ptr[1], &brand_ptr[2], &brand_ptr[3]);
        cpuid(0x80000003, 0, &brand_ptr[4], &brand_ptr[5], &brand_ptr[6], &brand_ptr[7]);
        cpuid(0x80000004, 0, &brand_ptr[8], &brand_ptr[9], &brand_ptr[10], &brand_ptr[11]);
        info->brand[48] = '\0';
    }
}

void cpu_print_info(const struct cpu_info *info) {
    klog(KLOG_INFO, "CPU Vendor: %s\n", info->vendor);
    if (info->brand[0]) {
        klog(KLOG_INFO, "CPU Model:  %s\n", info->brand);
    }
    klog(KLOG_INFO, "CPU Family: 0x%x, Model: 0x%x, Stepping: 0x%x\n",
         info->family, info->model, info->stepping);
    klog(KLOG_INFO, "Features:   %s%s%s%s%s%s%s%s%s%s\n",
         info->has_long_mode ? "LM " : "",
         info->has_syscall ? "SYSCALL " : "",
         info->has_nx ? "NX " : "",
         info->has_1gb_pages ? "1GB_PAGES " : "",
         info->has_apic ? "APIC " : "",
         info->has_x2apic ? "x2APIC " : "",
         info->has_sse ? "SSE " : "",
         info->has_sse2 ? "SSE2 " : "",
         info->has_sse4_2 ? "SSE4.2 " : "",
         info->has_avx ? "AVX " : "");
}

void fpu_init(void) {
    /* 1. Configure CR0: Clear EM (bit 2), Set MP (bit 1), Set NE (bit 5) */
    uint64_t cr0 = read_cr0();
    cr0 &= ~(1ULL << 2); /* Clear EM: No x87 emulation */
    cr0 |= (1ULL << 1);  /* Set MP: Monitor coprocessor */
    cr0 |= (1ULL << 5);  /* Set NE: Native x87 FPU errors */
    write_cr0(cr0);

    /* 2. Configure CR4: Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10) for SSE support */
    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 9);  /* OSFXSR: Enable FXSAVE/FXRSTOR and SSE instructions */
    cr4 |= (1ULL << 10); /* OSXMMEXCPT: Enable unmasked SSE exceptions */
    write_cr4(cr4);

    /* 3. Initialize FPU state */
    __asm__ volatile("fninit");

    klog(KLOG_INFO, "FPU and SSE/SSE2 instruction extensions initialized\n");
}
