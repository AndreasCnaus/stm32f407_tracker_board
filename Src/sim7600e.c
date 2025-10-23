#include "sim7600e.h"
#include "uart.h"
#include "systick.h"

#include <string.h>
#include <stdio.h>


#define TX_TIMEOUT_MS   100 // TX timeout in miliseconds 
#define TX_BUF_SIZE     32  // Size for the AT command reques
#define RX_BUF_SIZE     64 // Size for the AT command response

typedef struct {
    const char *string;
    AtResponseStatus_t status;
} AtLookupEntry_t;

// The lookup table, prioritized for searching the full response buffer.
const AtLookupEntry_t StatusLookupTable[] = {

    // ritical Error Codes 
    {"ERROR\r\n",           AT_ERROR},          // Must be checked before specific errors
    {"+CME ERROR:",         AT_CME_ERROR},
    {"+CMS ERROR:",         AT_CMS_ERROR},

    // CREG (Network Registration)
    {"+CREG: ",             AT_INFO_CREG},          // Matches +CREG: <n>,<stat>

    // CGPS
    {"+CGPS: ",             AT_INFO_CGPS},          // Matches +CGPS: <on/off>,<mode>
    {"+CGPSINFO: ",         AT_INFO_CGPSINFO},      // Matches +CGPSINFO: <data>
   
    // CSQ (Signal Quality)
    {"+CSQ: ",              AT_INFO_CSQ},           // Matches +CSQ: <rssi>,<ber>
    
    // CPIN (Fixed strings, easiest to match directly)
    {"+CPIN: READY",        AT_CPIN_READY},
    {"+CPIN: SIM PIN",      AT_CPIN_SIM_PIN},
    {"+CPIN: SIM PUK",      AT_CPIN_SIM_PUK},
    {"+CPIN: PH-SIM PIN",   AT_CPIN_PH_SIM_PIN},
    
    // Generic Information Codes 
    {"NO CARRIER\r\n",       AT_NO_CARRIER},
    {"CONNECT\r\n",          AT_CONNECT},
    {"DOWNLOAD\r\n",         AT_DOWNLOAD_READY},
    {"OK\r\n",               AT_OK},

    // Sentinel to mark the end of the table
    {NULL,                   (AtResponseStatus_t)0} 
};


// Function Pointer Type definitions
typedef int (*uart_tx_char_t)(int ch);
typedef int (*uart_rx_char_t)(void);

// Forward declarations
AtResponseStatus_t parse_at_response(const char *response, uint8_t debug);
int sim7600e_write_command(uart_tx_char_t tx_func_nb, const char *cmd, size_t len, uint32_t timeout_ms);
char* sim7600e_read_full_response(uart_rx_char_t rx_func_nb, char *out_buf, size_t max_len, uint32_t timeout_ms);
CregState_t parse_creg_status(const char *response_str);
CgpsState_t parse_cgps_status(const char *response_str);

// Parse single AT-Response line using lookup table
AtResponseStatus_t parse_at_response(const char *response, uint8_t debug)
{
    // Check input parameters
    if (response == NULL) {
        return AT_INVALID_PARAM;
    }

    // Iterate trough every entry in the lookup table
    for (size_t i = 0;  StatusLookupTable[i].string != NULL; i++) {

        // Check if the matching string is presented anywhere in the response buffer
        if (strstr(response, StatusLookupTable[i].string) != NULL) {

            // Match found, log it and return specific enum status
            if (debug) printf("<<< %s", response);
            return StatusLookupTable[i].status;  
        }
    }

    return AT_RX_PARTIAL;   // no match, keep-reading
}

int sim7600e_write_command(uart_tx_char_t tx_func_nb, const char *cmd, size_t len, uint32_t timeout_ms)
{
    int chars_written = 0;
    
    // Save the start time 
    uint32_t start_time = system_get_tick_ms();

    for (const char *ptr = cmd; *ptr != '\0' && chars_written < len; ptr++) {
        
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

char *sim7600e_read_full_response(uart_rx_char_t rx_func_nb, char *out_buf, size_t max_len, uint32_t timeout_ms)
{
    // Input parameter check
    if (out_buf == NULL || max_len == 0) {
        return NULL;
    }

    // Ensure space for at least the null terminator
    if (max_len == 1) {
        out_buf[0] = '\0';
        return NULL;
    }

    size_t i = 0;
    int received_char;
    const uint32_t INTER_CHAR_TIMEOUT_MS = 50;
    uint32_t start_time = system_get_tick_ms();
    uint32_t last_char_time = system_get_tick_ms(); // Initialize for silence check

    // Loop as long as the total elapsed time is less than the timeout
    while ((system_get_tick_ms() - start_time) < timeout_ms) {

        // 1. SILENCE CHECK (Critical for fixing this bug)
        if (i > 0 && (system_get_tick_ms() - last_char_time) > INTER_CHAR_TIMEOUT_MS) {
            break; // Stop reading if the modem is silent.
        }

        // Call the non-blocking read
        received_char = rx_func_nb();
        
        if (received_char >= 0) { // Data received successfully

            // Check against output buffer limit
            if (i < (max_len - 1)) {
                
                out_buf[i++] = (char)received_char; // Store character and increment index
                out_buf[i] = '\0';                  // Null-terminate the new end

            } else {
                // Buffer capacity reached. Treat as end of read.
                break;
            }
        }
    }

    // Terminate the buffer at the current index
    out_buf[i] = '\0';

    // Check for read failure
    if ((i == 0) && ((system_get_tick_ms() - start_time) >= timeout_ms)) {
        return NULL;
    }

    return out_buf; // return the result pointer 
}

// Send an AT command and return enum response
AtResponseStatus_t send_at(const char *cmd, uint32_t rx_timeout_ms, char *rx_buf, size_t rx_buf_size, uint8_t debug)
{
    // Checking AT-Command arguments 
    if (cmd == NULL || *cmd == '\0') {
        if (debug) printf("Error: AT command is empty.\r\n");
        return AT_INVALID_PARAM;
    }

    if (rx_buf == NULL || rx_buf_size == 0) {
        if (debug) printf("Error: Receive Buffer is empty.\r\n");
        return AT_INVALID_PARAM;
    }

    if (debug == 1) printf(">>> %s\r\n", cmd);  // Print AT-Command (should already include '\r')

    // Send Command with dedicated, short TX timeout
    size_t bytes_to_send = strlen(cmd);
    int bytes_send = sim7600e_write_command((uart_tx_char_t)uart1_write_nb, cmd, bytes_to_send, TX_TIMEOUT_MS);
    if (bytes_send < 0) {
        if (debug) printf("Error: UART write timed out during TX.\r\n");
        return AT_TIMEOUT;
    }

    // Check Transmit Status
    if ((size_t)bytes_send != bytes_to_send) {
        if (debug) printf("Error: Only %zu of %zu bytes were sent to modem.\r\n", bytes_send, bytes_to_send);
        return AT_TX_FAILURE;
    }

    // Read the response from SIM7600E-Modul
    char *response = sim7600e_read_full_response((uart_rx_char_t)uart1_read_nb, rx_buf, rx_buf_size, rx_timeout_ms);
    if (response != NULL) {
        return parse_at_response(response, debug);
    }

    if (debug) printf("Error: No response or read timeout.\r\n");
    return AT_TIMEOUT;
}

// Parse Network Registration Status
CregState_t parse_creg_status(const char *response_str) {

    // Check input parameter 
    if (response_str == NULL) {
        return CREG_STATE_INVALID;
    }

    // Locate the specific information line within the full response
    const char *start_pos = strstr(response_str, "+CREG: ");
    if (start_pos == NULL) {
        return CREG_STATE_INVALID;
    }

    // Advance the pointer past the "+CREG: " prefix to the numbers
    // start_pos will now point to the first digit of <n>.
    start_pos += strlen("+CREG: ");

    // Attempt to read the two required integers <n>,<stat>
    int n = -1;
    int stat = -1;

    int scan_count = sscanf(start_pos, "%d,%d", &n, &stat); 
    
    if (scan_count != 2) {
        // Parsing failure (couldn't read two comma-separated integers)
        return CREG_STATE_INVALID;
    }

    // Map the extracted <stat> value to the CregState_t enum
    switch (stat) {
        case 0:
            return CREG_STATE_NOT_REGISTERED;
        case 1:
            return CREG_STATE_HOME_NETWORK;
        case 2:
            return CREG_STATE_SEARCHING;
        case 3:
            return CREG_STATE_DENIED;
        case 4:
            return CREG_STATE_UNKNOWN;
        case 5:
            return CREG_STATE_ROAMING;
        default:
            // Unrecognized status code
            return CREG_STATE_INVALID;
    }
}

// Parse GPS Status 
CgpsState_t parse_cgps_status(const char *response_str)
{
    if (response_str == NULL) {
        return CGPS_STATE_INVALID;
    }

    // Handle +CGPS: (GPS Engine Status)
    const char *start_pos = strstr(response_str, "+CGPS: ");
    if (start_pos != NULL) {
        start_pos += strlen("+CGPS: ");
        
        int mode = -1;
        int type = -1;

        // Attempt to read the two required integers mode and type.
        int scan_count = sscanf(start_pos, "%d,%d", &mode, &type); 

        // Handle GPS OFF state (+CGPS: 0)
        if (mode == 0) {
            // The modem might send "+CGPS: 0" (scan_count=1) or "+CGPS: 0,1" (scan_count=2).
            if (scan_count >= 1) { 
                return CGPS_STATE_OFF;
            }
        }
        
        // Handle GPS ON states (+CGPS: 1,X)
        if (mode == 1 && scan_count == 2) {
            if (type == 1) return CGPS_STATE_ON_STANDALONE;
            if (type == 2) return CGPS_STATE_ON_AGPS_UE;
            if (type == 3) return CGPS_STATE_ON_AGPS_ASSIST;
        }

        // If we found +CGPS: but the parameters were unhandled
        return CGPS_STATE_INVALID; 
    }

    // Handle +CGPSINFO: (Positional Fix Status)
    const char *info_prefix = "+CGPSINFO: ";
    start_pos = strstr(response_str, info_prefix);
    if (start_pos != NULL) {
        
        // Find the "no fix" pattern first: +CGPSINFO: ,,,,,,,,
        if (strstr(start_pos, "+CGPSINFO: ,,,,,,,,") != NULL) {
            return CGPS_STATE_NO_FIX;
        }
        
        // Find the "fix available" pattern
        const char *latitude_start = start_pos + strlen(info_prefix);
        
        // Check the character content
        if (*latitude_start != ',') {
            // Fix is available.
            return CGPS_STATE_FIX_AVAILABLE;
        }
    }
    
    // The response string contained neither +CGPS: nor +CGPSINFO:
    return CGPS_STATE_INVALID;
}

int sim7600e_init(const char *pin, const char *url, uint8_t debug)
{
    char cmd[TX_BUF_SIZE];
    char rx_buf[RX_BUF_SIZE];     
    AtResponseStatus_t resp;
    int rv = 0;

    // Send the software reset command (timeout can be short, e.g., 500ms, as it only needs to process the command)
    resp = send_at("AT+CFUN=1,1\r", 500, rx_buf, RX_BUF_SIZE, debug);
    if (resp != AT_OK) {
        if (debug) printf("Error: failed to trigger SIM7600E reset. Continuing with caution.\r\n");
        return --rv;
    }

    // Initial delay for the modem to begin the power-up sequence
    uint16_t delay_s = 40;
    if (debug) printf("Modem initiated reset. Waiting %d seconds for boot...\r\n", delay_s);
    systick_delay_ms(delay_s * 1000); // Wait delay_s seconds

    uart1_flush_rx_buffer();    // Flush the UART1 RX-Buffer 

    // Check the communication after reset 
    resp = send_at("AT\r", 500, rx_buf, RX_BUF_SIZE, debug);
    if (resp != AT_OK) {
        if (debug) printf("Error: Initial communication attempt with SIM7600E failed.\r\n");
        return --rv;
    }
    systick_delay_ms(1000); // Wait 1 second

    // Unlock SIM if necessary
    resp = send_at("AT+CPIN?\r", 1000, rx_buf, RX_BUF_SIZE, debug);  // Query SIM-Lock status 
    if (resp == AT_CPIN_READY) {
        if (debug) printf("SIM already unlocked.\r\n");
    } else if (resp == AT_CPIN_SIM_PIN) {
        if (debug) printf("Try to unlock SIM\r\n");
        snprintf(cmd, TX_BUF_SIZE, "AT+CPIN=\"%s\"\r", pin);        
        resp = send_at(cmd, 1000, rx_buf, RX_BUF_SIZE, debug);
        if (resp != AT_OK) {
            if (debug) printf("Failed to unlock SIM\r\n");
            return --rv;
        }
    } else if (resp == AT_CPIN_SIM_PUK) {
        if (debug) printf("Error: SIM is PUK-locked. Manual intervention required.\r\n");
        return --rv;
    } else {
        // Catch-all for NOT INSERTED, etc.
        if (debug) printf("Error: CPIN response not supported or failure (Code: %d).\r\n", resp); 
        return --rv; 
    }
    systick_delay_ms(5000);    // Wait 5 seconds to stabilize 
    uart1_flush_rx_buffer();    // Flush the UART1 RX-Buffer (erase the unsolicited status codes)

    // Register SIM on the Network
    const uint8_t registration_attempts = 10;
    uint8_t sim_registered = 0;

    if (debug) printf("Attempting network registration...\r\n");

    const uint32_t INITIAL_CREG_TIMEOUT = 5000; // 5 seconds for the very first check
    const uint32_t POLLING_CREG_TIMEOUT = 1000; // 1 second for subsequent checks
    const uint32_t POLLING_INTERVAL = 3000;     // 3 seconds between checks

    for (uint8_t i = 0; i < registration_attempts; i++) {

        // Adaptive Timeout: Use 5s for the first attempt (i=0), 1s thereafter.
        uint32_t current_timeout_ms = (i == 0) ? INITIAL_CREG_TIMEOUT : POLLING_CREG_TIMEOUT;

        resp = send_at("AT+CREG?\r", current_timeout_ms, rx_buf, RX_BUF_SIZE, debug);
        if (resp == AT_INFO_CREG) {
            CregState_t state = parse_creg_status(rx_buf);

            // Check for definitive success states (Registered Home or Roaming)
            if ((state == CREG_STATE_HOME_NETWORK) || (state == CREG_STATE_ROAMING)) {
                if (debug) printf("SIM successfully registered on network.\r\n");
                sim_registered = 1;
                break; // Exit loop on success
            } 
            // Check for expected 'In Progress' or Transient states
            else if ((state == CREG_STATE_NOT_REGISTERED) || (state == CREG_STATE_SEARCHING)) {
                if (debug) {
                    printf("Network registration in progress (Status: %d). Waiting...\r\n", state);
                }
                // Delay to allow modem time to register
                systick_delay_ms(POLLING_INTERVAL); 
            } 
            else {  // Handle all failure, denied, or unknown states
                if (debug) {
                    printf("Network registration failed or denied (Status: %d). Stopping attempts.\r\n", state);
                }
                sim_registered = 0;
                break; // Exit loop on critical state
            }

        } else {  // Handle all other codes (AT_TIMEOUT, AT_ERROR, etc.)
            if (debug) printf("Waiting for network registration. Status: %u. Retrying...\r\n", resp);
            // Delay to allow modem time to register
            systick_delay_ms(POLLING_INTERVAL); 
        }
    }

    if (sim_registered == 0) {
        if (debug) printf("Failed to register on network after %u attempts.\r\n", registration_attempts);
        return --rv;
    }
    systick_delay_ms(1000);     // Wait 1 second

    // Query Signal Quality Information: RSSI and BER 
    resp = send_at("AT+CSQ\r", 500, rx_buf, RX_BUF_SIZE, debug);
    if (resp == AT_INFO_CSQ) {
        // ToDo: parse the csq 

    } else {    // Handle all other codes (AT_TIMEOUT, AT_ERROR, etc.)
        if (debug) printf("Failed to query Signal Quality information,. Status code: %d.\r\n", resp);
        return --rv;
    }
    systick_delay_ms(1000);     // Wait 1 second
    
    // Initialize GPS module 
    resp = send_at("AT+CGPS?\r", 500, rx_buf, RX_BUF_SIZE, debug); // Query GPS engine status
    if (resp == AT_INFO_CGPS) {
        CgpsState_t state = parse_cgps_status(rx_buf);
        if (state == CGPS_STATE_OFF) {
            if (debug) printf("GPS is OFF, trying to enable...\r\n");
            resp = send_at("AT+CGPS=1\r", 500, rx_buf, RX_BUF_SIZE, debug);
            if (resp == AT_OK) {
                if (debug) printf("GPS engine enabled.\r\n");
            } else {
                if (debug) printf ("Failed to enable GPS.\r\n");
                return --rv;
            }
        } else if ((state == CGPS_STATE_ON_STANDALONE)  ||
                (state == CGPS_STATE_ON_AGPS_UE)        ||
                (state == CGPS_STATE_ON_AGPS_ASSIST)) {

            if (debug) printf("GPS engine enabled.\r\n");       
        }
    } else { 
        printf("Failed to query GPS engine status. Status Code: %d.\r\n", resp);
        return --rv;
    }
    systick_delay_ms(1000);     // Wait 1 second

    // ToDo: init http engine 

    return rv;   // 0 on success 
}

