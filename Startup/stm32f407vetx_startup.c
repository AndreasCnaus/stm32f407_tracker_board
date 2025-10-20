#include <stdint.h>
#include <reent.h>
#include <stdio.h>
#include <string.h>

#include "stm32f4xx.h" // For SCB and FPU definitions


/*
 * The following symbols from the linker script (_estack, _etext, _sdata, _edata, _sbss, _ebss) are defined by the linker script.
 * They mark the boundaries of the stack, text, data, and bss sections in memory, allowing the startup code
 * to initialize RAM sections properly before calling main().
 */
extern uint32_t _estack;        /* Top of stack (initial stack pointer) */
extern uint32_t _etext;         /* End of .text section in FLASH */
extern uint32_t _sidata;        /* Start of initialization values for .data in FLASH */ 
extern uint32_t _sdata;         /* Start of .data section in SRAM */
extern uint32_t _edata;         /* End of .data section in SRAM */
extern uint32_t __bss_start__;  /* Start of .bss section in SRAM */
extern uint32_t __bss_end__;    /* End of .bss section in SRAM */
extern unsigned int __stack_start__;
extern unsigned int __stack_end__;

// C++ constructor arrays (even if not using C++)
extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);


// Function prototypes
void __libc_init_array(void);
int main(void);

void Default_Handler(void);
void Reset_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);

void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void EXTI4_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));


// Vector table (order matters!)
uint32_t vector_tbl[] __attribute__((section(".isr_vector_tbl"))) = {
    (uint32_t)&_estack,          // Initial stack pointer
    (uint32_t)&Reset_Handler,    // Reset
    (uint32_t)&NMI_Handler,      // NMI
    (uint32_t)&HardFault_Handler,// HardFault
    (uint32_t)&MemManage_Handler,// MemManage
    (uint32_t)&BusFault_Handler, // BusFault
    (uint32_t)&UsageFault_Handler,// UsageFault
    0, 0, 0, 0,                  // Reserved (4 words)
    (uint32_t)&SVC_Handler,      // SVCall
    (uint32_t)&DebugMon_Handler, // Debug monitor
    0,                           // Reserved
    (uint32_t)&PendSV_Handler,   // PendSV
    (uint32_t)&SysTick_Handler,  // SysTick

    // IRQ0 - WWDG
    (uint32_t)&Default_Handler,
    // IRQ1 - PVD
    (uint32_t)&Default_Handler,
    // IRQ2 - TAMP_STAMP
    (uint32_t)&Default_Handler,
    // IRQ3 - RTC_WKUP
    (uint32_t)&Default_Handler,
    // IRQ4 - FLASH
    (uint32_t)&Default_Handler,
    // IRQ5 - RCC
    (uint32_t)&Default_Handler,
    // IRQ6 - EXTI0
    (uint32_t)&Default_Handler,
    // IRQ7 - EXTI1
    (uint32_t)&Default_Handler,
    // IRQ8 - EXTI2
    (uint32_t)&Default_Handler,
    // IRQ9 - EXTI3
    (uint32_t)&Default_Handler,
    // IRQ10 - EXTI4
    (uint32_t)&EXTI4_IRQHandler,
    // ... continue for rest if needed
};

void Default_Handler(void)
{
    while (1)
    {
    }
}

void HardFault_Handler(void)
{
    // When a hard fault occurs, this breakpoint will be hit if debugging.
    __asm volatile("BKPT #0");
    while (1)
    {
    }
}

void MemManage_Handler(void)
{
    // When a memory management fault occurs, this breakpoint will be hit.
    __asm volatile("BKPT #0");
    while (1)
    {
    }
}

void Reset_Handler(void)
{
    /*
     * Enable the FPU. This must be done before any floating-point operations
     * are performed. The project is compiled with -mfloat-abi=hard, which means
     * the FPU will be used. Attempting to use it while disabled will result in a
     * Hard Fault. The FPU is enabled by granting full access to coprocessors 10 and 11.
     */
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */
#endif

    // Copy .data
    for (uint32_t *src = &_sidata, *dst = &_sdata; dst < &_edata;)
        *dst++ = *src++;

    // Zero .bss
    for (uint32_t *dst = &__bss_start__; dst < &__bss_end__;)
        *dst++ = 0;
 
    // Initialize the C library (constructors, etc.)
    __libc_init_array();

    // Call main
    main();

    // If main ever returns, loop forever
    while (1) { }
}


void __libc_init_array(void) {
    // Call init array (constructors)
    for (size_t i = 0; i < __init_array_end - __init_array_start; i++)
        __init_array_start[i]();
}


