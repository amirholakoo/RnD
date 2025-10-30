/**
 * @file http_client.c
 * @brief Simple and robust HTTP client implementation
 */

#include "http_client.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "string.h"

static const char* TAG = "HTTP_Client";
static esp_http_client_handle_t client = NULL;
static http_client_config_t client_config;

esp_err_t http_client_init(const http_client_config_t* config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy configuration
    memcpy(&client_config, config, sizeof(http_client_config_t));
    
    // Create HTTP client configuration
    esp_http_client_config_t http_config = {
        .url = client_config.url,
        .timeout_ms = client_config.timeout_ms,
        .skip_cert_common_name_check = !client_config.verify_ssl,
        .crt_bundle_attach = NULL,
    };
    
    // Create HTTP client
    client = esp_http_client_init(&http_config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (strlen(client_config.auth_header) > 0) {
        esp_http_client_set_header(client, "Authorization", client_config.auth_header);
    }
    
    ESP_LOGI(TAG, "HTTP client initialized for URL: %s", client_config.url);
    return ESP_OK;
}

esp_err_t http_client_send_json(const char* json_data, http_response_t* response)
{
    if (client == NULL || json_data == NULL || response == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Set POST method and data
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, json_data, strlen(json_data));
    
    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Get response
    response->status_code = esp_http_client_get_status_code(client);
    response->response_len = esp_http_client_get_content_length(client);
    
    if (response->response_len > 0) {
        response->response_data = malloc(response->response_len + 1);
        if (response->response_data == NULL) {
            ESP_LOGE(TAG, "Failed to allocate response buffer");
            return ESP_ERR_NO_MEM;
        }
        
        int read_len = esp_http_client_read(client, response->response_data, response->response_len);
        response->response_data[read_len] = '\0';
        response->response_len = read_len;
    } else {
        response->response_data = NULL;
    }
    
    ESP_LOGI(TAG, "HTTP POST successful, status: %d", response->status_code);
    return ESP_OK;
}

void http_client_cleanup(void)
{
    if (client != NULL) {
        esp_http_client_cleanup(client);
        client = NULL;
        ESP_LOGI(TAG, "HTTP client cleaned up");
    }
} 