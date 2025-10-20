#include "uart.h"
#include "my_stdio.h"

#include <stdint.h>
#include <stdio.h>


int main(void)
{ 
    // Initialize  UART1 to communicate with SIM7600E-Module
    uart1_init();

    // Initialize UART2 to print message on host-pc (debugging)
    uart2_init();
    
    // Initialize stdio to use printf correctly 
    stdio_init();
    
    /* Loop forever */
    while (1)
    {
        printf("Hello from STM32F407_tracker_board firmware!\r\n");
    }
}

