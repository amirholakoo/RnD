# HTTP Client System for ESP32

A simple, robust, and modular HTTP client system for sending JSON data to servers.

## 🏗️ **Architecture**

The system is built with **3 modular components**:

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Data Sender   │───▶│  HTTP Client    │───▶│   Server        │
│   (High Level)  │    │  (Low Level)    │    │                 │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │
         ▼                       ▼
┌─────────────────┐    ┌─────────────────┐
│   cJSON Lib     │    │   FreeRTOS      │
│   (Built-in)    │    │   Tasks         │
└─────────────────┘    └─────────────────┘
```

## 📁 **Files Overview**

| File | Purpose | Complexity |
|------|---------|------------|
| `http_client.h/c` | **Low-level HTTP operations** | ⭐⭐ |
| `data_sender.h/c` | **High-level data transmission** | ⭐⭐ |
| `data_sender_example.c` | **Usage example** | ⭐ |

## 🚀 **Quick Start**

### **1. Basic Usage (3 lines of code)**

```c
#include "data_sender.h"

// Initialize with your server
data_sender_init("http://your-server.com/api", "your-token");

// Send sensor data
data_sender_send_sensor_data("ESP32_001", 25.5, 60.0);
```

### **2. Complete Example**

```c
#include "data_sender.h"

void app_main(void)
{
    // Initialize WiFi first (your existing code)
    // ... WiFi initialization ...
    
    // Start data sender
    start_data_sender_example();
    
    // Your main application loop
    while (1) {
        // Data is sent automatically every 30 seconds
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## ⚙️ **Configuration**

### **Server Settings**
```c
#define SERVER_URL "http://your-server.com/api/data"
#define AUTH_TOKEN "your-auth-token-here"  // Set to NULL if no auth
#define DEVICE_ID "ESP32_Sensor_001"
```

### **HTTP Client Settings**
```c
http_client_config_t config = {
    .timeout_ms = 10000,        // 10 second timeout
    .verify_ssl = false,        // Skip SSL verification
};
```

## 📊 **JSON Data Format**

### **Sensor Data**
```json
{
  "device_id": "ESP32_001",
  "temperature": 25.5,
  "humidity": 60.0,
  "timestamp": 1703123456,
  "type": "sensor_data"
}
```

### **Status Updates**
```json
{
  "device_id": "ESP32_001",
  "status": "online",
  "timestamp": 1703123456,
  "type": "status_update"
}
```

## 🔧 **API Reference**

### **Data Sender (High Level)**
```c
// Initialize the system
esp_err_t data_sender_init(const char* server_url, const char* auth_token);

// Send sensor readings
esp_err_t data_sender_send_sensor_data(const char* device_id, float temp, float humidity);

// Send status updates
esp_err_t data_sender_send_status(const char* device_id, const char* status);

// Cleanup resources
void data_sender_cleanup(void);
```

### **HTTP Client (Low Level)**
```c
// Initialize HTTP client
esp_err_t http_client_init(const http_client_config_t* config);

// Send JSON data
esp_err_t http_client_send_json(const char* json_data, http_response_t* response);

// Cleanup
void http_client_cleanup(void);
```

## 🛡️ **Robustness Features**

### **Error Handling**
- ✅ **Parameter validation** for all functions
- ✅ **Memory allocation** error checking
- ✅ **HTTP response** status checking
- ✅ **Network timeout** handling (10 seconds)

### **Resource Management**
- ✅ **Automatic cleanup** of JSON data
- ✅ **Memory leak prevention** with proper free()
- ✅ **HTTP client lifecycle** management
- ✅ **Task-safe** operations

### **MISRA Compliance**
- ✅ **Static memory allocation** where possible
- ✅ **Bounds checking** for string operations
- ✅ **Type safety** with proper casting
- ✅ **Error code** consistency

## 📈 **Performance**

| Metric | Value | Notes |
|--------|-------|-------|
| **Memory Usage** | ~2KB per task | Minimal overhead |
| **JSON Size** | ~150 bytes | Compact format |
| **Transmission** | Every 30s | Configurable |
| **Timeout** | 10 seconds | Network resilient |

## 🔍 **Troubleshooting**

### **Common Issues**

1. **"Failed to initialize data sender"**
   - Check WiFi connection
   - Verify server URL format
   - Check network connectivity

2. **"HTTP request failed"**
   - Server might be down
   - Check firewall settings
   - Verify authentication token

3. **"Failed to create JSON"**
   - Check device_id length
   - Verify parameter types
   - Check available memory

### **Debug Logs**
Enable debug logging to see detailed HTTP operations:
```c
esp_log_level_set("HTTP_Client", ESP_LOG_DEBUG);
esp_log_level_set("Data_Sender", ESP_LOG_DEBUG);
```

## 🔗 **Integration with WiFi Framework**

The HTTP client system works seamlessly with your existing WiFi framework:

```c
// In your main application
void app_main(void)
{
    // 1. Initialize WiFi (your existing code)
    wifi_framework_init(&wifi_config, wifi_event_callback, NULL);
    wifi_framework_connect();
    
    // 2. Wait for WiFi connection
    while (!wifi_framework_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // 3. Start HTTP client system
    start_data_sender_example();
    
    // 4. Your application continues...
}
```

## 📝 **Customization**

### **Add New Data Types**
```c
// In data_sender.c, add new function:
esp_err_t data_sender_send_custom_data(const char* device_id, 
                                       const char* custom_field,
                                       float value)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "custom_field", custom_field);
    cJSON_AddNumberToObject(root, "value", value);
    cJSON_AddNumberToObject(root, "timestamp", (int32_t)time(NULL));
    cJSON_AddStringToObject(root, "type", "custom_data");
    
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // Send via HTTP
    http_response_t response;
    esp_err_t ret = http_client_send_json(json_string, &response);
    
    // Cleanup
    free(json_string);
    cJSON_Delete(root);
    
    if (response.response_data != NULL) {
        free(response.response_data);
    }
    
    return ret;
}
```

### **Modify Transmission Frequency**
```c
// In data_sender_example.c, change:
vTaskDelay(pdMS_TO_TICKS(30000U)); // 30 seconds
// To:
vTaskDelay(pdMS_TO_TICKS(5000U));  // 5 seconds
```

## 🎯 **Best Practices**

1. **Always check return codes** from initialization functions
2. **Use meaningful device IDs** for server identification
3. **Implement exponential backoff** for failed transmissions
4. **Monitor memory usage** in long-running applications
5. **Test with your actual server** before deployment

## 📚 **Dependencies**

- **ESP-IDF v5.4** (as per your requirements)
- **esp_http_client** component
- **cJSON** library (included in ESP-IDF)
- **FreeRTOS** for task management

---

**Ready to send data to your server! 🚀** 