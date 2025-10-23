#include "uart.h"
#include "gpio.h"
#include "systick.h"
#include <stdio.h>

#include <stdint.h>
#include <stddef.h>


#define DBG_UART_BAUDRATE       115200
#define UART1_BAUDRATE          115200
#define SYS_FREQ                16000000
#define APB1_CLK                SYS_FREQ
#define APB2_CLK                SYS_FREQ

// Forward declarations 
static void uart_set_baudrate(USART_TypeDef *USARTx, uint32_t periph_clk, uint32_t baudrate);
static uint16_t compute_uart_bd(uint32_t periph_clk, uint32_t baudrate);

__attribute__((used))
int __io_putchar(int ch)
{
    uart2_write(ch);
    return ch;
}

// Initialize UART1
int uart1_init(void)
{
    const uint8_t pb6 = 6;  // TX
    const uint8_t pb7 = 7;  // RX

    // Enable clock access to GPIOB
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    // Set (TX) to Push-Pull for clean signal edges
    GPIO_SetOutType(GPIOB, pb6, PUSH_PULL); 
    
    // Set (RX) to use the internal Pull-Up resistor (CRITICAL for idle stability)
    GPIO_SetPuPd(GPIOB, pb7, PULL_UP);

    // Set the mode of PB6/PB7 (USART1_TX/_RX) to alternate function mode
    GPIO_SetMode(GPIOB, pb6, GPIO_MODE_ALTERNATE);
    GPIO_SetMode(GPIOB, pb7, GPIO_MODE_ALTERNATE);

    // Set alternate function type to AF7(USART_TX)
    GPIO_SetAlternateFunction(GPIOB, pb6, AF7);
    // Set alternate function type to AF7(USART_RX )
    GPIO_SetAlternateFunction(GPIOB, pb7, AF7);

    // Enable clock access to UART2
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    // Configure UART baudrate (This should come first)
    uart_set_baudrate(USART1, APB2_CLK, UART1_BAUDRATE);

    // Configure transfer direction TX + RX 
    USART1->CR1 |= (USART_CR1_TE | USART_CR1_RE);

    // Set Pull-Up resisto on RX line 
    // GPIO_SetPuPd(GPIOB, pb7, PULL_UP);

    // Enable UART Module (This should be last)
    USART1->CR1 |= USART_CR1_UE;

    return 0;   // success
}

// Initialize UART2
int uart2_init(void)
{
    const uint8_t pa2 = 2;  // TX
    const uint8_t pa3 = 3;  // RX

    // Enable clock access to GPIOA
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // Set (TX) to Push-Pull for clean signal edges
    GPIO_SetOutType(GPIOA, pa2, PUSH_PULL); 
    
    // Set (RX) to use the internal Pull-Up resistor (CRITICAL for idle stability)
    GPIO_SetPuPd(GPIOA, pa3, PULL_UP);

    // Set the mode of PA2/PA3 (USART2_TX/_RX) to alternate function mode
    GPIO_SetMode(GPIOA, pa2, GPIO_MODE_ALTERNATE);
    GPIO_SetMode(GPIOA, pa3, GPIO_MODE_ALTERNATE);

    // Set alternate function type to AF7(USART_TX)
    GPIO_SetAlternateFunction(GPIOA, pa2, AF7);
    // Set alternate function type to AF7(USART2_RX )
    GPIO_SetAlternateFunction(GPIOA, pa3, AF7);

    // Enable clock access to UART2
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // Configure UART baudrate (This should come first)
    uart_set_baudrate(USART2, APB1_CLK, DBG_UART_BAUDRATE);

    // Configure transfer direction TX + RX 
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE);

    // Enable UART Module (This should be last)
    USART2->CR1 |= USART_CR1_UE;

    return 0;   // success
}

// Non-blocking write: attempts to write the character and returns status.
int uart1_write_nb(int ch)
{
    // Check if the Transmit Data Register is empty (TXE).
    if (USART1->SR & USART_SR_TXE) {
        // Hardware is ready: write the character.
        USART1->DR = (ch & 0xFF);
        return 0; // Success
    } else {
        // Hardware is NOT ready (transmit buffer is full).
        return -1; // Failure: buffer busy
    }
}

// Non-blocking read: checks for data and returns character or error.
int uart1_read_nb(void)
{
    // Check if the Receive Data Register is NOT empty (RXNE).
    if (USART1->SR & USART_SR_RXNE) {
        // Data is ready: read the character.
        int ch = USART1->DR & 0xFF;
        return ch; // Success: returns the received character (0-255)
    } else {
        // No data available.
        return -1; // Failure: no data
    }
}

// Flushes the UART1 RX Buffer by reading all available data
void uart1_flush_rx_buffer(void) {

    // Loop as long as the RXNE flag is set 
    while((USART1->SR & USART_SR_RXNE) != 0) {
      (void)USART1->DR; // read register data into dummy variable 
    }
}

__attribute__((used))   // don't optimize this function away
int uart2_write(int ch)
{
    // Make sure transmit data register is empty
    while(!(USART2->SR & USART_SR_TXE)) {}

    // Write to transmit data register 
    USART2->DR = (ch & 0xFF);
    return 0;   // success
}

__attribute__((used))   // don't optimize this function away
int uart2_read(void)
{
    // Wait until data is received (SR_RXNE = 1)
    while(!(USART2->SR & USART_SR_RXNE)) {}

    // Read character from data register
    int ch = USART2->DR & 0xFF;
    return ch;
}

// Helper function to compute UART baudrate 
static uint16_t compute_uart_bd(uint32_t periph_clk, uint32_t baudrate)
{
    return ((periph_clk + (baudrate)/2U)/baudrate);
}

// Helper function to set UART baudrate 
static void uart_set_baudrate(USART_TypeDef *USARTx, uint32_t periph_clk, uint32_t baudrate)
{
    USARTx->BRR = compute_uart_bd(periph_clk, baudrate);
}




