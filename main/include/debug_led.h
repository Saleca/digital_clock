#ifndef DEBUG_LED_H
#define DEBUG_LED_H
#include "stdint.h"

void ledc_pwm_init(void);
void ledc_pwm_set_duty(uint8_t inverted_duty_percent);
void ledc_pwm_disable(void);

#endif