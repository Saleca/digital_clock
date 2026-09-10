#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "stdbool.h"
extern volatile bool wifi_manager_ap_connected;
extern volatile bool wifi_manager_sta_connected;

void wifi_manager_init(void);
void wifi_register_route(void);

#endif