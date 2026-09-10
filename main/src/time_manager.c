#include "time_manager.h"
#include "wifi_manager.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "esp_timer.h" //
#include "esp_log.h"
#include "freertos/freeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "string.h"
#include "time.h"
#include "sys/time.h"

static const char *TAG = "TIME_MANAGER";

static void sync_task(void *arg);
static void sync_callback(struct timeval *tv);
static bool get_location(void);
static bool get_sunrise_sunset_times(time_t *sunrise, time_t *sunset);
static void log_time(void);
static char *http_get(const char *url);

static SemaphoreHandle_t sync_semaphore = NULL;
volatile bool time_manager_is_synced = false;

static bool has_location = false;
static float lat = 0;
static float lon = 0;
static int32_t time_offset = 0;

static bool day = false;
static time_t sunrise;
static time_t sunset;

void time_manager_init()
{
    xTaskCreate(sync_task, "sync_task", 8192, NULL, 5, NULL);
}

void time_manager_is_day_check(void)
{
    time_t now;
    time(&now);
    day = now >= sunrise && now <= sunset;
}

int32_t time_manager_get_time_offset(void)
{
    return time_offset;
}

bool time_manager_is_day(void)
{
    return day;
}

bool time_manager_is_sunrise(void)
{
    time_t now;
    time(&now);
    double diff = difftime(now, sunrise);
    if (diff >= 0 && diff < 60)
    {
        day = true;
        ESP_LOGI(TAG, "sunrise start");
        return true;
    }
    return false;
}

bool time_manager_is_sunset(void)
{
    time_t now;
    time(&now);
    double diff = difftime(now, sunset);
    if (diff > -60 && diff <= 0)
    {
        day = false;
        ESP_LOGI(TAG, "sunset start");
        return true;
    }
    return false;
}

static char *http_get(const char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        esp_http_client_cleanup(client);
        return NULL;
    }

    int content_length = esp_http_client_fetch_headers(client);

    char *buf;
    size_t len = 0;

    if (content_length > 0)
    {
        buf = malloc(content_length + 1);
        if (!buf)
        {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return NULL;
        }

        int read_len = esp_http_client_read(client, buf, content_length);
        len = read_len > 0 ? read_len : 0;
    }
    else
    {
        size_t cap = 1024;
        buf = malloc(cap);
        if (!buf)
        {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return NULL;
        }

        int read_len;
        while ((read_len = esp_http_client_read(client, buf + len, cap - len - 1)) > 0)
        {
            len += read_len;
            if (len + 1 >= cap)
            {
                cap *= 2;
                char *new_buf = realloc(buf, cap);
                if (!new_buf)
                {
                    free(buf);
                    esp_http_client_close(client);
                    esp_http_client_cleanup(client);
                    return NULL;
                }
                buf = new_buf;
            }
        }
    }

    buf[len] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return buf;
}

static bool get_location(void)
{
    char *body = http_get("http://ip-api.com/json/?fields=status,lat,lon,offset");
    if (!body)
    {
        return false;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root)
    {
        return false;
    }

    cJSON *status_field = cJSON_GetObjectItem(root, "status");
    if (status_field && strcmp(status_field->valuestring, "success") == 0)
    {
        cJSON *lat_item = cJSON_GetObjectItem(root, "lat");
        cJSON *lon_item = cJSON_GetObjectItem(root, "lon");
        cJSON *offset_item = cJSON_GetObjectItem(root, "offset");

        if (cJSON_IsNumber(lat_item) &&
            cJSON_IsNumber(lon_item) &&
            cJSON_IsNumber(offset_item))
        {
            lat = lat_item->valuedouble;
            lon = lon_item->valuedouble;
            time_offset = offset_item->valueint;
            ESP_LOGI(TAG, "lat: %.6f, lon: %.6f, time_offset: %d", lat, lon, time_offset);
            has_location = true;
        }
    }

    cJSON_Delete(root);
    return has_location;
}

static bool get_sunrise_sunset_times(time_t *sunrise, time_t *sunset)
{
    char url[256];
    snprintf(url, sizeof(url), "http://api.sunrise-sunset.org/json?lat=%.6f&lng=%.6f&formatted=0", lat, lon);

    char *body = http_get(url);
    if (!body)
    {
        ESP_LOGE(TAG, "failled to get sunrise sunset json");
        return false;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root)
    {
        ESP_LOGE(TAG, "failled to format sunrise sunset json");
        return false;
    }

    bool result = false;
    cJSON *status_field = cJSON_GetObjectItem(root, "status");
    if (status_field && strcmp(status_field->valuestring, "OK") == 0)
    {
        cJSON *results_item = cJSON_GetObjectItem(root, "results");
        cJSON *sunrise_item = cJSON_GetObjectItem(results_item, "sunrise");
        cJSON *sunset_item = cJSON_GetObjectItem(results_item, "sunset");

        if (cJSON_IsString(sunrise_item) && cJSON_IsString(sunset_item))
        {
            struct tm tm_sunrise = {0};
            struct tm tm_sunset = {0};

            strptime(sunrise_item->valuestring, "%Y-%m-%dT%H:%M:%S", &tm_sunrise);
            strptime(sunset_item->valuestring, "%Y-%m-%dT%H:%M:%S", &tm_sunset);

            *sunrise = mktime(&tm_sunrise);
            *sunset = mktime(&tm_sunset);

            char sunrise_buf[9], sunset_buf[9];
            struct tm tmp;

            gmtime_r(sunrise, &tmp);
            strftime(sunrise_buf, sizeof(sunrise_buf), "%H:%M:%S", &tmp);

            gmtime_r(sunset, &tmp);
            strftime(sunset_buf, sizeof(sunset_buf), "%H:%M:%S", &tmp);

            ESP_LOGI(TAG, "sunrise: %s, sunset: %s (UTC)", sunrise_buf, sunset_buf);

            result = true;
        }
    }
    else
    {
        ESP_LOGE(TAG, "API returned non-OK status");
    }

    cJSON_Delete(root);
    return result;
}

static void log_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t now = tv.tv_sec + time_offset;
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char time_buffer[32];
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "current time: %s.%03ld.%03ld", time_buffer, tv.tv_usec / 1000, tv.tv_usec % 1000);
}

static void sync_callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "time synced.");
    xSemaphoreGive(sync_semaphore);
}

static void sync_task(void *arg)
{
    sync_semaphore = xSemaphoreCreateBinary();
    if (!wifi_manager_sta_connected)
    {
        ESP_LOGE(TAG, "trying to initialize time without sta connection");
    }
    get_location();

    while (1)
    {
        if (!wifi_manager_sta_connected)
        {
            ESP_LOGE(TAG, "trying to sync time without sta connection");
        }
        
        ESP_LOGI(TAG, "sync started");

        if (esp_sntp_enabled())
        {
            esp_sntp_stop();
        }

        esp_sntp_set_time_sync_notification_cb(sync_callback);
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pt.pool.ntp.org");
        esp_sntp_setservername(1, "europe.pool.ntp.org");
        esp_sntp_init();
        bool sync = true;
        if (xSemaphoreTake(sync_semaphore, pdMS_TO_TICKS(15 * 1000)) != pdTRUE)
        {
            sync = false;
        }

        esp_sntp_stop();

        get_sunrise_sunset_times(&sunrise, &sunset);

        if (sync)
        {
            time_manager_is_synced = true;
        }
        else
        {
            time_manager_is_synced = false;
        }
        log_time();

        time_t now;
        struct tm tm_now;
        time(&now);
        now += time_offset;
        gmtime_r(&now, &tm_now);

        struct tm tm_next_sync = tm_now;
        tm_next_sync.tm_mday++;
        tm_next_sync.tm_hour = 1;
        tm_next_sync.tm_min = 0;
        tm_next_sync.tm_sec = 0;
        time_t next_sync = mktime(&tm_next_sync);

        vTaskDelay(pdMS_TO_TICKS((next_sync - now) * 1000));
    }

    vTaskDelete(NULL);
}