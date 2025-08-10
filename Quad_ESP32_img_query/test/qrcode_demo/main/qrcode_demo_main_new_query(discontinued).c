#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "quirc.h"
#include "quirc_internal.h"
#include "esp_http_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char *TAG = "qr_psram2";

#define IMG_W    1280
#define IMG_H    720
#define CAM_FS   FRAMESIZE_HD

#define WIFI_SSID "esp"
#define WIFI_PASS "12345678"
#define SERVER_URL "http://192.168.144.207:5000"  // Replace with your server IP and port
#define MAX_RETRIES 10

static char device_id[18];

// WiFi event handler
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Initialize and connect to WiFi
static void connect_wifi(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network interface and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(60));

    ESP_LOGI(TAG, "Connecting to WiFi...");
    // Wait a bit for connection (handled by event handler)
    vTaskDelay(pdMS_TO_TICKS(5000));
}

// Check if server is ready to receive image
static esp_err_t check_server_ready(void)
{
    esp_http_client_config_t config = {
        .url = SERVER_URL "/request_send",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,  // Set a 5-second timeout
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "X-Device-ID", device_id);

    ESP_LOGI(TAG, "Sending GET request to %s", SERVER_URL "/request_send");
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int content_len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP status: %d, Content length: %d", status, content_len);

        if (status == 200) {
            if (content_len > 0) {
                char *buffer = malloc(content_len + 1);
                if (buffer) {
                    int read_len = esp_http_client_read(client, buffer, content_len);

                    buffer[read_len] = 0;
                    ESP_LOGI(TAG, "Response body: '%s'", buffer);
                    if (strcmp(buffer, "ready") == 0) {
                        free(buffer);
                        esp_http_client_cleanup(client);
                        return ESP_OK;
                    } else {
                        ESP_LOGW(TAG, "Unexpected response: '%s'", buffer);
                    }

                    // if (read_len > 0) {
                    //     buffer[read_len] = 0;
                    //     ESP_LOGI(TAG, "Response body: '%s'", buffer);
                    //     if (strcmp(buffer, "ready") == 0) {
                    //         free(buffer);
                    //         esp_http_client_cleanup(client);
                    //         return ESP_OK;
                    //     } else {
                    //         ESP_LOGW(TAG, "Unexpected response: '%s'", buffer);
                    //     }
                    // } else {
                    //     ESP_LOGW(TAG, "No data read from response");
                    // }
                    free(buffer);
                } else {
                    ESP_LOGE(TAG, "Failed to allocate buffer for response");
                }
            } else {
                ESP_LOGW(TAG, "Content length is 0");
            }
        } else {
            ESP_LOGW(TAG, "Unexpected status code: %d", status);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return ESP_FAIL;
}

// Send image to server with retries
static void send_image_to_server(camera_fb_t *fb)
{
    int retry_count = 0;
    bool success = false;

    while (retry_count < MAX_RETRIES && !success) {
        esp_http_client_config_t config = {
            .url = SERVER_URL "/send_image",
            .method = HTTP_METHOD_POST,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        char width_str[10];
        snprintf(width_str, 10, "%d", fb->width);
        char height_str[10];
        snprintf(height_str, 10, "%d", fb->height);

        esp_http_client_set_header(client, "X-Device-ID", device_id);
        esp_http_client_set_header(client, "X-Image-Width", width_str);
        esp_http_client_set_header(client, "X-Image-Height", height_str);
        esp_http_client_set_post_field(client, (const char *)fb->buf, fb->len);

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            if (status == 200) {
                ESP_LOGI(TAG, "Image sent successfully");
                success = true;
            } else {
                ESP_LOGE(TAG, "Failed to send image, status: %d", status);
            }
        } else {
            ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);

        if (!success) {
            retry_count++;
            ESP_LOGI(TAG, "Retrying image send (%d/%d)", retry_count, MAX_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    if (!success) {
        ESP_LOGE(TAG, "Failed to send image after %d retries", MAX_RETRIES);
    }
}

// QR code processing task
static void qr_task(void *arg)
{
    struct quirc *qr = arg;

    while (1) {
        // Capture frame
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera frame capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Send image to server directly
        send_image_to_server(fb);

        // Process QR code
        int qrw, qrh;
        uint8_t *dst = quirc_begin(qr, &qrw, &qrh);
        for (int y = 0; y < qrh; y++) {
            memcpy(dst + y * qrw, fb->buf + y * fb->width, qrw);
        }
        quirc_end(qr);

        // Detect and decode QR codes
        int n = quirc_count(qr);
        if (n > 0) {
            ESP_LOGI(TAG, "Found %d QR code(s)", n);
            for (int i = 0; i < n; i++) {
                struct quirc_code code;
                struct quirc_data data;
                quirc_extract(qr, i, &code);
                int err = quirc_decode(&code, &data);
                if (err == 0) {
                    ESP_LOGI(TAG, "QR[%d]: %s", i, data.payload);
                } else {
                    ESP_LOGE(TAG, "Decode error[%d]: %s", i, quirc_strerror(err));
                }
            }
        } else {
            ESP_LOGD(TAG, "No QR codes found");
        }

        // Return frame buffer
        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


void app_main(void)
{
    // Set device ID from MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(device_id, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device ID: %s", device_id);

    // Connect to WiFi
    connect_wifi();

    // Initialize camera
    camera_config_t cam_cfg = {
        .pin_d0       = 8,  .pin_d1       = 9,
        .pin_d2       = 18, .pin_d3       = 10,
        .pin_d4       = 17, .pin_d5       = 11,
        .pin_d6       = 16, .pin_d7       = 12,
        .pin_xclk     = 15, .pin_pclk     = 13,
        .pin_vsync    = 5,  .pin_href     = 7,
        .pin_sccb_sda = 4,  .pin_sccb_scl = 6,
        .pin_pwdn     = -1, .pin_reset    = -1,
        .xclk_freq_hz = 16000000,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_GRAYSCALE,
        .frame_size   = CAM_FS,
        .fb_count     = 1,
        .grab_mode    = CAMERA_GRAB_LATEST,
        .fb_location  = CAMERA_FB_IN_PSRAM,
    };
    ESP_ERROR_CHECK(esp_camera_init(&cam_cfg));
    sensor_t *s = esp_camera_sensor_get();
    s->set_contrast(s, 2);
    ESP_LOGI(TAG, "Camera initialized: %dx%d GRAYSCALE", IMG_W, IMG_H);

    // Initialize Quirc
    struct quirc *qr = quirc_new();
    if (!qr) {
        ESP_LOGE(TAG, "Failed to initialize Quirc");
        return;
    }
    if (quirc_resize(qr, IMG_W, IMG_H) < 0) {
        ESP_LOGE(TAG, "Quirc resize failed");
        quirc_destroy(qr);
        return;
    }
    ESP_LOGI(TAG, "Quirc PSRAM buffer %dx%d allocated", IMG_W, IMG_H);

    // Start QR task
    if (xTaskCreate(qr_task, "qr", 64 * 1024, qr, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create QR task");
        quirc_destroy(qr);
    }
}