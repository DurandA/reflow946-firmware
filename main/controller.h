#ifndef H_CONTROLLER_
#define H_CONTROLLER_

#include <stdatomic.h>
#include "driver/spi_master.h"

extern atomic_int ato_target;
extern atomic_uint ato_half_ac_freq;

void controller_init(void);
void controller_start (spi_device_handle_t *spi);

int get_temperature();
void set_target_temperature(int value);

#endif
