#include "debug_led.h"
#include "driver/ledc.h"

#define LEDC_GPIO GPIO_NUM_8
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_DUTY_RES LEDC_TIMER_10_BIT

void ledc_pwm_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num = LEDC_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
        .intr_type = LEDC_INTR_DISABLE};
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));
}

void ledc_pwm_set_duty(uint8_t inverted_duty_percent)
{
    if (inverted_duty_percent > 100)
    {
        inverted_duty_percent = 0;
    }

    uint32_t max_duty = (1 << LEDC_DUTY_RES) - 1;
    uint32_t duty = (max_duty * (100 - inverted_duty_percent)) / 100;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void ledc_pwm_disable(void)
{
    ledc_stop(LEDC_MODE, LEDC_CHANNEL, 1);
}