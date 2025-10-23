#ifndef SIM7600E_H_
#define SIM7600E_H_

#include <stdint.h>
#include <stddef.h>


typedef enum {
    // ----------------------------------------------------------------------
    // 0x00 - 0x0F: Final Result Codes (Standard AT V.250 & 3GPP)
    // ----------------------------------------------------------------------
    AT_OK = 0x00,               // Final success response ("OK\r\n")
    AT_ERROR = 0x01,            // Generic command execution error ("ERROR\r\n")
    AT_NO_CARRIER = 0x02,       // Data connection failed or dropped ("NO CARRIER\r\n")
    AT_CONNECT = 0x03,          // Data connection established ("CONNECT\r\n")
    AT_CME_ERROR = 0x04,        // Extended cellular error ("+CME ERROR: <n>")
    AT_CMS_ERROR = 0x05,        // Extended messaging error ("+CMS ERROR: <n>")
    AT_DOWNLOAD_READY = 0x06,   // Modem ready to receive binary data ("DOWNLOAD\r\n")
    AT_NO_RESPONSE = 0x07,      // No data received from modem (distinguished from AT_TIMEOUT)

    // ----------------------------------------------------------------------
    // 0x10 - 0x1F: CPIN (Specific Statuses - Matched in Lookup Table)
    // ----------------------------------------------------------------------
    AT_CPIN_READY = 0x10,       // Matches "+CPIN: READY"
    AT_CPIN_SIM_PIN = 0x11,     // Matches "+CPIN: SIM PIN"
    AT_CPIN_SIM_PUK = 0x12,     // Matches "+CPIN: SIM PUK"
    AT_CPIN_PH_SIM_PIN = 0x13,  // Matches "+CPIN: PH-SIM PIN"
    AT_INFO_CPIN = 0x14,        // GENERIC match for "+CPIN: " 

    // ----------------------------------------------------------------------
    // 0x20 - 0x2F: Network Registration/Attachment Info (CREG/CGATT)
    // ----------------------------------------------------------------------
    AT_INFO_CREG = 0x20,        // GENERIC match for "+CREG: "
    AT_INFO_CGATT = 0x21,       // GENERIC match for "+CGATT: " (Critical for data)

    // ----------------------------------------------------------------------
    // 0x30 - 0x3F: Protocol Statuses (HTTP/TCP/PDP Context)
    // ----------------------------------------------------------------------
    AT_HTTP_ACTION = 0x30,      // Matches "+HTTPACTION:"
    AT_NETOPEN_SUCCESS = 0x31,  // Matches "+NETOPEN:"
    AT_IP_ADDRESS = 0x32,       // Matches "+QIBASTCP:"

    // ----------------------------------------------------------------------
    // 0x40 - 0x4F: CGPS (Generic Info - Requires Detailed Parsing)
    // ----------------------------------------------------------------------
    AT_INFO_CGPS = 0x40,        // GENERIC match for "+CGPS: "
    AT_INFO_CGPSINFO = 0x41,    // GENERIC match for "+CGPSINFO:"

    // ----------------------------------------------------------------------
    // 0x50 - 0x5F: URCs and Common Informational Codes
    // ----------------------------------------------------------------------
    AT_URC_RDY = 0x50,          // Matches "RDY" (Unsolicited)
    AT_URC_SMS_DONE = 0x51,     // Matches "SMS DONE" (Unsolicited)
    AT_URC_RING = 0x52,         // Matches "RING" (Unsolicited)
    AT_INFO_CSQ = 0x53,         // GENERIC match for "+CSQ: " (Signal Quality)
    
    // ----------------------------------------------------------------------
    // 0xF0 - 0xFF: Local System/Internal Statuses
    // ----------------------------------------------------------------------
    AT_TIMEOUT = 0xF0,          // Overall response timeout reached
    AT_RX_PARTIAL = 0xF1,       // Line received but not a definitive final status
    AT_TX_FAILURE = 0xF2,       // Failure during command transmission
    AT_INVALID_PARAM = 0xF3,    // Function called with a bad parameter
    AT_PARSING_FAILURE = 0xF4,  // Response received but contains no recognizable status

} AtResponseStatus_t;

typedef enum {
    // 3GPP TS 27.007, Section 7.2
    CREG_STATE_NOT_REGISTERED = 0, // Not registered, MT is not currently searching for a new operator to register to
    CREG_STATE_HOME_NETWORK = 1,   // Registered, home network
    CREG_STATE_SEARCHING = 2,      // Not registered, but MT is currently trying to attach or searching
    CREG_STATE_DENIED = 3,         // Registration denied
    CREG_STATE_UNKNOWN = 4,        // Unknown (e.g., out of coverage area)
    CREG_STATE_ROAMING = 5,        // Registered, roaming
    
    // --- Additional Status Codes for Parsing Logic ---
    CREG_STATE_INVALID = 6        // Parsing failed (e.g., string format unexpected)
} CregState_t;

typedef enum {
    // --- Configuration States (Derived from AT+CGPS?) ---
    CGPS_STATE_OFF = 0,               // +CGPS: 0 (GPS engine is powered down)
    CGPS_STATE_ON_STANDALONE = 1,     // +CGPS: 1,1 (GPS is ON, Standalone mode)
    CGPS_STATE_ON_AGPS_UE = 2,        // +CGPS: 1,2 (GPS is ON, UE-based A-GPS mode)
    CGPS_STATE_ON_AGPS_ASSIST = 3,    // +CGPS: 1,3 (GPS is ON, UE-assisted A-GPS mode)

    // --- Positional Fix States (Derived from AT+CGPSINFO) ---
    CGPS_STATE_NO_FIX = 4,            // +CGPSINFO: ,,,,,,,, (GPS is ON but has not acquired a fix yet)
    CGPS_STATE_FIX_AVAILABLE = 5,     // +CGPSINFO: <data> (GPS is ON and has a valid coordinate fix)
    
    // --- Additional Status Codes for Parsing Logic ---
    CGPS_STATE_INVALID = 6            // Parsing failed or unexpected modem state
} CgpsState_t;

AtResponseStatus_t send_at(const char *cmd, uint32_t rx_timeout_ms, char *rx_buf, size_t rx_buf_size, uint8_t debug);
int sim7600e_init(const char *pin, const char *url, uint8_t debug);

#endif  // SIM7600E_H_