#ifndef SIM7600E_H_
#define SIM7600E_H_

#include <stdint.h>

typedef enum {
    AT_OK = 0,               // Definitive success, typically from finding "OK\r\n"
    AT_ERROR = 1,            // Definitive failure, typically from finding "ERROR\r\n"
    
    // Statuses indicating successful completion of the command with specific data
    AT_DOWNLOAD_READY = 2,   // Command successful, modem ready for file download ("DOWNLOAD")
    AT_HTTP_ACTION = 3,      // Command successful, specific HTTP action status found ("+HTTPACTION")
    
    // CPIN SIM Card Statuses (Success codes for AT+CPIN?)
    // These statuses indicate a successful AT+CPIN? read but require specific action.
    AT_CPIN_READY = 4,       // SIM is ready and operational ("+CPIN: READY")
    AT_CPIN_SIM_PIN = 5,     // SIM requires a PIN ("+CPIN: SIM PIN")
    AT_CPIN_SIM_PUK = 6,     // SIM is blocked and requires a PUK ("+CPIN: SIM PUK")
    
    // Statuses related to communication failure (timeouts, bad buffers)
    AT_TIMEOUT = 10,         // Overall timeout reached while waiting for a response
    AT_TX_FAILURE = 11,      // Failure during the command transmission phase
    AT_RX_PARTIAL = 12,      // End of command response was reached but no final status code was found
    AT_INVALID_PARAM = 13    // Initial check failed (e.g., NULL buffer passed to function)
    
} AtResponseStatus_t;

AtResponseStatus_t send_at(const char *cmd, uint32_t rx_timeout_ms, uint8_t debug);
int sim7600e_init(const char *pin, const char *url, uint8_t debug);

#endif  // SIM7600E_H_