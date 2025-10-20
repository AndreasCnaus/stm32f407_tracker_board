/* Specifying the firmware's entry point */
ENTRY(Reset_Handler)

/* Detailing the available memory */
MEMORY
{
    FLASH(rx) : ORIGIN = 0x8000000, LENGTH = 512K
    SRAM(rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Specifying the necessary heap and stack sizes */
__max_heap_size  = 0x1000; /* 4 KB heap */
__max_stack_size = 0x4000; /* 16 KB stack */

/* Defining the output sections */
SECTIONS
{
    /* Vector table, code and constant data section */
    .text : {
        . = ALIGN(4);
        KEEP(*(.isr_vector_tbl))
        *(.text*)
        *(.rodata*)

        /* Exception index table */
       __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
        
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .init_array : {
    . = ALIGN(4);
    PROVIDE(__init_array_start = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE(__init_array_end = .);
    } >FLASH

    .data : AT (ADDR(.text) + SIZEOF(.text) + SIZEOF(.init_array)) {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM
    _sidata = LOADADDR(.data);

    /* Uninitialized data (.bss + small bss + COMMON) */
    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
        PROVIDE(end = .);
    } > SRAM

    /* Heap section */
    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __heap_start__ = .;
        . = . + __max_heap_size;
        . = ALIGN(8);
        __heap_end__ = .;
    } > SRAM

    /* Stack section (starts at top of SRAM and grows downwards) */
    .stack (NOLOAD) : {
        . = ORIGIN(SRAM) + LENGTH(SRAM) - __max_stack_size;
        . = ALIGN(8);
        __stack_start__ = .;
        . = . + __max_stack_size;
        . = ALIGN(8);
        __stack_end__ = .;
    } > SRAM
}

/* Define the symbols for C code */
PROVIDE(_estack = ORIGIN(SRAM) + LENGTH(SRAM));



