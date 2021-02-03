#ifndef H_CONTROLLER_
#define H_CONTROLLER_

#include "driver/spi_master.h"

void controller_init(void);
void controller_start (spi_device_handle_t *spi);

int get_temperature();
void set_target_temperature(int value);

#endif
