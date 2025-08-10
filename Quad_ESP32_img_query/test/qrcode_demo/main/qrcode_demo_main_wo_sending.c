#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "quirc.h"
#include "quirc_internal.h"

static const char *TAG = "qr_psram2";

#define IMG_W    1280
#define IMG_H    720
#define CAM_FS   FRAMESIZE_HD

static void qr_task(void *arg)
{
    struct quirc *qr = arg;

    while (1) {
        // 1) grab frame
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "fb_get failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 2) copy into Quirc’s PSRAM buffer
        int qrw, qrh;
        uint8_t *dst = quirc_begin(qr, &qrw, &qrh);
        for (int y = 0; y < qrh; y++) {
            memcpy(dst + y * qrw, fb->buf + y * fb->width, qrw);
        }
        quirc_end(qr);

        // 3) detect & decode
        int n = quirc_count(qr);
        if (n) {
            ESP_LOGI(TAG, "Found %d QR code(s)", n);
            for (int i = 0; i < n; i++) {
                struct quirc_code code;
                struct quirc_data data;
                quirc_extract(qr, i, &code);
                int err = quirc_decode(&code, &data);
                if (err == 0) {
                    ESP_LOGI(TAG, "QR[%d]: %s", i, data.payload);
                } else {
                    ESP_LOGE(TAG, "decode err[%d]: %s",
                             i, quirc_strerror(err));
                }
            }
        } else {
            ESP_LOGD(TAG, "No QR codes");
        }

        // 4) return frame
        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    // --- Camera init (2 PSRAM fb’s) ---
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
        .fb_count     = 1,                     // two PSRAM frames
        .grab_mode    = CAMERA_GRAB_LATEST,
        .fb_location  = CAMERA_FB_IN_PSRAM,
    };
    ESP_ERROR_CHECK(esp_camera_init(&cam_cfg));
    sensor_t *s = esp_camera_sensor_get();
    s->set_contrast(s, 2);
    ESP_LOGI(TAG, "Camera ready: %dx%d GRAYSCALE", IMG_W, IMG_H);

    // --- Quirc init (allocates its own PSRAM buffer) ---
    struct quirc *qr = quirc_new();
    assert(qr);
    if (quirc_resize(qr, IMG_W, IMG_H) < 0) {
        ESP_LOGE(TAG, "quirc_resize failed");
        quirc_destroy(qr);
        return;
    }
    ESP_LOGI(TAG, "Quirc PSRAM buffer %dx%d allocated", IMG_W, IMG_H);

    // --- Spawn the QR task on a large stack ---
    if (xTaskCreate(qr_task, "qr", 64 * 1024, qr, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create qr_task");
        quirc_destroy(qr);
    }
}
