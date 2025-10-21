#ifndef UART_H_
#define UART_H_

#include "stm32f4xx.h"
#include <stddef.h> // For size_t

// UART1 is used for communication with SIM7600E-Module
int uart1_init(void);
int uart1_write_nb(int ch);
int uart1_read_nb(void);

// UART2 is used to print the Debug-Mesages on the Host-PC 
int uart2_init(void);
int uart2_write(int ch);
int uart2_read(void);

// Function Pointer Type definitions
typedef int (*uart_tx_char_t)(int ch);
typedef int (*uart_rx_char_t)(void);

int uart_write_str_nb(uart_tx_char_t tx_func_nb, const char *str, size_t max_len, uint32_t timeout_ms);
char* uart_read_str_nb(uart_rx_char_t rx_func_nb, char *out_str, size_t max_len, uint32_t timeout_ms);

#endif  // UART_H_