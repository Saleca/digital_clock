#include "led_colours.h"
#include "led_strip.h"
#include "esp_log.h"

#define WEIGHT_R 218UL
#define WEIGHT_G 732UL
#define WEIGHT_B 74UL
#define TARGET_LUMINANCE (255UL * WEIGHT_B)

#define TAG "LED_COLOURS"

// CIE1931
// Forward: raw PWM (0-255) -> perceptual brightness (0-255)
const uint8_t BRIGHTNESS[] = {
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3,
    3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6,
    6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 10, 10,
    10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 15, 15,
    15, 16, 16, 17, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22,
    22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 28, 28, 29, 29, 30,
    31, 31, 32, 32, 33, 34, 34, 35, 36, 37, 37, 38, 39, 39, 40,
    41, 42, 43, 43, 44, 45, 46, 47, 47, 48, 49, 50, 51, 52, 53,
    54, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67,
    68, 70, 71, 72, 73, 74, 75, 76, 77, 79, 80, 81, 82, 83, 85,
    86, 87, 88, 90, 91, 92, 94, 95, 96, 98, 99, 100, 102, 103, 105,
    106, 108, 109, 110, 112, 113, 115, 116, 118, 120, 121, 123, 124, 126, 128,
    129, 131, 132, 134, 136, 138, 139, 141, 143, 145, 146, 148, 150, 152, 154,
    155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179, 181, 183,
    185, 187, 189, 191, 193, 196, 198, 200, 202, 204, 207, 209, 211, 214, 216,
    218, 220, 223, 225, 228, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252,
    255};

// Inverse: perceptual brightness (0-255) -> raw PWM (0-255)
const uint8_t INV_BRIGHTNESS[] = {
    0, 5, 14, 23, 31, 37, 42, 47, 51, 55, 58, 62, 65, 68, 71,
    73, 76, 78, 81, 83, 85, 87, 89, 91, 93, 95, 97, 99, 100, 102,
    104, 105, 107, 109, 110, 112, 113, 114, 116, 117, 119, 120, 121, 122, 124,
    125, 126, 127, 129, 130, 131, 132, 133, 134, 135, 137, 138, 139, 140, 141,
    142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 151, 152, 153, 154, 155,
    156, 157, 158, 159, 159, 160, 161, 162, 163, 164, 164, 165, 166, 167, 168,
    168, 169, 170, 171, 171, 172, 173, 174, 174, 175, 176, 177, 177, 178, 179,
    179, 180, 181, 181, 182, 183, 184, 184, 185, 186, 186, 187, 188, 188, 189,
    189, 190, 191, 191, 192, 193, 193, 194, 194, 195, 196, 196, 197, 198, 198,
    199, 199, 200, 200, 201, 202, 202, 203, 203, 204, 204, 205, 206, 206, 207,
    207, 208, 208, 209, 209, 210, 211, 211, 212, 212, 213, 213, 214, 214, 215,
    215, 216, 216, 217, 217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222,
    223, 223, 224, 224, 225, 225, 226, 226, 227, 227, 228, 228, 229, 229, 230,
    230, 230, 231, 231, 232, 232, 233, 233, 234, 234, 235, 235, 235, 236, 236,
    237, 237, 238, 238, 238, 239, 239, 240, 240, 241, 241, 242, 242, 242, 243,
    243, 244, 244, 244, 245, 245, 246, 246, 247, 247, 247, 248, 248, 249, 249,
    249, 250, 250, 251, 251, 251, 252, 252, 253, 253, 253, 254, 254, 255, 255,
    255};

colour_rgb_t hsv2rgb(colour_hsv_t hsv)
{
    colour_rgb_t rgb;

    hsv.hue %= 360;
    uint32_t rgb_max = hsv.brightness * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - hsv.saturation) / 100.0f;

    uint32_t i = hsv.hue / 60;
    uint32_t diff = hsv.hue % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i)
    {
    case 0:
        rgb.red = rgb_max;
        rgb.green = rgb_min + rgb_adj;
        rgb.blue = rgb_min;
        break;
    case 1:
        rgb.red = rgb_max - rgb_adj;
        rgb.green = rgb_max;
        rgb.blue = rgb_min;
        break;
    case 2:
        rgb.red = rgb_min;
        rgb.green = rgb_max;
        rgb.blue = rgb_min + rgb_adj;
        break;
    case 3:
        rgb.red = rgb_min;
        rgb.green = rgb_max - rgb_adj;
        rgb.blue = rgb_max;
        break;
    case 4:
        rgb.red = rgb_min + rgb_adj;
        rgb.green = rgb_min;
        rgb.blue = rgb_max;
        break;
    default:
        rgb.red = rgb_max;
        rgb.green = rgb_min;
        rgb.blue = rgb_max - rgb_adj;
        break;
    }
    return rgb;
}

colour_rgb_t normalize_colour(colour_hsv_t colour)
{

    colour_rgb_t rgb = hsv2rgb(colour);

    uint32_t current_luminance = (BRIGHTNESS[rgb.red] * WEIGHT_R) +
                                 (BRIGHTNESS[rgb.green] * WEIGHT_G) +
                                 (BRIGHTNESS[rgb.blue] * WEIGHT_B);

    if (current_luminance <= TARGET_LUMINANCE || current_luminance == 0)
    {
        return rgb;
    }

    rgb.red = INV_BRIGHTNESS[(uint8_t)((BRIGHTNESS[rgb.red] * TARGET_LUMINANCE) / current_luminance)];
    rgb.green = INV_BRIGHTNESS[(uint8_t)((BRIGHTNESS[rgb.green] * TARGET_LUMINANCE) / current_luminance)];
    rgb.blue = INV_BRIGHTNESS[(uint8_t)((BRIGHTNESS[rgb.blue] * TARGET_LUMINANCE) / current_luminance)];

    return rgb;
}