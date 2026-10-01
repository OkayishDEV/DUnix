.code16
.section .text
.global _start

_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x8000, %sp
    sti

    movb %dl, (boot_drive)

    /* Print progress banner */
    movw $msg_stage2, %si
    call print_str

    /* Step 1: Detect Memory via BIOS INT 15h E820 into 0x9000 */
    movw $0x9000, %di
    xorl %ebx, %ebx
    xorw %bp, %bp
.e820_loop:
    movl $20, (%di)             /* Multiboot entry size = 20 */
    addw $4, %di
    movl $0x534D4150, %edx      /* 'SMAP' */
    movl $0xE820, %eax
    movl $20, %ecx
    int $0x15
    jc .e820_done
    cmpl $0x534D4150, %eax
    jne .e820_done
    incw %bp
    addw $20, %di
    testl %ebx, %ebx
    jnz .e820_loop
.e820_done:
    movw %bp, (mmap_count)

    /* Step 2: Enable A20 Gate */
    movw $0x2401, %ax
    int $0x15
    inb $0x92, %al
    orb $2, %al
    andb $0xFE, %al
    outb %al, $0x92

    /* Step 3: Enter Unreal Mode for 4GB data access on FS */
    cli
    lgdtl (gdt16_ptr)
    movl %cr0, %eax
    orb $1, %al
    movl %eax, %cr0
    movw $0x10, %bx
    movw %bx, %fs
    andb $0xFE, %al
    movl %eax, %cr0
    sti

    /* Step 4: Read kernel sectors starting at LBA 64 to 0x100000 (1 MB) */
    movl $0x100000, %edi       /* 1 MB destination */
    movl (kernel_sectors), %ecx
    movl $64, (dap_lba)
    movl $0, (dap_lba + 4)

.chunk_loop:
    testl %ecx, %ecx
    jz .kernel_loaded

    movl %ecx, %eax
    cmpl $64, %eax
    jbe .last_chunk
    movl $64, %eax
.last_chunk:
    movw %ax, (dap_count)
    subl %eax, %ecx
    pushl %ecx
    pushl %eax

    xorw %ax, %ax
    movw %ax, %ds
    movw $dap, %si
    movb (boot_drive), %dl
    movb $0x42, %ah
    int $0x13
    jc .disk_error

    popl %eax
    pushl %eax
    shll $7, %eax              /* sectors * 512 / 4 = sectors * 128 dwords */

    /* Copy from bounce buffer at 0x2000:0x0000 to FS:(EDI) */
    movw $0x2000, %bx
    movw %bx, %ds
    xorl %esi, %esi
.copy_dw:
    movl (%esi), %edx
    movl %edx, %fs:(%edi)
    addl $4, %esi
    addl $4, %edi
    decl %eax
    jnz .copy_dw

    xorw %ax, %ax
    movw %ax, %ds

    popl %eax
    addl %eax, (dap_lba)
    adcl $0, (dap_lba + 4)
    popl %ecx
    jmp .chunk_loop

.kernel_loaded:
    /* Step 5: Setup Multiboot Info structure at 0x9800 */
    movw $0x9800, %di
    xorw %ax, %ax
    movw $64, %cx
    rep stosw

    /* MBI Flags: 0x47 = MEMINFO(1) | BOOTDEV(2) | CMDLINE(4) | MMAP(0x40) */
    movl $0x00000047, 0x9800
    movl $640, 0x9804          /* mem_lower = 640 KB */
    movl $262144, 0x9808       /* mem_upper = 256 MB */
    movl $0x80000000, 0x980C   /* boot_device = 0x80 */
    movl $0x9900, 0x9810       /* cmdline pointer */

    /* Copy cmdline to 0x9900 */
    movw $cmdline_str, %si
    movw $0x9900, %di
.copy_cmd:
    lodsb
    stosb
    testb %al, %al
    jnz .copy_cmd

    /* mmap_length & mmap_addr */
    movzwl (mmap_count), %eax
    imull $24, %eax, %eax
    movl %eax, 0x982C          /* mbi.mmap_length */
    movl $0x9000, 0x9830       /* mbi.mmap_addr */

    /* Copy bootloader name to 0x9940 */
    movw $bootloader_name, %si
    movw $0x9940, %di
.copy_bname:
    lodsb
    stosb
    testb %al, %al
    jnz .copy_bname
    movl $0x9940, 0x9840       /* mbi.boot_loader_name */

    /* Step 6: Transition to 32-bit Protected Mode */
    cli
    lgdtl (gdt32_ptr)
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $.pm32

.code32
.pm32:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movl $0x7C00, %esp

    /* Multiboot 1 registers */
    movl $0x2BADB002, %eax     /* MULTIBOOT_BOOTLOADER_MAGIC */
    movl $0x9800, %ebx         /* multiboot info pointer */

    /* Jump to DUnix kernel entry point at 1 MB (_start32) */
    movl $0x100028, %ecx
    jmp *%ecx

.code16
.disk_error:
    movw $msg_disk_err, %si
    call print_str
.halt:
    hlt
    jmp .halt

print_str:
    lodsb
    testb %al, %al
    jz .pdone
    movb $0x0E, %ah
    movw $0x0007, %bx
    int $0x10
    jmp print_str
.pdone:
    ret

.align 4
dap:
    .byte 0x10
    .byte 0
dap_count:
    .word 64
    .word 0x0000
    .word 0x2000               /* buffer 0x2000:0x0000 = 0x20000 */
dap_lba:
    .long 64
    .long 0

boot_drive:
    .byte 0x80
mmap_count:
    .word 0
kernel_sectors:
    .long 4096                 /* Default 4096 sectors = 2 MB */

cmdline_str:
    .asciz "root=/dev/sda1 rw"
bootloader_name:
    .asciz "DUnix dboot 1.0"
msg_stage2:
    .asciz "Starting DUnix kernel...\r\n"
msg_disk_err:
    .asciz "Stage 2 error: failed to read kernel!\r\n"

/* 16-bit Unreal Mode GDT */
.align 8
gdt16:
    .quad 0x0000000000000000   /* Null */
    .quad 0x00009A000000FFFF   /* 16-bit code */
    .quad 0x00CF92000000FFFF   /* Flat 4GB data */
gdt16_ptr:
    .word . - gdt16 - 1
    .long gdt16

/* 32-bit Protected Mode GDT */
.align 8
gdt32:
    .quad 0x0000000000000000   /* Null */
    .quad 0x00CF9A000000FFFF   /* 32-bit 4GB code */
    .quad 0x00CF92000000FFFF   /* 32-bit 4GB data */
gdt32_ptr:
    .word . - gdt32 - 1
    .long gdt32
