/**
 * @file wifi_framework.h
 * @brief Robust WiFi Framework for ESP32 Infrastructure Applications
 * 
 * This module provides a robust WiFi implementation with proper error handling,
 * state management, and event handling for infrastructure applications.
 * 
 * MISRA-C compliant and designed for ESP-IDF v5.4
 * 
 * @author Infrastructure Development Team
 * @version 1.0.0
 * @date 2024
 */

#ifndef WIFI_FRAMEWORK_H
#define WIFI_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

/* ============================== CONFIGURATION CONSTANTS ============================== */

/* WiFi Configuration */
#define WIFI_FRAMEWORK_MAX_SSID_LEN          (32U)
#define WIFI_FRAMEWORK_MAX_PASSWORD_LEN      (64U)
#define WIFI_FRAMEWORK_MAX_RETRY_COUNT       (10U)
#define WIFI_FRAMEWORK_CONNECTION_TIMEOUT_MS (30000U)  /* 30 seconds */
#define WIFI_FRAMEWORK_RETRY_DELAY_MS        (5000U)   /* 5 seconds */
#define WIFI_FRAMEWORK_WATCHDOG_TIMEOUT_MS   (60000U)  /* 1 minute */

/* WiFi Framework States */
typedef enum {
    WIFI_FRAMEWORK_STATE_UNINITIALIZED = 0U,
    WIFI_FRAMEWORK_STATE_INITIALIZING,
    WIFI_FRAMEWORK_STATE_INITIALIZED,
    WIFI_FRAMEWORK_STATE_CONNECTING,
    WIFI_FRAMEWORK_STATE_CONNECTED,
    WIFI_FRAMEWORK_STATE_DISCONNECTED,
    WIFI_FRAMEWORK_STATE_ERROR,
    WIFI_FRAMEWORK_STATE_MAX
} wifi_framework_state_t;

/* WiFi Framework Events */
typedef enum {
    WIFI_FRAMEWORK_EVENT_CONNECTED = 0U,
    WIFI_FRAMEWORK_EVENT_DISCONNECTED,
    WIFI_FRAMEWORK_EVENT_IP_ACQUIRED,
    WIFI_FRAMEWORK_EVENT_IP_LOST,
    WIFI_FRAMEWORK_EVENT_CONNECTION_FAILED,
    WIFI_FRAMEWORK_EVENT_RECONNECTING,
    WIFI_FRAMEWORK_EVENT_MAX
} wifi_framework_event_t;

/* WiFi Framework Configuration Structure */
typedef struct {
    char ssid[WIFI_FRAMEWORK_MAX_SSID_LEN + 1U];
    char password[WIFI_FRAMEWORK_MAX_PASSWORD_LEN + 1U];
    wifi_auth_mode_t auth_mode;
    uint8_t max_retry_count;
    uint32_t connection_timeout_ms;
    uint32_t retry_delay_ms;
    bool pmf_required;
    bool pmf_capable;
    int8_t max_tx_power;
    uint8_t channel;
    bool auto_reconnect;
} wifi_framework_config_t;

/* WiFi Framework Status Structure */
typedef struct {
    wifi_framework_state_t state;
    bool is_connected;
    bool has_ip;
    uint8_t retry_count;
    uint32_t connection_time;
    uint32_t last_connection_attempt;
    esp_netif_ip_info_t ip_info;
    char ssid[WIFI_FRAMEWORK_MAX_SSID_LEN + 1U];
    int8_t rssi;
    wifi_ap_record_t ap_info;
} wifi_framework_status_t;

/* WiFi Framework Event Callback */
typedef void (*wifi_framework_event_callback_t)(wifi_framework_event_t event, void* user_data);

/* ============================== FUNCTION PROTOTYPES ============================== */

/**
 * @brief Initialize the WiFi framework
 * 
 * @param config Pointer to WiFi configuration structure
 * @param event_callback Optional event callback function (can be NULL)
 * @param user_data User data passed to callback function
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_init(const wifi_framework_config_t* config,
                              wifi_framework_event_callback_t event_callback,
                              void* user_data);

/**
 * @brief Deinitialize the WiFi framework
 * 
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_deinit(void);

/**
 * @brief Connect to WiFi network
 * 
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_connect(void);

/**
 * @brief Disconnect from WiFi network
 * 
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_disconnect(void);

/**
 * @brief Get current WiFi framework status
 * 
 * @param status Pointer to status structure to fill
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_get_status(wifi_framework_status_t* status);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected, false otherwise
 */
bool wifi_framework_is_connected(void);

/**
 * @brief Check if WiFi has valid IP address
 * 
 * @return true if IP is available, false otherwise
 */
bool wifi_framework_has_ip(void);

/**
 * @brief Get current IP address information
 * 
 * @param ip_info Pointer to IP info structure to fill
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_get_ip_info(esp_netif_ip_info_t* ip_info);

/**
 * @brief Get current RSSI value
 * 
 * @param rssi Pointer to RSSI value to fill
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_get_rssi(int8_t* rssi);

/**
 * @brief Reconnect to WiFi network
 * 
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_reconnect(void);

/**
 * @brief Set WiFi configuration
 * 
 * @param config Pointer to new WiFi configuration
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_set_config(const wifi_framework_config_t* config);

/**
 * @brief Get WiFi configuration
 * 
 * @param config Pointer to configuration structure to fill
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_get_config(wifi_framework_config_t* config);

/**
 * @brief Start WiFi framework watchdog
 * 
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_start_watchdog(void);

/**
 * @brief Stop WiFi framework watchdog
 * 
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t wifi_framework_stop_watchdog(void);

/* ============================== DEFAULT CONFIGURATION ============================== */

/**
 * @brief Get default WiFi configuration
 * 
 * @param config Pointer to configuration structure to fill
 * @param ssid WiFi SSID
 * @param password WiFi password
 */
void wifi_framework_get_default_config(wifi_framework_config_t* config,
                                      const char* ssid,
                                      const char* password);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_FRAMEWORK_H */ 