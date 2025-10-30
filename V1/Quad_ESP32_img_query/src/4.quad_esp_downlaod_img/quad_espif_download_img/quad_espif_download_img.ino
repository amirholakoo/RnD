#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#include <FastLED.h>
#include <nvs_flash.h>

// Logging tag
static const char *TAG = "ESP32_PROJECT";

// WiFi credentials
#define SSID "VPN"
#define PASSWORD "09126141426"

// Server and image URLs
#define SERVER_URL "http://192.168.1.194:5000"
#define IMAGE_URL "https://fastly.picsum.photos/id/682/800/600.jpg?hmac=nDvj6j28PV7_q1jWXRsp0xS7jtAZYzHophmak9J1ymU"

// Image buffer
#define IMAGE_SIZE (1024 * 1024) // 1MB
uint8_t *image_buffer;
size_t image_size = 0;

// Semaphores
SemaphoreHandle_t image_ready_sem;
SemaphoreHandle_t sending_complete_sem;
SemaphoreHandle_t wifi_connected_sem;

// Task handles
TaskHandle_t getImageTaskHandle = NULL;
TaskHandle_t imageSendTaskHandle = NULL;
TaskHandle_t ledUpdateTaskHandle = NULL;

// LED definitions
#define NUM_LEDS 1
#define DATA_PIN 48
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define BRIGHTNESS 50
CRGB leds[NUM_LEDS];

// LED states
enum LedState {
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
};
LedState currentLedState = LED_IDLE;

// Device ID (MAC address)
char device_id[18]; // Format: XX:XX:XX:XX:XX:XX

// Function prototypes
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void connect_wifi(void);
bool download_image(uint8_t *buffer, size_t max_size, size_t *out_size);
bool get_server_ack(void);
bool send_image_to_server(void);
void get_image_task(void *pvParameters);
void image_send_task(void *pvParameters);
void led_update_task(void *pvParameters);
void update_led(void);

void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create WiFi connected semaphore
    wifi_connected_sem = xSemaphoreCreateBinary();
    if (wifi_connected_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create wifi_connected_sem");
        return;
    }

    // Initialize WiFi
    connect_wifi();

    // Get MAC address for device ID
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_STA, mac));
    snprintf(device_id, sizeof(device_id), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device ID (MAC): %s", device_id);

    // Wait for WiFi connection
    if (xSemaphoreTake(wifi_connected_sem, pdMS_TO_TICKS(30000)) != pdTRUE) {
        ESP_LOGE(TAG, "WiFi connection timeout");
        currentLedState = LED_WIFI_FAILED;
        return;
    }

    // Allocate image buffer in PSRAM
    image_buffer = (uint8_t *)heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_SPIRAM);
    if (image_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffer");
        currentLedState = LED_WIFI_FAILED;
        return;
    }
    ESP_LOGI(TAG, "PSRAM buffer allocated: %d bytes", IMAGE_SIZE);

    // Initialize FastLED
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    ESP_LOGI(TAG, "FastLED initialized");

    // Create semaphores
    image_ready_sem = xSemaphoreCreateBinary();
    sending_complete_sem = xSemaphoreCreateBinary();
    if (image_ready_sem == NULL || sending_complete_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphores");
        heap_caps_free(image_buffer);
        return;
    }

    // Create tasks
    xTaskCreate(led_update_task, "LED_UPDATE_TASK", 2048, NULL, 1, &ledUpdateTaskHandle);
    xTaskCreate(get_image_task, "GET_IMAGE_TASK", 4096, NULL, 1, &getImageTaskHandle);
    xTaskCreate(image_send_task, "IMAGE_SEND_TASK", 4096, NULL, 1, &imageSendTaskHandle);
    ESP_LOGI(TAG, "Tasks created");
}

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi STA started, attempting to connect");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        currentLedState = LED_CONNECTING_WIFI;
        ESP_LOGW(TAG, "WiFi disconnected, retrying");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        currentLedState = LED_WIFI_CONNECTED;
        xSemaphoreGive(wifi_connected_sem);
    }
}

void connect_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    currentLedState = LED_CONNECTING_WIFI;
    ESP_LOGI(TAG, "WiFi initialization complete");
}

bool download_image(uint8_t *buffer, size_t max_size, size_t *out_size) {
    esp_http_client_config_t config = {
        .url = IMAGE_URL,
        .timeout_ms = 10000, // 10 seconds timeout
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for download");
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            int64_t content_length = esp_http_client_get_content_length(client);
            if (content_length <= 0 || content_length > max_size) {
                ESP_LOGE(TAG, "Invalid or excessive content length: %lld", content_length);
                esp_http_client_cleanup(client);
                return false;
            }
            int read_len = esp_http_client_read(client, (char *)buffer, content_length);
            if (read_len == content_length) {
                *out_size = content_length;
                ESP_LOGI(TAG, "Image downloaded: %d bytes", read_len);
                esp_http_client_cleanup(client);
                return true;
            } else {
                ESP_LOGE(TAG, "Failed to read full image data: %d of %lld", read_len, content_length);
            }
        } else {
            ESP_LOGE(TAG, "HTTP GET failed, status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET error: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return false;
}

bool get_server_ack(void) {
    char url[128];
    snprintf(url, sizeof(url), "%s/request_send", SERVER_URL);
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for ack");
        return false;
    }

    esp_http_client_set_header(client, "X-Device-ID", device_id);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            char response[10];
            int read_len = esp_http_client_read(client, response, sizeof(response) - 1);
            if (read_len > 0) {
                response[read_len] = '\0';
                if (strcmp(response, "ready") == 0) {
                    ESP_LOGI(TAG, "Server acknowledged: ready");
                    esp_http_client_cleanup(client);
                    return true;
                }
            }
        } else {
            ESP_LOGE(TAG, "Server ack HTTP status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "Server ack error: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return false;
}

bool send_image_to_server(void) {
    char url[128];
    snprintf(url, sizeof(url), "%s/send_image", SERVER_URL);
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for send");
        return false;
    }

    esp_http_client_set_header(client, "X-Device-ID", device_id);
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (const char *)image_buffer, image_size);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            ESP_LOGI(TAG, "Image sent successfully");
            esp_http_client_cleanup(client);
            return true;
        } else {
            ESP_LOGE(TAG, "HTTP POST failed, status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST error: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return false;
}

void get_image_task(void *pvParameters) {
    while (true) {
        currentLedState = LED_ACQUIRING_IMAGE;
        bool success = download_image(image_buffer, IMAGE_SIZE, &image_size);
        if (success) {
            currentLedState = LED_IMAGE_SAVED;
            ESP_LOGI(TAG, "Image acquired and saved to PSRAM");
            xSemaphoreGive(image_ready_sem);
            xSemaphoreTake(sending_complete_sem, portMAX_DELAY);
        } else {
            currentLedState = LED_IMAGE_ACQUIRE_FAILED;
            ESP_LOGE(TAG, "Image acquisition failed; retrying in 1s");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void image_send_task(void *pvParameters) {
    while (true) {
        xSemaphoreTake(image_ready_sem, portMAX_DELAY);
        currentLedState = LED_WAITING_SERVER_ACK;
        if (get_server_ack()) {
            currentLedState = LED_SERVER_ACK_RECEIVED;
            vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay to show state
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

void led_update_task(void *pvParameters) {
    while (true) {
        update_led();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void update_led(void) {
    static bool blinkState = false;
    blinkState = !blinkState;
    switch (currentLedState) {
        case LED_IDLE:
            leds[0] = CRGB::Black;
            break;
        case LED_CONNECTING_WIFI:
            leds[0] = blinkState ? CRGB::Yellow : CRGB::Black;
            break;
        case LED_WIFI_CONNECTED:
            leds[0] = CRGB::Green;
            break;
        case LED_WIFI_FAILED:
            leds[0] = CRGB::Red;
            break;
        case LED_ACQUIRING_IMAGE:
            leds[0] = CRGB::Blue;
            break;
        case LED_IMAGE_ACQUIRE_FAILED:
            leds[0] = blinkState ? CRGB::Red : CRGB::Black;
            break;
        case LED_IMAGE_SAVED:
            leds[0] = CRGB::Cyan;
            break;
        case LED_WAITING_SERVER_ACK:
            leds[0] = CRGB::Magenta;
            break;
        case LED_SERVER_ACK_RECEIVED:
            leds[0] = CRGB::White;
            break;
        case LED_SENDING_IMAGE:
            leds[0] = CRGB::Orange;
            break;
        case LED_IMAGE_SENT_SUCCESS:
            leds[0] = CRGB::Green;
            break;
        case LED_IMAGE_SENT_FAILED:
            leds[0] = blinkState ? CRGB::Red : CRGB::Black;
            break;
    }
    FastLED.show();
}