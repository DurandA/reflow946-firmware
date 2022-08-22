#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "driver/rmt.h"
// #include "bler946.h"
#include "controller.h"
#include "max31855.h"
#include "ui.h"
#include "segments.h"

static const char *tag = "Controller";

#define STORAGE_NAMESPACE "storage"

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define LERP(a, b, f)  ((a + f * (b - a)))

#define GPIO_INPUT_ZEROCROSS  4
#define GPIO_OUTPUT_OPTOCOUPLER 33
#define ESP_INTR_FLAG_DEFAULT 0

static atomic_int temperature;
atomic_int ato_target;
atomic_uint ato_half_ac_freq;

static TaskHandle_t reflow_handle = NULL;
static reflow_profile_t reflow_profile = {0};

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
    atomic_store(&ato_target, value);
}

void reflow_task(void *param) {
    int last_temperature = 30;
    const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;

    set_dp(1);

    for (int step = 0; step < MAX_REFLOW_STEPS; step++) {
        ESP_LOGI(tag, "Ramping temperature to %i for %i s", reflow_profile.data[step].temperature, reflow_profile.data[step].duration);
        TickType_t step_start_time = xTaskGetTickCount();
        for ( ;; ) {
            TickType_t elapsed = xTaskGetTickCount() - step_start_time;
            TickType_t duration = reflow_profile.data[step].duration * 1000 / portTICK_PERIOD_MS;
            if (elapsed > duration)
                break;
            float interpolation = (float)elapsed / duration;
            int target_temperature = LERP(last_temperature, reflow_profile.data[step].temperature, interpolation);

            vTaskDelay(xDelay);
        }
        last_temperature = reflow_profile.data[step].temperature;
    }
    set_target_temperature(0);
    set_dp(0);

    reflow_handle = NULL;
    vTaskDelete(NULL);
}

void reflow_start() {
    if(reflow_handle == NULL){
        xTaskCreate(reflow_task, "reflow_task", 8192, NULL, 1, &reflow_handle);
    }
}

void reflow_stop() {
    if(reflow_handle != NULL){
        vTaskDelete(reflow_handle);
        reflow_handle = NULL;
        set_dp(0);
    }
}

bool reflow_is_running() {
    return reflow_handle != NULL;
}

void store_profile() {
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    ESP_ERROR_CHECK(err);
    err = nvs_set_blob(my_handle, "reflow_profile", &reflow_profile, sizeof(reflow_profile));
    ESP_ERROR_CHECK(err);
    err = nvs_commit(my_handle);
    ESP_ERROR_CHECK(err);

    nvs_close(my_handle);
}

void load_profile() {
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    ESP_ERROR_CHECK(err);
    size_t required_size = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "reflow_profile", NULL, &required_size);
    ESP_ERROR_CHECK(err);

    nvs_close(my_handle);
}

void set_profile(reflow_profile_t *profile) {
    memcpy(&reflow_profile, profile, sizeof(reflow_profile));
}

void controller_task(void *param) {
    spi_device_handle_t *spi = (spi_device_handle_t*)param;
    max31855_data_t data;
    const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
    for( ;; )
    {
        max31855_read(*spi, &data);
        // LSB = 0.25 degrees C
        int centigrade = data.thermocouple_temp >> 2;
        atomic_store(&temperature, centigrade);
        ui_display_temperature();
        int target = atomic_load(&ato_target);
        ESP_LOGI(tag, "Temperature: %i (target: %i)", centigrade, target);

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
        // bler_tx_temperature(centigrade);
        vTaskDelay(xDelay);
    }
}

static void IRAM_ATTR pcnt_ac_intr_handler(void *arg)
{
    static unsigned long last_time = 0;
    unsigned long pcnt_time = esp_timer_get_time();
    unsigned long elapsed = pcnt_time - last_time;
    uint32_t half_ac_freq = 100*100*1000000ULL/elapsed;
    atomic_store(&ato_half_ac_freq, half_ac_freq);
    last_time = pcnt_time;
    pcnt_counter_clear(PCNT_UNIT_0);
}

void controller_start (spi_device_handle_t *spi) {
    static TaskHandle_t controller_handle;
    xTaskCreate(controller_task, "controller_task", 8192, spi, 1, &controller_handle);
#ifndef CONFIG_ZERO_CROSSING_DRIVER
    static TaskHandle_t firing_handle;
    xTaskCreate(firing_task, "firing_task", 8192, NULL, configMAX_PRIORITIES-1, &firing_handle);
#endif
}

static void pcnt_ac_init()
{
    pcnt_unit_t unit = PCNT_UNIT_0;
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = GPIO_INPUT_ZEROCROSS,
        .ctrl_gpio_num = -1,
        .channel = PCNT_CHANNEL_0,
        .unit = unit,
        // What to do on the positive / negative edge of pulse input?
        .pos_mode = PCNT_COUNT_DIS,   // Keep the counter value on the positive edge
        .neg_mode = PCNT_COUNT_INC,   // Count up on the negative edge
        // Set the maximum and minimum limit values to watch
        .counter_h_lim = 100,
        .counter_l_lim = 0,
    };
    /* Initialize PCNT unit */
    pcnt_unit_config(&pcnt_config);

    /* Configure and enable the input filter */
    pcnt_set_filter_value(unit, 1023);
    pcnt_filter_enable(unit);

    pcnt_event_enable(unit, PCNT_EVT_H_LIM);

    /* Initialize PCNT's counter */
    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);

    /* Install interrupt service and add isr callback handler */
    pcnt_isr_service_install(0);
    pcnt_isr_handler_add(unit, pcnt_ac_intr_handler, (void *)unit);

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(unit);
}

void controller_init (void) {
    atomic_init(&temperature, 0);
    atomic_init(&ato_target, 25);
    atomic_init(&ato_half_ac_freq, 0);

    pcnt_ac_init();
#ifdef CONFIG_ZERO_CROSSING_DRIVER
    gpio_config_t opto_io = {0};
    opto_io.pin_bit_mask = 1ULL<<GPIO_OUTPUT_OPTOCOUPLER;
    opto_io.mode = GPIO_MODE_OUTPUT;
    gpio_config(&opto_io);
#else
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
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
