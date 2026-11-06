ENTRY(core1_reset)

MEMORY
{
    CORE1_CODE (rx) : ORIGIN = 0x00000000, LENGTH = 0x10000
}

SECTIONS
{
    .text :
    {
        KEEP(*(.core1_vectors))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(8);
    } > CORE1_CODE

    .data :
    {
        *(.data .data.*)
    } > CORE1_CODE

    .bss 0x01000000 + SIZEOF(.text) (NOLOAD) :
    {
        core1_bss_start = .;
        *(.bss .bss.*)
        . = ALIGN(4);
        core1_bss_end = .;
    }

    .comment 0 : { *(.comment) }
    .ARM.attributes 0 : { *(.ARM.attributes) }

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}

core1_stack_top = 0x01010000;

ASSERT(ADDR(.text) == 0, "core 1 code must start at its address 0")
ASSERT(SIZEOF(.data) == 0, "core 1 has no initialised data: make globals const")
ASSERT(core1_bss_end <= 0x0100E000, "the FLAC program keeps 8 KB of stack below its first bank at 0x01010000")
