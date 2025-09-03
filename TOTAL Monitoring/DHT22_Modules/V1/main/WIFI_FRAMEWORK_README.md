# Robust WiFi Framework for ESP32 Infrastructure Applications

## Overview

This WiFi framework provides a robust, production-ready WiFi implementation for ESP32 infrastructure applications. It is designed with MISRA-C compliance in mind and follows best practices for critical infrastructure systems.

## Features

### 🚀 **Robustness & Reliability**
- **Comprehensive Error Handling**: All functions return proper error codes with detailed logging
- **State Management**: Clear state machine for WiFi connection lifecycle
- **Thread Safety**: Mutex-protected operations for multi-threaded environments
- **Resource Management**: Proper cleanup and resource allocation

### 🔄 **Auto-Recovery**
- **Automatic Reconnection**: Configurable retry logic with exponential backoff
- **Connection Monitoring**: Watchdog system to detect and recover from connection issues
- **Health Checks**: Continuous monitoring of connection quality and RSSI

### ⚙️ **Configuration & Flexibility**
- **Configurable Parameters**: Retry counts, timeouts, power settings, and more
- **Runtime Configuration**: Ability to change settings without recompilation
- **Default Configurations**: Sensible defaults with easy customization

### 📊 **Monitoring & Diagnostics**
- **Event Callbacks**: Real-time notifications for connection state changes
- **Status Information**: Comprehensive connection status and statistics
- **Logging**: Detailed logging for debugging and monitoring

## Architecture

### Core Components

1. **WiFi Framework Core** (`wifi_framework.c/h`)
   - Main WiFi management and state machine
   - Event handling and callback system
   - Configuration management

2. **Event Handlers**
   - WiFi event processing
   - IP event management
   - Automatic reconnection logic

3. **Watchdog System**
   - Connection health monitoring
   - Automatic recovery mechanisms
   - Configurable timeout handling

4. **Status Management**
   - Real-time connection status
   - RSSI and signal quality monitoring
   - Connection statistics

## Usage

### Basic Initialization

```c
#include "wifi_framework.h"

// Get default configuration
wifi_framework_config_t config;
wifi_framework_get_default_config(&config, "YourSSID", "YourPassword");

// Customize for infrastructure use
config.max_retry_count = 10;
config.auto_reconnect = true;
config.max_tx_power = 60;

// Initialize framework
esp_err_t ret = wifi_framework_init(&config, event_callback, NULL);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi framework init failed");
    return;
}

// Connect to WiFi
ret = wifi_framework_connect();
```

### Event Callback

```c
static void wifi_event_callback(wifi_framework_event_t event, void* user_data)
{
    switch (event) {
        case WIFI_FRAMEWORK_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected");
            break;
            
        case WIFI_FRAMEWORK_EVENT_IP_ACQUIRED:
            ESP_LOGI(TAG, "IP address acquired");
            break;
            
        case WIFI_FRAMEWORK_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi disconnected");
            break;
            
        case WIFI_FRAMEWORK_EVENT_RECONNECTING:
            ESP_LOGI(TAG, "Attempting reconnection");
            break;
    }
}
```

### Status Monitoring

```c
// Check connection status
if (wifi_framework_is_connected() && wifi_framework_has_ip()) {
    // WiFi is ready for use
    
    // Get detailed status
    wifi_framework_status_t status;
    if (wifi_framework_get_status(&status) == ESP_OK) {
        ESP_LOGI(TAG, "RSSI: %d, Retry Count: %d", 
                 status.rssi, status.retry_count);
    }
    
    // Get IP information
    esp_netif_ip_info_t ip_info;
    if (wifi_framework_get_ip_info(&ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ip_info.ip));
    }
}
```

## Configuration Options

### WiFi Configuration Structure

```c
typedef struct {
    char ssid[33];                    // WiFi SSID (max 32 chars)
    char password[65];                // WiFi password (max 64 chars)
    wifi_auth_mode_t auth_mode;       // Authentication mode
    uint8_t max_retry_count;          // Maximum reconnection attempts
    uint32_t connection_timeout_ms;   // Connection timeout
    uint32_t retry_delay_ms;          // Delay between retries
    bool pmf_required;                // Protected Management Frames
    bool pmf_capable;                 // PMF capability
    int8_t max_tx_power;             // Maximum transmit power
    uint8_t channel;                  // WiFi channel (0 = auto)
    bool auto_reconnect;              // Enable auto-reconnection
} wifi_framework_config_t;
```

### Default Values

- **Max Retry Count**: 5
- **Connection Timeout**: 30 seconds
- **Retry Delay**: 5 seconds
- **Auto Reconnect**: Enabled
- **Max TX Power**: 60 (maximum)
- **PMF**: Capable but not required

## Error Handling

### Error Codes

The framework returns standard ESP-IDF error codes:

- `ESP_OK`: Operation successful
- `ESP_ERR_INVALID_ARG`: Invalid parameter
- `ESP_ERR_INVALID_STATE`: Invalid state for operation
- `ESP_ERR_NO_MEM`: Memory allocation failed
- `ESP_ERR_TIMEOUT`: Operation timed out
- `ESP_ERR_NOT_FOUND`: Resource not found

### Error Recovery

```c
esp_err_t ret = wifi_framework_connect();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Connection failed: %s", esp_err_to_name(ret));
    
    // Wait and retry
    vTaskDelay(pdMS_TO_TICKS(5000));
    ret = wifi_framework_connect();
}
```

## MISRA Compliance

### Coding Standards

- **No Dynamic Memory**: All memory is statically allocated
- **Bounds Checking**: All array accesses are bounds-checked
- **Error Handling**: All functions return error codes
- **Type Safety**: Strict type checking and validation
- **Resource Management**: Proper cleanup and resource tracking

### Safety Features

- **Input Validation**: All parameters are validated
- **State Validation**: Operations check current state
- **Resource Protection**: Mutex-based thread safety
- **Timeout Handling**: All operations have timeouts

## Performance Considerations

### Memory Usage

- **Static Allocation**: No dynamic memory allocation during operation
- **Configurable Stack Sizes**: Adjustable task stack sizes
- **Efficient Data Structures**: Optimized for embedded systems

### CPU Usage

- **Event-Driven**: Minimal CPU usage when idle
- **Configurable Monitoring**: Adjustable watchdog intervals
- **Efficient Logging**: Conditional logging based on log levels

## Integration with Existing Code

### Replacing Old WiFi Code

```c
// OLD CODE
static void wifi_init_sta(void) {
    // ... old implementation
}

// NEW CODE
wifi_framework_config_t config;
wifi_framework_get_default_config(&config, WIFI_SSID, WIFI_PASS);
wifi_framework_init(&config, event_callback, NULL);
wifi_framework_connect();
```

### Migration Steps

1. **Include Header**: Add `#include "wifi_framework.h"`
2. **Replace Functions**: Use framework functions instead of direct ESP-IDF calls
3. **Add Event Handling**: Implement event callback for status monitoring
4. **Update Configuration**: Use framework configuration structure

## Troubleshooting

### Common Issues

1. **Connection Failures**
   - Check SSID and password
   - Verify network availability
   - Check authentication mode

2. **Memory Issues**
   - Verify stack sizes are sufficient
   - Check for memory leaks in application code

3. **Event Handling**
   - Ensure callback function is properly implemented
   - Check event registration

### Debug Logging

Enable debug logging to see detailed operation information:

```c
// In sdkconfig or menuconfig
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
```

## Examples

### Complete Example

See `wifi_framework_example.c` for a complete working example that demonstrates:

- Framework initialization
- Configuration customization
- Event handling
- Status monitoring
- Error handling

### Integration Example

```c
// In your main application
void app_main(void) {
    // Initialize WiFi framework
    wifi_framework_config_t wifi_config;
    wifi_framework_get_default_config(&wifi_config, WIFI_SSID, WIFI_PASS);
    wifi_config.auto_reconnect = true;
    
    ESP_ERROR_CHECK(wifi_framework_init(&wifi_config, wifi_callback, NULL));
    ESP_ERROR_CHECK(wifi_framework_connect());
    
    // Wait for connection
    while (!wifi_framework_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Start your application
    start_sensor_tasks();
    start_data_transmission();
}
```

## Support

### Documentation

- This README
- Function documentation in header files
- Example code
- ESP-IDF documentation for underlying APIs

### Issues

For issues or questions:
1. Check the logs for error messages
2. Verify configuration parameters
3. Test with minimal example code
4. Review ESP-IDF WiFi documentation

## Version History

- **v1.0.0**: Initial release with core functionality
  - WiFi initialization and management
  - Event handling system
  - Auto-reconnection logic
  - Status monitoring
  - MISRA compliance features

## License

This framework is provided as part of the infrastructure development toolkit. Please refer to your project's licensing terms. 