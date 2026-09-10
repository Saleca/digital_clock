#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include "stdbool.h"
#include "stdint.h"

extern volatile bool time_manager_is_synced;

void time_manager_init(void);
void time_manager_is_day_check(void);

int32_t time_manager_get_time_offset(void);

bool time_manager_is_day(void);
bool time_manager_is_sunrise(void);
bool time_manager_is_sunset(void);

#endif