#include "uart.h"
#include "my_stdio.h"
#include "systick.h"
#include "sim7600e.h"
#include "gps.h"

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#define GPS_INFO_MAX_LEN    128

int main(void)
{ 
    const char *pin = "4949";
    const char *url = "https://89b0716c1a07.ngrok-free.app";
    uint8_t debug = 1;
    char gps_info[GPS_INFO_MAX_LEN];
    char *payload_ptr = gps_info;
    int rv;

    // Initialize system tick 
    systick_init();

    // Initialize  UART1 to communicate with SIM7600E-Module
    uart1_init();

    // Initialize UART2 to print message on host-pc (debugging)
    uart2_init();
    
    // Initialize stdio to use printf correctly 
    stdio_init();

    // Initialize SIM7600E-Module 
    rv = sim7600e_init(pin, url, debug);
    if (rv) {
        if (debug) printf("Failed to initialize SIM7660E module. Status code: %d", rv);
        // ToDo: turn on the init_error led
        return -1;
    }


    // Get the first GPS-info 
    uint32_t delay_ms = 20000;      // Delay between each attempt to get GPS info 
    uint32_t timeout_ms = 5*60000;   // Total timout 3*60s

    rv = sim7600e_get_gps_fix(&payload_ptr, sizeof(gps_info), delay_ms, timeout_ms, debug);
    
    if (rv == 0) {
        if (debug) printf("Success! GPS data acquired within %lds. \r\n Data: %s \r\n", timeout_ms/1000, payload_ptr);
    } else if (rv == -3) {
        if (debug) printf("GPS fix acquisition timed out after %lds.\r\n", timeout_ms/1000);
    } else {
        if (debug) printf("Failed to get GPS info due to communication error. Status Code: %d \r\n", rv);
        return -2;
    }

    gps_data_t gps_data;
    rv = parse_gps_info(payload_ptr, &gps_data, debug);
    if (rv == 0) {
        if (debug) printf("GPS info successfully parse.\r\n");
    } else {
        if (debug) printf("Faild to parse GPS info. Status code: %d\r\n", rv);
        return -3;
    }


    /* Loop forever */
    while (1)
    {
        
    }
}

