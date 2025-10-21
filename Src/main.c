#include "uart.h"
#include "my_stdio.h"
#include "systick.h"
#include "sim7600e.h"

#include <stdint.h>
#include <stdio.h>


int main(void)
{ 
    const char *pin = "4949";
    const char *url = "test_url";
    uint8_t debug = 1;

    // Initialize system tick 
    systick_init();

    // Initialize  UART1 to communicate with SIM7600E-Module
    uart1_init();

    // Initialize UART2 to print message on host-pc (debugging)
    uart2_init();
    
    // Initialize stdio to use printf correctly 
    stdio_init();

    // Initialize SIM7600E-Module 
    sim7600e_init(pin, url, debug);
    
    /* Loop forever */
    while (1)
    {
        
    }
}

