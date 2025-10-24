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

    /* Stack section outside region (no overlap in accounting) */
    .stack (NOLOAD) : {
        __stack_end__ = ORIGIN(SRAM) + LENGTH(SRAM);
        __stack_start__ = __stack_end__ - __max_stack_size;
    } > NONE
}

/* Define the symbols for C code */
PROVIDE(_estack = ORIGIN(SRAM) + LENGTH(SRAM));


/*
 Address ↑  (higher addresses)
───────────────────────────────────────────────────────────────
0x20020000  <-- __stack_end__ / _estack
               (Top of SRAM / Initial SP value)
               • Stack pointer starts here at reset
               • Stack grows downward (uninitialized)

      ↓ Stack (Runtime)
      ↓ Used for: function calls, local variables, interrupt frames
      ↓ Uninitialized, volatile at runtime

0x2001C000  <-- __stack_start__ = __stack_end__ - 16 KB
               • Reserved 16,384 bytes (16.0 KB) for stack

───────────────────────────────────────────────────────────────
      ↑ (Unused / Free space between heap and stack, if any)
      ↑ This space depends on runtime memory usage

      ↑ Heap grows upward (allocated at runtime via malloc/new)
      ↑ Uninitialized (NOLOAD section)

0x2001B000  <-- __heap_end__ ≈ 0x20001370 + 4,096 bytes
0x20001370  <-- __heap_start__
               • Reserved 4,096 bytes (4.0 KB)
               • Used for dynamic allocations (malloc, etc.)

───────────────────────────────────────────────────────────────
0x20001370  <-- __bss_end__
0x20000060  <-- __bss_start__
               • Size: 5,104 bytes (≈4.98 KB)
               • Contains *uninitialized global/static variables*
               • Zero-filled at startup by the C runtime

───────────────────────────────────────────────────────────────
0x20000060  <-- _edata (end of .data)
0x20000000  <-- _sdata (start of .data)
               • Size: 96 bytes (0.094 KB)
               • Contains *initialized global/static variables*
               • Copied from Flash to SRAM during startup

───────────────────────────────────────────────────────────────
0x20000000  <-- ORIGIN(SRAM) = Start of SRAM (Total 128 KB = 131,072 bytes)

*/



