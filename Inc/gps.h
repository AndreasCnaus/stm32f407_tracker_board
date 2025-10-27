#ifndef GPS_H_
#define GPS_H_

// #include "pack.h" ToDo: add pack.h 
#include <stdint.h>

#define GPS_PACKET_SIZE     18

typedef struct {
    float latitude;
    float longitude;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t altitude;
    uint16_t speed;
} gps_data_t;

int parse_gps_info(const char *gps_info, gps_data_t *gps_data, uint8_t debug);
// uint32_t pack_gps(uint8_t *buf, gps_data_t *data);
// void unpack_gps(uint8_t *buf, gps_data_t *data);

#endif