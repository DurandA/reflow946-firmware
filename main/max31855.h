#ifndef H_MAX31855_
#define H_MAX31855_

#include <stdint.h>
#include "driver/spi_master.h"

typedef union {
    struct  {
        uint32_t oc : 1;
        uint32_t scg : 1;
        uint32_t scb : 1;
        uint32_t reserved0 : 1;
        uint32_t junction_temp : 12;
        uint32_t fault : 1;
        uint32_t reserved1 : 1;
        uint32_t thermocouple_temp : 13;
        uint32_t thermocouple_sign : 1;
    };
    uint32_t data;
} max31855_data_t;

void max31855_read(spi_device_handle_t spi, max31855_data_t *out);
int max31855_read_temperature(spi_device_handle_t spi);
void max31855_init(spi_device_handle_t *spi);

#endif
