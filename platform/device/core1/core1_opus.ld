ENTRY(core1_reset)

MEMORY
{
    CORE1_CODE (rx) : ORIGIN = 0x00000000, LENGTH = 0x20000
    CORE1_DATA (rw) : ORIGIN = 0x01021000, LENGTH = 0xC000
}

SECTIONS
{
    .text :
    {
        KEEP(*(.core1_vectors))
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
        . = ALIGN(8);
    } > CORE1_CODE

    .core1_big 0x01000000 + SIZEOF(.text) (NOLOAD) :
    {
        core1_big_start = .;
        *(.core1_big)
        . = ALIGN(8);
        core1_big_end = .;
    }

    .data :
    {
        *(.data .data.*)
        . = ALIGN(4);
    } > CORE1_DATA AT > CORE1_CODE

    .bss (NOLOAD) :
    {
        core1_bss_start = .;
        *(.bss .bss.*)
        . = ALIGN(4);
        core1_bss_end = .;
    } > CORE1_DATA

    .comment 0 : { *(.comment) }
    .ARM.attributes 0 : { *(.ARM.attributes) }

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}

ASSERT(ADDR(.text) == 0, "core 1 code must start at its address 0")
ASSERT(SIZEOF(.data) == 0, "core 1 has no initialised data: make globals const")
ASSERT(core1_big_end <= 0x01020000, "the Opus buffers must end below the shared block at 0x01020000")
ASSERT(core1_bss_end <= 0x01022000, "the Opus program needs 48 KB of stack below 0x0102E000")
