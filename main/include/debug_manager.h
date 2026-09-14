#ifndef DEBUG_MANAGER_H
#define DEBUG_MANAGER_H
#include "stdint.h"
#include "stdarg.h"

void debug_manager_init(void);
void debug_manager_add_log(const char *entry, ...);
void debug_manager_register_route();

void debug_manager_led_set_brightness(uint8_t inverted_duty_percent);
void debug_manager_disable_led(void);

#endif