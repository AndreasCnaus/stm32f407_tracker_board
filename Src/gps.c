#include "gps.h"
#include "my_stdio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
float nmea_to_decimal(float value, char dir);

float nmea_to_decimal(float value, char dir) {
    int degrees = (int)(value / 100.0);
    float minutes = value - (degrees * 100.0);
    float decimal = degrees + (minutes / 60.0);

    if (dir == 'S' || dir == 'W') {
        decimal = -decimal;
    }

    return decimal;
}

// Parses GPS info string into the given GPS data structure
int parse_gps_info(const char *gps_info, gps_data_t *gps_data, uint8_t debug)
{
    // Input parameter check
    if (gps_info == NULL || gps_data == NULL) {
        if (debug) printf("Error in parse_gps_info(): invalid input parameter.\r\n");
        return -1;
    }

    // Local buffers for data extraction
    char lat_str[16], lon_str[16];
    char alt_str[16], speed_str[16];
    char date_str[16], time_str[16];
    char ns, ew;
    int day, month, year;
    int hour, minute, second;

    // Initial extraction <lat>,<N|S>,<lon>,<E|W>,<date>,<time>,<alt>,<speed>,
    int scan_count = sscanf(
        gps_info,
        "%15[^,],%c,%15[^,],%c,%15[^,],%15[^,],%15[^,],%15[^,],",
        lat_str, &ns,
        lon_str, &ew,
        date_str, time_str,
        alt_str, speed_str
    );

    if (scan_count != 8) {
        if (debug) {
            printf("Parsing failed. Read %d fields (Expected 8).\r\n", scan_count);
            printf("Raw data causing error: %s\r\n", gps_info);
        }
        return -2;
    }

    // Convert numeric values
    float latitude  = nmea_to_decimal(atof(lat_str), ns);
    float longitude = nmea_to_decimal(atof(lon_str), ew);
    uint16_t altitude = (uint16_t)strtol(alt_str, NULL, 10);
    float speed = strtof(speed_str, NULL);

    // Store in output struct
    gps_data->latitude  = latitude;
    gps_data->longitude = longitude;
    gps_data->altitude  = altitude;
    gps_data->speed     = (uint16_t)(speed * 100); // scale to preserve 0.01 km/h resolution

    // Parse date (ddmmyy)
    scan_count = sscanf(date_str, "%2d%2d%2d", &day, &month, &year);
    if (scan_count != 3) {
        if (debug) printf("Failed to parse GPS Date String: '%s'\r\n", date_str);
        return -3;
    }

    gps_data->day = (uint8_t)day;
    gps_data->month = (uint8_t)month;
    gps_data->year = (uint8_t)year;

    // Parse time (hhmmss)
    scan_count = sscanf(time_str, "%2d%2d%2d", &hour, &minute, &second);
    if (scan_count != 3) {
        if (debug) printf("Failed to parse GPS Time String to numbers.\r\n");
        return -4;
    }

    gps_data->hour = (uint8_t)hour;
    gps_data->minute = (uint8_t)minute;
    gps_data->second = (uint8_t)second;

    if (debug) {
        lat_str[0] = '\0';  // prepare buffer 
        lon_str[0] = '\0';

        printf("Latitude: %s, Longitude: %s, Date: %02u/%02u/%02u, "
               "Time: %02u:%02u:%02u, Altitude: %u m, Speed: %u km/h\r\n",
               float_to_str(lat_str, gps_data->latitude, 6),    // convert back to string for print 
               float_to_str(lon_str, gps_data->longitude, 6),   // convert back to string for print 
               gps_data->day, gps_data->month, gps_data->year,
               gps_data->hour, gps_data->minute, gps_data->second,
               gps_data->altitude, gps_data->speed);
    }

    return 0; // Success
}
