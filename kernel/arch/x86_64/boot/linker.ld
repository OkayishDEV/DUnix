ENTRY(_start32)

SECTIONS
{
    /* Physical entry at 1 MB */
    . = 0x100000;
    _kernel_phys_start = .;

    /* Early bootstrap code and data (executed before long mode and high-half jump) */
    .boot ALIGN(4K) :
    {
        KEEP(*(.multiboot))
        *(.boot.text)
        *(.boot.data)
        _boot_bss_start = .;
        *(.boot.bss)
        _boot_bss_end = .;
    }

    /* Higher-half virtual base at -2 GB (0xFFFFFFFF80000000) */
    . += 0xFFFFFFFF80000000;
    _kernel_virt_start = .;
    _kernel_start = .;

    .text ALIGN(4K) : AT(ADDR(.text) - 0xFFFFFFFF80000000)
    {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xFFFFFFFF80000000)
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - 0xFFFFFFFF80000000)
    {
        _data_start = .;
        *(.data .data.*)
        _data_end = .;
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xFFFFFFFF80000000)
    {
        _bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(8);
        _bss_end = .;
    }

    . = ALIGN(4K);
    _kernel_virt_end = .;
    _kernel_end = .;
    _kernel_phys_end = . - 0xFFFFFFFF80000000;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame)
    }
}
