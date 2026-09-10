#include "ota_manager.h"
#include "server_manager.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_log.h"

static const char *TAG = "OTA_MANAGER";

static esp_err_t flash_handler(httpd_req_t *req);
static esp_err_t version_handler(httpd_req_t *req);

static route_t flash_route = {
    .uri = "/post/flash",
    .method = HTTP_POST,
    .route_handler = flash_handler,
    .args = NULL,
};

static route_t version_route = {
    .uri = "/get/version",
    .method = HTTP_GET,
    .route_handler = version_handler,
    .args = NULL,
};

void ota_manager_register_route()
{
    server_manager_add_route(&flash_route);
    server_manager_add_route(&version_route);
}

static esp_err_t version_handler(httpd_req_t *req)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, app_desc->version);
    ESP_LOGI(TAG, "%s", app_desc->version);
    return ESP_OK;
}

static esp_err_t flash_handler(httpd_req_t *req)
{
    server_manager_enable_routing(false);
    ESP_LOGI(TAG, "starting update");

    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL)
    {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));

        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[512];
    int received;
    int remaining = req->content_len;

    int last_logged_progress = -1;
    while (remaining > 0)
    {
        received = httpd_req_recv(req, buf, remaining < sizeof(buf) ? remaining : sizeof(buf));
        if (received <= 0)
        {
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK)
        {
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        remaining -= received;
        int progress = (int)(100 * (1.0f - ((float)remaining / req->content_len)) / 10) * 10;
        if (progress > last_logged_progress)
        {
            ESP_LOGI(TAG, "OTA progress: %d%%", progress);
            last_logged_progress = progress;
        }
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "OTA success, rebooting...");
    ESP_LOGI(TAG, "OTA success, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}