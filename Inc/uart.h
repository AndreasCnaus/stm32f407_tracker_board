#ifndef UART_H_
#define UART_H_

#include "stm32f4xx.h"

// UART1 is used for communication with SIM7600E-Module
void uart1_init(void);
void uart1_write(int ch);
int uart1_read(void);

// UART2 is used to print the Debug-Mesages on the Host-PC 
void uart2_init(void);
void uart2_write(int ch);
int uart2_read(void);

#endif  // UART_H_