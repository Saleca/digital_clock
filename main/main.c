#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_app_desc.h"
#include "esp_pm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "time.h"
#include "sys/time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "wifi_manager.h"
#include "server_manager.h"
#include "time_manager.h"
#include "clock_face.h"
#include "debug_led.h"
static const char *TAG = "LED_CLOCK";

static void time_task(void *arg);

void app_main(void)
{
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);
    esp_log_level_set("phy_init", ESP_LOG_WARN);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI(TAG, "Firmware version: %s - %s %s", app_desc->version, app_desc->date, app_desc->time);
    
    ledc_pwm_init();
    ledc_pwm_set_duty(20);

    esp_err_t err;
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        err = nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
    }

    clock_face_init(GPIO_NUM_3);
    wifi_manager_init();
    while (!wifi_manager_ap_connected && !wifi_manager_sta_connected)
    {
        clock_face_flash_animation(clock_face.night_brightness);
    }
    server_manager_init();
    ESP_LOGI(TAG, "server initialized");

    if (wifi_manager_ap_connected)
    {
        // AP will only display animations
        clock_face_refresh();
        while (!wifi_manager_sta_connected)
        {
            for (int h = 0; h < 12; h++)
            {
                for (int m = 0; m < 60; m++)
                {
                    clock_face_set_time(h, m, 60);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                if (wifi_manager_sta_connected)
                {
                    break;
                }
            }
        }
    }
    else if (wifi_manager_sta_connected)
    {
        time_manager_init();
        while (!time_manager_is_synced)
        {
            clock_face_flash_animation(clock_face.night_brightness);
        }
        time_manager_is_day_check();

        esp_pm_config_t pm_config = {
            .max_freq_mhz = 80,
            .min_freq_mhz = 40,
            .light_sleep_enable = true};
        esp_pm_configure(&pm_config);

        xTaskCreate(time_task, "time_task", 8192, NULL, 5, NULL);
    }
    else
    {
        ESP_LOGE(TAG, "Failled to initialize clock");
    }
}

static void time_task(void *arg)
{
    if (time_manager_is_day())
    {
        clock_face_set_day_mode(true);
    }
    clock_face_refresh();

    while (1)
    {
        ledc_pwm_set_duty(10);
        time_t now;
        time(&now);

        if (!time_manager_is_day())
        {
            if (time_manager_is_sunrise())
            {
                clock_face_set_day_mode(true);
            }
        }
        else
        {
            if (time_manager_is_sunset())
            {
                clock_face_set_day_mode(false);
            }
        }

        now += time_manager_get_time_offset();
        struct tm *timeinfo = gmtime(&now);
        clock_face_set_time(timeinfo->tm_hour % 12, timeinfo->tm_min, timeinfo->tm_sec);

        struct timeval tv;
        gettimeofday(&tv, NULL);
        uint64_t remaining_ms;

        if (clock_face.has_seconds)
        {
            remaining_ms = (((1000000ULL - tv.tv_usec) / 1000) + 10) / 10 * 10;
        }
        else
        {
            remaining_ms = (60000ULL - ((tv.tv_sec % 60) * 1000 + (tv.tv_usec / 1000)) + 10) / 10 * 10;
        }

        ledc_pwm_disable();
        vTaskDelay(pdMS_TO_TICKS(remaining_ms));
    }
    vTaskDelete(NULL);
}