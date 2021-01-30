#include "stdio.h"
#include "stdatomic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "unity.h"
#include "iot_button.h"
#include "esp_system.h"
#include "esp_log.h"
#include "segments.h"

#define PIN_BTN_A 32
#define PIN_BTN_B 35
#define PIN_BTN_C 15

#define BIT_TEMPERATURE (1 << 0)
#define BIT_BTN_A (1 << 1)
#define BIT_BTN_B (1 << 2)
#define BIT_BTN_C (1 << 3)


atomic_int target_temperature;
static TaskHandle_t ui_handle;

void button_ab_cb(void* arg)
{
    char* pstr = (char*) arg;
    if (*pstr == 'A') {
        atomic_fetch_add(&target_temperature, 1);
        xTaskNotify(ui_handle, BIT_BTN_A, eSetBits);
    }
    if (*pstr == 'B') {
        atomic_fetch_sub(&target_temperature, 1);
        xTaskNotify(ui_handle, BIT_BTN_B, eSetBits);
    }
    if (*pstr == 'C') {
        xTaskNotify(ui_handle, BIT_BTN_C, eSetBits);
    }
}

void ui_display_temperature(uint32_t temperature){
    xTaskNotify(ui_handle, BIT_TEMPERATURE, eSetBits);
}

void ui_task(void *param){
    uint32_t ulNotificationValue;
    int temperature = 0;
    long press_time = 0;
    //ui_handle = xTaskGetCurrentTaskHandle();
    for( ;; )
    {
        xTaskNotifyWait(0x00, /* Don’t clear any notification bits on entry. */
                        ULONG_MAX, /* Reset the notification value to 0 on exit. */
                        &ulNotificationValue, /* Notified value pass out in ulNotifiedValue. */
                        100/portTICK_RATE_MS);

        if (ulNotificationValue & (BIT_BTN_A|BIT_BTN_B|BIT_BTN_C)) {
            press_time = esp_timer_get_time();
        }
        if (ulNotificationValue & (BIT_TEMPERATURE)) {
            //temperature = get_temperature();
        }
        if (press_time+1500*1000 < esp_timer_get_time()) {
            write_digits(temperature);
        } else {
            int _target_temperature = atomic_load(&target_temperature);
            write_digits(_target_temperature);
        }
    }
}

void ui_init(void)
{
    atomic_init(&target_temperature, 30);
    xTaskCreate(ui_task, "ui_task", 2048, NULL, 2, &ui_handle);
    button_handle_t btn_a_handle = iot_button_create(PIN_BTN_A, BUTTON_ACTIVE_LOW);
    iot_button_set_evt_cb(btn_a_handle, BUTTON_CB_PUSH, button_ab_cb, "A");
    iot_button_set_serial_cb(btn_a_handle, 1, 60/portTICK_RATE_MS, button_ab_cb, "A");
    button_handle_t btn_b_handle = iot_button_create(PIN_BTN_B, BUTTON_ACTIVE_LOW);
    iot_button_set_evt_cb(btn_b_handle, BUTTON_CB_PUSH, button_ab_cb, "B");
    iot_button_set_serial_cb(btn_b_handle, 1, 60/portTICK_RATE_MS, button_ab_cb, "B");
}
