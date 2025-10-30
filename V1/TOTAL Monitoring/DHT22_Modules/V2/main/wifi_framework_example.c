/**
 * @file wifi_framework_example.c
 * @brief Simple framework of of the Robust WiFi Framework with HTTP Client and DHT22 Sensor
 */

#include "wifi_framework.h"
#include "data_sender.h"
#include "dht.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include <time.h>

/* WiFi Configuration - CHANGE THESE FOR YOUR NETWORK */
#define WIFI_SSID     "Homayoun"
#define WIFI_PASSWORD "1q2w3e4r$@"

/* HTTP Client Configuration - CHANGE THESE FOR YOUR SERVER */
#define SERVER_URL "http://192.168.2.20:7500/"
#define AUTH_TOKEN "NULL"  // Set to NULL if no auth needed

/* DHT22 Sensor Configuration - CHANGE GPIO PIN AS NEEDED */
#define DHT22_GPIO_PIN GPIO_NUM_4  // Change this to your GPIO pin

/* Error Handling Configuration */
#define MAX_WIFI_CONNECTION_FAILURES 5U    /* Maximum WiFi connection failures before restart */
#define MAX_HTTP_SEND_FAILURES 10U         /* Maximum HTTP send failures before restart */
#define WIFI_FAILURE_RESET_TIME_MS 300000U /* 5 minutes to reset WiFi failure counter */
#define HTTP_FAILURE_RESET_TIME_MS 600000U /* 10 minutes to reset HTTP failure counter */
#define WIFI_RECONNECTION_TIMEOUT_MS 5000U /* 5 seconds timeout for WiFi reconnection */

static const char* WIFI_TAG = "WiFi_Framework";
static const char* DATA_Q_TAG = "WiFi_Framework";
static const char* MAIN_TAG = "Main";
static bool g_wifi_connected = false;

/* Error tracking variables */
static uint32_t wifi_connection_failures = 0U;
static uint32_t http_send_failures = 0U;
static uint64_t last_wifi_failure_time = 0U;
static uint64_t last_http_failure_time = 0U;
static uint64_t wifi_reconnection_start_time = 0U;
static bool wifi_reconnection_in_progress = false;

/* MAC address for device identification */
static char device_mac_str[18]; // "XX:XX:XX:XX:XX:XX" + null terminator

/* ============================== FUNCTION PROTOTYPES ============================== */

static void wifi_event_callback(wifi_framework_event_t event, void* user_data);
static void wifi_monitor_task(void* pvParameters);
static void data_sender_task(void* pvParameters);
static void get_device_mac_address(void);
static void handle_wifi_connection_failure(void);
static void handle_http_send_failure(void);
static void reset_failure_counters_if_needed(void);
static void restart_esp32(const char* reason);
static void check_wifi_reconnection_timeout(void);

/* ============================== MAC ADDRESS FUNCTIONS ============================== */

/**
 * @brief Get the device's MAC address and format it as a string
 */
static void get_device_mac_address(void)
{
    uint8_t mac[6];
    esp_err_t ret = esp_efuse_mac_get_default(mac);
    
    if (ret == ESP_OK) {
        snprintf(device_mac_str, sizeof(device_mac_str), 
                "%02X:%02X:%02X:%02X:%02X:%02X", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        ESP_LOGI(MAIN_TAG, "Device MAC Address: %s", device_mac_str);
    } else {
        /* Fallback to a default MAC if reading fails */
        snprintf(device_mac_str, sizeof(device_mac_str), "00:00:00:00:00:00");
        ESP_LOGE(MAIN_TAG, "Failed to read MAC address, using default: %s", device_mac_str);
    }
}

/* ============================== ERROR HANDLING FUNCTIONS ============================== */

/**
 * @brief Handle WiFi connection failure and track failures
 */
static void handle_wifi_connection_failure(void)
{
    uint64_t current_time = esp_timer_get_time() / 1000U; /* Convert to milliseconds */
    
    /* Reset counter if enough time has passed since last failure */
    if ((current_time - last_wifi_failure_time) > WIFI_FAILURE_RESET_TIME_MS) {
        wifi_connection_failures = 0U;
    }
    
    wifi_connection_failures++;
    last_wifi_failure_time = current_time;
    
    ESP_LOGW(WIFI_TAG, "WiFi connection failure #%lu", (unsigned long)wifi_connection_failures);
    
    if (wifi_connection_failures >= MAX_WIFI_CONNECTION_FAILURES) {
        ESP_LOGE(WIFI_TAG, "Too many WiFi connection failures (%lu), restarting ESP32", 
                 (unsigned long)wifi_connection_failures);
        restart_esp32("WiFi connection failures");
    }
}

/**
 * @brief Handle HTTP send failure and track failures
 */
static void handle_http_send_failure(void)
{
    uint64_t current_time = esp_timer_get_time() / 1000U; /* Convert to milliseconds */
    
    /* Reset counter if enough time has passed since last failure */
    if ((current_time - last_http_failure_time) > HTTP_FAILURE_RESET_TIME_MS) {
        http_send_failures = 0U;
    }
    
    http_send_failures++;
    last_http_failure_time = current_time;
    
    ESP_LOGW(DATA_Q_TAG, "HTTP send failure #%lu", (unsigned long)http_send_failures);
    
    if (http_send_failures >= MAX_HTTP_SEND_FAILURES) {
        ESP_LOGE(DATA_Q_TAG, "Too many HTTP send failures (%lu), restarting ESP32", 
                 (unsigned long)http_send_failures);
        restart_esp32("HTTP send failures");
    }
}

/**
 * @brief Reset failure counters if enough time has passed
 */
static void reset_failure_counters_if_needed(void)
{
    uint64_t current_time = esp_timer_get_time() / 1000U; /* Convert to milliseconds */
    
    /* Reset WiFi failure counter if enough time has passed */
    if ((current_time - last_wifi_failure_time) > WIFI_FAILURE_RESET_TIME_MS) {
        if (wifi_connection_failures > 0U) {
            ESP_LOGI(WIFI_TAG, "Resetting WiFi failure counter after %lu ms", 
                     (unsigned long)WIFI_FAILURE_RESET_TIME_MS);
            wifi_connection_failures = 0U;
        }
    }
    
    /* Reset HTTP failure counter if enough time has passed */
    if ((current_time - last_http_failure_time) > HTTP_FAILURE_RESET_TIME_MS) {
        if (http_send_failures > 0U) {
            ESP_LOGI(DATA_Q_TAG, "Resetting HTTP failure counter after %lu ms", 
                     (unsigned long)HTTP_FAILURE_RESET_TIME_MS);
            http_send_failures = 0U;
        }
    }
}

/**
 * @brief Check if WiFi reconnection has timed out and restart if necessary
 */
static void check_wifi_reconnection_timeout(void)
{
    if (wifi_reconnection_in_progress) {
        uint64_t current_time = esp_timer_get_time() / 1000U; /* Convert to milliseconds */
        
        if ((current_time - wifi_reconnection_start_time) > WIFI_RECONNECTION_TIMEOUT_MS) {
            ESP_LOGE(WIFI_TAG, "WiFi reconnection timeout after %lu ms, restarting ESP32", 
                     (unsigned long)WIFI_RECONNECTION_TIMEOUT_MS);
            restart_esp32("WiFi reconnection timeout");
        }
    }
}

/**
 * @brief Restart the ESP32 with a given reason
 * @param reason Reason for restart (for logging)
 */
static void restart_esp32(const char* reason)
{
    if (reason != NULL) {
        ESP_LOGE(MAIN_TAG, "Restarting ESP32 due to: %s", reason);
    } else {
        ESP_LOGE(MAIN_TAG, "Restarting ESP32 due to critical error");
    }
    
    /* Give some time for logs to be sent */
    vTaskDelay(pdMS_TO_TICKS(2000U));
    
    /* Restart the ESP32 */
    esp_restart();
}

/* ============================== EVENT CALLBACK ============================== */

/**
 * @brief WiFi framework event callback
 */
static void wifi_event_callback(wifi_framework_event_t event, void* user_data)
{
    (void)user_data; /* Unused parameter */
    
    switch (event) {
        case WIFI_FRAMEWORK_EVENT_CONNECTED:
            ESP_LOGI(WIFI_TAG, "WiFi connected event received");
            g_wifi_connected = true;
            break;
            
        case WIFI_FRAMEWORK_EVENT_DISCONNECTED:
            ESP_LOGI(WIFI_TAG, "WiFi disconnected event received");
            g_wifi_connected = false;
            wifi_reconnection_in_progress = false;
            break;
            
        case WIFI_FRAMEWORK_EVENT_IP_ACQUIRED:
            ESP_LOGI(WIFI_TAG, "IP address acquired event received");
            break;
            
        case WIFI_FRAMEWORK_EVENT_IP_LOST:
            ESP_LOGW(WIFI_TAG, "IP address lost event received");
            break;
            
        case WIFI_FRAMEWORK_EVENT_CONNECTION_FAILED:
            ESP_LOGE(WIFI_TAG, "WiFi connection failed event received");
            g_wifi_connected = false;
            wifi_reconnection_in_progress = false;
            handle_wifi_connection_failure();
            break;
            
        case WIFI_FRAMEWORK_EVENT_RECONNECTING:
            ESP_LOGI(WIFI_TAG, "WiFi reconnecting event received");
            wifi_reconnection_in_progress = true;
            wifi_reconnection_start_time = esp_timer_get_time() / 1000U; /* Convert to milliseconds */
            break;
            
        default:
            ESP_LOGW(WIFI_TAG, "Unknown WiFi event: %d", event);
            break;
    }
}

/* ============================== MONITOR TASK ============================== */

/**
 * @brief WiFi monitoring task
 */
static void wifi_monitor_task(void* pvParameters)
{
    (void)pvParameters; /* Unused parameter */
    
    wifi_framework_status_t status;
    
    while (1) {
        /* Get current WiFi status */
        esp_err_t ret = wifi_framework_get_status(&status);
        if (ret == ESP_OK) {
            ESP_LOGI(WIFI_TAG, "WiFi Status - State: %d, Connected: %s, Has IP: %s, RSSI: %d, Retry Count: %d",
                     status.state,
                     status.is_connected ? "Yes" : "No",
                     status.has_ip ? "Yes" : "No",
                     status.rssi,
                     status.retry_count);
            
            /* Check connection health */
            if (status.is_connected && status.has_ip) {
                esp_netif_ip_info_t ip_info;
                if (wifi_framework_get_ip_info(&ip_info) == ESP_OK) {
                    ESP_LOGI(WIFI_TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
                    ESP_LOGI(WIFI_TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
                    ESP_LOGI(WIFI_TAG, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
                }
            }
        } else {
            ESP_LOGE(WIFI_TAG, "Failed to get WiFi status: %s", esp_err_to_name(ret));
        }
        
        /* Wait before next check */
        vTaskDelay(pdMS_TO_TICKS(60000U)); /* 60 seconds */
    }
}

/* ============================== DATA SENDER TASK ============================== */

/**
 * @brief Data sender task that reads DHT22 sensor and sends data periodically
 */
static void data_sender_task(void* pvParameters)
{
    (void)pvParameters; /* Unused parameter */
    
    ESP_LOGI(DATA_Q_TAG, "Data sender task started");
    
    /* Wait for WiFi to be ready */
    while (!wifi_framework_is_connected() || !wifi_framework_has_ip()) {
        ESP_LOGI(DATA_Q_TAG, "Waiting for WiFi connection before starting data sender...");
        vTaskDelay(pdMS_TO_TICKS(2000U)); /* 2 seconds */
    }
    
    /* Initialize DHT22 sensor using esp32-dht library */
    esp_err_t ret = dht_init(DHT22_GPIO_PIN, DHT_TYPE_DHT22);
    if (ret != ESP_OK) {
        ESP_LOGE(DATA_Q_TAG, "Failed to initialize DHT22 sensor: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    /* Configure GPIO for DHT22 */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT22_GPIO_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(DATA_Q_TAG, "DHT22 sensor initialized on GPIO %d using esp32-dht library", DHT22_GPIO_PIN);
    
    /* Wait for sensor to stabilize (DHT22 needs time after power-up) */
    ESP_LOGI(DATA_Q_TAG, "Waiting for DHT22 sensor to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(2000U)); /* 2 seconds warm-up time */
    
    /* Test sensor reading to verify it's working */
    float test_temp, test_humidity;
    ret = dht_read(&test_temp, &test_humidity);
    if (ret == ESP_OK) {
        ESP_LOGI(DATA_Q_TAG, "DHT22 test reading successful: Temp=%.1f°C, Humidity=%.1f%%", 
                 test_temp, test_humidity);
    } else {
        ESP_LOGE(DATA_Q_TAG, "DHT22 test reading failed: %s", esp_err_to_name(ret));
        ESP_LOGE(DATA_Q_TAG, "Check sensor connections and GPIO configuration");
    }
    
    /* Initialize data sender */
    ret = data_sender_init(SERVER_URL, AUTH_TOKEN);
    if (ret != ESP_OK) {
        ESP_LOGE(DATA_Q_TAG, "Failed to initialize data sender: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(DATA_Q_TAG, "Data sender initialized successfully");
    
    /* Send initial status */
    ret = data_sender_send_status(device_mac_str, "online");
    if (ret == ESP_OK) {
        ESP_LOGI(DATA_Q_TAG, "Initial status sent successfully");
    } else {
        ESP_LOGE(DATA_Q_TAG, "Failed to send initial status");
        handle_http_send_failure();
    }
    
    /* Main loop - read sensor and send data every 30 seconds */
    while (1) {
        /* Check if WiFi is still connected */
        if (!wifi_framework_is_connected() || !wifi_framework_has_ip()) {
            ESP_LOGW(DATA_Q_TAG, "WiFi disconnected, waiting for reconnection...");
            
            /* Check for reconnection timeout */
            check_wifi_reconnection_timeout();
            
            vTaskDelay(pdMS_TO_TICKS(1000U)); /* 1 second - check more frequently */
            continue;
        }
        
        /* Read DHT22 sensor data using esp32-dht library */
        float temperature, humidity;
        ret = dht_read(&temperature, &humidity);
        
        if (ret == ESP_OK) {
            /* Validate sensor readings */
            if (temperature == 0.0f && humidity == 0.0f) {
                ESP_LOGW(DATA_Q_TAG, "DHT22 returned zero values - possible sensor communication issue");
                ESP_LOGW(DATA_Q_TAG, "Check: wiring, power supply, pull-up resistor, GPIO configuration");
                
                /* Send error status */
                esp_err_t status_ret = data_sender_send_status(device_mac_str, "sensor_zero_values");
                if (status_ret != ESP_OK) {
                    handle_http_send_failure();
                }
                continue;
            }
            
            /* Check for reasonable sensor ranges */
            if (temperature < -40.0f || temperature > 80.0f || humidity < 0.0f || humidity > 100.0f) {
                ESP_LOGW(DATA_Q_TAG, "DHT22 readings out of range - Temp: %.1f°C, Humidity: %.1f%%", 
                         temperature, humidity);
                ESP_LOGW(DATA_Q_TAG, "Expected: Temp -40°C to +80°C, Humidity 0%% to 100%%");
                
                /* Send error status */
                esp_err_t status_ret = data_sender_send_status(device_mac_str, "sensor_out_of_range");
                if (status_ret != ESP_OK) {
                    handle_http_send_failure();
                }
                continue;
            }
            
            ESP_LOGI(DATA_Q_TAG, "DHT22 Read: Temp=%.1f°C, Humidity=%.1f%%", temperature, humidity);
            
            /* Send sensor data to server */
            uint64_t timestamp = time(NULL) * 1000ULL; /* Convert to milliseconds */
            ret = data_sender_send_dht22_data(device_mac_str, "DHT22", 
                                             temperature, 
                                             humidity, 
                                             timestamp);
            
            if (ret == ESP_OK) {
                ESP_LOGI(DATA_Q_TAG, "DHT22 data sent successfully to server");
                /* Reset HTTP failure counter on successful send */
                if (http_send_failures > 0U) {
                    ESP_LOGI(DATA_Q_TAG, "Resetting HTTP failure counter after successful send");
                    http_send_failures = 0U;
                }
            } else {
                ESP_LOGE(DATA_Q_TAG, "Failed to send DHT22 data: %s", esp_err_to_name(ret));
                handle_http_send_failure();
            }
        } else {
            ESP_LOGE(DATA_Q_TAG, "Failed to read DHT22 sensor: %s", esp_err_to_name(ret));
            
            /* Send error status */
            esp_err_t status_ret = data_sender_send_status(device_mac_str, "sensor_error");
            if (status_ret != ESP_OK) {
                handle_http_send_failure();
            }
        }
        
        /* Wait before next transmission */
        vTaskDelay(pdMS_TO_TICKS(60000)); /* 60 seconds */
    }
}

/* ============================== MAIN APPLICATION ============================== */

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    ESP_LOGI(DATA_Q_TAG, "Starting WiFi Framework Example with HTTP Client");
    
    /* Get device MAC address */
    get_device_mac_address();
    
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* Get default WiFi configuration */
    wifi_framework_config_t wifi_config;
    wifi_framework_get_default_config(&wifi_config, WIFI_SSID, WIFI_PASSWORD);
    
    /* Customize configuration for infrastructure use */
    wifi_config.max_retry_count = 10U;           /* More retries for infrastructure */
    wifi_config.connection_timeout_ms = 60000U;  /* 60 seconds timeout */
    wifi_config.retry_delay_ms = 10000U;         /* 10 seconds between retries */
    wifi_config.auto_reconnect = true;           /* Enable auto-reconnect */
    wifi_config.max_tx_power = 60;               /* Maximum transmit power */
    
    ESP_LOGI(WIFI_TAG, "WiFi Configuration:");
    ESP_LOGI(WIFI_TAG, "  SSID: %s", wifi_config.ssid);
    ESP_LOGI(WIFI_TAG, "  Max Retries: %d", wifi_config.max_retry_count);
    ESP_LOGI(WIFI_TAG, "  Connection Timeout: %lu ms", wifi_config.connection_timeout_ms);
    ESP_LOGI(WIFI_TAG, "  Retry Delay: %lu ms", wifi_config.retry_delay_ms);
    ESP_LOGI(WIFI_TAG, "  Auto Reconnect: %s", wifi_config.auto_reconnect ? "Enabled" : "Disabled");
    ESP_LOGI(WIFI_TAG, "  Max TX Power: %d", wifi_config.max_tx_power);
    
    ESP_LOGI(WIFI_TAG, "HTTP Client Configuration:");
    ESP_LOGI(WIFI_TAG, "  Server URL: %s", SERVER_URL);
    ESP_LOGI(WIFI_TAG, "  Device MAC: %s", device_mac_str);
    ESP_LOGI(WIFI_TAG, "  Auth Token: %s", AUTH_TOKEN ? "Configured" : "None");
    
    ESP_LOGI(WIFI_TAG, "DHT22 Sensor Configuration:");
    ESP_LOGI(WIFI_TAG, "  GPIO Pin: %d", DHT22_GPIO_PIN);
    ESP_LOGI(WIFI_TAG, "  Library: chimpieters/esp32-dht");
    
    /* Initialize WiFi framework */
    ret = wifi_framework_init(&wifi_config, wifi_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to initialize WiFi framework: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(WIFI_TAG, "WiFi framework initialized successfully");
    
    /* Connect to WiFi */
    ret = wifi_framework_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to connect to WiFi: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(WIFI_TAG, "WiFi connection initiated");
    
    /* Create WiFi monitoring task */
    TaskHandle_t monitor_task_handle = NULL;
    ret = xTaskCreate(wifi_monitor_task,
                      "wifi_monitor",
                      4096U,
                      NULL,
                      5U,
                      &monitor_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(WIFI_TAG, "Failed to create WiFi monitor task");
        return;
    }
    
    ESP_LOGI(WIFI_TAG, "WiFi monitor task created");
    
    /* Create data sender task */
    TaskHandle_t data_sender_task_handle = NULL;
    ret = xTaskCreate(data_sender_task,
                      "data_sender",
                      4096U,
                      NULL,
                      5U,
                      &data_sender_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(WIFI_TAG, "Failed to create data sender task");
        return;
    }
    
    ESP_LOGI(WIFI_TAG, "Data sender task created");
    
    /* Main application loop */
    while (1) {
        /* Reset failure counters if enough time has passed */
        reset_failure_counters_if_needed();
        
        /* Check for WiFi reconnection timeout */
        check_wifi_reconnection_timeout();
        
        /* Check if WiFi is connected and has IP */
        if (wifi_framework_is_connected() && wifi_framework_has_ip()) {
            /* WiFi is ready for application use */
            ESP_LOGI(MAIN_TAG, "WiFi is ready for application use");
            
            /* Reset WiFi failure counter on successful connection */
            if (wifi_connection_failures > 0U) {
                ESP_LOGI(MAIN_TAG, "Resetting WiFi failure counter after successful connection");
                wifi_connection_failures = 0U;
            }
            
            /* Clear reconnection state on successful connection */
            if (wifi_reconnection_in_progress) {
                ESP_LOGI(MAIN_TAG, "WiFi reconnection completed successfully");
                wifi_reconnection_in_progress = false;
            }
            
            /* Here you would typically start your main application tasks */
            /* For example: sensor reading, data transmission, etc. */
            
        } else {
            ESP_LOGW(MAIN_TAG, "Waiting for WiFi connection...");
        }
        
        /* Wait before next check */
        vTaskDelay(pdMS_TO_TICKS(20000U)); /* 20 seconds */
    }
} 