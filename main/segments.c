#include <stdio.h>
#include <inttypes.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "soc/timer_group_struct.h"
#include "driver/gptimer.h"
#include "segments.h"

#define GPIO_OUTPUT_DIGIT_0   26
#define GPIO_OUTPUT_DIGIT_1   18
#define GPIO_OUTPUT_DIGIT_2   21
#define GPIO_OUTPUT_DP        5
#define GPIO_OUTPUT_SEGMENTS  0b1000110010110010000000000000
#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER) // convert counter value to seconds

inline void IRAM_ATTR scan(const int) __attribute__((always_inline));
//volatile char segments[3];
atomic_ulong segments;

static const uint32_t digit_codemap[] = {
    0b1000110010110000000000000000, // 0   "0"          AAA
    0b0000010010000000000000000000, // 1   "1"         F   B
    0b1000010000110010000000000000, // 2   "2"         F   B
    0b0000010010110010000000000000, // 3   "3"          GGG
    0b0000110010000010000000000000, // 4   "4"         E   C
    0b0000100010110010000000000000, // 5   "5"         E   C
    0b1000100010110010000000000000, // 6   "6"          DDD
    0b0000010010100000000000000000, // 7   "7"
    0b1000110010110010000000000000, // 8   "8"
    0b0000110010110010000000000000, // 9   "9"
};

void IRAM_ATTR scan(const int digit) {
    GPIO.out_w1ts = ~digit_codemap[digit] & GPIO_OUTPUT_SEGMENTS;
    //__asm__ __volatile__("nop;nop;nop;nop;nop;nop;nop;");
    GPIO.out_w1tc = digit_codemap[digit];
    //__asm__ __volatile__("nop;nop;nop;nop;nop;nop;nop;");
}

static void write_segments(const char *data) {
    atomic_store(&segments, *((unsigned long*)data));
}

void write_digits(int value) {
    char data[4];
    sprintf(data, "%03d", value);
    data[0] &= 0x0F;
    data[1] &= 0x0F;
    data[2] &= 0x0F;
    write_segments(data);
}

void set_dp(int level) {
    gpio_set_level(GPIO_OUTPUT_DP, level);
}

static bool IRAM_ATTR segments_timer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    static int digit = 0;
    unsigned long _segments = atomic_load(&segments);
    
    //portDISABLE_INTERRUPTS();
    scan(((uint8_t*)&_segments)[digit] & 0x0F); //Anding with 0x0F to remove upper nibble
    switch (digit) {
        case 0:
            GPIO.out_w1tc = ((uint32_t)1 << GPIO_OUTPUT_DIGIT_2);
            GPIO.out_w1ts = ((uint32_t)1 << GPIO_OUTPUT_DIGIT_0);
            break;
        case 1:
            GPIO.out_w1tc = ((uint32_t)1 << GPIO_OUTPUT_DIGIT_0);
            GPIO.out_w1ts = ((uint32_t)1 << GPIO_OUTPUT_DIGIT_1);
            break;
        case 2:
            GPIO.out_w1tc = ((uint32_t)1 << GPIO_OUTPUT_DIGIT_1);
            GPIO.out_w1ts = ((uint32_t)1 << GPIO_OUTPUT_DIGIT_2);
            break;
    }
    //portENABLE_INTERRUPTS();
    digit++;
    if (digit==3) {digit = 0;}

    return true;
}

void segments_init(void)
{
    atomic_store(&segments, 0UL);
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (uint64_t)GPIO_OUTPUT_SEGMENTS | (1ULL<<GPIO_OUTPUT_DP) | (1ULL<<GPIO_OUTPUT_DIGIT_0) | (1ULL<<GPIO_OUTPUT_DIGIT_1) | (1ULL<<GPIO_OUTPUT_DIGIT_2);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    gptimer_handle_t gptimer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0, // counter will reload with 0 on alarm event
        .alarm_count = 1000, // period = 1ms @resolution 1MHz
        .flags.auto_reload_on_alarm = true, // enable auto-reload
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = segments_timer_on_alarm_cb, // register user callback
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}
