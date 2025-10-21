#include "sim7600e.h"
#include "uart.h"
#include "systick.h"

#include <string.h>
#include <stdio.h>

#define TX_TIMEOUT_MS   100 // TX timeout in miliseconds 
#define CMD_BUF_SIZE    32  // Site for the single AT command reques line 
#define LINE_BUF_SIZE   128 // Size for the single AT command response line

typedef struct {
    const char *string;
    AtResponseStatus_t status;
} AtLookupEntry_t;

const AtLookupEntry_t StatusLookupTable[] = {
    // Most Frequent
    {"OK\r\n",             AT_OK},
    {"ERROR\r\n",          AT_ERROR},         // Corrected: Must be all caps

    // Medium Frequent
    {"+HTTPACTION",        AT_HTTP_ACTION},   // Corrected: Removed '\r\n' to match the prefix of the line
    {"DOWNLOAD\r\n",       AT_DOWNLOAD_READY},

    // Low Frequency
    {"+CPIN: READY\r\n",   AT_CPIN_READY},
    {"+CPIN: SIM PIN\r\n", AT_CPIN_SIM_PIN},
    {"+CPIN: SIM PUK\r\n", AT_CPIN_SIM_PUK},
};

const size_t LookUpTableSize = sizeof(StatusLookupTable) / sizeof(AtLookupEntry_t);

// Forward declarations
AtResponseStatus_t parse_at_response_line(const char *response_line, uint8_t debug);

// Parse single AT-Response line using lookup table
AtResponseStatus_t parse_at_response_line(const char *response_line, uint8_t debug)
{
    // Check input parameters
    if (response_line == NULL) {
        return AT_RX_PARTIAL;
    }

    // Loop trough every entry in the lookup table
    for (size_t i = 0; i < LookUpTableSize; i++) {

        if (strstr(response_line, StatusLookupTable[i].string) != NULL) {

            // Match found, log it and return specific enum status
            if (debug) printf("<<< %s", response_line);
            return StatusLookupTable[i].status;  
        }

    }

    return AT_RX_PARTIAL;   // no match, keep-reading
}

// Send an AT command and return enum response
AtResponseStatus_t send_at(const char *cmd, uint32_t rx_timeout_ms, uint8_t debug)
{
    // Checking AT-Command arguments 
    if (cmd == NULL || *cmd == '\0') {
        if (debug) printf("AT command is empty.\r\n");
        return AT_INVALID_PARAM;
    }

    if (debug == 1) printf(">>> %s\r\n", cmd);  // Print AT-Command (should already include '\r')

    // Send Command with dedicated, short TX timeout
    size_t bytes_to_send = strlen(cmd);
    int bytes_send = uart_write_str_nb((uart_tx_char_t)uart1_write_nb, cmd, bytes_to_send, TX_TIMEOUT_MS);
    if (bytes_send < 0) {
        if (debug) printf("Error: UART write timed out during TX.\r\n");
        return AT_TIMEOUT;
    }

    // Check Transmit Status
    if ((size_t)bytes_send != bytes_to_send) {
        if (debug) printf("Error: Only %zu of %zu bytes were sent to modem.\r\n", bytes_send, bytes_to_send);
        return AT_TX_FAILURE;
    }

    char line_buf[LINE_BUF_SIZE];
    uint32_t rx_start_time = system_get_tick_ms();
    uint32_t elapsed_time, time_remaining;

    // Loop until the overall RX timeout budget is exceeded
    while (1) {
        elapsed_time = system_get_tick_ms() - rx_start_time;

        if (elapsed_time >= rx_timeout_ms) {
            if (debug) printf("Error: Overall RX timeout reached.\r\n");
            return AT_TIMEOUT;
        }

        // Calculate remaining time for next read attempt
        time_remaining = rx_timeout_ms - elapsed_time;

         // Read the line response from SIM7600E-Modul
        char *response_line = uart_read_str_nb((uart_rx_char_t)uart1_read_nb, line_buf, LINE_BUF_SIZE, time_remaining);
        if (response_line != NULL) {

            AtResponseStatus_t response_status = parse_at_response_line(response_line, debug);
            if (response_status != AT_RX_PARTIAL) {
                return response_status;
            } // else continue reading 
        }
    }
}

int sim7600e_init(const char *pin, const char *url, uint8_t debug)
{
    char cmd[32];     
    AtResponseStatus_t resp;
    
    // Check the communication
    resp = send_at("AT\r", 100, debug);
    if (resp != AT_OK) {
        if (debug) printf("Error: Initial communication attempt with SIM7600E failed.\r\n");
        return -1;
    }

    // Unlock SIM if necessary
    resp = send_at("AT+CPIN?\r", 100, debug);
    if (resp == AT_CPIN_READY) {
        if (debug) printf("SIM already unlocked.\r\n");
    } else if (resp == AT_CPIN_SIM_PIN) {
        printf("Try to unlock SIM\r\n");
        snprintf(cmd, CMD_BUF_SIZE, "AT+CPIN=\"%s\"\r", pin);        
        resp = send_at(cmd, 100, debug);
        if (resp != AT_OK) {
            printf("Failed to unlock SIM\r\n");
            return -2;
        }
    } else if (resp == AT_CPIN_SIM_PUK) {
        printf("Error: SIM is PUK-locked. Manual intervention required.\r\n");
        return -3;
    } else {
        // Catch-all for NOT INSERTED, etc.
        printf("Error: CPIN response not supported or failure (Code: %d).\r\n", resp); 
        return -4; 
    }

    // 

    return 0;   // success 
}

