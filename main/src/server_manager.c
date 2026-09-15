
#include "server_manager.h"
#include "wifi_manager.h"
#include "clock_face.h"
#include "ota_manager.h"
#include "debug_manager.h"
#include "ctype.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "esp_app_desc.h"
#include "esp_log.h"

static const char *TAG = "SERVER_MANAGER";

#define DEFAULT_MDNS "relogio"

typedef struct
{
    const char *uri;
    const char *type;
    const uint8_t *start;
    const uint8_t *end;
} static_file_t;

extern const uint8_t favicon_start[] asm("_binary_favicon_svg_start");
extern const uint8_t favicon_end[] asm("_binary_favicon_svg_end");

extern const uint8_t index_start[] asm("_binary_index_html_start");
extern const uint8_t index_end[] asm("_binary_index_html_end");

extern const uint8_t settings_start[] asm("_binary_settings_html_start");
extern const uint8_t settings_end[] asm("_binary_settings_html_end");

extern const uint8_t update_start[] asm("_binary_update_html_start");
extern const uint8_t update_end[] asm("_binary_update_html_end");

extern const uint8_t logs_start[] asm("_binary_logs_html_start");
extern const uint8_t logs_end[] asm("_binary_logs_html_end");

extern const uint8_t css_start[] asm("_binary_style_css_start");
extern const uint8_t css_end[] asm("_binary_style_css_end");

extern const uint8_t js_start[] asm("_binary_script_js_start");
extern const uint8_t js_end[] asm("_binary_script_js_end");

extern const uint8_t clock_face_js_start[] asm("_binary_clock_face_js_start");
extern const uint8_t clock_face_js_end[] asm("_binary_clock_face_js_end");

extern const uint8_t settings_js_start[] asm("_binary_settings_js_start");
extern const uint8_t settings_js_end[] asm("_binary_settings_js_end");

extern const uint8_t update_js_start[] asm("_binary_update_js_start");
extern const uint8_t update_js_end[] asm("_binary_update_js_end");

extern const uint8_t logs_js_start[] asm("_binary_logs_js_start");
extern const uint8_t logs_js_end[] asm("_binary_logs_js_end");

static const static_file_t files[] = {
    {"/", "text/html", index_start, index_end},
    {"/settings", "text/html", settings_start, settings_end},
    {"/update", "text/html", update_start, update_end},
    {"/logs", "text/html", logs_start, logs_end},
    {"/style.css", "text/css", css_start, css_end},
    {"/script.js", "application/javascript", js_start, js_end},
    {"/settings.js", "application/javascript", settings_js_start, settings_js_end},
    {"/clock_face.js", "application/javascript", clock_face_js_start, clock_face_js_end},
    {"/update.js", "application/javascript", update_js_start, update_js_end},
    {"/logs.js", "application/javascript", logs_js_start, logs_js_end},
    {"/favicon.svg", "image/svg+xml", favicon_start, favicon_end},
};

static void configure_mdns();
static esp_err_t generic_handler(httpd_req_t *req);
static esp_err_t mdns_handler(httpd_req_t *req);
esp_err_t file_handler(httpd_req_t *req);

static bool routing_enabled = true;
static httpd_handle_t server = NULL;

static route_t mdns_route = {
    .uri = "/post/mdns",
    .method = HTTP_POST,
    .route_handler = mdns_handler,
    .args = NULL,
};

static route_t index_route = {
    .uri = "/",
    .method = HTTP_GET,
    .route_handler = file_handler,
    .args = NULL,
};
static route_t files_route = {
    .uri = "/*",
    .method = HTTP_GET,
    .route_handler = file_handler,
    .args = NULL,
};

static char *current_mdns;

void server_manager_init()
{
    if (server != NULL)
    {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 32;

    if (httpd_start(&server, &config) != ESP_OK)
    {
        return;
    }

    mdns_init();
    configure_mdns();

    ota_manager_register_route();
    wifi_register_route();
    clock_face_register_route();
    debug_manager_register_route();
    server_manager_add_route(&mdns_route);
    server_manager_add_route(&index_route);
    server_manager_add_route(&files_route);
}

void server_manager_deinit()
{
    mdns_free();

    if (server != NULL)
    {
        httpd_stop(server);
        server = NULL;
    }
}

void server_manager_add_route(const route_t *route)
{
    httpd_uri_t uri = {
        .uri = route->uri,
        .method = route->method,
        .handler = generic_handler,
        .user_ctx = (void *)route,
    };
    httpd_register_uri_handler(server, &uri);
}

void server_manager_enable_routing(const bool enable)
{
    routing_enabled = enable;
}

static esp_err_t generic_handler(httpd_req_t *req)
{
    if (!routing_enabled)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA in progress");
        return ESP_FAIL;
    }

    route_t *route = (route_t *)req->user_ctx;
    return route->route_handler(req);
}

static bool valid_mdns(const char *s)
{
    size_t len = strlen(s);
    if (len < 1 || len > 63)
        return false;
    if (s[0] == '-' || s[len - 1] == '-')
        return false;
    for (size_t i = 0; i < len; i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '-')
            return false;
    return true;
}

static esp_err_t mdns_handler(httpd_req_t *req)
{
    char buf[256];
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received <= 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[received] = '\0';

    // parse JSON  {"mdns":"..."}
    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"invalid json\"}");
        return ESP_OK;
    }

    cJSON *j_mdns = cJSON_GetObjectItem(root, "mdns");

    if (!cJSON_IsString(j_mdns) || !valid_mdns(j_mdns->valuestring))
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"mdns must be 1-63 characters, letters digits and hyphens only\"}");
        return ESP_OK;
    }

    // save to NVS
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(TAG, NVS_READWRITE, &nvs);
    if (err == ESP_OK)
    {
        nvs_set_str(nvs, "mdns", j_mdns->valuestring);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    cJSON_Delete(root);

    if (err != ESP_OK)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");

    configure_mdns();
    return ESP_OK;
}

static void configure_mdns()
{
    if (current_mdns == NULL)
    {
        current_mdns = malloc(64);
    }

    bool valid_mdns = false;
    nvs_handle_t nvs;
    if (nvs_open(TAG, NVS_READONLY, &nvs) == ESP_OK)
    {
        size_t len = 64;
        nvs_get_str(nvs, "mdns", current_mdns, &len);
        nvs_close(nvs);
        valid_mdns = true;
    }

    if (!valid_mdns)
    {
        strlcpy(current_mdns, DEFAULT_MDNS, 64);
    }

    mdns_hostname_set(current_mdns);
    mdns_instance_name_set(current_mdns);
    if (mdns_service_exists("_http", "_tcp", NULL))
    {
        mdns_service_remove("_http", "_tcp");
    }
    mdns_service_add(current_mdns, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "settings available from: %s.local/", current_mdns);
}

esp_err_t file_handler(httpd_req_t *req)
{
    for (int i = 0; i < sizeof(files) / sizeof(static_file_t); i++)
    {
        if (strcmp(req->uri, files[i].uri) != 0)
        {
            continue;
        }

        const esp_app_desc_t *app_desc = esp_app_get_description();
        char etag[40];
        snprintf(etag, sizeof(etag), "\"%s\"", app_desc->version);

        char client_etag[40] = {0};
        if (httpd_req_get_hdr_value_str(req, "If-None-Match", client_etag, sizeof(client_etag)) == ESP_OK)
        {
            if (strcmp(client_etag, etag) == 0)
            {
                httpd_resp_set_status(req, "304 Not Modified");
                return httpd_resp_send(req, NULL, 0);
            }
        }

        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, must-revalidate");
        httpd_resp_set_hdr(req, "ETag", etag);

        httpd_resp_set_type(req, files[i].type);
        return httpd_resp_send(req, (const char *)files[i].start, files[i].end - files[i].start);
    }

    httpd_resp_send_404(req);
    return ESP_OK;
}
