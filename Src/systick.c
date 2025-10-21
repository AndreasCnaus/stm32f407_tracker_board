#include "systick.h"

// Define the system clock frequency
uint32_t SystemCoreClock = 16000000;  // 16 MHz
volatile uint32_t systick_ms = 0; 

void systick_init(void)
{
    // Load number of clock cycles per millisecond
    SysTick->LOAD = (SystemCoreClock / 1000) - 1;

    
    // Clear the SysTick current value register
    SysTick->VAL = 0;

    // Select processor clock, enable interrupt, start timer
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |    // Bit 2: Use processor clock (HCLK) instead of HCLK/8
                    SysTick_CTRL_TICKINT_Msk   |    // Bit 1: Enable SysTick exception (interrupt) when counter reaches 0
                    SysTick_CTRL_ENABLE_Msk;        // Bit 0: Start the SysTick counter (enable counting)
}

void SysTick_Handler(void)
{
    systick_ms++;
}

uint32_t system_get_tick_ms(void)
{
    return systick_ms;
}

void systick_delay_ms(uint32_t delay_ms)
{
    uint32_t start_time = system_get_tick_ms();
    while ((system_get_tick_ms() - start_time) < delay_ms) {
        // busy wait
    }
}
