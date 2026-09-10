#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include "esp_http_server.h"
typedef esp_err_t (*route_handler_t)(httpd_req_t *req);

typedef struct
{
    const char *uri;
    const httpd_method_t method;
    route_handler_t route_handler;
    void *args;
} route_t;

void server_manager_init();
void server_manager_deinit();
void server_manager_add_route(const route_t *route);
void server_manager_enable_routing(const bool enable);

#endif