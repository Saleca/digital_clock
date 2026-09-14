#include "debug_manager.h"
#include "server_manager.h"
#include "driver/ledc.h"
#include "string.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define LEDC_GPIO GPIO_NUM_8
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_DUTY_RES LEDC_TIMER_10_BIT

#define LOG_ENTRY "e%d"

static esp_err_t get_logs_handler(httpd_req_t *req);
static esp_err_t delete_logs_handler(httpd_req_t *req);

// temp to clear on console: allow paste \n fetch('/delete/logs', { method: 'DELETE' }).then(res => res.text()).then(console.log);
static route_t delete_logs_route = {
    .uri = "/delete/logs",
    .method = HTTP_DELETE,
    .route_handler = delete_logs_handler,
    .args = NULL,
};

static route_t get_logs_route = {
    .uri = "/get/logs",
    .method = HTTP_GET,
    .route_handler = get_logs_handler,
    .args = NULL,
};

static const char *TAG = "DEBUG_MANAGER";
static int entry_index = 0;

void debug_manager_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num = LEDC_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
        .intr_type = LEDC_INTR_DISABLE};
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));

    nvs_handle_t nvs_handle;

    esp_err_t err = nvs_open(TAG, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            return;
        }
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }

    int32_t saved_index = 0;
    err = nvs_get_i32(nvs_handle, "entry_index", &saved_index);
    if (err == ESP_OK)
    {
        entry_index = (int)saved_index;
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        // ESP_LOGI(TAG, "Key 'entry_index' not found. Defaulting to 0.");
    }
    else
    {
        ESP_LOGE(TAG, "Error reading entry_index: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

void debug_manager_register_route()
{
    server_manager_add_route(&get_logs_route);
    server_manager_add_route(&delete_logs_route);
}

void debug_manager_add_log(const char *entry, ...)
{
    if (entry == NULL)
    {
        return;
    }

    char formatted_entry[256];
    va_list args;
    va_start(args, entry);
    vsnprintf(formatted_entry, sizeof(formatted_entry), entry, args);
    va_end(args);

    ESP_LOGE(TAG, "%s", formatted_entry);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(TAG, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for write: %s", esp_err_to_name(err));
        return;
    }

    char entry_key[16];
    snprintf(entry_key, sizeof(entry_key), LOG_ENTRY, entry_index);

    err = nvs_set_str(nvs_handle, entry_key, formatted_entry);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to store entry '%s': %s", entry_key, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }

    err = nvs_set_i32(nvs_handle, "entry_index", (int32_t)entry_index);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to store entry_index: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS Commit failed: %s", esp_err_to_name(err));
    }
    else
    {
        entry_index++;
    }

    nvs_close(nvs_handle);
}

static esp_err_t get_logs_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");

    if (entry_index == 0)
    {
        httpd_resp_sendstr(req, "No logs stored.\n");
        return ESP_OK;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(TAG, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for reading logs: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open NVS");
        return ESP_FAIL;
    }

    for (int i = 0; i < entry_index; i++)
    {
        char entry_key[16];
        snprintf(entry_key, sizeof(entry_key), LOG_ENTRY, i);

        size_t required_size = 0;
        err = nvs_get_str(nvs_handle, entry_key, NULL, &required_size);
        if (err == ESP_OK && required_size > 0)
        {
            char *buf = malloc(required_size + 1);
            if (buf == NULL)
            {
                ESP_LOGE(TAG, "Failed to allocate memory for log chunk");
                continue;
            }

            err = nvs_get_str(nvs_handle, entry_key, buf, &required_size);
            if (err == ESP_OK)
            {

                strcat(buf, "\n");
                httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
            }
            free(buf);
        }
    }

    nvs_close(nvs_handle);

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t delete_logs_handler(httpd_req_t *req)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(TAG, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for deletion: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open NVS");
        return ESP_FAIL;
    }

    err = nvs_erase_all(nvs_handle);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);

    if (err == ESP_OK)
    {
        entry_index = 0; // Reset in-memory counter
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Logs cleared successfully.\n");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to erase NVS namespace %s: %s", TAG, esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to erase NVS");
    return ESP_FAIL;
}

void debug_manager_led_set_brightness(uint8_t inverted_duty_percent)
{
    if (inverted_duty_percent > 100)
    {
        inverted_duty_percent = 0;
    }

    uint32_t max_duty = (1 << LEDC_DUTY_RES) - 1;
    uint32_t duty = (max_duty * (100 - inverted_duty_percent)) / 100;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void debug_manager_disable_led(void)
{
    ledc_stop(LEDC_MODE, LEDC_CHANNEL, 1);
}