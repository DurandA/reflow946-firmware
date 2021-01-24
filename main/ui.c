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

static const char* TAG_BTN = "BTN_TEST";

atomic_int target_temperature = 30;
static TaskHandle_t ui_handle;

void button_ab_cb(void* arg)
{
    char* pstr = (char*) arg;
    if (*pstr == 'A') {
        atomic_fetch_sub(&target_temperature, 1);
    }
    if (*pstr == 'B') {
        atomic_fetch_add(&target_temperature, 1);
    }
    xTaskNotifyGive(ui_handle);
}

void ui_poll_task(void *param){
    uint32_t ulNotificationValue;
    long press_time = 0;
    ui_handle = xTaskGetCurrentTaskHandle();
    for( ;; )
    {
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 100/portTICK_RATE_MS);
        if (ulNotificationValue) {
            press_time = esp_timer_get_time();
        }
        if (press_time+1500*1000 < esp_timer_get_time()) {

        } else {
            int _target_temperature = atomic_load(&target_temperature);
            write_digits(_target_temperature);
        }
    }
}

void ui_init(void)
{
    xTaskCreate(ui_poll_task, "ui_task", 2048, NULL, 2, &ui_handle);
    button_handle_t btn_a_handle = iot_button_create(32, BUTTON_ACTIVE_LOW);
    iot_button_set_evt_cb(btn_a_handle, BUTTON_CB_PUSH, button_ab_cb, "A");
    iot_button_set_serial_cb(btn_a_handle, 1, 60/portTICK_RATE_MS, button_ab_cb, "A");
    button_handle_t btn_b_handle = iot_button_create(35, BUTTON_ACTIVE_LOW);
    iot_button_set_evt_cb(btn_b_handle, BUTTON_CB_PUSH, button_ab_cb, "B");
    iot_button_set_serial_cb(btn_b_handle, 1, 60/portTICK_RATE_MS, button_ab_cb, "B");
}
