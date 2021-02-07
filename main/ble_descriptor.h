#ifndef H_BLE_DESCRIPTOR_
#define H_BLE_DESCRIPTOR_

#include <stdint.h>

typedef struct ble2904_data_t {
	uint8_t  format;
	int8_t   exponent;
	uint16_t unit;      // See https://www.bluetooth.com/specifications/assigned-numbers/units
	uint8_t  namespace;
	uint16_t description;
} __attribute__((packed)) ble2904_data_t;

#endif
