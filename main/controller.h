#ifndef H_CONTROLLER_
#define H_CONTROLLER_

#include <stdatomic.h>
#include "driver/spi_master.h"

extern atomic_int ato_target;
extern atomic_uint ato_half_ac_freq;

#define MAX_REFLOW_STEPS 5 // can fit in any BLE packet

typedef struct reflow_profile_t {
    struct {
        uint32_t duration: 16;
        uint32_t temperature: 16;
    } data[MAX_REFLOW_STEPS];
} reflow_profile_t;

void controller_init(void);
void controller_start(spi_device_handle_t *spi);

void reflow_start(void);
void reflow_stop(void);
bool reflow_is_running(void);

void store_profile(void);
void load_profile(void);
void set_profile(reflow_profile_t *profile);

int get_temperature();
void set_target_temperature(int value);

#endif
