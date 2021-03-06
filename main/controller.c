#include "esp_log.h"
#include <stdio.h>
#include <limits.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "bler946.h"
#include "controller.h"
#include "max31855.h"
#include "ui.h"

static const char *tag = "Controller";

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define GPIO_INPUT_ZEROCROSS  4
#define GPIO_OUTPUT_OPTOCOUPLER 33
#define ESP_INTR_FLAG_DEFAULT 0

static atomic_int temperature;
static atomic_int target_temperature;

#ifndef CONFIG_ZERO_CROSSING_DRIVER
static TaskHandle_t firing_handle;

static rmt_config_t firing_conf;
static rmt_item32_t firing_pulse;

static atomic_uint pulse_delay;

static void IRAM_ATTR zerocross_isr_handler() {
    static unsigned long last_time = 0;
    unsigned long cross_time = esp_timer_get_time();
    unsigned long elapsed = cross_time - last_time;
    if (elapsed < 2500) {
        return;
    }
    last_time = cross_time;
    xTaskNotifyFromISR(firing_handle, cross_time, eSetValueWithOverwrite, NULL);
    portYIELD_FROM_ISR();
    return;
}

void firing_task(void *param) {
    firing_handle = xTaskGetCurrentTaskHandle();
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_ZEROCROSS, zerocross_isr_handler, (void*) GPIO_INPUT_ZEROCROSS);

    TickType_t xLastWakeTime = xTaskGetTickCount ();
    for( ;; ) {
        uint32_t isr_time;
        xTaskNotifyWait(0, ULONG_MAX, &isr_time, portMAX_DELAY);
        uint32_t elapsed_time = esp_timer_get_time() - isr_time;

        firing_pulse.duration0 = atomic_load(&pulse_delay);
        ESP_ERROR_CHECK(rmt_write_items(firing_conf.channel, &firing_pulse, 1, 0));
    }
}
#endif // !CONFIG_ZERO_CROSSING_DRIVER

int get_temperature() {
    return atomic_load(&temperature);
}

void set_target_temperature(int value) {
    atomic_store(&target_temperature, value);
}

void controller_task(void *param) {
    spi_device_handle_t *spi = (spi_device_handle_t*)param;
    max31855_data_t data;
    const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
    for( ;; )
    {
        max31855_read(*spi, &data);
        // LSB = 0.25 degrees C
        int centigrade = data.thermocouple_temp>>2;
        atomic_store(&temperature, centigrade);
        ui_display_temperature(centigrade);
        ESP_LOGI(tag, "Temperature: %i", centigrade);
        int target = atomic_load(&target_temperature);
#ifdef CONFIG_ZERO_CROSSING_DRIVER
        if (centigrade < target) {
            gpio_set_level(GPIO_OUTPUT_OPTOCOUPLER, 1);
        } else {
            gpio_set_level(GPIO_OUTPUT_OPTOCOUPLER, 0);
        }
#else
        if (centigrade < target) {
            // pulse_delay should be clamped between 1400 and 9600 @ 100Hz
            atomic_store(&pulse_delay, 1400);
        } else {
            atomic_store(&pulse_delay, 0);
        }
#endif
        bler_tx_temperature(centigrade);
        vTaskDelay(xDelay);
    }
}

void controller_start (spi_device_handle_t *spi) {
    static TaskHandle_t controller_handle;
    xTaskCreate(&controller_task, "controller_task", 8192, spi, 1, &controller_handle);
#ifndef CONFIG_ZERO_CROSSING_DRIVER
    static TaskHandle_t firing_handle;
    xTaskCreate(&firing_task, "firing_task", 8192, NULL, configMAX_PRIORITIES-1, &firing_handle);
#endif
}

void controller_init (void) {
    atomic_init(&temperature, 0);
    atomic_init(&target_temperature, 0);
#ifdef CONFIG_ZERO_CROSSING_DRIVER
    gpio_config_t opto_io = {0};
    opto_io.pin_bit_mask = 1ULL<<GPIO_OUTPUT_OPTOCOUPLER;
    opto_io.mode = GPIO_MODE_OUTPUT;
    gpio_config(&opto_io);
#else
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;
    io_conf.pin_bit_mask = 1ULL<<GPIO_INPUT_ZEROCROSS;
    io_conf.mode = GPIO_MODE_INPUT;
    //io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    firing_conf.rmt_mode = RMT_MODE_TX;
    firing_conf.channel = RMT_CHANNEL_0;
    firing_conf.gpio_num = GPIO_OUTPUT_OPTOCOUPLER;
    firing_conf.mem_block_num = 1;
    firing_conf.tx_config.loop_en = 0;
    firing_conf.tx_config.carrier_en = 0;
    firing_conf.tx_config.idle_output_en = 1;
    firing_conf.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    firing_conf.clk_div = 80; // 80MHz / 80 = 1MHz or 1uS per count
    esp_err_t ret;
    ret = rmt_config(&firing_conf);
    ret = rmt_driver_install(firing_conf.channel, 0, 0);

    firing_pulse.duration0 = 0;
    firing_pulse.level0 = 0;
    firing_pulse.duration1 = 100; // pulse duration
    firing_pulse.level1 = 1;

    atomic_init(&pulse_delay, 0);
#endif
}
