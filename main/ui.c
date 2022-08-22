#include "stdio.h"
#include "stdatomic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "unity.h"
#include "iot_button.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "segments.h"
#include "controller.h"

#define PIN_BTN_A 32
#define PIN_BTN_B 35
#define PIN_BTN_C 25

#define BIT_TEMPERATURE (1 << 0)
#define BIT_BTN_A (1 << 1)
#define BIT_BTN_B (1 << 2)
#define BIT_BTN_C (1 << 3)


static TaskHandle_t ui_handle;

static void button_a_cb(void* arg)
{
    xTaskNotify(ui_handle, BIT_BTN_A, eSetBits);
}

static void button_b_cb(void* arg)
{
    xTaskNotify(ui_handle, BIT_BTN_B, eSetBits);
}

static void button_c_cb(void* arg)
{
    xTaskNotify(ui_handle, BIT_BTN_C, eSetBits);
}

void ui_display_temperature(){
    xTaskNotify(ui_handle, BIT_TEMPERATURE, eSetBits);
}

void ui_task(void *param){
    uint32_t ulNotificationValue;
    int temperature = 0;
    long press_time = 0;

    for( ;; )
    {
        xTaskNotifyWait(0x00, /* Don’t clear any notification bits on entry. */
                        ULONG_MAX, /* Reset the notification value to 0 on exit. */
                        &ulNotificationValue, /* Notified value pass out in ulNotifiedValue. */
                        100/portTICK_PERIOD_MS);

        long time = esp_timer_get_time();

        if (press_time + 1500 * 1000 < time) {
            write_digits(temperature);
        } else {
            int target;
            target = atomic_load(&ato_target);
            write_digits(target);
        }

        if (ulNotificationValue & (BIT_BTN_A|BIT_BTN_B|BIT_BTN_C)) {
            if (press_time + 100 * 1000 < time) {
                press_time = time;

                if (ulNotificationValue & (BIT_BTN_A)) {
                    atomic_fetch_add(&ato_target, 1);
                }
                if (ulNotificationValue & (BIT_BTN_B)) {
                    atomic_fetch_sub(&ato_target, 1);
                }
            }
        }

        if (ulNotificationValue & (BIT_BTN_C)) {
            if (!reflow_is_running()) {
                reflow_start();
            } else {
                reflow_stop();
            }
        }

        if (ulNotificationValue & (BIT_TEMPERATURE)) {
            temperature = get_temperature();
        }
    }
}

void ui_init(void)
{
    xTaskCreate(ui_task, "ui_task", 2048, NULL, 2, &ui_handle);

    button_config_t btn_a_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = PIN_BTN_A,
            .active_level = 0,
        },
    };
    button_handle_t btn_a_handle = iot_button_create(&btn_a_cfg);
    iot_button_register_cb(btn_a_handle, BUTTON_PRESS_DOWN, &button_a_cb, "A");
    iot_button_register_cb(btn_a_handle, BUTTON_LONG_PRESS_HOLD, &button_a_cb, "A");

    button_config_t btn_b_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = PIN_BTN_B,
            .active_level = 0,
        },
    };
    button_handle_t btn_b_handle = iot_button_create(&btn_b_cfg);
    iot_button_register_cb(btn_b_handle, BUTTON_PRESS_DOWN, &button_b_cb, "B");
    iot_button_register_cb(btn_b_handle, BUTTON_LONG_PRESS_HOLD, &button_b_cb, "B");

    button_config_t btn_c_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = PIN_BTN_C,
            .active_level = 0,
        },
    };
    button_handle_t btn_c_handle = iot_button_create(&btn_c_cfg);
    iot_button_register_cb(btn_c_handle, BUTTON_LONG_PRESS_START, &button_c_cb, "C");
}
