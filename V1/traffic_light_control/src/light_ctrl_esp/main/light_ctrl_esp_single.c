#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <esp_now.h>
#include <esp_mac.h>

#include "esp_wifi.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

// GPIO pin definitions (replace with actual pin numbers)
#define RED_LIGHT_PIN    GPIO_NUM_7
#define YELLOW_LIGHT_PIN GPIO_NUM_5
#define GREEN_LIGHT_PIN  GPIO_NUM_6
#define BUZZER_PIN       GPIO_NUM_4

// Configurable parameters
#define TRANSITION_DURATION_MS 3000
#define BLINK_PERIOD_MS 500

static const char *TAG = "LIGHT_CONTROL";

// Control Room ESP32 MAC address
static uint8_t control_room_mac[6] = {0xb4, 0x3a, 0x45, 0x3f, 0x3c, 0xf4};

// State definitions
typedef enum {
    STATE_RED,
    STATE_GREEN,
    STATE_TRANSITION
} light_state_t;

light_state_t current_state = STATE_RED;

// Command types (must match ctrl_room_esp.c)
typedef enum {
    CMD_BLANK,
    CMD_TOGGLE_RED,
    CMD_TOGGLE_GREEN,
    CMD_TOGGLE_LIGHTS,
    CMD_GET_STATE,
    CMD_TOGGLE_TO_GREEN,
    CMD_TOGGLE_TO_RED
} command_t;

// Global variables
TimerHandle_t blink_timer;
TaskHandle_t transition_task_handle = NULL;

// Function prototypes
void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
void send_ack(void);
void transition_task(void *pvParameters);
void blink_timer_callback(TimerHandle_t xTimer);
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

    // Get and log MAC address
    get_mac_address();

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    // Add Control Room ESP32 as peer
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, control_room_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));


    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RED_LIGHT_PIN) | (1ULL << YELLOW_LIGHT_PIN) | 
                        (1ULL << GREEN_LIGHT_PIN) | (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));


    ESP_ERROR_CHECK(gpio_set_level(RED_LIGHT_PIN, 1));
    ESP_ERROR_CHECK(gpio_set_level(GREEN_LIGHT_PIN, 0));
    ESP_ERROR_CHECK(gpio_set_level(YELLOW_LIGHT_PIN, 0));
    ESP_ERROR_CHECK(gpio_set_level(BUZZER_PIN, 0));

}

void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == 1) {
        command_t cmd = (command_t)data[0];
        ESP_LOGI(TAG, "Received command: %d", cmd);
        if (current_state != STATE_TRANSITION) {
            if (cmd == CMD_TOGGLE_TO_GREEN && current_state != STATE_GREEN) {
                current_state = STATE_TRANSITION;
                xTaskCreate(transition_task, "transition_task", 2048, (void *)STATE_GREEN, 5, &transition_task_handle);
            } else if (cmd == CMD_TOGGLE_TO_RED && current_state != STATE_RED) {
                current_state = STATE_TRANSITION;
                xTaskCreate(transition_task, "transition_task", 2048, (void *)STATE_RED, 5, &transition_task_handle);
            }
        } else {
            ESP_LOGW(TAG, "Transition in progress, ignoring command");
        }
        send_ack();
    }
}

void send_ack(void) {
    uint8_t ack_data[1] = {0xAA};
    esp_err_t err = esp_now_send(control_room_mac, ack_data, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ack send failed: %s", esp_err_to_name(err));
    }
}

void transition_task(void *pvParameters) {
    light_state_t target_state = (light_state_t)pvParameters;

    // Turn off current lights
    gpio_set_level(RED_LIGHT_PIN, 0);
    gpio_set_level(GREEN_LIGHT_PIN, 0);

    // Start blinking yellow and buzzer
    gpio_set_level(BUZZER_PIN, 1);
    gpio_set_level(YELLOW_LIGHT_PIN, 1);
    // blink_timer = xTimerCreate("blink_timer", pdMS_TO_TICKS(BLINK_PERIOD_MS), pdTRUE, NULL, blink_timer_callback);
    // xTimerStart(blink_timer, 0);

    // Wait for transition duration
    vTaskDelay(pdMS_TO_TICKS(TRANSITION_DURATION_MS));

    // Stop blinking and buzzer
    // xTimerStop(blink_timer, 0);
    // xTimerDelete(blink_timer, 0);
    gpio_set_level(YELLOW_LIGHT_PIN, 0);
    gpio_set_level(BUZZER_PIN, 0);

    // Set target state
    if (target_state == STATE_RED) {
        gpio_set_level(RED_LIGHT_PIN, 1);
    } else if (target_state == STATE_GREEN) {
        gpio_set_level(GREEN_LIGHT_PIN, 1);
    }
    current_state = target_state;

    vTaskDelete(NULL);
}

void blink_timer_callback(TimerHandle_t xTimer) {
    static bool yellow_on = false;
    yellow_on = !yellow_on;
    gpio_set_level(YELLOW_LIGHT_PIN, yellow_on);
}

void get_mac_address(void) {
    uint8_t mac_addr[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac_addr);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MAC Address: " MACSTR, MAC2STR(mac_addr));
    } else {
        ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
    }
}