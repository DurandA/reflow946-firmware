#include <stdio.h>
#include <inttypes.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "segments.h"

#define GPIO_OUTPUT_DIGIT_0   26
#define GPIO_OUTPUT_DIGIT_1   18
#define GPIO_OUTPUT_DIGIT_2   21
#define GPIO_OUTPUT_SEGMENTS  0b1000110010110010000000100000
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

void IRAM_ATTR timer_segments_isr() {
    static int cc=0;
    unsigned long _segments = atomic_load(&segments);
    
    //portDISABLE_INTERRUPTS();
    scan(((uint8_t*)&_segments)[cc] /*& 0x0F*/); //Anding with 0x0F to remove upper nibble
    switch (cc) {
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
    cc++;
    if (cc==3) {cc=0;}
    TIMERG0.int_clr_timers.t0 = 1;
    TIMERG0.hw_timer[0].config.alarm_en = 1;
}

void segments_init(void)
{
    atomic_store(&segments, 0UL);
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (uint64_t)GPIO_OUTPUT_SEGMENTS | (1ULL<<GPIO_OUTPUT_DIGIT_0) | (1ULL<<GPIO_OUTPUT_DIGIT_1) | (1ULL<<GPIO_OUTPUT_DIGIT_2);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    int timer_idx = 0;
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = true;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, 5000);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_segments_isr, 
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}
