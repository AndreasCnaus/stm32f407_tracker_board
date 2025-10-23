#ifndef UART_H_
#define UART_H_

#include "stm32f4xx.h"
#include <stddef.h> // For size_t

// UART1 is used for communication with SIM7600E-Module
int uart1_init(void);
int uart1_write_nb(int ch);
int uart1_read_nb(void);
void uart1_flush_rx_buffer(void);

// UART2 is used to print the Debug-Mesages on the Host-PC 
int uart2_init(void);
int uart2_write(int ch);
int uart2_read(void);


#endif  // UART_H_