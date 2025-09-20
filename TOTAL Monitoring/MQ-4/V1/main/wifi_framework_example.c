/**
 * @file wifi_framework_example.c
 * @brief Simple framework of of the Robust WiFi Framework with HTTP Client and DHT22 Sensor
 */

#include "wifi_framework.h"
#include "data_sender.h"
// #include "dht.h"  // DHT22 commented out - replaced with MQ-4
#include "mq4_sensor.h"  // MQ-4 methane sensor
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include <time.h>

/* WiFi Configuration - CHANGE THESE FOR YOUR NETWORK */
#define WIFI_SSID     "Homayoun"
#define WIFI_PASSWORD "1q2w3e4r$@"
/* HTTP Client Configuration - CHANGE THESE FOR YOUR SERVER */
#define SERVER_URL "http://192.168.2.20:7500/"
#define AUTH_TOKEN "NULL"  // Set to NULL if no auth needed

/* MQ-4 Sensor Configuration - CHANGED TO OPTIMAL PIN */
/*
 * MQ-4 Methane Gas Sensor Pin Configuration:
 * 
 * RECOMMENDED: GPIO 32 (ADC_CHANNEL_4) - Current configuration
 * - ADC1 channel works reliably with Wi-Fi active
 * - Less prone to interference from other peripherals
 * - Standard practice for analog sensors in ESP32 projects
 * 
 * Hardware Connections:
 * - MQ-4 VCC    → ESP32 3.3V or 5V (check sensor spec)
 * - MQ-4 GND    → ESP32 GND
 * - MQ-4 A0     → ESP32 GPIO 32 (analog output)
 * - MQ-4 D0     → Optional, any GPIO (digital output)
 * 
 * IMPORTANT: MQ-4 requires load resistor (10kΩ) between A0 and GND
 * Voltage divider may be needed if sensor outputs > 3.3V
 */
#define MQ4_ADC_CHANNEL ADC_CHANNEL_4  // GPIO 32 - Optimal for Wi-Fi applications
#define MQ4_POWER_PIN GPIO_NUM_NC      // Optional power control pin (set to GPIO_NUM_NC if not used)

/* DHT22 Sensor Configuration - COMMENTED OUT - REPLACED WITH MQ-4 */
// #define DHT22_GPIO_PIN GPIO_NUM_4  // Change this to your GPIO pin

static const char* WIFI_TAG = "WiFi_Framework";
static const char* DATA_Q_TAG = "WiFi_Framework";
static const char* MAIN_TAG = "Main";
static bool g_wifi_connected = false;

/* MAC address for device identification */
static char device_mac_str[18]; // "XX:XX:XX:XX:XX:XX" + null terminator

/* ============================== FUNCTION PROTOTYPES ============================== */

static void wifi_event_callback(wifi_framework_event_t event, void* user_data);
static void wifi_monitor_task(void* pvParameters);
static void data_sender_task(void* pvParameters);
static void get_device_mac_address(void);

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
            break;
            
        case WIFI_FRAMEWORK_EVENT_RECONNECTING:
            ESP_LOGI(WIFI_TAG, "WiFi reconnecting event received");
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
 * @brief Data sender task that reads MQ-4 sensor and sends data periodically
 * DHT22 functionality has been commented out and replaced with MQ-4 methane sensor
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
    
    /* Initialize MQ-4 sensor */
    mq4_config_t mq4_config;
    mq4_get_default_config(&mq4_config, ADC_UNIT_1, MQ4_ADC_CHANNEL, MQ4_POWER_PIN);
    
    /* Reduce warmup time for testing (normally 24 hours) */
    mq4_config.warmup_time_ms = 10000U; /* 10 seconds for testing */
    mq4_config.reading_interval_ms = 5000U; /* 5 seconds between readings */
    
    esp_err_t ret = mq4_init(&mq4_config);
    if (ret != ESP_OK) {
        ESP_LOGE(DATA_Q_TAG, "Failed to initialize MQ-4 sensor: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(DATA_Q_TAG, "MQ-4 sensor initialized on ADC1_CH%d (GPIO 32)", MQ4_ADC_CHANNEL);
    
    /* Wait for sensor to stabilize */
    ESP_LOGI(DATA_Q_TAG, "Waiting for MQ-4 sensor to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(2000U)); /* 2 seconds warm-up time */
    
    /* Test sensor reading to verify it's working */
    mq4_reading_t test_reading;
    ret = mq4_read(&test_reading);
    if (ret == ESP_OK && test_reading.is_valid) {
        ESP_LOGI(DATA_Q_TAG, "MQ-4 test reading successful: PPM=%.2f, Voltage=%.3fV, Resistance=%.2fΩ", 
                 test_reading.ppm_methane, test_reading.voltage, test_reading.resistance);
    } else {
        ESP_LOGE(DATA_Q_TAG, "MQ-4 test reading failed: %s", esp_err_to_name(ret));
        ESP_LOGE(DATA_Q_TAG, "Check sensor connections and ADC configuration");
    }
    
    /* DHT22 CODE COMMENTED OUT - REPLACED WITH MQ-4 */
    /*
    // Initialize DHT22 sensor using esp32-dht library
    esp_err_t ret = dht_init(DHT22_GPIO_PIN, DHT_TYPE_DHT22);
    if (ret != ESP_OK) {
        ESP_LOGE(DATA_Q_TAG, "Failed to initialize DHT22 sensor: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    // Configure GPIO for DHT22
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT22_GPIO_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(DATA_Q_TAG, "DHT22 sensor initialized on GPIO %d using esp32-dht library", DHT22_GPIO_PIN);
    
    // Wait for sensor to stabilize (DHT22 needs time after power-up)
    ESP_LOGI(DATA_Q_TAG, "Waiting for DHT22 sensor to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(2000U)); // 2 seconds warm-up time
    
    // Test sensor reading to verify it's working
    float test_temp, test_humidity;
    ret = dht_read(&test_temp, &test_humidity);
    if (ret == ESP_OK) {
        ESP_LOGI(DATA_Q_TAG, "DHT22 test reading successful: Temp=%.1f°C, Humidity=%.1f%%", 
                 test_temp, test_humidity);
    } else {
        ESP_LOGE(DATA_Q_TAG, "DHT22 test reading failed: %s", esp_err_to_name(ret));
        ESP_LOGE(DATA_Q_TAG, "Check sensor connections and GPIO configuration");
    }
    */
    
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
    }
    
    vTaskDelay(pdMS_TO_TICKS(10000U)); /* 10 seconds */

    /* Main loop - read sensor and send data every 30 seconds */
    while (1) {
        /* Check if WiFi is still connected */
        if (!wifi_framework_is_connected() || !wifi_framework_has_ip()) {
            ESP_LOGW(DATA_Q_TAG, "WiFi disconnected, waiting for reconnection...");
            vTaskDelay(pdMS_TO_TICKS(5000U)); /* 5 seconds */
            continue;
        }
        
        /* Read MQ-4 sensor data */
        mq4_reading_t reading;
        ret = mq4_read(&reading);
        
        if (ret == ESP_OK && reading.is_valid) {
            /* Validate sensor readings */
            if (reading.voltage == 0.0f && reading.resistance == 0.0f) {
                ESP_LOGW(DATA_Q_TAG, "MQ-4 returned zero values - possible sensor communication issue");
                ESP_LOGW(DATA_Q_TAG, "Check: wiring, power supply, load resistor, ADC configuration");
                
                /* Send error status */
                data_sender_send_status(device_mac_str, "sensor_zero_values");
                /* Don't use continue - let it go to the delay */
            }
            /* Check for reasonable sensor ranges (voltage now in volts, not millivolts) */
            else if (reading.voltage < 0.0f || reading.voltage > 5.0f || 
                reading.resistance < 1.0f || reading.resistance > 1000000.0f ||
                reading.ppm_methane < 0.0f || reading.ppm_methane > 10000.0f) {
                ESP_LOGW(DATA_Q_TAG, "MQ-4 readings out of range - Voltage: %.3fV, Resistance: %.2fΩ, PPM: %.2f", 
                         reading.voltage, reading.resistance, reading.ppm_methane);
                ESP_LOGW(DATA_Q_TAG, "Expected: Voltage 0-5V, Resistance 1Ω-1MΩ, PPM 0-10000");
                
                /* Send error status */
                data_sender_send_status(device_mac_str, "sensor_out_of_range");
                /* Don't use continue - let it go to the delay */
            }
            else {
                /* Valid readings - send data to server */
                ESP_LOGI(DATA_Q_TAG, "MQ-4 Read: PPM=%.2f, Voltage=%.3fV, Resistance=%.2fΩ", 
                         reading.ppm_methane, reading.voltage, reading.resistance);
                
                /* Send sensor data to server */
                ret = data_sender_send_mq4_data(device_mac_str, "MQ4", 
                                               reading.ppm_methane, 
                                               reading.voltage, 
                                               reading.resistance, 
                                               reading.timestamp);
                
                if (ret == ESP_OK) {
                    ESP_LOGI(DATA_Q_TAG, "MQ-4 data sent successfully to server");
                } else {
                    ESP_LOGE(DATA_Q_TAG, "Failed to send MQ-4 data: %s", esp_err_to_name(ret));
                }
            }
        } else {
            ESP_LOGE(DATA_Q_TAG, "Failed to read MQ-4 sensor: %s", esp_err_to_name(ret));
            
            /* Send error status */
            data_sender_send_status(device_mac_str, "sensor_error");
        }
        

        
        /* Wait 60 seconds before next transmission */
        ESP_LOGI(DATA_Q_TAG, "Waiting 60 seconds before next sensor reading...");
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
    
    ESP_LOGI(WIFI_TAG, "MQ-4 Sensor Configuration:");
    ESP_LOGI(WIFI_TAG, "  ADC Channel: %d (GPIO 32)", MQ4_ADC_CHANNEL);
    ESP_LOGI(WIFI_TAG, "  Power Pin: %d", MQ4_POWER_PIN);
    ESP_LOGI(WIFI_TAG, "  Library: Custom MQ-4 driver");
    
    /* DHT22 CONFIGURATION COMMENTED OUT - REPLACED WITH MQ-4 */
    /*
    ESP_LOGI(WIFI_TAG, "DHT22 Sensor Configuration:");
    ESP_LOGI(WIFI_TAG, "  GPIO Pin: %d", DHT22_GPIO_PIN);
    ESP_LOGI(WIFI_TAG, "  Library: chimpieters/esp32-dht");
    */
    
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

    // First, run the example to see how it works
    mq4_tuning_example();
    
    // Then replace the example data with your actual chart data
    float your_rs_ro_ratios[] = {0.25f, 0.18f, 0.12f, 0.1f, 0.07f, 0.048f, 0.038f, 0.31f, 0.028f};  // Your chart data
    float your_ppm_values[] = {300.0f, 500.0f, 800.0f, 1000.0, 2000.0f, 4000.0f, 6000.0f, 8000.0f, 10000.0f};  // Your chart data
    int num_points = 9;
    
    float A, B;
    esp_err_t ret_o = mq4_tune_parameters_regression(your_rs_ro_ratios, your_ppm_values, num_points, &A, &B);
    
    if (ret_o == ESP_OK) {
        ESP_LOGI(DATA_Q_TAG, "Your tuned parameters: A = %f, B = %f", A, B);
        
        // Test with your current sensor reading
        float current_rs_ro = 2.0f;  // Your current Rs/R0 ratio
        float calculated_ppm = mq4_test_ppm_calculation(current_rs_ro, A, B);
        ESP_LOGI(DATA_Q_TAG, "Current Rs/R0 = %f -> PPM = %f", current_rs_ro, calculated_ppm);
    }
    else
        ESP_LOGE(DATA_Q_TAG, "Failed to tune parameters: %s", esp_err_to_name(ret_o));

    
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
        /* Check if WiFi is connected and has IP */
        if (wifi_framework_is_connected() && wifi_framework_has_ip()) {
            /* WiFi is ready for application use */
            ESP_LOGI(MAIN_TAG, "WiFi is ready for application use");
            
            /* Here you would typically start your main application tasks */
            /* For example: sensor reading, data transmission, etc. */
            
        } else {
            ESP_LOGW(MAIN_TAG, "Waiting for WiFi connection...");
        }
        
        /* Wait before next check */
        vTaskDelay(pdMS_TO_TICKS(20000U)); /* 20 seconds */
    }
} 