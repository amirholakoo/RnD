#include <stdio.h>
#include <sys/param.h>
#include <string.h>
// #include <inttypes.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_camera.h"
#include "quirc.h"
#include "quirc_internal.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "defines.h"

// WiFi credentials
#define WIFI_SSID "esp"
#define WIFI_PASS "12345678"

// Server URL
#define SERVER_URL "http://192.168.144.207:5000"

static const char *TAG = "example";

// Image size for QR code detection
#define IMG_WIDTH 1280
#define IMG_HEIGHT 720
#define CAM_FRAME_SIZE FRAMESIZE_HD

static QueueHandle_t processing_queue;
static QueueHandle_t sending_queue;
static char mac_address[18];
static bool wifi_connected = false;

static void wifi_init(void);
static void get_mac_address(void);
static bool send_image_to_server(camera_fb_t *fb);
static void sending_task(void *arg);
static void processing_task(void *arg);
static void main_task(void *arg);

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ESP_LOGI(TAG, "Connected to WiFi");
    }
}

// Initialize WiFi
static void wifi_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .scan_method = DEFAULT_SCAN_METHOD,
            .sort_method = DEFAULT_SORT_METHOD,
            .threshold.rssi = DEFAULT_RSSI,
            .threshold.authmode = DEFAULT_AUTHMODE,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Get MAC address as string
static void get_mac_address(void) {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(mac_address, sizeof(mac_address), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Send image to server
static bool send_image_to_server(camera_fb_t *fb) {
    esp_http_client_config_t config = {
        .url = SERVER_URL "/send_image",
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    char width_str[10];
    char height_str[10];
    snprintf(width_str, sizeof(width_str), "%d", fb->width);
    snprintf(height_str, sizeof(height_str), "%d", fb->height);
    esp_http_client_set_header(client, "X-Device-ID", mac_address);
    esp_http_client_set_header(client, "X-Image-Width", width_str);
    esp_http_client_set_header(client, "X-Image-Height", height_str);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_post_field(client, (const char*)fb->buf, fb->len);
    esp_err_t err = esp_http_client_perform(client);
    bool success = (err == ESP_OK && esp_http_client_get_status_code(client) == 200);
    if (success) {
        ESP_LOGI(TAG, "Image sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send image");
    }
    esp_http_client_cleanup(client);
    return success;
}

// Sending task: sends images to the server
static void sending_task(void *arg) {
    QueueHandle_t input_queue = (QueueHandle_t)arg;
    while (true) {
        camera_fb_t *pic;
        xQueueReceive(input_queue, &pic, portMAX_DELAY);
        send_image_to_server(pic);
        esp_camera_fb_return(pic);
    }
}

// Processing task: detects and decodes QR codes, then sends to sending task
static void processing_task(void *arg) {
    struct quirc *qr = quirc_new();
    assert(qr);

    int qr_width = IMG_WIDTH;
    int qr_height = IMG_HEIGHT;
    if (quirc_resize(qr, qr_width, qr_height) < 0) {
        ESP_LOGE(TAG, "Failed to allocate QR buffer");
        return;
    }

    QueueHandle_t input_queue = (QueueHandle_t)arg;
    ESP_LOGI(TAG, "Processing task ready");

    while (true) {
        camera_fb_t *pic;
        uint8_t *qr_buf = quirc_begin(qr, NULL, NULL);

        // Get the next frame from the queue
        int res = xQueueReceive(input_queue, &pic, portMAX_DELAY);
        assert(res == pdPASS);

        // Directly copy the grayscale image to the quirc buffer
        memcpy(qr_buf, pic->buf, qr_width * qr_height);

        // Process the frame for QR code detection
        quirc_end(qr);
        int count = quirc_count(qr);

        // Decode detected QR codes and output to serial
        for (int i = 0; i < count; i++) {
            struct quirc_code code = {};
            struct quirc_data qr_data = {};
            quirc_extract(qr, i, &code);
            quirc_decode_error_t err = quirc_decode(&code, &qr_data);
            if (err == 0) {
                ESP_LOGI(TAG, "QR code detected: %s", qr_data.payload);
            } else {
                ESP_LOGE(TAG, "QR decode error: %d \r\n%s ", err, quirc_strerror(err));
            }
        }

        if (count == 0) {
            ESP_LOGI(TAG, "No QR codes detected in frame");
        }

        // Send the image to the sending queue
        xQueueSend(sending_queue, &pic, portMAX_DELAY);
    }
}

// Main task: initializes the camera and starts the processing task
static void main_task(void *arg) {
    // Initialize the camera with settings for grayscale and higher resolution
    camera_config_t camera_config = {
        .pin_d0 = 8,
        .pin_d1 = 9,
        .pin_d2 = 18,
        .pin_d3 = 10,
        .pin_d4 = 17,
        .pin_d5 = 11,
        .pin_d6 = 16,
        .pin_d7 = 12,
        .pin_xclk = 15,
        .pin_pclk = 13,
        .pin_vsync = 5,
        .pin_href = 7,
        .pin_sccb_sda = 4,
        .pin_sccb_scl = 6,
        .pin_pwdn = -1,
        .pin_reset = -1,
        .xclk_freq_hz = 16000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_GRAYSCALE,  // Capture in grayscale
        .frame_size = CAM_FRAME_SIZE,         // 1280x720
        .jpeg_quality = 10,                   // Not applicable for grayscale
        .fb_count = 1,
        .grab_mode = CAMERA_GRAB_LATEST,
        .fb_location = CAMERA_FB_IN_PSRAM,    // Allocate frame buffer in PSRAM
    };
    ESP_ERROR_CHECK(esp_camera_init(&camera_config));
    sensor_t *s = esp_camera_sensor_get();
    s->set_contrast(s, 2);
    ESP_LOGI(TAG, "Camera initialized");

    

    // Create queues for processing and sending tasks
    processing_queue = xQueueCreate(1, sizeof(camera_fb_t *));
    sending_queue = xQueueCreate(1, sizeof(camera_fb_t *));
    assert(processing_queue);
    assert(sending_queue);

    ESP_LOGI(TAG, "Queue init: 2 x %"PRIu64 , (uint64_t)sizeof(camera_fb_t *));

    // Start the processing and sending tasks
    xTaskCreatePinnedToCore(&processing_task, "processing", 35000, processing_queue, 0, NULL, 1);
    xTaskCreatePinnedToCore(&sending_task, "sending", 4096, sending_queue, 1, NULL, 1);
    ESP_LOGI(TAG, "Processing and sending tasks started");

    // Main loop to capture frames and send to processing task
    while (1) {
        camera_fb_t *pic = esp_camera_fb_get();
        if (pic == NULL) {
            ESP_LOGE(TAG, "Failed to capture frame");
            continue;
        }

        // Send the frame to the processing task with a short timeout
        int res = xQueueSend(processing_queue, &pic, pdMS_TO_TICKS(10));
        if (res == pdFAIL) {
            esp_camera_fb_return(pic);  // Drop frame if processing task is busy
        }
    }
}

// Application entry point
void app_main(void) {
    // Initialize WiFi
    wifi_init();
    get_mac_address();

    // Wait for WiFi connection
    while (!wifi_connected) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    xTaskCreatePinnedToCore(&main_task, "main", 4096, NULL, 5, NULL, 0);
}