/**
 * @file wifi_framework.c
 * @brief Robust WiFi Framework Implementation for ESP32 Infrastructure Applications
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

#include "wifi_framework.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <string.h>
#include <stdbool.h>

/* ============================== PRIVATE CONSTANTS ============================== */

#define WIFI_FRAMEWORK_TAG "WiFi_Framework"
#define WIFI_FRAMEWORK_TASK_STACK_SIZE (4096U)
#define WIFI_FRAMEWORK_TASK_PRIORITY   (5U)
#define WIFI_FRAMEWORK_MAX_EVENT_HANDLERS (4U)

/* ============================== PRIVATE TYPES ============================== */

typedef struct {
    wifi_framework_state_t state;
    wifi_framework_config_t config;
    wifi_framework_status_t status;
    wifi_framework_event_callback_t event_callback;
    void* user_data;
    
    /* Event handling */
    esp_event_handler_instance_t wifi_event_instance;
    esp_event_handler_instance_t ip_event_instance;
    EventGroupHandle_t event_group;
    
    /* Task and timer handles */
    TaskHandle_t watchdog_task_handle;
    esp_timer_handle_t watchdog_timer;
    esp_timer_handle_t retry_timer;
    
    /* Internal state */
    bool is_initialized;
    bool watchdog_running;
    uint32_t connection_start_time;
    uint32_t last_activity_time;
    
    /* Mutex for thread safety */
    SemaphoreHandle_t mutex;
} wifi_framework_context_t;

/* ============================== PRIVATE VARIABLES ============================== */

static wifi_framework_context_t s_wifi_context = {0};
static const char* TAG = WIFI_FRAMEWORK_TAG;

/* ============================== PRIVATE FUNCTION PROTOTYPES ============================== */

static void wifi_framework_event_handler(void* arg, esp_event_base_t event_base,
                                       int32_t event_id, void* event_data);
static void wifi_framework_ip_event_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data);
static void wifi_framework_watchdog_task(void* pvParameters);
static void wifi_framework_watchdog_timer_callback(void* arg);
static void wifi_framework_retry_timer_callback(void* arg);
static esp_err_t wifi_framework_set_state(wifi_framework_state_t new_state);
static esp_err_t wifi_framework_validate_config(const wifi_framework_config_t* config);
static void wifi_framework_update_connection_info(void);
static esp_err_t wifi_framework_cleanup_resources(void); 

/* ============================== PRIVATE FUNCTIONS ============================== */

/**
 * @brief WiFi event handler for WiFi events
 */
static void wifi_framework_event_handler(void* arg, esp_event_base_t event_base,
                                       int32_t event_id, void* event_data)
{
    
    if (arg == NULL) {
        ESP_LOGE(TAG, "Invalid argument in WiFi event handler");
        return;
    }
    
    wifi_framework_context_t* ctx = (wifi_framework_context_t*)arg;
    
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi station started");
            if (wifi_framework_set_state(WIFI_FRAMEWORK_STATE_CONNECTING) == ESP_OK) {
                ctx->connection_start_time = esp_timer_get_time() / 1000ULL;
                esp_err_t connect_ret = esp_wifi_connect();
                if (connect_ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to connect to AP: %s", esp_err_to_name(connect_ret));
                    wifi_framework_set_state(WIFI_FRAMEWORK_STATE_ERROR);
                }
            }
            break;
            
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Connected to AP: %s", ctx->config.ssid);
            wifi_framework_set_state(WIFI_FRAMEWORK_STATE_CONNECTED);
            ctx->status.is_connected = true;
            ctx->status.retry_count = 0U;
            ctx->last_activity_time = esp_timer_get_time() / 1000ULL;
            
            if (ctx->event_callback != NULL) {
                ctx->event_callback(WIFI_FRAMEWORK_EVENT_CONNECTED, ctx->user_data);
            }
            break;
            
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from AP: %s", ctx->config.ssid);
            wifi_framework_set_state(WIFI_FRAMEWORK_STATE_DISCONNECTED);
            ctx->status.is_connected = false;
            ctx->status.has_ip = false;
            
            if (ctx->event_callback != NULL) {
                ctx->event_callback(WIFI_FRAMEWORK_EVENT_DISCONNECTED, ctx->user_data);
            }
            
            /* Auto-reconnect logic */
            if (ctx->config.auto_reconnect && (ctx->status.retry_count < ctx->config.max_retry_count)) {
                ctx->status.retry_count++;
                ESP_LOGI(TAG, "Attempting to reconnect (attempt %d/%d)", 
                         ctx->status.retry_count, ctx->config.max_retry_count);
                
                if (ctx->event_callback != NULL) {
                    ctx->event_callback(WIFI_FRAMEWORK_EVENT_RECONNECTING, ctx->user_data);
                }
                
                /* Start retry timer */
                esp_timer_start_once(ctx->retry_timer, ctx->config.retry_delay_ms * 1000ULL);
            } else if (ctx->status.retry_count >= ctx->config.max_retry_count) {
                ESP_LOGE(TAG, "Maximum retry attempts reached");
                wifi_framework_set_state(WIFI_FRAMEWORK_STATE_ERROR);
                
                if (ctx->event_callback != NULL) {
                    ctx->event_callback(WIFI_FRAMEWORK_EVENT_CONNECTION_FAILED, ctx->user_data);
                }
            }
            break;
            
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            ESP_LOGI(TAG, "WiFi authentication mode changed");
            break;
            
        default:
            ESP_LOGD(TAG, "Unhandled WiFi event: %ld", event_id);
            break;
    }
}

/**
 * @brief IP event handler for IP-related events
 */
static void wifi_framework_ip_event_handler(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data)
{
    if (arg == NULL) {
        ESP_LOGE(TAG, "Invalid argument in IP event handler");
        return;
    }
    
    wifi_framework_context_t* ctx = (wifi_framework_context_t*)arg;
    
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            if (event_data != NULL) {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
                
                /* Update IP information */
                memcpy(&ctx->status.ip_info, &event->ip_info, sizeof(esp_netif_ip_info_t));
                ctx->status.has_ip = true;
                ctx->last_activity_time = esp_timer_get_time() / 1000ULL;
                
                if (ctx->event_callback != NULL) {
                    ctx->event_callback(WIFI_FRAMEWORK_EVENT_IP_ACQUIRED, ctx->user_data);
                }
            }
            break;
            
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGW(TAG, "Lost IP address");
            ctx->status.has_ip = false;
            
            if (ctx->event_callback != NULL) {
                ctx->event_callback(WIFI_FRAMEWORK_EVENT_IP_LOST, ctx->user_data);
            }
            break;
            
        default:
            ESP_LOGD(TAG, "Unhandled IP event: %ld", event_id);
            break;
    }
} 

/**
 * @brief WiFi framework watchdog task
 */
static void wifi_framework_watchdog_task(void* pvParameters)
{
    wifi_framework_context_t* ctx = (wifi_framework_context_t*)pvParameters;
    
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Invalid context in watchdog task");
        vTaskDelete(NULL);
        return;
    }
    
    while (ctx->watchdog_running) {
        /* Check connection health */
        if (ctx->status.is_connected) {
            uint32_t current_time = esp_timer_get_time() / 1000ULL;
            
            /* Check if we haven't had activity for too long */
            if ((current_time - ctx->last_activity_time) > WIFI_FRAMEWORK_WATCHDOG_TIMEOUT_MS) {
                ESP_LOGW(TAG, "WiFi watchdog timeout - no activity for %lu ms", 
                         current_time - ctx->last_activity_time);
                
                /* Force reconnection */
                if (ctx->config.auto_reconnect) {
                    ESP_LOGI(TAG, "Watchdog forcing reconnection");
                    ESP_LOGI(TAG, "Ignored reconnetion please refer to code");
                    //esp_wifi_disconnect();
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000U)); /* Check every 10 seconds */
    }
    
    ESP_LOGI(TAG, "WiFi watchdog task exiting");
    vTaskDelete(NULL);
}

/**
 * @brief WiFi framework watchdog timer callback
 */
static void wifi_framework_watchdog_timer_callback(void* arg)
{
    wifi_framework_context_t* ctx = (wifi_framework_context_t*)arg;
    
    if (ctx != NULL && ctx->watchdog_running) {
        /* Trigger watchdog check */
        if (ctx->watchdog_task_handle != NULL) {
            /* Task will handle the timeout check */
        }
    }
}

/**
 * @brief WiFi framework retry timer callback
 */
static void wifi_framework_retry_timer_callback(void* arg)
{
    wifi_framework_context_t* ctx = (wifi_framework_context_t*)arg;
    
    if (ctx != NULL && ctx->config.auto_reconnect) {
        ESP_LOGI(TAG, "Retry timer expired, attempting reconnection");
        esp_wifi_connect();
    }
}

/**
 * @brief Set WiFi framework state
 */
static esp_err_t wifi_framework_set_state(wifi_framework_state_t new_state)
{
    if (new_state >= WIFI_FRAMEWORK_STATE_MAX) {
        ESP_LOGE(TAG, "Invalid state: %d", new_state);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) == pdTRUE) {
        wifi_framework_state_t old_state = s_wifi_context.state;
        s_wifi_context.state = new_state;
        xSemaphoreGive(s_wifi_context.mutex);
        
        ESP_LOGI(TAG, "WiFi framework state changed: %d -> %d", old_state, new_state);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to acquire mutex for state change");
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Validate WiFi configuration
 */
static esp_err_t wifi_framework_validate_config(const wifi_framework_config_t* config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Configuration is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(config->ssid) == 0U || strlen(config->ssid) > WIFI_FRAMEWORK_MAX_SSID_LEN) {
        ESP_LOGE(TAG, "Invalid SSID length: %zu", strlen(config->ssid));
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(config->password) > WIFI_FRAMEWORK_MAX_PASSWORD_LEN) {
        ESP_LOGE(TAG, "Password too long: %zu", strlen(config->password));
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config->max_retry_count > WIFI_FRAMEWORK_MAX_RETRY_COUNT) {
        ESP_LOGE(TAG, "Max retry count too high: %d", config->max_retry_count);
        return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

/**
 * @brief Update connection information
 */
static void wifi_framework_update_connection_info(void)
{
    wifi_framework_context_t* ctx = &s_wifi_context;
    
    if (ctx->status.is_connected) {
        /* Update RSSI */
        wifi_ap_record_t ap_info = {0};
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ctx->status.rssi = ap_info.rssi;
            memcpy(&ctx->status.ap_info, &ap_info, sizeof(wifi_ap_record_t));
        }
        
        /* Update connection time */
        if (ctx->connection_start_time > 0U) {
            uint32_t current_time = esp_timer_get_time() / 1000ULL;
            ctx->status.connection_time = current_time - ctx->connection_start_time;
        }
    }
}

/**
 * @brief Cleanup WiFi framework resources
 */
static esp_err_t wifi_framework_cleanup_resources(void)
{
    wifi_framework_context_t* ctx = &s_wifi_context;
    esp_err_t ret = ESP_OK;
    
    /* Stop watchdog */
    if (ctx->watchdog_running) {
        wifi_framework_stop_watchdog();
    }
    
    /* Delete timers */
    if (ctx->watchdog_timer != NULL) {
        esp_timer_delete(ctx->watchdog_timer);
        ctx->watchdog_timer = NULL;
    }
    
    if (ctx->retry_timer != NULL) {
        esp_timer_delete(ctx->retry_timer);
        ctx->retry_timer = NULL;
    }
    
    /* Delete event group */
    if (ctx->event_group != NULL) {
        vEventGroupDelete(ctx->event_group);
        ctx->event_group = NULL;
    }
    
    /* Delete mutex */
    if (ctx->mutex != NULL) {
        vSemaphoreDelete(ctx->mutex);
        ctx->mutex = NULL;
    }
    
    /* Unregister event handlers */
    if (ctx->is_initialized) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ctx->wifi_event_instance);
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ctx->ip_event_instance);
    }
    
    /* Reset context */
    memset(ctx, 0, sizeof(wifi_framework_context_t));
    
    return ret;
} 

/* ============================== PUBLIC FUNCTIONS ============================== */

esp_err_t wifi_framework_init(const wifi_framework_config_t* config,
                              wifi_framework_event_callback_t event_callback,
                              void* user_data)
{
    esp_err_t ret = ESP_OK;
    
    /* Validate input parameters */
    ret = wifi_framework_validate_config(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Configuration validation failed");
        return ret;
    }
    
    /* Check if already initialized */
    if (s_wifi_context.is_initialized) {
        ESP_LOGW(TAG, "WiFi framework already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Create mutex for thread safety */
    s_wifi_context.mutex = xSemaphoreCreateMutex();
    if (s_wifi_context.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    /* Copy configuration */
    memcpy(&s_wifi_context.config, config, sizeof(wifi_framework_config_t));
    s_wifi_context.event_callback = event_callback;
    s_wifi_context.user_data = user_data;
    
    /* Create event group */
    s_wifi_context.event_group = xEventGroupCreate();
    if (s_wifi_context.event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        wifi_framework_cleanup_resources();
        return ESP_ERR_NO_MEM;
    }
    
    /* Initialize netif */
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        wifi_framework_cleanup_resources();
        return ret;
    }
    
    /* Create default event loop */
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        wifi_framework_cleanup_resources();
        return ret;
    }
    
    /* Create default WiFi station netif */
    esp_netif_create_default_wifi_sta();
    
    /* Initialize WiFi */
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        wifi_framework_cleanup_resources();
        return ret;
    }
    
    /* Register event handlers */
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                            &wifi_framework_event_handler,
                                            &s_wifi_context,
                                            &s_wifi_context.wifi_event_instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        wifi_framework_cleanup_resources();
        return ret;
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                            &wifi_framework_ip_event_handler,
                                            &s_wifi_context,
                                            &s_wifi_context.ip_event_instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        wifi_framework_cleanup_resources();
        return ret;
    }
    
    /* Create timers */
    esp_timer_create_args_t watchdog_timer_args = {
        .callback = wifi_framework_watchdog_timer_callback,
        .arg = &s_wifi_context,
        .name = "wifi_watchdog"
    };
    
    ret = esp_timer_create(&watchdog_timer_args, &s_wifi_context.watchdog_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create watchdog timer: %s", esp_err_to_name(ret));
        wifi_framework_cleanup_resources();
        return ret;
    }
    
    esp_timer_create_args_t retry_timer_args = {
        .callback = wifi_framework_retry_timer_callback,
        .arg = &s_wifi_context,
        .name = "wifi_retry"
    };
    
    // ret = esp_timer_create(&retry_timer_args, &s_wifi_context.retry_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create retry timer: %s", esp_err_to_name(ret));
        wifi_framework_cleanup_resources();
        return ret;
    }
    
    /* Set initial state */
    ret = wifi_framework_set_state(WIFI_FRAMEWORK_STATE_INITIALIZED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set initial state");
        wifi_framework_cleanup_resources();
        return ret;
    }
    
    s_wifi_context.is_initialized = true;
    ESP_LOGI(TAG, "WiFi framework initialized successfully");
    
    return ESP_OK;
}

esp_err_t wifi_framework_deinit(void)
{
    if (!s_wifi_context.is_initialized) {
        ESP_LOGW(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing WiFi framework");
    
    /* Disconnect if connected */
    if (s_wifi_context.status.is_connected) {
        esp_wifi_disconnect();
    }
    
    /* Stop WiFi */
    esp_wifi_stop();
    esp_wifi_deinit();
    
    /* Cleanup resources */
    esp_err_t ret = wifi_framework_cleanup_resources();
    
    ESP_LOGI(TAG, "WiFi framework deinitialized");
    return ret;
} 

esp_err_t wifi_framework_connect(void)
{
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = ESP_OK;
    
    /* Set WiFi configuration */
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, s_wifi_context.config.ssid, sizeof(wifi_config.sta.ssid) - 1U);
    strncpy((char*)wifi_config.sta.password, s_wifi_context.config.password, sizeof(wifi_config.sta.password) - 1U);
    wifi_config.sta.threshold.authmode = s_wifi_context.config.auth_mode;
    wifi_config.sta.pmf_cfg.capable = s_wifi_context.config.pmf_capable;
    wifi_config.sta.pmf_cfg.required = s_wifi_context.config.pmf_required;
    
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_wifi_context.mutex);
        return ret;
    }
    
    /* Set WiFi mode */
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_wifi_context.mutex);
        return ret;
    }
    
    /* Start WiFi */
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_wifi_context.mutex);
        return ret;
    }

    /* Set maximum transmit power if specified */
    if (s_wifi_context.config.max_tx_power > 0) {
        vTaskDelay(pdMS_TO_TICKS(200U));
        ret = esp_wifi_set_max_tx_power(s_wifi_context.config.max_tx_power);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set max TX power: %s", esp_err_to_name(ret));
            /* Continue anyway, this is not critical */
        }
    }
    
    /* Start watchdog if auto-reconnect is enabled */
    if (s_wifi_context.config.auto_reconnect) {
        //wifi_framework_start_watchdog();
    }
    
    xSemaphoreGive(s_wifi_context.mutex);
    
    ESP_LOGI(TAG, "WiFi connection initiated to SSID: %s", s_wifi_context.config.ssid);
    return ESP_OK;
}

esp_err_t wifi_framework_disconnect(void)
{
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Disconnecting from WiFi");
    
    /* Stop watchdog */
    if (s_wifi_context.config.auto_reconnect) {
        wifi_framework_stop_watchdog();
    }
    
    /* Disconnect WiFi */
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Update status */
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) == pdTRUE) {
        s_wifi_context.status.is_connected = false;
        s_wifi_context.status.has_ip = false;
        xSemaphoreGive(s_wifi_context.mutex);
    }
    
    return ESP_OK;
}

esp_err_t wifi_framework_get_status(wifi_framework_status_t* status)
{
    if (status == NULL) {
        ESP_LOGE(TAG, "Status pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    /* Update connection information */
    wifi_framework_update_connection_info();
    
    /* Copy status */
    memcpy(status, &s_wifi_context.status, sizeof(wifi_framework_status_t));
    
    xSemaphoreGive(s_wifi_context.mutex);
    
    return ESP_OK;
}

bool wifi_framework_is_connected(void)
{
    if (!s_wifi_context.is_initialized) {
        return false;
    }
    
    bool is_connected = false;
    
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) == pdTRUE) {
        is_connected = s_wifi_context.status.is_connected;
        xSemaphoreGive(s_wifi_context.mutex);
    }
    
    return is_connected;
}

bool wifi_framework_has_ip(void)
{
    if (!s_wifi_context.is_initialized) {
        return false;
    }
    
    bool has_ip = false;
    
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) == pdTRUE) {
        has_ip = s_wifi_context.status.has_ip;
        xSemaphoreGive(s_wifi_context.mutex);
    }
    
    return has_ip;
} 

esp_err_t wifi_framework_get_ip_info(esp_netif_ip_info_t* ip_info)
{
    if (ip_info == NULL) {
        ESP_LOGE(TAG, "IP info pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_wifi_context.status.has_ip) {
        memcpy(ip_info, &s_wifi_context.status.ip_info, sizeof(esp_netif_ip_info_t));
        xSemaphoreGive(s_wifi_context.mutex);
        return ESP_OK;
    }
    
    xSemaphoreGive(s_wifi_context.mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t wifi_framework_get_rssi(int8_t* rssi)
{
    if (rssi == NULL) {
        ESP_LOGE(TAG, "RSSI pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_wifi_context.status.is_connected) {
        *rssi = s_wifi_context.status.rssi;
        xSemaphoreGive(s_wifi_context.mutex);
        return ESP_OK;
    }
    
    xSemaphoreGive(s_wifi_context.mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t wifi_framework_reconnect(void)
{
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Reconnecting to WiFi");
    
    /* Reset retry count */
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) == pdTRUE) {
        s_wifi_context.status.retry_count = 0U;
        xSemaphoreGive(s_wifi_context.mutex);
    }
    
    /* Disconnect and reconnect */
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect for reconnection: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Small delay before reconnecting */
    vTaskDelay(pdMS_TO_TICKS(1000U));
    
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconnect: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t wifi_framework_set_config(const wifi_framework_config_t* config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Configuration pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = wifi_framework_validate_config(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Configuration validation failed");
        return ret;
    }
    
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    /* Update configuration */
    memcpy(&s_wifi_context.config, config, sizeof(wifi_framework_config_t));
    
    xSemaphoreGive(s_wifi_context.mutex);
    
    ESP_LOGI(TAG, "WiFi configuration updated");
    return ESP_OK;
}

esp_err_t wifi_framework_get_config(wifi_framework_config_t* config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Configuration pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_wifi_context.mutex, pdMS_TO_TICKS(1000U)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    /* Copy configuration */
    memcpy(config, &s_wifi_context.config, sizeof(wifi_framework_config_t));
    
    xSemaphoreGive(s_wifi_context.mutex);
    
    return ESP_OK;
}

esp_err_t wifi_framework_start_watchdog(void)
{
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_wifi_context.watchdog_running) {
        ESP_LOGW(TAG, "Watchdog already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Create watchdog task */
    BaseType_t ret = xTaskCreate(wifi_framework_watchdog_task,
                                 "wifi_watchdog",
                                 WIFI_FRAMEWORK_TASK_STACK_SIZE,
                                 &s_wifi_context,
                                 WIFI_FRAMEWORK_TASK_PRIORITY,
                                 &s_wifi_context.watchdog_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create watchdog task");
        return ESP_ERR_NO_MEM;
    }
    
    /* Start watchdog timer */
    esp_err_t timer_ret = esp_timer_start_periodic(s_wifi_context.watchdog_timer,
                                                   WIFI_FRAMEWORK_WATCHDOG_TIMEOUT_MS * 1000ULL);
    if (timer_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start watchdog timer: %s", esp_err_to_name(timer_ret));
        vTaskDelete(s_wifi_context.watchdog_task_handle);
        s_wifi_context.watchdog_task_handle = NULL;
        return timer_ret;
    }
    
    s_wifi_context.watchdog_running = true;
    ESP_LOGI(TAG, "WiFi watchdog started");
    
    return ESP_OK;
}

esp_err_t wifi_framework_stop_watchdog(void)
{
    if (!s_wifi_context.is_initialized) {
        ESP_LOGE(TAG, "WiFi framework not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!s_wifi_context.watchdog_running) {
        ESP_LOGW(TAG, "Watchdog not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Stop watchdog timer */
    esp_timer_stop(s_wifi_context.watchdog_timer);
    
    /* Stop watchdog task */
    s_wifi_context.watchdog_running = false;
    
    /* Wait for task to exit */
    if (s_wifi_context.watchdog_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(1000U));
        s_wifi_context.watchdog_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "WiFi watchdog stopped");
    
    return ESP_OK;
}

void wifi_framework_get_default_config(wifi_framework_config_t* config,
                                      const char* ssid,
                                      const char* password)
{
    if (config == NULL || ssid == NULL || password == NULL) {
        return;
    }
    
    /* Clear configuration */
    memset(config, 0, sizeof(wifi_framework_config_t));
    
    /* Set SSID and password */
    strncpy(config->ssid, ssid, WIFI_FRAMEWORK_MAX_SSID_LEN);
    strncpy(config->password, password, WIFI_FRAMEWORK_MAX_PASSWORD_LEN);
    
    /* Set default values */
    config->auth_mode = WIFI_AUTH_WPA2_PSK;
    config->max_retry_count = 5U;
    config->connection_timeout_ms = WIFI_FRAMEWORK_CONNECTION_TIMEOUT_MS;
    config->retry_delay_ms = WIFI_FRAMEWORK_RETRY_DELAY_MS;
    config->pmf_capable = true;
    config->pmf_required = false;
    config->max_tx_power = 60;
    config->channel = 0U; /* Auto */
    config->auto_reconnect = false;
} 