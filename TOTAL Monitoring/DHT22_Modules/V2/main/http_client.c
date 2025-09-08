/**
 * @file http_client.c
 * @brief Simple and robust HTTP client implementation
 */

#include "http_client.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "HTTP_Client";
static esp_http_client_handle_t client = NULL;
static http_client_config_t client_config;
static bool client_initialized = false;

esp_err_t http_client_init(const http_client_config_t* config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy configuration
    memcpy(&client_config, config, sizeof(http_client_config_t));
    
    // Create HTTP client configuration with improved settings
    esp_http_client_config_t http_config = {
        .url = client_config.url,
        .timeout_ms = client_config.timeout_ms,
        .skip_cert_common_name_check = !client_config.verify_ssl,
        .crt_bundle_attach = NULL,
        .keep_alive_enable = true,           /* Enable keep-alive for connection reuse */
        .keep_alive_idle = 5,                /* Keep connection alive for 5 seconds */
        .keep_alive_interval = 5,            /* Send keep-alive packet every 5 seconds */
        .keep_alive_count = 3,               /* Maximum keep-alive probes */
        .disable_auto_redirect = false,      /* Allow automatic redirects */
        .max_redirection_count = 3,          /* Maximum redirects */
        .buffer_size = 1024,                 /* Buffer size for HTTP operations */
        .buffer_size_tx = 1024,              /* Buffer size for transmission */
    };
    
    // Create HTTP client
    client = esp_http_client_init(&http_config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Connection", "keep-alive");
    if (strlen(client_config.auth_header) > 0) {
        esp_http_client_set_header(client, "Authorization", client_config.auth_header);
    }
    
    client_initialized = true;
    ESP_LOGI(TAG, "HTTP client initialized for URL: %s", client_config.url);
    return ESP_OK;
}

esp_err_t http_client_send_json(const char* json_data, http_response_t* response)
{
    if (!client_initialized || client == NULL || json_data == NULL || response == NULL) {
        ESP_LOGE(TAG, "Invalid parameters or client not initialized");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err = ESP_OK;
    int retry_count = 0;
    const int max_retries = 2; /* Maximum retry attempts */
    
    /* Clear response structure */
    response->status_code = 0;
    response->response_len = 0;
    response->response_data = NULL;
    
    while (retry_count <= max_retries) {
        /* Set POST method and data for each attempt */
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_post_field(client, json_data, strlen(json_data));
        
        /* Perform request */
        err = esp_http_client_perform(client);
        
        if (err == ESP_OK) {
            /* Request successful, get response */
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
        } else {
            /* Request failed */
            ESP_LOGW(TAG, "HTTP request failed (attempt %d/%d): %s", 
                     retry_count + 1, max_retries + 1, esp_err_to_name(err));
            
            /* If it's a connection-related error, try to close and reopen connection */
            if (err == ESP_ERR_HTTP_EAGAIN || err == ESP_ERR_HTTP_CONNECT || 
                err == ESP_ERR_TIMEOUT || err == ESP_ERR_HTTP_FETCH_HEADER) {
                
                ESP_LOGW(TAG, "Connection issue detected, attempting to reconnect...");
                
                /* Close current connection */
                esp_http_client_close(client);
                
                /* Small delay before retry */
                vTaskDelay(pdMS_TO_TICKS(1000U));
                
                /* Reopen connection */
                esp_http_client_open(client, 0);
                
                retry_count++;
            } else {
                /* Non-recoverable error, don't retry */
                ESP_LOGE(TAG, "Non-recoverable HTTP error: %s", esp_err_to_name(err));
                break;
            }
        }
    }
    
    ESP_LOGE(TAG, "HTTP request failed after %d attempts: %s", max_retries + 1, esp_err_to_name(err));
    return err;
}

void http_client_cleanup(void)
{
    if (client != NULL) {
        esp_http_client_cleanup(client);
        client = NULL;
        client_initialized = false;
        ESP_LOGI(TAG, "HTTP client cleaned up");
    }
} 