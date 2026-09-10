#ifndef LED_COLOURS_H
#define LED_COLOURS_H

#include "stdint.h"

typedef struct
{
    //[0-360]
    uint16_t hue;
    //[0-100]
    uint8_t saturation;
    //[0-100]
    uint8_t brightness;
} colour_hsv_t;

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} colour_rgb_t;

colour_rgb_t hsv2rgb(colour_hsv_t hsv);
colour_rgb_t normalize_colour(colour_hsv_t colour);

static const uint16_t RED = 0;
static const uint16_t ORANGE = 30;
static const uint16_t YELLOW = 60;
static const uint16_t LIME = 90;
static const uint16_t GREEN = 120;
static const uint16_t MINT = 150;
static const uint16_t CYAN = 180;
static const uint16_t AZURE = 210;
static const uint16_t BLUE = 240;
static const uint16_t PURPLE = 270;
static const uint16_t MAGENTA = 300;
static const uint16_t PINK = 330;

static const colour_rgb_t BLACK = (colour_rgb_t){
    .red = 0,
    .green = 0,
    .blue = 0,
};

#endif