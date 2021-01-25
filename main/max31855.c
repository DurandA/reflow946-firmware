#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp_log.h"
#include "driver/spi_master.h"
#include "max31855.h"

#define PIN_NUM_MISO 12
#define PIN_NUM_CLK  14
#define PIN_NUM_CS 15

static const char *TAG = "MAX31855";

void max31855_read(spi_device_handle_t spi, max31855_data_t *out)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8*4;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void*)1;

    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert( ret == ESP_OK );

    out->data = SPI_SWAP_DATA_RX(*(uint32_t*)t.rx_data, 32);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &out->data, 4, ESP_LOG_DEBUG);
}

void max31855_init(spi_device_handle_t *spi) {
    esp_err_t ret;

    static spi_bus_config_t buscfg = {
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=-1,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
    };
    static spi_device_interface_config_t devcfg = {
        .clock_speed_hz=4*1000*1000, //Clock out at 4 MHz
        .mode=0,                     //SPI mode 0
        .spics_io_num=PIN_NUM_CS,    //CS pin
        .queue_size=2,               //We want to be able to queue 2 transactions at a time
    };
    ret=spi_bus_initialize(VSPI_HOST, &buscfg, 1);
    ESP_ERROR_CHECK(ret);
    ret=spi_bus_add_device(VSPI_HOST, &devcfg, spi);
    ESP_ERROR_CHECK(ret);
}
