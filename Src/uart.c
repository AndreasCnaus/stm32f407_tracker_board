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

// Non-Blocking UART string write with timeout
int uart_write_str_nb(uart_tx_char_t tx_func_nb, const char *str, size_t max_len, uint32_t timeout_ms)
{
    int chars_written = 0;
    
    // Save the start time 
    uint32_t start_time = system_get_tick_ms();

    for (const char *ptr = str; *ptr != '\0' && chars_written < max_len; ptr++) {
        
        int status = -1;

        // Try to send current character 
        while (status != 0) {
            
            status = tx_func_nb(*ptr); // Call the non-blocking write (returns 0 or -1)
            
            if (status == 0) {
                chars_written++;
                break; // Character sent successfully
            }
            
            // Check if timeout time is exceeded (Overall Timeout Check)
            if ((system_get_tick_ms() - start_time) >= timeout_ms) {
                return -1; // Abort transmission
            }
        }
    }
    
    return chars_written;
}

// Non-Blocking UART single line string read with timeout 
char* uart_read_str_nb(uart_rx_char_t rx_func_nb, char *out_str, size_t max_len, uint32_t timeout_ms)
{
    // Input parameter check
    if (out_str == NULL || max_len == 0) {
        return NULL;
    }

    // Ensure space for at least the null terminator
    if (max_len == 1) {
        out_str[0] = '\0';
        return NULL;
    }

    size_t i = 0;
    int received_char;
    uint32_t start_time = system_get_tick_ms();

    // Loop as long as the total elapsed time is less than the timeout
    while ((system_get_tick_ms() - start_time) < timeout_ms) {

        // Call the non-blocking read (return character (0-255) or -1 fo no data)
        received_char = rx_func_nb();
        if (received_char >= 0) {   // Data received successfully

            // Check againts output buffer limit
            if (i < (max_len - 1)) { // -1 because of null terminator 

                out_str[i] = (char)received_char;

                // Check for a termination sequence (newline '\n' from the modem)
                if (out_str[i] == '\n') {
                    i++;    // increment to include '\n' in the count
                    break; // exit the loop
                }

                i++; // increment once per stored character

            } else {
                // Buffer capacity reched (overflow prevented)
                break;
            }
        }
    }

    // Terminate the buffer at the current index
    out_str[i] = '\0';

    // Check for read failure
    if ((i == 0) && ((system_get_tick_ms() - start_time) >= timeout_ms)) {
        return NULL;
    }

    return out_str; // return the result pointer 
}



