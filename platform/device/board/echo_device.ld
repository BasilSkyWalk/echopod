ENTRY(device_reset)

MEMORY
{
    DATA_RAM (rw) : ORIGIN = 0x03004000, LENGTH = 0x48000
    CODE_RAM (rx) : ORIGIN = 0x03050000, LENGTH = 0x2666C
}

SECTIONS
{
    .code :
    {
        KEEP(*(.code_vectors))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > CODE_RAM

    .data :
    {
        KEEP(*(.entry_vector))
        *(.data .data.*)
        . = ALIGN(4);
    } > DATA_RAM AT > CODE_RAM

    .bss (NOLOAD) :
    {
        device_bss_start = .;
        *(.bss .bss.*)
        . = ALIGN(4);
        device_bss_end = .;
    } > DATA_RAM

    .comment 0 : { *(.comment) }
    .ARM.attributes 0 : { *(.ARM.attributes) }

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}

ASSERT(ADDR(.code) == 0x03050000, "the code vector table must be at 0x03050000")
ASSERT(ADDR(.data) == 0x03004000, "the entry vector must be at 0x03004000")
ASSERT(SIZEOF(.data) == 8, "only the entry vector may be in .data: make initialised globals const")
ASSERT(device_bss_end <= 0x0304C000, ".data and .bss must end below 0x0304C000")
