/**
 * @file data_sender.c
 * @brief Simple data sender implementation
 */

#include "data_sender.h"
#include "http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <time.h>
#include <string.h>

static const char* TAG = "Data_Sender";
static bool is_initialized = false;

esp_err_t data_sender_init(const char* server_url, const char* auth_token)
{
    if (server_url == NULL) {
        ESP_LOGE(TAG, "Invalid server URL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Prepare HTTP client configuration with increased timeout
    http_client_config_t config = {
        .timeout_ms = 30000,  /* Increased timeout to 30 seconds for better reliability */
        .verify_ssl = false,
    };
    
    // Copy URL
    strncpy(config.url, server_url, sizeof(config.url) - 1);
    config.url[sizeof(config.url) - 1] = '\0';
    
    // Copy auth token if provided
    if (auth_token != NULL) {
        snprintf(config.auth_header, sizeof(config.auth_header), "Bearer %s", auth_token);
    } else {
        config.auth_header[0] = '\0';
    }
    
    // Initialize HTTP client
    esp_err_t ret = http_client_init(&config);
    if (ret == ESP_OK) {
        is_initialized = true;
        ESP_LOGI(TAG, "Data sender initialized for server: %s", server_url);
    }
    
    return ret;
}

esp_err_t data_sender_send_sensor_data(const char* device_id, float temp, float humidity)
{
    if (!is_initialized || device_id == NULL) {
        ESP_LOGE(TAG, "Data sender not initialized or invalid device ID");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create JSON data using cJSON
    cJSON* root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }
    
    // Add fields to JSON
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddNumberToObject(root, "temperature", temp);
    cJSON_AddNumberToObject(root, "humidity", humidity);
    cJSON_AddNumberToObject(root, "timestamp", (int32_t)time(NULL));
    cJSON_AddStringToObject(root, "type", "sensor_data");
    
    // Convert to string
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // Send via HTTP
    http_response_t response;
    esp_err_t ret = http_client_send_json(json_string, &response);
    
    // Cleanup JSON data
    free(json_string);
    cJSON_Delete(root);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Sensor data sent successfully, HTTP status: %d", response.status_code);
        
        // Cleanup response if it has data
        if (response.response_data != NULL) {
            free(response.response_data);
        }
    } else {
        ESP_LOGE(TAG, "Failed to send sensor data");
    }
    
    return ret;
}

esp_err_t data_sender_send_dht22_data(const char* device_id, const char* sensor_type, float temperature, float humidity, uint64_t timestamp)
{
    if (!is_initialized || device_id == NULL || sensor_type == NULL) {
        ESP_LOGE(TAG, "Data sender not initialized or invalid parameters");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create JSON data using cJSON
    cJSON* root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }
    
    // Part 1: device_id
    cJSON_AddStringToObject(root, "device_id", device_id);
    
    // Part 2: sensor_type
    cJSON_AddStringToObject(root, "sensor_type", sensor_type);
    
    // Part 3: data (contains all sensor data)
    cJSON* data_obj = cJSON_CreateObject();
    if (data_obj == NULL) {
        ESP_LOGE(TAG, "Failed to create data object");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    cJSON_AddNumberToObject(data_obj, "temperature", temperature);
    cJSON_AddNumberToObject(data_obj, "humidity", humidity);
    cJSON_AddNumberToObject(data_obj, "timestamp", (int64_t)timestamp);
    cJSON_AddStringToObject(data_obj, "type", "dht22_data");
    
    // Add the data object to the root
    cJSON_AddItemToObject(root, "data", data_obj);
    
    // Convert to string
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "DHT22 data: %s", json_string);
    
    // Send via HTTP
    http_response_t response;
    esp_err_t ret = http_client_send_json(json_string, &response);
    
    // Cleanup JSON data
    free(json_string);
    cJSON_Delete(root);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "DHT22 data sent successfully, HTTP status: %d", response.status_code);
        
        // Cleanup response if it has data
        if (response.response_data != NULL) {
            free(response.response_data);
        }
    } else {
        ESP_LOGE(TAG, "Failed to send DHT22 data");
        // Note: Error handling for restart is done in the calling function
    }
    
    return ret;
}

esp_err_t data_sender_send_status(const char* device_id, const char* status)
{
    if (!is_initialized || device_id == NULL || status == NULL) {
        ESP_LOGE(TAG, "Data sender not initialized or invalid parameters");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create JSON data using cJSON
    cJSON* root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }
    
    // Part 1: device_id
    cJSON_AddStringToObject(root, "device_id", device_id);
    
    // Part 2: sensor_type
    cJSON_AddStringToObject(root, "sensor_type", "status");
    
    // Part 3: data (contains status information)
    cJSON* data_obj = cJSON_CreateObject();
    if (data_obj == NULL) {
        ESP_LOGE(TAG, "Failed to create data object");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    cJSON_AddStringToObject(data_obj, "status", status);
    cJSON_AddNumberToObject(data_obj, "timestamp", (int32_t)time(NULL));
    cJSON_AddStringToObject(data_obj, "type", "status_update");
    
    // Add the data object to the root
    cJSON_AddItemToObject(root, "data", data_obj);
    
    // Convert to string
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // Send via HTTP
    http_response_t response;
    esp_err_t ret = http_client_send_json(json_string, &response);
    
    // Cleanup JSON data
    free(json_string);
    cJSON_Delete(root);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Status data sent successfully, HTTP status: %d", response.status_code);
        
        // Cleanup response if it has data
        if (response.response_data != NULL) {
            free(response.response_data);
        }
    } else {
        ESP_LOGE(TAG, "Failed to send status data");
        // Note: Error handling for restart is done in the calling function
    }
    
    return ret;
}

void data_sender_cleanup(void)
{
    if (is_initialized) {
        http_client_cleanup();
        is_initialized = false;
        ESP_LOGI(TAG, "Data sender cleaned up");
    }
} 