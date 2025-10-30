#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"

// WiFi credentials
#define WIFI_SSID "RnD"
#define WIFI_PASSWORD "wnOPxFSCxb"

// Server and image URLs
#define SERVER_URL "http://192.168.10.100:5000"
#define IMAGE_URL "https://fastly.picsum.photos/id/682/800/600.jpg?hmac=nDvj6j28PV7_q1jWXRsp0xS7jtAZYzHophmak9J1ymU"

// Image buffer size (1MB)
#define IMAGE_SIZE (1024 * 1024)
static uint8_t* image_buffer = NULL;
static size_t image_size = 0;

// Semaphores
static SemaphoreHandle_t image_ready_sem = NULL;
static SemaphoreHandle_t sending_complete_sem = NULL;

// Task handles
static TaskHandle_t getImageTaskHandle = NULL;
static TaskHandle_t imageSendTaskHandle = NULL;
static TaskHandle_t ledUpdateTaskHandle = NULL;

// LED Definitions (assuming WS2812 on GPIO 48)
#define LED_PIN 48

// LED States
typedef enum {
    LED_IDLE,              // Black (off)
    LED_CONNECTING_WIFI,   // Blinking Yellow
    LED_WIFI_CONNECTED,    // Solid Green
    LED_WIFI_FAILED,       // Solid Red
    LED_ACQUIRING_IMAGE,   // Solid Blue
    LED_IMAGE_ACQUIRE_FAILED, // Blinking Red
    LED_IMAGE_SAVED,       // Solid Cyan
    LED_WAITING_SERVER_ACK, // Solid Magenta
    LED_SERVER_ACK_RECEIVED, // Solid White
    LED_SENDING_IMAGE,     // Solid Orange
    LED_IMAGE_SENT_SUCCESS, // Solid Green
    LED_IMAGE_SENT_FAILED  // Blinking Red
} LedState;
static volatile LedState currentLedState = LED_IDLE;

// Logging tag
static const char* TAG = "ESP32_IMAGE_TRANSFER";

// Placeholder for LED color structure (replace with actual WS2812 library)
typedef struct {
    uint8_t r, g, b;
} CRGB;

// Placeholder LED function (implement with RMT or library)
static void set_led_color(CRGB color) {
    // TODO: Implement WS2812 control using RMT peripheral
    ESP_LOGI(TAG, "Set LED color: R=%d, G=%d, B=%d", color.r, color.g, color.b);
}

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                currentLedState = LED_CONNECTING_WIFI;
                ESP_LOGI(TAG, "WiFi started, connecting...");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                esp_wifi_connect();
                currentLedState = LED_CONNECTING_WIFI;
                ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        currentLedState = LED_WIFI_CONNECTED;
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Initialize WiFi
static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Download image from URL
static bool download_image(uint8_t* buffer, size_t max_size, size_t* out_size) {
    esp_http_client_config_t config = {
        .url = IMAGE_URL,
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for download");
        return false;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0 || content_length > max_size) {
        ESP_LOGE(TAG, "Invalid content length: %d", content_length);
        esp_http_client_cleanup(client);
        return false;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP GET failed, status: %d", status_code);
        esp_http_client_cleanup(client);
        return false;
    }

    int total_read = 0;
    int read_len;
    while ((read_len = esp_http_client_read(client, (char*)buffer + total_read, content_length - total_read)) > 0) {
        total_read += read_len;
    }

    if (total_read == content_length) {
        *out_size = total_read;
        ESP_LOGI(TAG, "Image downloaded, size: %d bytes", total_read);
        esp_http_client_cleanup(client);
        return true;
    }

    ESP_LOGE(TAG, "Failed to read full image, read: %d, expected: %d", total_read, content_length);
    esp_http_client_cleanup(client);
    return false;
}

// Placeholder for camera capture
static bool capture_image(uint8_t* buffer, size_t max_size, size_t* out_size) {
    ESP_LOGW(TAG, "Camera capture not implemented yet");
    return false;
}

// Function pointer for image acquisition
typedef bool (*acquire_image_func_t)(uint8_t*, size_t, size_t*);
static acquire_image_func_t acquire_image = download_image;

// Get server acknowledgment
static bool get_server_ack(void) {
    char url[64];
    snprintf(url, sizeof(url), "%s/request_send", SERVER_URL);

    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for ack");
        return false;
    }

    esp_http_client_set_header(client, "X-Device-ID", mac_str);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK && esp_http_client_get_status_code(client) == 200) {
        char response[10];
        int len = esp_http_client_read(client, response, sizeof(response) - 1);
        response[len] = '\0';
        if (strcmp(response, "ready") == 0) {
            ESP_LOGI(TAG, "Server ready");
            esp_http_client_cleanup(client);
            return true;
        }
    }

    ESP_LOGE(TAG, "Server ack failed, status: %d", esp_http_client_get_status_code(client));
    esp_http_client_cleanup(client);
    return false;
}

// Send image to server
static bool send_image_to_server(void) {
    char url[64];
    snprintf(url, sizeof(url), "%s/send_image", SERVER_URL);

    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for send");
        return false;
    }

    esp_http_client_set_header(client, "X-Device-ID", mac_str);
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (const char*)image_buffer, image_size);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK && esp_http_client_get_status_code(client) == 200) {
        ESP_LOGI(TAG, "Image sent successfully");
        esp_http_client_cleanup(client);
        return true;
    }

    ESP_LOGE(TAG, "Failed to send image, status: %d", esp_http_client_get_status_code(client));
    esp_http_client_cleanup(client);
    return false;
}

// Update LED state
static void updateLed(void) {
    static bool blinkState = false;
    blinkState = !blinkState;
    CRGB color;
    switch (currentLedState) {
        case LED_IDLE: color = (CRGB){0, 0, 0}; break;
        case LED_CONNECTING_WIFI: color = blinkState ? (CRGB){255, 255, 0} : (CRGB){0, 0, 0}; break;
        case LED_WIFI_CONNECTED: color = (CRGB){0, 255, 0}; break;
        case LED_WIFI_FAILED: color = (CRGB){255, 0, 0}; break;
        case LED_ACQUIRING_IMAGE: color = (CRGB){0, 0, 255}; break;
        case LED_IMAGE_ACQUIRE_FAILED: color = blinkState ? (CRGB){255, 0, 0} : (CRGB){0, 0, 0}; break;
        case LED_IMAGE_SAVED: color = (CRGB){0, 255, 255}; break;
        case LED_WAITING_SERVER_ACK: color = (CRGB){255, 0, 255}; break;
        case LED_SERVER_ACK_RECEIVED: color = (CRGB){255, 255, 255}; break;
        case LED_SENDING_IMAGE: color = (CRGB){255, 165, 0}; break;
        case LED_IMAGE_SENT_SUCCESS: color = (CRGB){0, 255, 0}; break;
        case LED_IMAGE_SENT_FAILED: color = blinkState ? (CRGB){255, 0, 0} : (CRGB){0, 0, 0}; break;
    }
    set_led_color(color);
    return;
}

// LED update task
static void ledUpdateTask(void* pvParameters) {
    while (true) {
        updateLed();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Image acquisition task
static void get_image_task(void* pvParameters) {
    while (true) {
        currentLedState = LED_ACQUIRING_IMAGE;
        bool success = acquire_image(image_buffer, IMAGE_SIZE, &image_size);
        if (success) {
            currentLedState = LED_IMAGE_SAVED;
            ESP_LOGI(TAG, "Image acquired and saved to PSRAM");
            xSemaphoreGive(image_ready_sem);
            xSemaphoreTake(sending_complete_sem, portMAX_DELAY);
        } else {
            currentLedState = LED_IMAGE_ACQUIRE_FAILED;
            ESP_LOGE(TAG, "Failed to acquire image, retrying in 1s");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// Image sending task
static void image_send_task(void* pvParameters) {
    while (true) {
        xSemaphoreTake(image_ready_sem, portMAX_DELAY);
        currentLedState = LED_WAITING_SERVER_ACK;
        if (get_server_ack()) {
            currentLedState = LED_SERVER_ACK_RECEIVED;
            currentLedState = LED_SENDING_IMAGE;
            if (send_image_to_server()) {
                currentLedState = LED_IMAGE_SENT_SUCCESS;
            } else {
                currentLedState = LED_IMAGE_SENT_FAILED;
                ESP_LOGE(TAG, "Failed to send image");
            }
        } else {
            currentLedState = LED_IMAGE_SENT_FAILED;
            ESP_LOGE(TAG, "Server acknowledgment failed");
        }
        xSemaphoreGive(sending_complete_sem);
    }
}

// Main application entry point
void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init();

    // Allocate PSRAM buffer
    image_buffer = (uint8_t*)heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_SPIRAM);
    if (!image_buffer) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffer");
        return;
    }

    // Create semaphores
    image_ready_sem = xSemaphoreCreateBinary();
    sending_complete_sem = xSemaphoreCreateBinary();
    if (!image_ready_sem || !sending_complete_sem) {
        ESP_LOGE(TAG, "Failed to create semaphores");
        return;
    }

    // Create tasks
    xTaskCreate(get_image_task, "GET_IMAGE_TASK", 4096, NULL, 1, &getImageTaskHandle);
    xTaskCreate(image_send_task, "IMAGE_SEND_TASK", 4096, NULL, 1, &imageSendTaskHandle);
    xTaskCreate(ledUpdateTask, "LED_UPDATE_TASK", 4096, NULL, 1, &ledUpdateTaskHandle);

    ESP_LOGI(TAG, "Application started");
}