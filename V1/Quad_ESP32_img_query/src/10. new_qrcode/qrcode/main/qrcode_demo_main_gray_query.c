#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "quirc.h"
#include "quirc_internal.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_timer.h"

static const char *TAG = "qr_psram2";

#define IMG_W    1280
#define IMG_H    720
#define CAM_FS   FRAMESIZE_HD

static bool wifi_connected = false;

// Wi-Fi event handler
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
         wifi_connected = false;
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retrying Wi-Fi connection...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ESP_LOGI(TAG, "Connected to Wi-Fi and got IP");
    }
}

// Initialize Wi-Fi
static void wifi_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS initialization failed with %s, erasing partition...", esp_err_to_name(ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "esp",
            .password = "12345678",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(60));
}

static void qr_task(void *arg)
{
    struct quirc *qr = arg;

    while (1) {
        // 1) Capture frame
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "fb_get failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        else ESP_LOGI(TAG, "fb_get successful");


        
        // // 2) Send image to laptop
        // esp_http_client_config_t config = {
        //     .url = "http://192.168.237.175:8000/upload",
        //     .method = HTTP_METHOD_POST,
        //     .timeout_ms = 10000,
        // };
        // esp_http_client_handle_t client = esp_http_client_init(&config);
        // char width_str[16];
        // char height_str[16];
        // snprintf(width_str, sizeof(width_str), "%d", fb->width);
        // snprintf(height_str, sizeof(height_str), "%d", fb->height);
        // esp_http_client_set_header(client, "Image-Width", width_str);
        // esp_http_client_set_header(client, "Image-Height", height_str);
        // esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
        // esp_http_client_set_post_field(client, (const char *)fb->buf, fb->len);
        // esp_err_t err = esp_http_client_perform(client);
        // if (err == ESP_OK) {
        //     ESP_LOGI(TAG, "Image sent successfully");
        // } else {
        //     ESP_LOGE(TAG, "Failed to send image: %s", esp_err_to_name(err));
        // }
        // esp_http_client_cleanup(client);
         


        
        // 3) Copy into Quirc’s PSRAM buffer and detect QR code (unchanged)
        int qrw, qrh;
        uint8_t *dst = quirc_begin(qr, &qrw, &qrh);
        for (int y = 0; y < qrh; y++) {
            memcpy(dst + y * qrw, fb->buf + y * fb->width, qrw);
        }
        quirc_end(qr);

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
                    
                    // Send QR code payload as JSON to server
                    esp_http_client_config_t config = {
                        .url = "http://192.168.237.15:5000/post-data",
                        .method = HTTP_METHOD_POST,
                        .timeout_ms = 5000,
                    };
                    esp_http_client_handle_t client = esp_http_client_init(&config);
                    
                    // Create JSON payload with proper size handling
                    char json_payload[1024];  // Increased buffer size
                    
                    // Safely handle the payload string with explicit length checking
                    const char* payload_str = (char*)data.payload;
                    size_t payload_len = strlen(payload_str);
                    if (payload_len > 800) {
                        payload_len = 800;
                        ESP_LOGW(TAG, "QR payload too long, truncating to 800 chars");
                    }
                    
                    // Build JSON using strcpy and strcat to avoid format string issues
                    strcpy(json_payload, "{\"qr_index\": ");
                    char temp[32];
                    snprintf(temp, sizeof(temp), "%d", i);
                    strcat(json_payload, temp);
                    strcat(json_payload, ", \"payload\": \"");
                    
                    // Copy payload with length limit
                    strncat(json_payload, payload_str, payload_len);
                    strcat(json_payload, "\", \"timestamp\": ");
                    
                    snprintf(temp, sizeof(temp), "%lld", esp_timer_get_time() / 1000);
                    strcat(json_payload, temp);
                    strcat(json_payload, "}");
                    
                    esp_http_client_set_header(client, "Content-Type", "application/json");
                    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

                    // Log the raw JSON payload
                    ESP_LOGI(TAG, "Raw JSON payload: %d\r\n%s", strlen(json_payload), json_payload);
                    
                    // Log parsed JSON data for verification
                    ESP_LOGI(TAG, "Parsed JSON data - QR index: %d, Payload: %.*s, Timestamp: %lld", 
                             i, (int)payload_len, payload_str, esp_timer_get_time() / 1000);
                    
                    esp_err_t http_err = esp_http_client_perform(client);
                    if (http_err == ESP_OK) {
                        int status_code = esp_http_client_get_status_code(client);
                        ESP_LOGI(TAG, "QR data sent successfully, status: %d", status_code);
                    } else {
                        ESP_LOGE(TAG, "Failed to send QR data: %s", esp_err_to_name(http_err));
                    }
                    esp_http_client_cleanup(client);
                    
                } 
                else {
                    ESP_LOGE(TAG, "decode err[%d]: %s",
                             i, quirc_strerror(err));
                }
            }
        } else {
            ESP_LOGD(TAG, "No QR codes");
        }
        
        // 4) Return frame
        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    // Initialize Wi-Fi
    wifi_init();

    while (!wifi_connected) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Camera init (unchanged)
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
    s->set_vflip(s, 1);

    // // Manually configure camera exposure settings by directly writing to sensor registers
    // // Disable Auto Exposure Control (AEC) and Auto Gain Control (AGC)

    
    // // For OV5640 sensor:
    // // Disable AGC and AEC
    // s->set_reg(s, 0x3503, 0xFF, 0x03);  // Bit[2]=1 manual gain, Bit[1:0]=11 manual exposure
    
    // // Set manual gain (lower values = less gain = clearer image)
    // // Gain registers: 0x350A[1:0], 0x350B[7:0] - combined to form 10-bit gain value
    // // Gain formula: Gain = (register_value) / 16
    // s->set_reg(s, 0x350A, 0x03, 0x02);  // Higher bits of gain (0x00)
    // s->set_reg(s, 0x350B, 0xFF, 0x00);  // Lower bits of gain (0x20 = 2x gain)
    
    // // Set manual exposure time
    // // Exposure registers: 0x3500[3:0], 0x3501[7:0], 0x3502[7:0] - 20-bit value
    // // Exposure time (in lines) = (register_value) / (2^4)
    // // For 30fps: max exposure time = 1/30 = 33.33ms
    // // For OV5640 with XCLK=16MHz, line time ≈ 29.5µs (depends on exact configuration)
    // // So max lines for 33.33ms ≈ 1130 lines
    // // Setting to ~800 lines (≈23.6ms) for good balance
    // s->set_reg(s, 0x3500, 0x0F, 0x00);  // Exposure[19:16]
    // s->set_reg(s, 0x3501, 0xFF, 0x01);  // Exposure[15:8] = 0x03
    // s->set_reg(s, 0x3502, 0xFF, 0xC2);  // Exposure[7:0] = 0x20
    //                                     // Combined: 0x0320 = 800 lines ≈ 23.6ms
    
    // // Additional settings for better QR code detection
    // // Adjust digital gain (1x = no digital gain)
    // s->set_reg(s, 0x5001, 0xFF, 0x83);  // Enable manual digital gain
    // s->set_reg(s, 0x5180, 0xFF, 0x00);  // Digital gain control

    // s->set_denoise(s, 8);        // 0 to 8, minimal noise reduction for clarity
    
    ESP_LOGI(TAG, "Camera configured with manual exposure and gain settings");
    
    ESP_LOGI(TAG, "Camera ready: %dx%d GRAYSCALE", IMG_W, IMG_H);

    // Quirc init (unchanged)
    struct quirc *qr = quirc_new();
    assert(qr);
    if (quirc_resize(qr, IMG_W, IMG_H) < 0) {
        ESP_LOGE(TAG, "quirc_resize failed");
        quirc_destroy(qr);
        return;
    }
    ESP_LOGI(TAG, "Quirc PSRAM buffer %dx%d allocated", IMG_W, IMG_H);

    // Spawn the QR task
    if (xTaskCreate(qr_task, "qr", 64 * 1024, qr, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create qr_task");
        quirc_destroy(qr);
    }
}