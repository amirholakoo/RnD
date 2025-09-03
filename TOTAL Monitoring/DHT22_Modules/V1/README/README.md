# Robust WiFi Framework for ESP32 Infrastructure Applications

## 🚀 **Overview**

This WiFi framework provides a **production-ready, self-healing WiFi implementation** for ESP32 infrastructure applications. It's designed with MISRA-C compliance in mind and follows best practices for critical infrastructure systems where reliability is paramount.

## 🏗️ **Architecture Overview**

The WiFi framework follows a **layered architecture** with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  (Your sensor tasks, data transmission, etc.)              │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                    WiFi Framework Layer                     │
│  • State Management                                        │
│  • Event Handling                                          │
│  • Auto-Recovery                                           │
│  • Configuration Management                                 │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                    ESP-IDF WiFi Layer                       │
│  • WiFi Driver                                             │
│  • Event System                                            │
│  • Network Interface                                       │
└─────────────────────────────────────────────────────────────┘
```

## 🔧 **Core Components**

### **1. State Machine**
The framework implements a **finite state machine** for WiFi connection lifecycle:

```c
typedef enum {
    WIFI_FRAMEWORK_STATE_UNINITIALIZED = 0U,  // Framework not initialized
    WIFI_FRAMEWORK_STATE_INITIALIZING,        // Initialization in progress
    WIFI_FRAMEWORK_STATE_INITIALIZED,         // Framework ready
    WIFI_FRAMEWORK_STATE_CONNECTING,          // Connecting to WiFi
    WIFI_FRAMEWORK_STATE_CONNECTED,           // WiFi connected
    WIFI_FRAMEWORK_STATE_DISCONNECTED,        // WiFi disconnected
    WIFI_FRAMEWORK_STATE_ERROR,               // Error state
    WIFI_FRAMEWORK_STATE_MAX
} wifi_framework_state_t;
```

**State Transitions**:
```
UNINITIALIZED → INITIALIZING → INITIALIZED → CONNECTING → CONNECTED
                                                      ↓
                                              DISCONNECTED → CONNECTING (retry)
                                                      ↓
                                              ERROR
```

### **2. Event System**
The framework provides **real-time event notifications** for connection state changes:

```c
typedef enum {
    WIFI_FRAMEWORK_EVENT_CONNECTED = 0U,      // WiFi connected
    WIFI_FRAMEWORK_EVENT_DISCONNECTED,        // WiFi disconnected
    WIFI_FRAMEWORK_EVENT_IP_ACQUIRED,         // IP address obtained
    WIFI_FRAMEWORK_EVENT_IP_LOST,            // IP address lost
    WIFI_FRAMEWORK_EVENT_CONNECTION_FAILED,   // Connection failed
    WIFI_FRAMEWORK_EVENT_RECONNECTING,        // Attempting reconnection
    WIFI_FRAMEWORK_EVENT_MAX
} wifi_framework_event_t;
```

### **3. Auto-Recovery System**
- **Configurable retry logic** with intelligent backoff
- **Watchdog monitoring** for connection health
- **Automatic reconnection** after disconnection

## 📋 **Usage Workflow**

### **Step 1: Include and Setup**

```c
#include "wifi_framework.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char* TAG = "WiFi_App";
static bool g_wifi_ready = false;
```

### **Step 2: Define Event Callback**

```c
static void wifi_event_callback(wifi_framework_event_t event, void* user_data)
{
    (void)user_data; // Unused parameter
    
    switch (event) {
        case WIFI_FRAMEWORK_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✅ WiFi connected successfully");
            break;
            
        case WIFI_FRAMEWORK_EVENT_IP_ACQUIRED:
            ESP_LOGI(TAG, "🌐 IP address acquired - network ready");
            g_wifi_ready = true;
            break;
            
        case WIFI_FRAMEWORK_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "❌ WiFi disconnected");
            g_wifi_ready = false;
            break;
            
        case WIFI_FRAMEWORK_EVENT_RECONNECTING:
            ESP_LOGI(TAG, "🔄 Attempting automatic reconnection");
            break;
            
        case WIFI_FRAMEWORK_EVENT_CONNECTION_FAILED:
            ESP_LOGE(TAG, "💥 Maximum reconnection attempts reached");
            g_wifi_ready = false;
            break;
            
        default:
            ESP_LOGW(TAG, "❓ Unknown WiFi event: %d", event);
            break;
    }
}
```

### **Step 3: Initialize Framework**

```c
esp_err_t init_wifi_framework(void)
{
    // 1. Get default configuration
    wifi_framework_config_t config;
    wifi_framework_get_default_config(&config, "YourWiFiSSID", "YourWiFiPassword");
    
    // 2. Customize for infrastructure use
    config.max_retry_count = 10U;           // More retries for infrastructure
    config.connection_timeout_ms = 60000U;  // 60 seconds timeout
    config.retry_delay_ms = 10000U;         // 10 seconds between retries
    config.auto_reconnect = true;           // Enable auto-reconnect
    config.max_tx_power = 60;               // Maximum transmit power
    
    // 3. Log configuration
    ESP_LOGI(TAG, "📡 WiFi Configuration:");
    ESP_LOGI(TAG, "  SSID: %s", config.ssid);
    ESP_LOGI(TAG, "  Max Retries: %d", config.max_retry_count);
    ESP_LOGI(TAG, "  Connection Timeout: %lu ms", config.connection_timeout_ms);
    ESP_LOGI(TAG, "  Retry Delay: %lu ms", config.retry_delay_ms);
    ESP_LOGI(TAG, "  Auto Reconnect: %s", config.auto_reconnect ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "  Max TX Power: %d", config.max_tx_power);
    
    // 4. Initialize WiFi framework
    esp_err_t ret = wifi_framework_init(&config, wifi_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize WiFi framework: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ WiFi framework initialized successfully");
    return ESP_OK;
}
```

### **Step 4: Connect to WiFi**

```c
esp_err_t connect_wifi(void)
{
    // Connect to WiFi network
    esp_err_t ret = wifi_framework_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to connect to WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "📡 WiFi connection initiated");
    return ESP_OK;
}
```

### **Step 5: Monitor Connection Status**

```c
void wifi_monitor_task(void* pvParameters)
{
    (void)pvParameters;
    
    wifi_framework_status_t status;
    
    while (1) {
        // Get current WiFi status
        esp_err_t ret = wifi_framework_get_status(&status);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "📊 WiFi Status:");
            ESP_LOGI(TAG, "  State: %d", status.state);
            ESP_LOGI(TAG, "  Connected: %s", status.is_connected ? "Yes" : "No");
            ESP_LOGI(TAG, "  Has IP: %s", status.has_ip ? "Yes" : "No");
            ESP_LOGI(TAG, "  RSSI: %d dBm", status.rssi);
            ESP_LOGI(TAG, "  Retry Count: %d", status.retry_count);
            
            // Check connection health
            if (status.is_connected && status.has_ip) {
                esp_netif_ip_info_t ip_info;
                if (wifi_framework_get_ip_info(&ip_info) == ESP_OK) {
                    ESP_LOGI(TAG, "🌐 Network Information:");
                    ESP_LOGI(TAG, "  IP Address: " IPSTR, IP2STR(&ip_info.ip));
                    ESP_LOGI(TAG, "  Gateway: " IPSTR, IP2STR(&ip_info.gw));
                    ESP_LOGI(TAG, "  Netmask: " IPSTR, IP2STR(&ip_info.netmask));
                }
            }
        } else {
            ESP_LOGE(TAG, "❌ Failed to get WiFi status: %s", esp_err_to_name(ret));
        }
        
        // Wait before next check
        vTaskDelay(pdMS_TO_TICKS(10000U)); // 10 seconds
    }
}
```

### **Step 6: Main Application Loop**

```c
void app_main(void)
{
    ESP_LOGI(TAG, "🚀 Starting WiFi Infrastructure Application");
    
    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 2. Initialize WiFi framework
    ret = init_wifi_framework();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WiFi framework initialization failed");
        return;
    }
    
    // 3. Connect to WiFi
    ret = connect_wifi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WiFi connection failed");
        return;
    }
    
    // 4. Create WiFi monitoring task
    TaskHandle_t monitor_task_handle = NULL;
    ret = xTaskCreate(wifi_monitor_task,
                      "wifi_monitor",
                      4096U,
                      NULL,
                      5U,
                      &monitor_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "❌ Failed to create WiFi monitor task");
        return;
    }
    
    ESP_LOGI(TAG, "✅ WiFi monitor task created");
    
    // 5. Main application loop
    while (1) {
        // Check if WiFi is ready for application use
        if (wifi_framework_is_connected() && wifi_framework_has_ip()) {
            // WiFi is ready - start your application tasks
            ESP_LOGI(TAG, "🎯 WiFi is ready for application use");
            
            // Here you would typically:
            // - Start sensor reading tasks
            // - Initialize data transmission
            // - Start monitoring systems
            // - etc.
            
        } else {
            ESP_LOGW(TAG, "⏳ Waiting for WiFi connection...");
        }
        
        // Wait before next check
        vTaskDelay(pdMS_TO_TICKS(5000U)); // 5 seconds
    }
}
```

## ⚙️ **Configuration Options**

### **Default Configuration**
```c
void wifi_framework_get_default_config(wifi_framework_config_t* config,
                                      const char* ssid,
                                      const char* password)
{
    // Infrastructure-optimized defaults
    config->auth_mode = WIFI_AUTH_WPA2_PSK;
    config->max_retry_count = 5U;
    config->connection_timeout_ms = 30000U;  // 30 seconds
    config->retry_delay_ms = 5000U;         // 5 seconds
    config->pmf_capable = true;
    config->pmf_required = false;
    config->max_tx_power = 60;              // Maximum power
    config->channel = 0U;                   // Auto channel selection
    config->auto_reconnect = true;          // Enable auto-reconnect
}
```

### **Infrastructure Configuration**
```c
// For critical infrastructure applications
config.max_retry_count = 15U;              // More retries
config.connection_timeout_ms = 120000U;     // 2 minutes timeout
config.retry_delay_ms = 15000U;            // 15 seconds between retries
config.max_tx_power = 60;                  // Maximum transmit power
config.auto_reconnect = true;              // Always enable auto-reconnect
```

## 🔍 **Status Monitoring**

### **Connection Status Check**
```c
// Quick status checks
if (wifi_framework_is_connected()) {
    ESP_LOGI(TAG, "✅ WiFi is connected");
}

if (wifi_framework_has_ip()) {
    ESP_LOGI(TAG, "🌐 IP address is available");
}

// Comprehensive status
wifi_framework_status_t status;
if (wifi_framework_get_status(&status) == ESP_OK) {
    ESP_LOGI(TAG, "📊 Connection Quality: RSSI %d dBm", status.rssi);
    ESP_LOGI(TAG, "⏱️  Connection Time: %lu seconds", status.connection_time);
    ESP_LOGI(TAG, "🔄 Retry Count: %d", status.retry_count);
}
```

### **IP Information**
```c
esp_netif_ip_info_t ip_info;
if (wifi_framework_get_ip_info(&ip_info) == ESP_OK) {
    ESP_LOGI(TAG, "🌐 IP Address: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "🚪 Gateway: " IPSTR, IP2STR(&ip_info.gw));
    ESP_LOGI(TAG, "🎭 Netmask: " IPSTR, IP2STR(&ip_info.netmask));
}
```

## 🛠️ **Advanced Features**

### **Manual Reconnection**
```c
// Force reconnection
esp_err_t ret = wifi_framework_reconnect();
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "🔄 Manual reconnection initiated");
} else {
    ESP_LOGE(TAG, "❌ Manual reconnection failed: %s", esp_err_to_name(ret));
}
```

### **Runtime Configuration Changes**
```c
// Update configuration at runtime
wifi_framework_config_t new_config;
wifi_framework_get_config(&new_config);
new_config.max_retry_count = 20U;  // Increase retry count
new_config.retry_delay_ms = 20000U; // Increase delay

esp_err_t ret = wifi_framework_set_config(&new_config);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "✅ WiFi configuration updated");
}
```

### **Watchdog Control**
```c
// Start/stop watchdog manually
esp_err_t ret = wifi_framework_start_watchdog();
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "🦮 WiFi watchdog started");
}

// Stop watchdog
ret = wifi_framework_stop_watchdog();
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "🦮 WiFi watchdog stopped");
}
```

## 🚨 **Error Handling**

### **Error Recovery Patterns**
```c
esp_err_t ret = wifi_framework_connect();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Connection failed: %s", esp_err_to_name(ret));
    
    // Wait and retry
    vTaskDelay(pdMS_TO_TICKS(5000U));
    ret = wifi_framework_connect();
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "💥 Retry failed - entering error state");
        // Handle critical error
    }
}
```

### **Common Error Codes**
- `ESP_OK`: Operation successful
- `ESP_ERR_INVALID_ARG`: Invalid parameter
- `ESP_ERR_INVALID_STATE`: Invalid state for operation
- `ESP_ERR_NO_MEM`: Memory allocation failed
- `ESP_ERR_TIMEOUT`: Operation timed out
- `ESP_ERR_NOT_FOUND`: Resource not found

## 📁 **File Structure**

```
main/
├── wifi_framework.h          # Header file with all declarations
├── wifi_framework.c          # Complete framework implementation
├── wifi_framework_example.c  # Working example application
├── WIFI_FRAMEWORK_README.md  # Detailed framework documentation
└── CMakeLists.txt           # Build configuration
```

## 🔧 **Building and Integration**

### **CMakeLists.txt**
```cmake
idf_component_register(SRCS   wifi_framework.c
                               wifi_framework_example.c
                        INCLUDE_DIRS ".")
```

### **Include in Your Project**
```c
#include "wifi_framework.h"
```

## 🎯 **Best Practices for Infrastructure**

### **1. Error Handling**
- **Always check return values** from framework functions
- **Implement proper error recovery** for critical failures
- **Log all errors** with appropriate log levels

### **2. Configuration**
- **Use infrastructure-optimized settings** for production
- **Test retry logic** in your specific environment
- **Monitor connection quality** and adjust power settings

### **3. Monitoring**
- **Implement status monitoring** in your application
- **Log connection events** for debugging
- **Monitor RSSI** for signal quality assessment

### **4. Resource Management**
- **Properly initialize** the framework before use
- **Clean up resources** when shutting down
- **Handle disconnections** gracefully

## 🚀 **Getting Started**

1. **Copy the framework files** to your project
2. **Update CMakeLists.txt** to include the source files
3. **Implement the event callback** for your application
4. **Initialize and connect** using the workflow above
5. **Monitor status** and start your application tasks

## 📚 **Additional Resources**

- **Framework Documentation**: See `WIFI_FRAMEWORK_README.md`
- **Example Implementation**: See `wifi_framework_example.c`
- **ESP-IDF Documentation**: [WiFi API Reference](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/wifi.html)

## 🤝 **Support**

For issues or questions:
1. Check the logs for error messages
2. Verify configuration parameters
3. Test with the provided example code
4. Review ESP-IDF WiFi documentation

---

**This framework transforms basic WiFi connectivity into a production-grade, self-healing network connection perfect for critical infrastructure applications where reliability is paramount.** 🚀
