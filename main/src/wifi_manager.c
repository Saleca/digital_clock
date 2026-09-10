#include "wifi_manager.h"
#include "server_manager.h"
#include "string.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "freertos/freeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "WIFI_MANAGER";

static void start_wifi_task(void *arg);
static void init_ap(void);
static void init_sta(const char *ssid, const char *pass);
static void sta2ap_task(void *arg);
static void deinit_wifi(void);
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static esp_err_t credentials_handler(httpd_req_t *req);

volatile bool wifi_manager_ap_connected = false;
volatile bool wifi_manager_sta_connected = false;

static uint8_t max_restart_count = 5;
static uint8_t restart_count = 0;
static esp_netif_t *netif = NULL;

static route_t credentials_route = {
    .uri = "/post/credentials",
    .method = HTTP_POST,
    .route_handler = credentials_handler,
    .args = NULL,
};

void wifi_manager_init(void)
{
  xTaskCreate(start_wifi_task, "start_wifi", 4096, NULL, 5, NULL);
}

void wifi_register_route(void)
{
  server_manager_add_route(&credentials_route);
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT &&
      event_id == WIFI_EVENT_AP_START)
  {
    wifi_manager_ap_connected = true;
    ESP_LOGI(TAG, "AP started");
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
  {
    wifi_event_ap_staconnected_t *evt = (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(TAG, "AP - device connected, aid=%d", evt->aid);
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
  {
    wifi_event_ap_stadisconnected_t *evt = (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(TAG, "AP - device disconnected, reason=%d", evt->reason);
  }
  else if (event_base == WIFI_EVENT &&
           event_id == WIFI_EVENT_STA_START)
  {
    restart_count = 0;
    esp_wifi_connect();
  }
  else if (event_base == IP_EVENT &&
           event_id == IP_EVENT_STA_GOT_IP)
  {
    wifi_manager_sta_connected = true;
    ESP_LOGI(TAG, "STA started");
  }
  else if (event_base == WIFI_EVENT &&
           event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    wifi_manager_ap_connected = false;//
    wifi_manager_sta_connected = false;

    restart_count++;
    ESP_LOGI(TAG, "Wifi disconnected, retrying connection... %d/%d", restart_count, max_restart_count);
    if (restart_count > max_restart_count)
    {
      restart_count = 0;
      xTaskCreate(sta2ap_task, "sta2ap_task", 4096, NULL, 5, NULL);
      return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_wifi_connect();
  }
}

static void start_wifi_task(void *arg)
{
  esp_netif_init();
  esp_event_loop_create_default();

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  char ssid[64], pass[64];

  bool valid_credentials = false;
  nvs_handle_t nvs;
  if (nvs_open(TAG, NVS_READONLY, &nvs) == ESP_OK)
  {
    size_t len = sizeof(ssid);
    nvs_get_str(nvs, "ssid", ssid, &len);
    len = sizeof(pass);
    nvs_get_str(nvs, "pass", pass, &len);
    nvs_close(nvs);
    valid_credentials = true;
  }

  if (valid_credentials)
  {
    ESP_LOGI(TAG,"ssid: %s", ssid);
    ESP_LOGI(TAG,"pass: %s", pass);
    init_sta(ssid, pass);
  }
  else
  {
    init_ap();
  }

  esp_wifi_start();
  vTaskDelete(NULL);
}

static void init_sta(const char *ssid, const char *pass)
{

  netif = esp_netif_create_default_wifi_sta();

  wifi_config_t wifi_cfg = {0};
  wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  strlcpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
  strlcpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
}

static void init_ap(void)
{
  netif = esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  wifi_config_t wifi_cfg = {0};
  strlcpy((char *)wifi_cfg.ap.ssid, "Relogio", sizeof(wifi_cfg.ap.ssid));
  wifi_cfg.ap.ssid_len = strlen("Relogio");
  strlcpy((char *)wifi_cfg.ap.password, "", sizeof(wifi_cfg.ap.password));
  wifi_cfg.ap.channel = 1;
  wifi_cfg.ap.max_connection = 2;
  wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
  wifi_cfg.ap.pmf_cfg.capable = true;
  wifi_cfg.ap.pmf_cfg.required = false;

  esp_wifi_init(&cfg);

  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
  esp_wifi_set_ps(WIFI_PS_NONE);
}

static void deinit_wifi(void)
{
  if (netif != NULL)
  {
    esp_netif_destroy_default_wifi(netif);
    netif = NULL;
  }
  esp_wifi_stop();
  esp_wifi_deinit();
  wifi_manager_ap_connected = false;
  wifi_manager_sta_connected = false;
}

// skips credentials checking
static void sta2ap_task(void *arg)
{
  deinit_wifi();
  init_ap();
  esp_wifi_start();
  vTaskDelete(NULL);
}

static bool valid_ssid(const char *s)
{
  size_t len = strlen(s);
  if (len < 1 || len > 32)
    return false;
  for (size_t i = 0; i < len; i++)
    if (s[i] < 0x20 || s[i] > 0x7e)
      return false;
  return true;
}

static bool valid_password(const char *s)
{
  size_t len = strlen(s);
  if (len == 0)
    return true; // open network
  if (len < 8 || len > 63)
    return false;
  for (size_t i = 0; i < len; i++)
    if (s[i] < 0x20 || s[i] > 0x7e)
      return false;
  return true;
}

static esp_err_t credentials_handler(httpd_req_t *req)
{
  char buf[256];
  int received = httpd_req_recv(req, buf, sizeof(buf) - 1);

  if (received <= 0)
  {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  buf[received] = '\0';

  // parse JSON  {"ssid":"...", "password":"..."}
  cJSON *root = cJSON_Parse(buf);
  if (!root)
  {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "{\"error\":\"invalid json\"}");
    return ESP_OK;
  }

  cJSON *j_ssid = cJSON_GetObjectItem(root, "ssid");
  cJSON *j_pass = cJSON_GetObjectItem(root, "password");

  if (!cJSON_IsString(j_ssid) || !valid_ssid(j_ssid->valuestring))
  {
    cJSON_Delete(root);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "{\"error\":\"ssid must be 1-32 printable characters\"}");
    return ESP_OK;
  }

  if (!cJSON_IsString(j_pass) || !valid_password(j_pass->valuestring))
  {
    cJSON_Delete(root);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "{\"error\":\"password must be 8-63 printable characters\"}");
    return ESP_OK;
  }

  // save to NVS
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(TAG, NVS_READWRITE, &nvs);
  if (err == ESP_OK)
  {
    nvs_set_str(nvs, "ssid", j_ssid->valuestring);
    nvs_set_str(nvs, "pass", j_pass->valuestring);
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

  vTaskDelay(pdMS_TO_TICKS(100));

  esp_restart();

  return ESP_OK;
}