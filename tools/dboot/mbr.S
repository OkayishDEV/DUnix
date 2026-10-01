.code16
.section .text
.global _start

_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp
    sti

    /* Save boot drive from DL */
    movb %dl, (boot_drive)

    /* Print loading message */
    movw $msg_loading, %si
    call print_string

    /* Read Stage 2 (LBA 1..63) to 0x0000:0x8000 via INT 13h AH=42h */
    movw $dap, %si
    movb (boot_drive), %dl
    movb $0x42, %ah
    int $0x13
    jc .read_error

    /* Far jump to Stage 2 with DL = boot drive */
    movb (boot_drive), %dl
    ljmp $0x0000, $0x8000

.read_error:
    movw $msg_error, %si
    call print_string
.halt:
    hlt
    jmp .halt

print_string:
    lodsb
    testb %al, %al
    jz .done
    movb $0x0E, %ah
    movw $0x0007, %bx
    int $0x10
    jmp print_string
.done:
    ret

.align 4
dap:
    .byte 0x10          /* DAP size = 16 */
    .byte 0             /* reserved */
    .word 63            /* sector count = 63 */
    .word 0x8000        /* buffer offset */
    .word 0x0000        /* buffer segment */
    .long 1             /* starting LBA = 1 */
    .long 0

boot_drive:
    .byte 0x80

msg_loading:
    .asciz "Loading DUnix (dboot)...\r\n"
msg_error:
    .asciz "Boot error: MBR cannot read stage2!\r\n"

/* MBR Partition Table at offset 446 (0x1BE) */
. = _start + 446
partition_table:
    .byte 0x80                  /* Partition 1: Bootable / Active */
    .byte 0x00, 0x02, 0x00      /* CHS start */
    .byte 0x83                  /* Type: Linux Native / Ext2 */
    .byte 0xFE, 0xFF, 0xFF      /* CHS end (LBA) */
    .long 8192                  /* Starting LBA: Sector 8192 (4 MB offset) */
    .long 0                     /* Sector count: set dynamically by installer */

    /* Partitions 2, 3, 4 empty */
    .space 48, 0

/* Boot Signature at offset 510 */
. = _start + 510
    .word 0xAA55
