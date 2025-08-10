#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_now.h>
#include <esp_mac.h>
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

// Select which unit you are building for. Override with -DTARGET_UNIT=X (1 or 2)
#ifndef TARGET_UNIT
    #define TARGET_UNIT 1
#endif

// Device-specific pin configurations
#if TARGET_UNIT == 1
    #define RED_LIGHT_PIN    GPIO_NUM_7
    #define YELLOW_LIGHT_PIN GPIO_NUM_5
    #define GREEN_LIGHT_PIN  GPIO_NUM_6
    #define BUZZER_PIN       GPIO_NUM_4
#elif TARGET_UNIT == 2
    // Example pin mapping for second unit; change to your wiring
    #define RED_LIGHT_PIN    GPIO_NUM_10
    #define YELLOW_LIGHT_PIN GPIO_NUM_11
    #define GREEN_LIGHT_PIN  GPIO_NUM_12
    #define BUZZER_PIN       GPIO_NUM_13
#else
    #error "Unsupported TARGET_UNIT value. Must be 1 or 2."
#endif

// Configurable parameters
#define TRANSITION_DURATION_MS 6000
#define BLINK_PERIOD_MS       500

static const char *TAG = "LIGHT_CONTROL_UNIT";

// Control Room ESP32 MAC address (same for both units)
static uint8_t control_room_mac[6] = { 0xb4, 0x3a, 0x45, 0x3f, 0x3c, 0xf4 };

// State definitions
typedef enum {
    STATE_RED,
    STATE_GREEN,
    STATE_TRANSITION
} light_state_t;

static light_state_t current_state = STATE_RED;

// Command types (must match control_room_esp)
typedef enum {
    CMD_BLANK,
    CMD_TOGGLE_RED,
    CMD_TOGGLE_GREEN,
    CMD_TOGGLE_LIGHTS,
    CMD_GET_STATE,
    CMD_TOGGLE_TO_GREEN,
    CMD_TOGGLE_TO_RED
} command_t;

// Globals
static TaskHandle_t transition_task_handle = NULL;

// Prototypes
void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
void send_ack(void);
void transition_task(void *pvParameters);
void get_mac_address(void);

void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Log own MAC
    get_mac_address();

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    // Add Control Room as peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, control_room_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    // Configure GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RED_LIGHT_PIN) |
                        (1ULL << YELLOW_LIGHT_PIN) |
                        (1ULL << GREEN_LIGHT_PIN) |
                        (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Initialize lights off (red on by default)
    gpio_set_level(RED_LIGHT_PIN, 1);
    gpio_set_level(GREEN_LIGHT_PIN, 0);
    gpio_set_level(YELLOW_LIGHT_PIN, 0);
    gpio_set_level(BUZZER_PIN, 0);
}

void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len < 1) return;
    command_t cmd = (command_t)data[0];
    ESP_LOGI(TAG, "Received cmd %d", cmd);
    if (current_state != STATE_TRANSITION) {
        if (cmd == CMD_TOGGLE_TO_GREEN && current_state != STATE_GREEN) {
            current_state = STATE_TRANSITION;
            xTaskCreate(transition_task, "transition", 2048, (void *)STATE_GREEN, 5, &transition_task_handle);
        } else if (cmd == CMD_TOGGLE_TO_RED && current_state != STATE_RED) {
            current_state = STATE_TRANSITION;
            xTaskCreate(transition_task, "transition", 2048, (void *)STATE_RED, 5, &transition_task_handle);
        }
    } else {
        ESP_LOGW(TAG, "Transition in progress");
    }
    send_ack();
}

void send_ack(void) {
    uint8_t ack = 0xAA;
    esp_now_send(control_room_mac, &ack, 1);
}

void transition_task(void *pvParameters) {
    light_state_t target = (light_state_t)pvParameters;
    // Turn off current
    gpio_set_level(RED_LIGHT_PIN, 0);
    gpio_set_level(GREEN_LIGHT_PIN, 0);
    // Flash yellow + buzzer
    gpio_set_level(YELLOW_LIGHT_PIN, 1);
    gpio_set_level(BUZZER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(TRANSITION_DURATION_MS));
    gpio_set_level(YELLOW_LIGHT_PIN, 0);
    gpio_set_level(BUZZER_PIN, 0);
    // Set new state
    if (target == STATE_RED) {
        gpio_set_level(RED_LIGHT_PIN, 1);
    } else {
        gpio_set_level(GREEN_LIGHT_PIN, 1);
    }
    current_state = target;
    vTaskDelete(NULL);
}

void get_mac_address(void) {
    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        ESP_LOGI(TAG, "Unit %d MAC: %02x:%02x:%02x:%02x:%02x:%02x", \
                 TARGET_UNIT, MAC2STR(mac));
    }
}
