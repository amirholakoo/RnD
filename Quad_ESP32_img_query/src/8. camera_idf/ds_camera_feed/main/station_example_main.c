#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_camera.h"
#include "led_strip.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "defines.h"

// WiFi credentials
// #define WIFI_SSID "RnD"
// #define WIFI_PASS "wnOPxFSCxb"

#define WIFI_SSID "esp"
#define WIFI_PASS "12345678"

// Server URL
// #define SERVER_URL "http://192.168.10.103:5000"
#define SERVER_URL "http://192.168.187.207:5000"


// Image buffer size (512KB per buffer)
#define IMAGE_SIZE (512 * 1024)
static uint8_t* buffers[2];    // Two buffers in PSRAM
static size_t image_sizes[2];  // Actual size of each captured image
static int next_capture = 0;   // Next buffer index for capture task
static int next_send = 0;      // Next buffer index for send task

// Semaphores for double buffering
static SemaphoreHandle_t empty_sem;
static SemaphoreHandle_t full_sem;

// Queue for log messages
static QueueHandle_t log_queue;

// LED configuration
#define LED_PIN 48
#define NUM_LEDS 1
static led_strip_handle_t led_strip;

// LED States
typedef enum {
    LED_IDLE,
    LED_CONNECTING_WIFI,
    LED_WIFI_CONNECTED,
    LED_WIFI_FAILED,
    LED_ACQUIRING_IMAGE,
    LED_IMAGE_ACQUIRE_FAILED,
    LED_IMAGE_SAVED,
    LED_WAITING_SERVER_ACK,
    LED_SERVER_ACK_RECEIVED,
    LED_SENDING_IMAGE,
    LED_IMAGE_SENT_SUCCESS,
    LED_IMAGE_SENT_FAILED
} LedState;
static LedState currentLedState = LED_IDLE;

static bool wifi_connected = false;
static char mac_address[18];

static const char *WIFI_EVENT_TAG = "wifi_event";
static const char *SERVER_LOG = "SERVER_LOG";
static const char *CAPTURE_TASK = "CAPTURE_TASK";
static const char *SEND_IMG_TASK = "SEND_TASK";

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        wifi_connected = false;
        currentLedState = LED_CONNECTING_WIFI;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        currentLedState = LED_WIFI_CONNECTED;
        ESP_LOGI(WIFI_EVENT_TAG, "Connected to WiFi");
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_EVENT_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
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

    // Initialize default station as network interface instance (esp-netif)
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // Initialize and start WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .scan_method = DEFAULT_SCAN_METHOD,
            .sort_method = DEFAULT_SORT_METHOD,
            .threshold.rssi = DEFAULT_RSSI,
            .threshold.authmode = DEFAULT_AUTHMODE,
            .threshold.rssi_5g_adjustment = DEFAULT_RSSI_5G_ADJUSTMENT,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    vTaskDelay(100 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(40));

    currentLedState = LED_CONNECTING_WIFI;
}

// Get MAC address as string
static void get_mac_address(void) {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(mac_address, sizeof(mac_address), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Log to server
static void log_to_server(const char* message) {
    esp_http_client_config_t config = {
        .url = SERVER_URL "/log",
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "X-Device-ID", mac_address);
    esp_http_client_set_header(client, "Content-Type", "text/plain");
    esp_http_client_set_post_field(client, message, strlen(message));
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(SERVER_LOG, "Log failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

// Log message task
static void log_send_task(void* pvParameters) {
    char* msg;
    while (true) {
        if (xQueueReceive(log_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (wifi_connected) {
                log_to_server(msg);
            }
            free(msg);
        }
    }
}


// Log message function
static void log_message(const char* message) {
    // ESP_LOGI(SERVER_LOG, "%s", message);
    if (log_queue != NULL) {
        char* msg = strdup(message);
        if (msg && xQueueSend(log_queue, &msg, 0) != pdTRUE) {
            free(msg);
        }
    }
}

static const char 
    *cap_fail = "Camera capture failed",
    *format_fail = "Non-JPEG data not supported",
    *storage_fail = "Image too large for buffer",
    *cap_success = "Image captured successfully";

// Capture image from camera
static bool capture_image(uint8_t* buffer, size_t max_size, size_t* out_size) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(CAPTURE_TASK, "%s", cap_fail);
        log_message(cap_fail);
        return false;
    }
    if (fb->format != PIXFORMAT_JPEG) {
        ESP_LOGE(CAPTURE_TASK, "%s", format_fail);
        log_message(format_fail);
        esp_camera_fb_return(fb);
        return false;
    }
    if (fb->len > max_size) {
        ESP_LOGE(CAPTURE_TASK, "%s", storage_fail);
        log_message(storage_fail);
        esp_camera_fb_return(fb);
        return false;
    }
    memcpy(buffer, fb->buf, fb->len);
    *out_size = fb->len;
    esp_camera_fb_return(fb);
    ESP_LOGI(CAPTURE_TASK, "%s", cap_success);
    log_message(cap_success);
    return true;
}


static const char 
    *img_cap_init = "Starting to acquire image",
    *img_cap_done = "Image saved to PSRAM",
    *img_cap_fail = "Image capture failure, SEND_IMAGE semaphore will not be released!";

// Image capture task
static void get_image_task(void* pvParameters) {
    while (true) {
        xSemaphoreTake(empty_sem, portMAX_DELAY);
        int buffer_to_capture = next_capture;
        next_capture = (next_capture + 1) % 2;

        currentLedState = LED_ACQUIRING_IMAGE;
        ESP_LOGI(CAPTURE_TASK, "%s", img_cap_init);
        log_message(img_cap_init);

        if (capture_image(buffers[buffer_to_capture], IMAGE_SIZE, &image_sizes[buffer_to_capture])) {
            currentLedState = LED_IMAGE_SAVED;

            ESP_LOGI(CAPTURE_TASK, "%s", img_cap_done);
            log_message(img_cap_done);
        } else {
            currentLedState = LED_IMAGE_ACQUIRE_FAILED;

            ESP_LOGE(CAPTURE_TASK, "%s", img_cap_fail);
            log_message(img_cap_fail);

            vTaskDelay(1000 / portTICK_PERIOD_MS);
            xSemaphoreGive(empty_sem);
            continue;
        }
        xSemaphoreGive(full_sem);
    }
}


static const char 
    *ack_rdy = "Server acknowledged: ready",
    *ack_fail = "Server ack failed";

// Get server acknowledgment
static bool get_server_ack(void) {
    esp_http_client_config_t config = {
        .url = SERVER_URL "/request_send",
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "X-Device-ID", mac_address);
    bool success = false;
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {

        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(SEND_IMG_TASK, "HTTP status: %d", status_code);
        char response[16];
        int len = esp_http_client_read_response(client, response, sizeof(response) - 1);
        response[len] = '\0';
        ESP_LOGI(SEND_IMG_TASK, "Response: %s", response);
        if (status_code == 200 && strcmp(response, "ready") == 0) {
        ESP_LOGI(SEND_IMG_TASK, "%s", ack_rdy);
        log_message(ack_rdy);
        success = true;
        }
    }
    else {
        ESP_LOGE(SEND_IMG_TASK, "HTTP error: %s", esp_err_to_name(err));
        ESP_LOGE(SEND_IMG_TASK, "%s", ack_fail);
        log_message(ack_fail);
    }

    // if (err == ESP_OK) {
    // int status_code = esp_http_client_get_status_code(client);
    // ESP_LOGI(SEND_IMG_TASK, "HTTP status: %d", status_code);
    // char response[16];
    // int len = esp_http_client_read_response(client, response, sizeof(response) - 1);
    // response[len] = '\0';
    // ESP_LOGI(SEND_IMG_TASK, "Response: %s", response);
    // if (status_code == 200 && strcmp(response, "ready") == 0) {
    //     ESP_LOGI(SEND_IMG_TASK, "%s", ack_rdy);
    //     log_message(ack_rdy);
    //     success = true;
    // }



    esp_http_client_cleanup(client);
    return success;
}


static const char 
    *img_sent = "Image sent successfully",
    *server_send_fail = "Failed to send image";

// Send image to server
static bool send_image_to_server(uint8_t* buffer, size_t size) {
    esp_http_client_config_t config = {
        .url = SERVER_URL "/send_image",
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "X-Device-ID", mac_address);
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (const char*)buffer, size);
    esp_err_t err = esp_http_client_perform(client);
    bool success = (err == ESP_OK && esp_http_client_get_status_code(client) == 200);
    if (success) {
        ESP_LOGI(SEND_IMG_TASK, "%s", img_sent);
        log_message(img_sent);
    } else {
        ESP_LOGE(SEND_IMG_TASK, "%s", server_send_fail);
        log_message(server_send_fail);
    }
    esp_http_client_cleanup(client);
    return success;
}


static const char 
    *req_ack = "Requesting server acknowledgment",
    *sending_image = "Sending image",
    *send_fail = "Sending Image to server failed!";

// Image send task
static void image_send_task(void* pvParameters) {
    while (true) {
        xSemaphoreTake(full_sem, portMAX_DELAY);
        int buffer_to_send = next_send;
        next_send = (next_send + 1) % 2;

        currentLedState = LED_WAITING_SERVER_ACK;
        ESP_LOGI(SEND_IMG_TASK, "%s", req_ack);
        log_message(req_ack);

        // if (get_server_ack()) {
        if (true) {
            currentLedState = LED_SERVER_ACK_RECEIVED;
            currentLedState = LED_SENDING_IMAGE;
            ESP_LOGI(SEND_IMG_TASK, "%s", sending_image);
            log_message(sending_image);
            if (send_image_to_server(buffers[buffer_to_send], image_sizes[buffer_to_send])) {
                currentLedState = LED_IMAGE_SENT_SUCCESS;
            } else {
                currentLedState = LED_IMAGE_SENT_FAILED;

                ESP_LOGE(SEND_IMG_TASK, "%s", send_fail);

            }
        } else {
            currentLedState = LED_IMAGE_SENT_FAILED;
        }
        xSemaphoreGive(empty_sem);
    }
}

// LED update task
static void led_update_task(void* pvParameters) {
    bool blink_state = false;
    while (true) {
        blink_state = !blink_state;
        switch (currentLedState) {
            case LED_IDLE:
                led_strip_set_pixel(led_strip, 0, 0, 0, 0); // Black
                break;
            case LED_CONNECTING_WIFI:
                led_strip_set_pixel(led_strip, 0, blink_state ? 255 : 0, 255, 0); // Yellow
                break;
            case LED_WIFI_CONNECTED:
                led_strip_set_pixel(led_strip, 0, 0, 255, 0); // Green
                break;
            case LED_WIFI_FAILED:
                led_strip_set_pixel(led_strip, 0, 255, 0, 0); // Red
                break;
            case LED_ACQUIRING_IMAGE:
                led_strip_set_pixel(led_strip, 0, 0, 0, 255); // Blue
                break;
            case LED_IMAGE_ACQUIRE_FAILED:
                led_strip_set_pixel(led_strip, 0, blink_state ? 255 : 0, 0, 0); // Red blink
                break;
            case LED_IMAGE_SAVED:
                led_strip_set_pixel(led_strip, 0, 0, 255, 255); // Cyan
                break;
            case LED_WAITING_SERVER_ACK:
                led_strip_set_pixel(led_strip, 0, 255, 0, 255); // Magenta
                break;
            case LED_SERVER_ACK_RECEIVED:
                led_strip_set_pixel(led_strip, 0, 255, 255, 255); // White
                break;
            case LED_SENDING_IMAGE:
                led_strip_set_pixel(led_strip, 0, 255, 165, 0); // Orange
                break;
            case LED_IMAGE_SENT_SUCCESS:
                led_strip_set_pixel(led_strip, 0, 0, 255, 0); // Green
                break;
            case LED_IMAGE_SENT_FAILED:
                led_strip_set_pixel(led_strip, 0, blink_state ? 255 : 0, 0, 0); // Red blink
                break;
        }
        led_strip_refresh(led_strip);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

// Main application entry point
void app_main(void) {
    // Initialize log queue
    log_queue = xQueueCreate(10, sizeof(char*));
    if (!log_queue) {
        ESP_LOGE("APP", "Failed to create log queue");
        return;
    }
    xTaskCreate(log_send_task, "log_send_task", 4096, NULL, 1, NULL);

    // Initialize WiFi
    wifi_init();
    get_mac_address();

    // Wait for WiFi connection with timeout
    const uint32_t timeout_ms = 10000; 
    uint32_t start_time = esp_timer_get_time() / 1000;
    while (!wifi_connected) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        if (current_time - start_time >= timeout_ms) {
            ESP_LOGE("APP", "WiFi connection timeout");
            currentLedState = LED_WIFI_FAILED;
            esp_restart();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Wait 100ms
    }

    // WiFi connected, proceed with the rest of the setup
    ESP_LOGI("APP", "WiFi connected, proceeding with initialization");

    // Allocate PSRAM buffers
    buffers[0] = (uint8_t*)heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_SPIRAM);
    buffers[1] = (uint8_t*)heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_SPIRAM);
    if (!buffers[0] || !buffers[1]) {
        ESP_LOGE("APP", "Failed to allocate PSRAM buffers");
        return;
    }

    // Initialize camera
    camera_config_t config = {

        // .pin_pwdn = -1,
        // .pin_reset = -1,
        // .pin_xclk = 15,
        // .pin_sccb_sda = 4,
        // .pin_sccb_scl = 5,
        // .pin_d7 = 16,
        // .pin_d6 = 17,
        // .pin_d5 = 18,
        // .pin_d4 = 12,
        // .pin_d3 = 10,
        // .pin_d2 = 8,
        // .pin_d1 = 9,
        // .pin_d0 = 11,
        // .pin_vsync = 6,
        // .pin_href = 7,
        // .pin_pclk = 13,

        // swappaed VSYNC and SCL
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
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_HD,
        .jpeg_quality = 5,
        .fb_count = 1,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Camera init failed: %s", esp_err_to_name(err));
        return;
    }
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_vflip(sensor, 1);
        log_message("Vertical flip enabled");
    }
    log_message("Camera initialized successfully");

    // Initialize LED strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN,
        .max_leds = NUM_LEDS,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);

    // Create semaphores
    empty_sem = xSemaphoreCreateCounting(2, 2);
    full_sem = xSemaphoreCreateCounting(2, 0);
    if (!empty_sem || !full_sem) {
        ESP_LOGE("APP", "Failed to create semaphores");
        return;
    }

    // Create tasks
    xTaskCreate(get_image_task, "get_image_task", 4096, NULL, 1, NULL);
    xTaskCreate(image_send_task, "image_send_task", 4096, NULL, 1, NULL);
    xTaskCreate(led_update_task, "led_update_task", 2048, NULL, 1, NULL);
}