#ifndef CLOCK_FACE_H
#define CLOCK_FACE_H
#include "led_colours.h"
#include "stdint.h"
#include "stdbool.h"
#include "driver/gpio.h"

typedef struct
{
    colour_hsv_t hour_hs;
    colour_hsv_t minute_hs;
    colour_hsv_t second_hs;
    colour_hsv_t background_hs;

    bool has_seconds;
    bool is_background_black;

    uint8_t day_brightness;
    uint8_t night_brightness;
} clock_face_t;

extern clock_face_t clock_face;
extern int current_second;

void clock_face_init(gpio_num_t data_gpio);
void clock_face_register_route();
void clock_face_set_time(int hour, int minute, int second);
void clock_face_set_day_mode(bool day);
void clock_face_refresh(void);

void clock_face_set_pixel(int i, colour_rgb_t colour);
void clock_face_fill_hours(colour_rgb_t colour);
void clock_face_fill_minutes(colour_rgb_t colour);

void clock_face_flash_animation(uint8_t brightness);
void clock_face_colour_wheel(uint8_t brightness);
void clock_face_spin_hours(int delay);

#endif