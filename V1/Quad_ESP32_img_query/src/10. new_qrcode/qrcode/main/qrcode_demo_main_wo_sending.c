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
        .xclk_freq_hz = 24000000,
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
    s->set_vflip(s, 1);

    // For OV5640 sensor:
    // Disable AGC and AEC
    s->set_reg(s, 0x3503, 0xFF, 0x03);  // Bit[2]=1 manual gain, Bit[1:0]=11 manual exposure
    
    // Set manual gain (lower values = less gain = clearer image)
    // Gain registers: 0x350A[1:0], 0x350B[7:0] - combined to form 10-bit gain value
    // Gain formula: Gain = (register_value) / 16
    s->set_reg(s, 0x350A, 0x03, 0x02);  // Higher bits of gain (0x00)
    s->set_reg(s, 0x350B, 0xFF, 0x00);  // Lower bits of gain (0x20 = 2x gain)
    
    // Set manual exposure time
    // Exposure registers: 0x3500[3:0], 0x3501[7:0], 0x3502[7:0] - 20-bit value
    // Exposure time (in lines) = (register_value) / (2^4)
    // For 30fps: max exposure time = 1/30 = 33.33ms
    // For OV5640 with XCLK=24MHz, line time ≈ 19.7µs (depends on exact configuration)
    // So max lines for 33.33ms ≈ 1690 lines
    // Setting to ~800 lines (≈15.8ms) for good balance
    s->set_reg(s, 0x3500, 0x0F, 0x00);  // Exposure[19:16]
    s->set_reg(s, 0x3501, 0xFF, 0x03);  // Exposure[15:8] = 0x03
    s->set_reg(s, 0x3502, 0xFF, 0x20);  // Exposure[7:0] = 0x20
                                        // Combined: 0x0320 = 800 lines ≈ 23.6ms
    
    // Additional settings for better QR code detection
    // Adjust digital gain (1x = no digital gain)
    s->set_reg(s, 0x5001, 0xFF, 0x83);  // Enable manual digital gain
    s->set_reg(s, 0x5180, 0xFF, 0x00);  // Digital gain control

    s->set_denoise(s, 8);        // 0 to 8, minimal noise reduction for clarity

    // // Configure camera light control parameters for OV5640 - optimized for steady QR code detection
    // s->set_brightness(s, 0);     // -3 to 3, neutral for consistent exposure
    // s->set_saturation(s, 0);     // -4 to 4, neutral since QR codes are monochromatic
    // s->set_sharpness(s, 0);      // -3 to 3, natural sharpness without artificial enhancement
    // s->set_denoise(s, 1);        // 0 to 8, minimal noise reduction for clarity
    // s->set_quality(s, 10);       // 0 to 63, high quality for better QR detection
    // s->set_gainceiling(s, GAINCEILING_2X); // 2X gain ceiling for minimal noise
    // s->set_colorbar(s, 0);       // 0 = disable test pattern, 1 = enable
    // s->set_whitebal(s, 1);       // 0 = disable, 1 = enable auto white balance
    // s->set_gain_ctrl(s, 1);      // 0 = disable, 1 = enable auto gain control
    // s->set_exposure_ctrl(s, 1);  // 0 = disable, 1 = enable auto exposure control
    // s->set_hmirror(s, 0);        // 0 = disable, 1 = enable horizontal mirror
    // s->set_awb_gain(s, 1);       // 0 = disable, 1 = enable auto white balance gain
    // s->set_aec2(s, 1);           // 0 = disable, 1 = enable advanced exposure control
    // s->set_ae_level(s, 0);       // -5 to 5, neutral exposure level target
    
    // // Critical shutter speed control parameters for steady images
    // s->set_aec_value(s, 200);    // 0 to max_val (typically 1200), lower = faster shutter = less motion blur
    // s->set_agc_gain(s, 0);       // 0 to 64, lower = less gain = clearer image with less noise

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
