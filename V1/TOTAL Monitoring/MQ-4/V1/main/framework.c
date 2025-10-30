/* MQ-2 Gas Sensor with ESP32
 * 
 * This project reads gas concentration data from MQ-2 sensor and transmits
 * it to a remote server via HTTP POST requests.
 * 
 * MQ-2 detects: LPG, Methane, Butane, Alcohol, Hydrogen, and combustible gases
 * 
 * MISRA-C compliant and designed for infrastructure applications.
 * ESP-IDF v5.4 compatible
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <math.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/event_groups.h"
 #include "esp_mac.h"
 #include "esp_wifi.h"
 #include "esp_event.h"
 #include "esp_log.h"
 #include "esp_http_client.h"
 #include "nvs_flash.h"
 #include "lwip/err.h"
 #include "lwip/sys.h"
 #include "cJSON.h"
 
 /* ============================== CONFIGURATION PARAMETERS ============================== */
 /* Modify these parameters to customize the sensor behavior */
 
 /* WiFi Configuration - CHANGE THESE FOR YOUR NETWORK */
 #define WIFI_SSID                    "Homayoun"
 #define WIFI_PASS                    "1q2w3e4r$@"
 #define WIFI_MAXIMUM_RETRY           (5)
 
 /* Server Configuration - CHANGE THIS TO YOUR SERVER */
 #define SERVER_URL                   "http://qrcodepi.local:8000/"
 #define SERVER_TIMEOUT_MS            (10000)
 
 
 
 
 
 
 
 
 
 
 /* Device Information */
#define FIRMWARE_VERSION             "1.0.0"
#define DEVICE_ID_MAX_LEN            (18)  /* MAC address format: XX:XX:XX:XX:XX:XX */
 
 
 
 
 
 /* Global Variables */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static const char *TAG = "Framework";

static int s_retry_num = 0;
static char device_id[DEVICE_ID_MAX_LEN];  /* MAC address as device ID */

 
 
 /* Function Prototypes */
static void wifi_init_sta(void);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static esp_err_t get_device_mac_address(void);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
 
 /**
  * @brief WiFi event handler
  */
 static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
 {
     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
         esp_wifi_connect();
     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
         if (s_retry_num < WIFI_MAXIMUM_RETRY) {
             esp_wifi_connect();
             s_retry_num++;
             ESP_LOGI(TAG, "retry to connect to the AP");
         } else {
             xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
         }
         ESP_LOGI(TAG,"connect to the AP fail");
     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
         ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
         ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
         s_retry_num = 0;
         xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
     }
 }
 
 /**
  * @brief Initialize WiFi in station mode
  */
 static void wifi_init_sta(void)
 {
     s_wifi_event_group = xEventGroupCreate();
     ESP_ERROR_CHECK(esp_netif_init());
     ESP_ERROR_CHECK(esp_event_loop_create_default());
     esp_netif_create_default_wifi_sta();
 
     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
 
     esp_event_handler_instance_t instance_any_id;
     esp_event_handler_instance_t instance_got_ip;
     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));
 
     wifi_config_t wifi_config = {
         .sta = {
             .ssid = WIFI_SSID,
             .password = WIFI_PASS,
             .threshold.authmode = WIFI_AUTH_WPA2_PSK,
             .pmf_cfg = {.capable = true, .required = false},
         },
     };
     
     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
     ESP_ERROR_CHECK(esp_wifi_start());

     vTaskDelay(pdMS_TO_TICKS(100));

     ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(60));
 
     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
 
     if (bits & WIFI_CONNECTED_BIT) {
         ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
     } else if (bits & WIFI_FAIL_BIT) {
         ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
     }
 }
 

/**
 * @brief Get device MAC address and format it as device ID
 */
static esp_err_t get_device_mac_address(void)
{
    uint8_t mac[6];
    esp_err_t ret = esp_efuse_mac_get_default(mac);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Format MAC address as XX:XX:XX:XX:XX:XX */
    snprintf(device_id, DEVICE_ID_MAX_LEN, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    ESP_LOGI(TAG, "Device MAC Address: %s", device_id);
    return ESP_OK;
}

 
 /**
  * @brief HTTP event handler
  */
 static esp_err_t http_event_handler(esp_http_client_event_t *evt)
 {
     switch(evt->event_id) {
         case HTTP_EVENT_ERROR:
             ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
             break;
         case HTTP_EVENT_ON_CONNECTED:
             ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
             break;
         case HTTP_EVENT_ON_DATA:
             ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
             break;
         default:
             break;
     }
     return ESP_OK;
 }
 
 
 /**
  * @brief Main application entry point
  */
 void app_main(void)
{
    ESP_LOGI(TAG, "Firmware Version: %s", FIRMWARE_VERSION);
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* Get device MAC address */
    ESP_ERROR_CHECK(get_device_mac_address());
    
    wifi_init_sta();
    
    }