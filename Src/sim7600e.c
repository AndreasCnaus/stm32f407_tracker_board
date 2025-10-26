#include "sim7600e.h"
#include "uart.h"
#include "systick.h"

#include <string.h>
#include <stdio.h>


#define TX_TIMEOUT_MS       100 // TX timeout in miliseconds 
#define TX_BUF_SIZE         32  // Size for the AT command reques
#define RX_BUF_SIZE         64  // Size for the AT command response
#define IPV6_ADDR_MAX_LEN   40  // For full IPv6 address string
#define HTTP_URL_MAX_LEN    128 // For HTTP-URL strng  

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

    // CREG (Network Registration: Circuit Switched (CS) Group)
    {"+CREG: ",             AT_INFO_CREG},          // Matches +CREG: <n>,<stat>
    // CGATT (Network Registration: Packet Switched (PS) Group)
    {"+CGATT: ",            AT_INFO_CGATT},
    // CGPADDR (IP Address confirmation)
    {"+CGPADDR: ",          AT_INFO_CGPADDR},

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

// Corresponds to the order of CsqRssiState_t enum (0, 1, 2, 3, 4, 5)
const char * const RSSI_STATE_STRINGS[] = {
    "EXCELLENT (>= -77 dBm)",
    "GOOD (-97 dBm to -79 dBm)",
    "MARGINAL (-111 dBm to -99 dBm)",
    "MINIMAL (<= -113 dBm)",
    "UNKNOWN (99)",
    "INVALID_PARSE_ERROR"
};

// Corresponds to the order of CsqBerState_t enum (0, 1, 2, 3, 4, 5)
const char * const BER_STATE_STRINGS[] = {
    "EXCELLENT (0)",
    "GOOD (1-2)",
    "ACCEPTABLE (3-4)",
    "POOR (5-7)",
    "UNKNOWN_LTE_NA (99)",
    "INVALID_PARSE_ERROR"
};

// Function Pointer Type definitions
typedef int (*uart_tx_char_t)(int ch);
typedef int (*uart_rx_char_t)(void);

// Forward declarations
AtResponseStatus_t send_at(const char *cmd, uint32_t rx_timeout_ms, char *rx_buf, size_t rx_buf_size, uint8_t debug);
AtResponseStatus_t parse_at_response(const char *response, uint8_t debug);
int sim7600e_write_command(uart_tx_char_t tx_func_nb, const char *cmd, size_t len, uint32_t timeout_ms);
char* sim7600e_read_full_response(uart_rx_char_t rx_func_nb, char *out_buf, size_t max_len, uint32_t timeout_ms);
CregState_t parse_creg_status(const char *response_str);
CgpsState_t parse_cgps_status(const char *response_str);
CsqState_t parse_csq_status(const char *response_str, CsqResult_t *result);
int sim7600e_eval_sq_result(CsqResult_t *result, uint8_t debug);
CgattState_t parse_cgatt_status(const char *response_str);
CgpaddrState_t parse_cgpaddr_status(const char *response_str, char *ip_addr);

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

// Parse Network Registration Status response 
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

// Parse GPS Status response 
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

// Parse Signal Quality response
CsqState_t parse_csq_status(const char *response_str, CsqResult_t *result)
{
    const char *info_prefix = "+CSQ: ";
    int raw_rssi = -1;
    int raw_ber = -1;
    
    // Check input parameters
    if (response_str == NULL || result == NULL) {
        return CSQ_STATE_INVALID;
    }

    // Locate the specific information line
    const char *start_pos = strstr(response_str, info_prefix);
    if (start_pos == NULL) {
        return CSQ_STATE_INVALID;
    }

    // Advance the pointer past the "+CSQ: " prefix
    start_pos += strlen(info_prefix);

    // Attempt to read the two required integers (RSSI and BER)
    int scan_count = sscanf(start_pos, "%d,%d", &raw_rssi, &raw_ber);
    if (scan_count != 2) {
        return CSQ_STATE_INVALID;
    }
    
    // Store raw values in the result structure for debugging/logging
    result->raw_rssi = raw_rssi;
    result->raw_ber = raw_ber;

    // Map RSSI to the appropriate status
    if (raw_rssi >= 20 && raw_rssi <= 31) {
        result->rssi_state = RSSI_STATE_EXCELLENT;
    }
    else if (raw_rssi >= 10 && raw_rssi <= 19) {
        result->rssi_state = RSSI_STATE_GOOD;
    }
    else if (raw_rssi >= 2 && raw_rssi <= 9) {
        result->rssi_state = RSSI_STATE_MARGINAL;
    }
    else if (raw_rssi >= 0 && raw_rssi <= 1) {
        result->rssi_state = RSSI_STATE_MINIMAL;
    }
    else if (raw_rssi == 99) {
        result->rssi_state = RSSI_STATE_UNKNOWN;
    }
    else {
        result->rssi_state = RSSI_STATE_INVALID;
    }

    // 5. Map BER to the appropriate status (using correct logic)
    if (raw_ber == 0) {
        result->ber_state = BER_STATE_EXCELLENT;
    }
    else if (raw_ber >= 1 && raw_ber <= 2) {
        result->ber_state = BER_STATE_GOOD;
    }
    else if (raw_ber >= 3 && raw_ber <= 4) { // Corrected: 3 and 4
        result->ber_state = BER_STATE_ACCEPTABLE;
    }
    else if (raw_ber >= 5 && raw_ber <= 7) { // Corrected: 5, 6, 7
        result->ber_state = BER_STATE_POOR;
    }
    else if (raw_ber == 99) {
        result->ber_state = BER_STATE_UNKNOWN;
    }
    else {
        result->ber_state = BER_STATE_INVALID;
    }

    return CSQ_STATE_OK;
}

// Evaluate Sqignal Quality results 
int sim7600e_eval_sq_result(CsqResult_t *result, uint8_t debug)
{
    int rv = 0;

    // Input parameter check 
    if (result == NULL) {
        if (debug) printf ("Error: Signal Quality result is null.\r\n");
        return --rv;
    }

    // Evaluate RSSI (Signal Strength)
    switch (result->rssi_state) {
        case RSSI_STATE_EXCELLENT:
        case RSSI_STATE_GOOD: {
            if (debug) printf("Signal Report: RSSI is %s. (Raw: %d).\r\n", 
                             RSSI_STATE_STRINGS[result->rssi_state], result->raw_rssi);
        
        } break;
        case RSSI_STATE_UNKNOWN: {
             if (debug) printf("Error: RSSI is %s (Raw: %d). Cannot establish connection yet. Stopping.\r\n", 
                             RSSI_STATE_STRINGS[result->rssi_state], result->raw_rssi);
            return --rv;
        }
        case RSSI_STATE_MARGINAL:
        case RSSI_STATE_MINIMAL:
        case RSSI_STATE_INVALID:
        default: {
             if (debug) printf("Error: RSSI is %s (Raw: %d). Signal too weak/invalid. Stopping.\r\n", 
                             RSSI_STATE_STRINGS[result->rssi_state], result->raw_rssi);
            return --rv;
        }
    }

    // Evaluate BER (Bit Error Rate)
    switch (result->ber_state) {
        case BER_STATE_EXCELLENT:
        case BER_STATE_GOOD:
        case BER_STATE_ACCEPTABLE: {
            if (debug) printf("Signal Report: BER is %s. (Raw: %d). Quality is OK.\r\n",
                             BER_STATE_STRINGS[result->ber_state], result->raw_ber);
        } break;
        case BER_STATE_UNKNOWN: {
            // BER 99 is common/acceptable on LTE if RSSI is already determined to be good/excellent.
            if (debug) printf("Signal Report: BER is %s (Raw: %d). Accepted due to strong RSSI.\r\n", 
                                BER_STATE_STRINGS[result->ber_state], result->raw_ber);
        } break;
        case BER_STATE_POOR:
        case BER_STATE_INVALID:
        default: {
            if (debug) printf("Error: BER is %s (Raw: %d). Poor quality/invalid value. Stopping.\r\n", 
                             BER_STATE_STRINGS[result->ber_state], result->raw_ber);
            return --rv;
        }
    }

    if (debug) printf("Signal quality check PASSED. Proceeding...\r\n");
    return 0; // Success
}

// Parse Network attachment status 
CgattState_t parse_cgatt_status(const char *response_str)
{
    // Input parameter check 
    if (response_str == NULL) {
        return CGATT_STATE_INVALID;
    }

    const char *info_prefix = "+CGATT: ";
    int state = -1;

    // Locate the specific information line
    const char *start_pos = strstr(response_str, info_prefix);
    if (start_pos == NULL) {
        return CGATT_STATE_INVALID;
    }

    // Advance the pointer past the "+CGATT: " prefix
    start_pos += strlen(info_prefix);

    // Attempt to read the two required integers <state>
    int scan_count = sscanf(start_pos, "%d", &state);
    if (scan_count != 1) {
        return CGATT_STATE_INVALID;
    }

    // Map <state> to the enum 
    if (state == 0) {
        return CGATT_STATE_DETACHED;
    } else if (state == 1) {
        return CGATT_STATE_ATTACHED;
    } else {
        // Handle unexpected values
        return CGATT_STATE_INVALID; 
    }
}

// Parse IP-Address confirmation 
CgpaddrState_t parse_cgpaddr_status(const char *response_str, char *ip_addr)
{
    // Check input parameters for NULL
    if (response_str == NULL || ip_addr == NULL) {
        return CGPADDR_STATE_INVALID;
    }

    const char *info_prefix = "+CGPADDR: ";
    int cid = -1;
    
    // Prevent garbage data if parsing fails
    ip_addr[0] = '\0'; 

    // Locate the specific information line
    const char *start_pos = strstr(response_str, info_prefix);
    if (start_pos == NULL) {
        // Did not find the informational prefix.
        return CGPADDR_STATE_INVALID;
    }

    // Advance the pointer past the "+CGPADDR: " prefix
    start_pos += strlen(info_prefix);

    // Extract the CID and IP-Address appropriately 
    int scan_count = sscanf(start_pos, "%d,%[0-9.]", &cid, ip_addr);

    // We must successfully read the CID and the IP string (2 items)
    if (scan_count != 2) {
        printf("Scan count error: %d\r\n", scan_count);
        return CGPADDR_STATE_INVALID;
    }

    // Check if the extracted IP address string is empty ("")
    if (ip_addr[0] == '\0') {
        return CGPADDR_STATE_NOT_ACTIVE;
    }

    return CGPADDR_STATE_OK;    // success 
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
        if (debug) printf("[CFUN] Error: failed to trigger SIM7600E reset. Continuing with caution.\r\n");
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
        if (debug) printf("[AT] Error: Initial communication attempt with SIM7600E failed.\r\n");
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
            if (debug) printf("[CPIN] Failed to unlock SIM\r\n");
            return --rv;
        }
    } else if (resp == AT_CPIN_SIM_PUK) {
        if (debug) printf("[CPIN] Error: SIM is PUK-locked. Manual intervention required.\r\n");
        return --rv;
    } else {
        // Catch-all for NOT INSERTED, etc.
        if (debug) printf("[CPIN] Error: response not supported or failure (Code: %d).\r\n", resp); 
        return --rv; 
    }
    systick_delay_ms(5000);    // Wait 5 seconds to stabilize 
    uart1_flush_rx_buffer();    // Flush the UART1 RX-Buffer (erase the unsolicited status codes)

    // Register SIM on the Network: CS Domain
    const uint8_t registration_attempts = 10;
    uint8_t sim_registered = 0;

    if (debug) printf("Attempting network registration...\r\n");

    const uint32_t INITIAL_CREG_TIMEOUT = 5000; // 5 seconds for the very first check
    const uint32_t POLLING_CREG_TIMEOUT = 1000; // 1 second for subsequent checks
    const uint32_t POLLING_INTERVAL = 3000;     // 3 seconds between checks

    for (uint8_t i = 0; i < registration_attempts; i++) {

        // Adaptive Timeout: Use 5s for the first attempt (i=0), 1s thereafter.
        uint32_t current_timeout_ms = (i == 0) ? INITIAL_CREG_TIMEOUT : POLLING_CREG_TIMEOUT;

        // Check modem's attachment to the Voice and Network domain 
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
        if (debug) printf("[CREG] Failed to register on network after %u attempts.\r\n", registration_attempts);
        return --rv;
    }
    systick_delay_ms(1000);     // Wait 1 second

    // Query Signal Quality Information: RSSI and BER
    resp = send_at("AT+CSQ\r", 1000, rx_buf, RX_BUF_SIZE, debug);

    if (resp == AT_INFO_CSQ) {
        CsqResult_t sq_result;
        
        // Parse the response
        CsqState_t state = parse_csq_status(rx_buf, &sq_result);

        if (state == CSQ_STATE_OK) {
            // Evaluate the parsed result
            if (sim7600e_eval_sq_result(&sq_result, debug) != 0) {
                // Evaluation failed (e.g., RSSI is Marginal or Unknown)
                if (debug) printf("Signal quality evaluation failed. Aborting initialization.\r\n");
                return --rv;
            }
            // Signal quality passed evaluation. Continue initialization.
            
        } else {
            // Critical: Failed to parse the +CSQ: line structure
            if (debug) printf("[CSQ] Failed to parse Signal Quality result from response: %s\r\n", rx_buf); 
            return --rv; // Abort on parsing failure
        }
    } else { 
        // Handle all other codes (AT_TIMEOUT, AT_ERROR, etc.)
        if (debug) printf("[CSQ] Failed to query Signal Quality information. Status code: %d.\r\n", resp);
        return --rv; // Abort on command execution failure
    }
    systick_delay_ms(1000);     // Wait 1 second

    // -- Initialize HTTP (Data Attached: PS Domain) --
    resp = send_at("AT+CGATT?\r", 1000, rx_buf, RX_BUF_SIZE, debug);

    if (resp == AT_INFO_CGATT) {
        CgattState_t state = parse_cgatt_status(rx_buf);
        
        if (state == CGATT_STATE_ATTACHED) {
            if (debug) printf("Data Network (PS Domain) is already attached. Proceeding.\r\n");
        
        } else if (state == CGATT_STATE_DETACHED) {
            if (debug) printf("Data Network (PS Domain) is detached. Attempting to attach...\r\n");

            // Attempting to attach to PS Domain. Using 5s timeout as attachment may take time.
            resp = send_at("AT+CGATT=1\r", 5000, rx_buf, RX_BUF_SIZE, debug);
            
            if (resp == AT_OK) {
                if (debug) printf("Data Network attached successfully (AT+CGATT=1). \r\n");
            } else {
                if (debug) printf("[CGATT] Failed to attach to PS Domain (AT+CGATT=1). Status code: %d.\r\n", resp);
                return --rv;
            }

        } else {
            // Handle parsing failure or an unknown state value
            if (debug) printf("[CGATT] Failed to parse CGATT status or received invalid state. Status code from Query: %d.\r\n", resp);
            return --rv;
        }

    } else {
        // Handle failures of the initial query (AT_TIMEOUT, AT_ERROR, etc.)
        if (debug) printf("[CGATT] Failed to query Data Network attachment status. Status code: %d.\r\n", resp);
        return --rv;
    }
    systick_delay_ms(1000);     // Wait 1 second

    // -- Data Connection Setup --
    // Delete the old context for data connection 
    resp = send_at("AT+CGDCONT=1\r", 500, rx_buf, RX_BUF_SIZE, debug); 
    
    if (resp == AT_OK) {
        if (debug) printf("PDP context 1 deleted successfully.\r\n");
    } else {
        // This can fail, as the context might not exist yet. 
        if (debug) printf("[CGDCONT] Warning: Failed to delete context. Status code: %d. Proceeding...\r\n", resp);
        // no error return 
    }

    // Define new context for data connection: Context ID 1, IP protocol, APN 'internet'
    const char *apn_cmd = "AT+CGDCONT=1,\"IP\",\"internet\"\r";
    resp = send_at(apn_cmd, 500, rx_buf, RX_BUF_SIZE, debug); 
    
    if (resp == AT_OK) {
        if (debug) printf("New context set: %s.\r\n", apn_cmd);
    } else {
        if (debug) printf("[CGDCONT] Failed to set context. Command: %s. Status: %d.\r\n", apn_cmd, resp);
        return --rv;
    }

    // Activate the new context for data connection 
    resp = send_at("AT+CGACT=1,1\r", 500, rx_buf, RX_BUF_SIZE, debug);
    
    if (resp == AT_OK) {
        if (debug) printf("New Context was successfully activated.\r\n");
    } else {
        if (debug) printf("[CGACT] Failed to activate new context. Status code: %d.\r\n", resp);
        return --rv;
    }

    // Confirm the assigned IP address
    resp = send_at("AT+CGPADDR=1\r", 500, rx_buf, RX_BUF_SIZE, debug);

    if (resp == AT_INFO_CGPADDR) {
        // A valid info line was received. Now, check the content.
        char ip_addr[IPV6_ADDR_MAX_LEN]; 
        
        CgpaddrState_t state = parse_cgpaddr_status(rx_buf, ip_addr);

        if (state == CGPADDR_STATE_OK) {
            // Success: A non-empty IP address was found and copied.
            if (debug) printf("Assigned IP-Address: %s.\r\n", ip_addr);

        } else if (state == CGPADDR_STATE_NOT_ACTIVE) {
            // Failure: The context is defined but the IP field was empty ("").
            if (debug) printf("[CGPADDR] PDP Context 1 defined, but NOT ACTIVE (IP is empty). Cannot proceed to data.\r\n");
            return --rv;

        } else { // CGPADDR_STATE_INVALID
            // Failure: Parsing failed (malformed response content).
            if (debug) printf("[CGPADDR] Failed to parse +CGPADDR: response content format.\r\n");
            return --rv;
        }
    } else {
        // Handle failures of the initial query (AT_TIMEOUT, AT_ERROR, etc.)
        if (debug) printf("[CGPADDR] Failed to query IP-Address. Status code: %d.\r\n", resp);
        return --rv;
    }
    systick_delay_ms(1000);     // Wait 1 second

    // Attempt to terminate HTTP service first, just in case it's already active.
    // Ignore the result of TERM, as we just want to ensure it's clean.
    send_at("AT+HTTPTERM\r", 300, rx_buf, RX_BUF_SIZE, debug); 

    // Initialize HTTP (start HTTP service and allocate modem resources)
    resp = send_at("AT+HTTPINIT\r", 500, rx_buf, RX_BUF_SIZE, debug); 

    if (resp == AT_OK) {
        if (debug) printf("HTTP service successfully initialized.\r\n");
    } else {
        if (debug) printf("[HTTPINIT] Failed to initialize HTTP service. Status code: %d.\r\n", resp);
        return --rv;
    }

    
    // Set HTTP Content-Type parameter
    // send_at("AT+HTTPPARA=\"CONTENT\",\"application/octet-stream\"\r", 500, rx_buf, RX_BUF_SIZE, debug);
    const char *http_ctype_cmd = "AT+HTTPPARA=\"CONTENT\",\"application/octet-stream\"\r";
    resp = send_at(http_ctype_cmd, 500, rx_buf, RX_BUF_SIZE, debug);

    if (resp == AT_OK) {
        if (debug) printf ("HTTP Content-Type successfully set.\r\n");
    } else {
        if (debug) printf ("[HTTPPARA] Failed to set HTTP Content-Type. Status Code: %d.\r\n", resp);
        return --rv;
    }

    // Set HTTP URL parameter
    char http_url_cmd[HTTP_URL_MAX_LEN]; 
    int chars_written = snprintf(
    http_url_cmd,
    HTTP_URL_MAX_LEN,
    "AT+HTTPPARA=\"URL\",\"%s\"\r",
    url
    );

    // Check for truncation error
    if (chars_written < 0 || chars_written >= HTTP_URL_MAX_LEN) {
        if (debug) printf ("[HTTPPARA] The URL command string was too long or invalid.\r\n");
        return --rv;
    }

    // Send the command
    resp = send_at(http_url_cmd, 500, rx_buf, RX_BUF_SIZE, debug);

    if (resp == AT_OK) {
        if (debug) printf ("HTTP URL parameter successfully set.\r\n");
    } else {
        if (debug) printf("[HTTPPARA] Failed to set HTTP URL parameter. Status code: %d.\r\n", resp);
        return --rv;
    }
    systick_delay_ms(1000);     // Wait 1 second

    // -- Initialize GPS module -- 
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

    return rv;   // 0 on success 
}
