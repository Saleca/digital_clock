#include "clock_face.h"
#include "led_strip.h"
#include "cJSON.h"
#include "server_manager.h"
#include "time_manager.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"

static int get_hour_index(int hour);
static int get_minute_index(int minute);
static void set_hour(int hour);
static void set_minute(int minute);
static void set_second(int second);
static void set_default_clock_face(void);
static void set_brightness(uint8_t brightness);
static void set_colours(void);
static bool save_clock_pallet();
static bool load_clock_pallet();

static bool valid_hue(int val);
static bool valid_percent(int val);
static bool decode_json_colour(cJSON *obj, colour_hsv_t *out);
static cJSON *encode_json_colour(const colour_hsv_t *c);

static esp_err_t get_clock_handler(httpd_req_t *req);
static esp_err_t post_clock_handler(httpd_req_t *req);
static esp_err_t save_handler(httpd_req_t *req);

static const char *TAG = "CLOCK_FACE";
static const char *nvs_clock_face = "data";
static const uint16_t hour_led_count = 24;
static const uint16_t min_led_count = 60;
static const uint16_t total_led_count = hour_led_count + min_led_count;

static route_t get_clock_route = {
    .uri = "/get/clock",
    .method = HTTP_GET,
    .route_handler = get_clock_handler,
    .args = NULL,
};

static route_t post_clock_route = {
    .uri = "/post/clock",
    .method = HTTP_POST,
    .route_handler = post_clock_handler,
    .args = NULL,
};

static route_t save_route = {
    .uri = "/post/clock_save",
    .method = HTTP_POST,
    .route_handler = save_handler,
    .args = NULL,
};

static led_strip_handle_t led_strip;
clock_face_t clock_face = {0};

static int current_hour = 12;
static int current_minute = 60;
int current_second = 60;

colour_rgb_t hour_colour;
colour_rgb_t minute_colour;
colour_rgb_t second_colour;
colour_rgb_t background_colour;

void clock_face_init(gpio_num_t data_gpio)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = data_gpio,
        .max_leds = total_led_count,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }};

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = (10 * 1000 * 1000),
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = 0,
        }};

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    if (!load_clock_pallet())
    {
        set_default_clock_face();
    }

    set_brightness(clock_face.night_brightness);
    set_colours();
    clock_face_refresh();
}

void clock_face_register_route()
{
    server_manager_add_route(&get_clock_route);
    server_manager_add_route(&post_clock_route);
    server_manager_add_route(&save_route);
}

static bool valid_hue(int val)
{
    return val >= 0 && val <= 360;
}

static bool valid_percent(int val)
{
    return val >= 0 && val <= 100;
}

static bool decode_json_colour(cJSON *obj, colour_hsv_t *out)
{
    if (!cJSON_IsObject(obj))
        return false;

    cJSON *j_h = cJSON_GetObjectItem(obj, "h");
    cJSON *j_s = cJSON_GetObjectItem(obj, "s");
    cJSON *j_v = cJSON_GetObjectItem(obj, "v");

    if (!cJSON_IsNumber(j_h) || !valid_hue(j_h->valueint))
    {
        return false;
    }
    if (!cJSON_IsNumber(j_s) || !valid_percent(j_s->valueint))
    {
        return false;
    }
    if (!cJSON_IsNumber(j_v) || !valid_percent(j_v->valueint))
    {
        return false;
    }

    out->hue = (uint16_t)j_h->valueint;
    out->saturation = (uint8_t)j_s->valueint;
    out->brightness = (uint8_t)j_v->valueint;
    return true;
}

static cJSON *encode_json_colour(const colour_hsv_t *c)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "h", c->hue);
    cJSON_AddNumberToObject(obj, "s", c->saturation);
    cJSON_AddNumberToObject(obj, "v", c->brightness);
    return obj;
}

static esp_err_t get_clock_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "hour_hs", encode_json_colour(&clock_face.hour_hs));
    cJSON_AddItemToObject(root, "minute_hs", encode_json_colour(&clock_face.minute_hs));
    cJSON_AddItemToObject(root, "second_hs", encode_json_colour(&clock_face.second_hs));
    cJSON_AddItemToObject(root, "background_hs", encode_json_colour(&clock_face.background_hs));

    cJSON_AddBoolToObject(root, "has_seconds", clock_face.has_seconds);
    cJSON_AddBoolToObject(root, "is_background_black", clock_face.is_background_black);
    cJSON_AddNumberToObject(root, "day_brightness", clock_face.day_brightness);
    cJSON_AddNumberToObject(root, "night_brightness", clock_face.night_brightness);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    free(json);
    return ESP_OK;
}

static esp_err_t post_clock_handler(httpd_req_t *req)
{
    char buf[512];
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (received <= 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"invalid json\"}");
        return ESP_OK;
    }

    clock_face_t preview_face = {0};

    if (!decode_json_colour(cJSON_GetObjectItem(root, "hour_hs"), &preview_face.hour_hs) ||
        !decode_json_colour(cJSON_GetObjectItem(root, "minute_hs"), &preview_face.minute_hs) ||
        !decode_json_colour(cJSON_GetObjectItem(root, "second_hs"), &preview_face.second_hs) ||
        !decode_json_colour(cJSON_GetObjectItem(root, "background_hs"), &preview_face.background_hs))
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"invalid colour, expected h 0-360, s/v 0-100\"}");
        return ESP_OK;
    }

    cJSON *j_has_seconds = cJSON_GetObjectItem(root, "has_seconds");
    cJSON *j_is_bg_black = cJSON_GetObjectItem(root, "is_background_black");
    cJSON *j_day = cJSON_GetObjectItem(root, "day_brightness");
    cJSON *j_night = cJSON_GetObjectItem(root, "night_brightness");

    if (!cJSON_IsBool(j_has_seconds) || !cJSON_IsBool(j_is_bg_black))
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"has_seconds and is_background_black must be booleans\"}");
        return ESP_OK;
    }

    if (!cJSON_IsNumber(j_day) || !valid_percent(j_day->valueint) ||
        !cJSON_IsNumber(j_night) || !valid_percent(j_night->valueint) ||
        j_night->valueint > j_day->valueint)
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"day/night brightness must be 0-100 and night <= day\"}");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");

    preview_face.has_seconds = cJSON_IsTrue(j_has_seconds);
    preview_face.is_background_black = cJSON_IsTrue(j_is_bg_black);
    preview_face.day_brightness = (uint8_t)j_day->valueint;
    preview_face.night_brightness = (uint8_t)j_night->valueint;

    cJSON_Delete(root);

    clock_face = preview_face;
    clock_face_set_day_mode(wifi_manager_sta_connected ? time_manager_is_day() : false);
    clock_face_refresh();

    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    if (save_clock_pallet())
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":true}");
        return ESP_OK;
    }
    else
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
}

void clock_face_refresh(void)
{
    if (!clock_face.is_background_black)
    {
        clock_face_fill_hours(background_colour);
        clock_face_fill_minutes(background_colour);
    }
    else
    {
        clock_face_fill_hours(BLACK);
        clock_face_fill_minutes(BLACK);
    }
}

void clock_face_set_day_mode(bool day)
{
    if (day)
    {
        set_brightness(clock_face.day_brightness);
    }
    else
    {
        set_brightness(clock_face.night_brightness);
    }
    set_colours();
}

static bool save_clock_pallet()
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(TAG, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failled to open nvs");
        return false;
    }

    err = nvs_set_blob(nvs, nvs_clock_face, &clock_face, sizeof(clock_face_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failled to save clock face");
    }
    else
    {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return err == ESP_OK;
}

static bool load_clock_pallet()
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(TAG, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failled to open nvs %s", esp_err_to_name(err));
        return false;
    }

    size_t len = sizeof(clock_face_t);
    err = nvs_get_blob(nvs, nvs_clock_face, &clock_face, &len);
    if (err != ESP_OK || len != sizeof(clock_face_t))
    {
        ESP_LOGE(TAG, "failled to load clock face");
    }

    nvs_close(nvs);
    return err == ESP_OK;
}

static void set_default_clock_face(void)
{
    colour_hsv_t colour_hs = {
        .hue = 0,
        .saturation = 100,
        .brightness = 100,
    };

    colour_hs.hue = YELLOW;
    clock_face.hour_hs = colour_hs;
    clock_face.minute_hs = colour_hs;

    clock_face.has_seconds = true;
    colour_hs.saturation = 0;
    colour_hs.brightness = 100,
    clock_face.second_hs = colour_hs;

    clock_face.is_background_black = true;
    if (!clock_face.is_background_black)
    {
        colour_hs.saturation = 100;
        colour_hs.brightness = 100;
        colour_hs.hue = BLUE;
    }
    else
    {
        colour_hs.saturation = 0;
        colour_hs.brightness = 0;
    }
    clock_face.background_hs = colour_hs;

    clock_face.day_brightness = 50;
    clock_face.night_brightness = 5;
}

static void set_brightness(uint8_t brightness)
{
    clock_face.hour_hs.brightness = brightness;
    clock_face.minute_hs.brightness = brightness;

    if (clock_face.has_seconds)
    {
        clock_face.second_hs.brightness = brightness;
    }

    if (!clock_face.is_background_black)
    {
        clock_face.background_hs.brightness = brightness;
    }
}

static void set_colours(void)
{
    hour_colour = normalize_colour(clock_face.hour_hs);
    minute_colour = normalize_colour(clock_face.minute_hs);

    if (clock_face.has_seconds)
    {
        second_colour = normalize_colour(clock_face.second_hs);
    }

    if (clock_face.is_background_black)
    {
        background_colour = BLACK;
    }
    else
    {
        background_colour = normalize_colour(clock_face.background_hs);
    }
}

void clock_face_set_time(int hour, int minute, int second)
{
    set_hour(hour);
    set_minute(minute);
    set_second(second);
    led_strip_refresh(led_strip);
}

static void set_second(int second)
{
    if (current_second < 60)
    {
        if (current_second == current_minute)
        {
            set_minute(current_minute);
        }
        else
        {
            clock_face_set_pixel(get_minute_index(current_second), background_colour);
        }
    }
    current_second = second;
    if (current_second == 60)
    {
        return;
    }

    clock_face_set_pixel(get_minute_index(current_second), second_colour);
}

static void set_minute(int minute)
{
    if (current_minute < 60)
    {
        clock_face_set_pixel(get_minute_index(current_minute), background_colour);
    }

    current_minute = minute;
    if (current_minute == 60)
    {
        return;
    }
    clock_face_set_pixel(get_minute_index(current_minute), minute_colour);
}

static void set_hour(int hour)
{
    int hour_index;
    if (current_hour < 12)
    {
        hour_index = get_hour_index(current_hour);
        clock_face_set_pixel(hour_index, background_colour);
        clock_face_set_pixel(hour_index + 1, background_colour);
    }

    current_hour = hour;
    if (current_hour == 12)
    {
        return;
    }

    hour_index = get_hour_index(current_hour);
    clock_face_set_pixel(hour_index, hour_colour);
    clock_face_set_pixel(hour_index + 1, hour_colour);
}

void clock_face_set_pixel(int i, colour_rgb_t colour)
{
    led_strip_set_pixel(led_strip, i, colour.red, colour.green, colour.blue);
}

void clock_face_fill_hours(colour_rgb_t colour)
{
    for (int i = 0; i < hour_led_count; i++)
    {
        clock_face_set_pixel(i, colour);
    }
}
void clock_face_fill_minutes(colour_rgb_t colour)
{
    for (int i = hour_led_count; i < total_led_count; i++)
    {
        clock_face_set_pixel(i, colour);
    }
}

// 0-11
static int get_hour_index(int hour)
{ //
    return hour * 2;
}

// 0-59
static int get_minute_index(int minute)
{ //
    return hour_led_count + minute;
}

// ANIMATIONS
void clock_face_spin_hours(int delay)
{
    for (int i = 0; i < 12; i++)
    {
        set_hour(i);
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

void clock_face_flash_animation(uint8_t brightness)
{
    clock_face_colour_wheel(brightness);
    vTaskDelay(pdMS_TO_TICKS(500));
    led_strip_clear(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
    clock_face_colour_wheel(brightness);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_strip_clear(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void clock_face_colour_wheel(uint8_t brightness)
{
    colour_hsv_t colour = {
        .hue = 0,
        .brightness = brightness,
        .saturation = 100,
    };
    int hue_step = 360 / (total_led_count - hour_led_count);
    int hue_step_hour = 360 / 12;
    colour.hue = 0;
    for (int i = 0; i < hour_led_count; i += 2)
    {
        clock_face_set_pixel(i, normalize_colour(colour));
        clock_face_set_pixel(i + 1, normalize_colour(colour));
        colour.hue += hue_step_hour;
    }
    colour.hue = 0;
    for (int i = hour_led_count; i < total_led_count; i++)
    {
        clock_face_set_pixel(i, normalize_colour(colour));
        colour.hue += hue_step;
    }
    led_strip_refresh(led_strip);
}