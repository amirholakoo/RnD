#include <stdio.h>
#include <sys/param.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_camera.h"
#include "quirc.h"
#include "quirc_internal.h"

static const char *TAG = "example";

// Image size for QR code detection
#define IMG_WIDTH 1280
#define IMG_HEIGHT 720
#define CAM_FRAME_SIZE FRAMESIZE_HD

static void processing_task(void *arg);
static void main_task(void *arg);

// Application entry point
void app_main(void)
{
    xTaskCreatePinnedToCore(&main_task, "main", 4096, NULL, 5, NULL, 0);
}

// Main task: initializes the camera and starts the processing task
static void main_task(void *arg)
{
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
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_GRAYSCALE,  // Capture in grayscale
        .frame_size = CAM_FRAME_SIZE,         // 640x480
        .jpeg_quality = 10,                   // Not applicable for grayscale
        .fb_count = 1,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location = CAMERA_FB_IN_PSRAM,    // Allocate frame buffer in PSRAM
    };
    ESP_ERROR_CHECK(esp_camera_init(&camera_config));
    sensor_t *s = esp_camera_sensor_get();
    s->set_contrast(s, 2);
    //s->set_vflip(s, 1);  // Vertical flip
    ESP_LOGI(TAG, "Camera initialized");

    // Create queue for passing frames to the processing task
    QueueHandle_t processing_queue = xQueueCreate(1, sizeof(camera_fb_t *));
    assert(processing_queue);

    // Start the processing task
    xTaskCreatePinnedToCore(&processing_task, "processing", 35000, processing_queue, 0, NULL, 1);
    ESP_LOGI(TAG, "Processing task started");

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

// Processing task: detects and decodes QR codes, outputs results to serial
static void processing_task(void *arg)
{
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

        // Return the frame buffer to the camera driver
        esp_camera_fb_return(pic);

        // Process the frame for QR code detection
        quirc_end(qr);
        int count = quirc_count(qr);

        // Decode detected QR codes and output to serial
        for (int i = 0; i < count; i++) {
            struct quirc_code code = {};
            struct quirc_data qr_data = {};
            quirc_extract(qr, i, &code);
            //quirc_flip(&code);
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
    }
}