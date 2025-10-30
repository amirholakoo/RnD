/* MQ-135 Air Quality Sensor with ESP32
 * 
 * This project reads air quality data from MQ-135 sensor and transmits
 * it to a remote server via HTTP POST requests.
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
 #include "esp_adc/adc_oneshot.h"
 #include "esp_adc/adc_cali.h"
 #include "esp_adc/adc_cali_scheme.h"
 #include "nvs_flash.h"
 #include "lwip/err.h"
 #include "lwip/sys.h"
 #include "cJSON.h"
 
 /* ============================== CONFIGURATION PARAMETERS ============================== */
 /* Modify these parameters to customize the sensor behavior */
 
 /* WiFi Configuration - CHANGE THESE FOR YOUR NETWORK */
 #define WIFI_SSID                     "Homayoun"
 #define WIFI_PASS                     "1q2w3e4r$@"
 #define WIFI_MAXIMUM_RETRY           (5)
 
 /* Server Configuration - CHANGE THIS TO YOUR SERVER */
 #define SERVER_URL                   "http://qrcodepi.local:8000/"
 #define SERVER_TIMEOUT_MS            (10000)
 
 /* Sensor Configuration */
 #define MQ135_ADC_CHANNEL            ADC_CHANNEL_0  /* GPIO36 for ESP32 */
 #define MQ135_ADC_UNIT               ADC_UNIT_1
 #define MQ135_ADC_ATTEN              ADC_ATTEN_DB_11
 #define MQ135_ADC_BITWIDTH           ADC_BITWIDTH_12
 
 /* Sensor Calibration Parameters - Adjust based on your specific MQ-135 sensor */
 #define MQ135_RZERO                  (76.63f)       /* Sensor resistance in clean air */
 #define MQ135_PARA_A                 (116.6020682f) /* Calibration parameter A */
 #define MQ135_PARA_B                 (-2.769034857f)/* Calibration parameter B */
 #define MQ135_RL_VALUE               (10.0f)        /* Load resistor value in kOhms */
 #define MQ135_ADC_VREF_MV            (3300)         /* ADC reference voltage */
 #define MQ135_ADC_MAX_VAL            (4095)         /* 12-bit ADC maximum value */
 
 /* Timing Configuration */
 #define SENSOR_READ_INTERVAL_MS      (1000)        /* 30 seconds between readings */
 #define SENSOR_WARMUP_TIME_MS        (20000)        /* 20 seconds sensor warmup */
 #define HTTP_RETRY_COUNT             (3)            /* Number of HTTP retry attempts */
 #define HTTP_RETRY_DELAY_MS          (2000)         /* Delay between HTTP retries */
 
 /* Data Processing */
 #define ADC_SAMPLES_COUNT            (10)           /* Number of ADC samples for averaging */
 #define ADC_SAMPLE_DELAY_MS          (100)          /* Delay between ADC samples */
 
 /* Device Information */
#define FIRMWARE_VERSION             "1.0.0"
#define DEVICE_ID_MAX_LEN            (18)  /* MAC address format: XX:XX:XX:XX:XX:XX */
 
 /* Global Variables */
static const char *TAG = "MQ135_SENSOR";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static int s_retry_num = 0;
static char device_id[DEVICE_ID_MAX_LEN];  /* MAC address as device ID */
 
 /* Sensor data structure */
 typedef struct {
     float voltage_mv;
     float resistance_kohm;
     float ppm_co2;
     float air_quality_index;
     uint32_t timestamp;
 } sensor_data_t;
 
 /* Function Prototypes */
static void wifi_init_sta(void);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t adc_init(void);
static esp_err_t get_device_mac_address(void);
static esp_err_t read_mq135_sensor(sensor_data_t* data);
static float calculate_ppm_co2(float resistance);
static float calculate_air_quality_index(float ppm_co2);
static esp_err_t send_data_to_server(const sensor_data_t* data);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static void sensor_task(void* pvParameters);
 
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

     ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(40));
 
     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
 
     if (bits & WIFI_CONNECTED_BIT) {
         ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
     } else if (bits & WIFI_FAIL_BIT) {
         ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
     }
 }
 
 /**
  * @brief Initialize ADC for MQ-135 sensor
  */
 static esp_err_t adc_init(void)
 {
     adc_oneshot_unit_init_cfg_t init_config1 = {
         .unit_id = MQ135_ADC_UNIT,
         .ulp_mode = ADC_ULP_MODE_DISABLE,
     };
     ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
 
     adc_oneshot_chan_cfg_t config = {
         .bitwidth = MQ135_ADC_BITWIDTH,
         .atten = MQ135_ADC_ATTEN,
     };
     ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, MQ135_ADC_CHANNEL, &config));
 
     adc_cali_curve_fitting_config_t cali_config = {
         .unit_id = MQ135_ADC_UNIT,
         .atten = MQ135_ADC_ATTEN,
         .bitwidth = MQ135_ADC_BITWIDTH,
     };
     
     esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
     if (ret == ESP_OK) {
         ESP_LOGI(TAG, "ADC calibration scheme: Curve Fitting");
     } else {
         ESP_LOGW(TAG, "ADC calibration scheme not supported, using raw values");
         adc1_cali_handle = NULL;
     }
 
         return ESP_OK;
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
 * @brief Read MQ-135 sensor data
 */
 static esp_err_t read_mq135_sensor(sensor_data_t* data)
 {
     if (data == NULL) {
         return ESP_ERR_INVALID_ARG;
     }
 
     int adc_raw_total = 0;
     int adc_voltage_mv_total = 0;
     
     for (int i = 0; i < ADC_SAMPLES_COUNT; i++) {
         int adc_raw = 0;
         ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, MQ135_ADC_CHANNEL, &adc_raw));
         adc_raw_total += adc_raw;
         
         if (adc1_cali_handle != NULL) {
             int voltage_mv = 0;
             adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_mv);
             adc_voltage_mv_total += voltage_mv;
         }
         vTaskDelay(pdMS_TO_TICKS(ADC_SAMPLE_DELAY_MS));
     }
     
     float adc_raw_avg = (float)adc_raw_total / ADC_SAMPLES_COUNT;
     
     if (adc1_cali_handle != NULL) {
         data->voltage_mv = (float)adc_voltage_mv_total / ADC_SAMPLES_COUNT;
     } else {
         data->voltage_mv = (adc_raw_avg / MQ135_ADC_MAX_VAL) * MQ135_ADC_VREF_MV;
     }
     
     float vrl = data->voltage_mv;
     float vcc = MQ135_ADC_VREF_MV;
     
     if (vrl <= 0.0f || vrl >= vcc) {
         ESP_LOGW(TAG, "Invalid voltage reading: %.2f mV", data->voltage_mv);
         return ESP_ERR_INVALID_RESPONSE;
     }
     
     data->resistance_kohm = ((vcc - vrl) / vrl) * MQ135_RL_VALUE;
     data->ppm_co2 = calculate_ppm_co2(data->resistance_kohm);
     data->air_quality_index = calculate_air_quality_index(data->ppm_co2);
     data->timestamp = esp_log_timestamp();
     
     ESP_LOGI(TAG, "Sensor Reading - Voltage: %.2f mV, Resistance: %.2f kΩ, CO2: %.1f ppm, AQI: %.1f", 
              data->voltage_mv, data->resistance_kohm, data->ppm_co2, data->air_quality_index);
     
     return ESP_OK;
 }
 
 /**
  * @brief Calculate CO2 PPM from sensor resistance
  */
 static float calculate_ppm_co2(float resistance)
 {
     if (resistance <= 0.0f) return 0.0f;
     
     float ratio = resistance / MQ135_RZERO;
     if (ratio <= 0.0f) return 0.0f;
     
     float ppm = MQ135_PARA_A * powf(ratio, MQ135_PARA_B);
     
     if (ppm < 10.0f) ppm = 10.0f;
     else if (ppm > 2000.0f) ppm = 2000.0f;
     
     return ppm;
 }
 
 /**
  * @brief Calculate Air Quality Index from CO2 PPM
  */
 static float calculate_air_quality_index(float ppm_co2)
 {
     float aqi = 0.0f;
     
     if (ppm_co2 <= 400.0f) {
         aqi = 50.0f;  /* Good */
     } else if (ppm_co2 <= 1000.0f) {
         aqi = 50.0f + ((ppm_co2 - 400.0f) / 600.0f) * 50.0f;  /* Good to Moderate */
     } else if (ppm_co2 <= 2000.0f) {
         aqi = 100.0f + ((ppm_co2 - 1000.0f) / 1000.0f) * 50.0f;  /* Moderate to Unhealthy */
     } else {
         aqi = 150.0f;  /* Unhealthy */
     }
     
     return aqi;
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
  * @brief Send sensor data to server via HTTP POST
  */
 static esp_err_t send_data_to_server(const sensor_data_t* data)
 {
     if (data == NULL) return ESP_ERR_INVALID_ARG;
     
     cJSON *json = cJSON_CreateObject();
     cJSON_AddStringToObject(json, "device_id", device_id);
    //  cJSON_AddStringToObject(json, "firmware_version", FIRMWARE_VERSION);
    cJSON_AddStringToObject(json, "sensor_type", "MQ-135");
    //  cJSON_AddNumberToObject(json, "timestamp", data->timestamp);
    //  cJSON_AddNumberToObject(json, "voltage_mv", data->voltage_mv);
    //  cJSON_AddNumberToObject(json, "resistance_kohm", data->resistance_kohm);


    cJSON *gas_data = cJSON_CreateObject();
     cJSON_AddNumberToObject(gas_data, "co2_ppm", data->ppm_co2);
     cJSON_AddNumberToObject(gas_data, "air_quality_index", data->air_quality_index);
     cJSON_AddItemToObject(json, "data", gas_data);

     
     char *json_string = cJSON_Print(json);
     if (json_string == NULL) {
         cJSON_Delete(json);
         return ESP_ERR_NO_MEM;
     }
     
     ESP_LOGI(TAG, "Sending data: %s", json_string);
     
     esp_http_client_config_t config = {
         .url = SERVER_URL,
         .event_handler = http_event_handler,
         .timeout_ms = SERVER_TIMEOUT_MS,
     };
     
     esp_http_client_handle_t client = esp_http_client_init(&config);
     esp_http_client_set_header(client, "Content-Type", "application/json");
     esp_http_client_set_method(client, HTTP_METHOD_POST);
     esp_http_client_set_post_field(client, json_string, strlen(json_string));
     
     esp_err_t ret = ESP_FAIL;
     for (int retry = 0; retry < HTTP_RETRY_COUNT; retry++) {
         ret = esp_http_client_perform(client);
         
         if (ret == ESP_OK) {
             int status_code = esp_http_client_get_status_code(client);
             ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);
             
             if (status_code >= 200 && status_code < 300) {
                 ESP_LOGI(TAG, "Data sent successfully");
                 break;
             }
         } else {
             ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(ret));
         }
         
         if (retry < HTTP_RETRY_COUNT - 1) {
             vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
         }
     }
     
     esp_http_client_cleanup(client);
     free(json_string);
     cJSON_Delete(json);
     
     return ret;
 }
 
 /**
  * @brief Main sensor task
  */
 static void sensor_task(void* pvParameters)
 {
     sensor_data_t sensor_data;
     
     ESP_LOGI(TAG, "Sensor warming up for %d seconds...", SENSOR_WARMUP_TIME_MS / 1000);
     vTaskDelay(pdMS_TO_TICKS(SENSOR_WARMUP_TIME_MS));
     ESP_LOGI(TAG, "Sensor warmup complete, starting measurements");
     
     while (1) {
         esp_err_t ret = read_mq135_sensor(&sensor_data);
         
         if (ret == ESP_OK) {
             ret = send_data_to_server(&sensor_data);
             if (ret != ESP_OK) {
                 ESP_LOGW(TAG, "Failed to send data to server: %s", esp_err_to_name(ret));
             }
         } else {
             ESP_LOGE(TAG, "Failed to read sensor: %s", esp_err_to_name(ret));
         }
         
         vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
     }
 }
 
 /**
  * @brief Main application entry point
  */
 void app_main(void)
{
    ESP_LOGI(TAG, "MQ-135 Air Quality Sensor starting...");
    ESP_LOGI(TAG, "Firmware Version: %s", FIRMWARE_VERSION);
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* Get device MAC address */
    ESP_ERROR_CHECK(get_device_mac_address());
    
    ESP_ERROR_CHECK(adc_init());
    ESP_LOGI(TAG, "ADC initialized successfully");
    
    wifi_init_sta();
    
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "MQ-135 Air Quality Sensor started successfully");
}