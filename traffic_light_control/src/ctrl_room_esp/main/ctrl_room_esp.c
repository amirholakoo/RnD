#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "lib/keypad.h"
#include "led_strip.h"

#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
static const char *TAG = "CONTROL_ROOM";

// LED configuration
#define LED_PIN 48
#define NUM_LEDS 1
static led_strip_handle_t led_strip;

// LED States
typedef enum {
    LED_IDLE,
    LED_GREEN_LIGHT,
    LED_RED_LIGHT,
    LED_REG_GREEN_LIGHT,
    LED_CONNECTING_WIFI,
    LED_WIFI_FAILED
} LedState;
static LedState currentLedState = LED_IDLE;

// MAC addresses
static uint8_t weighbridge_mac[6] = {0xb4, 0x3a, 0x45, 0x3f, 0xcb, 0xd8};
static uint8_t light_control_mac_1[6] = {0xb4, 0x3a, 0x45, 0x3f, 0x1a, 0xf4};
static uint8_t light_control_mac_2[6] = {0xb4, 0x3a, 0x45, 0x3f, 0x18, 0xa0};

// Command types
typedef enum {
    CMD_BLANK,
    CMD_TOGGLE_RED,        // For relay_esp
    CMD_TOGGLE_GREEN,      // For relay_esp
    CMD_TOGGLE_LIGHTS,     // For relay_esp
    CMD_GET_STATE,         // For relay_esp
    CMD_TOGGLE_TO_GREEN,   // For light_control_esp
    CMD_TOGGLE_TO_RED      // For light_control_esp
} command_t;

const char *commands_name[] = {
    "CMD_BLANK",
    "CMD_TOGGLE_RED",
    "CMD_TOGGLE_GREEN",
    "CMD_TOGGLE_LIGHTS",
    "CMD_GET_STATE",
    "CMD_TOGGLE_TO_GREEN",
    "CMD_TOGGLE_TO_RED"
};

// Relay states (for relay_esp)
typedef struct {
    bool red_on;
    bool green_on;
} relay_state_t;

relay_state_t current_state = {false, false};

// Pending command structure for retries
typedef struct {
    command_t cmd;
    int retry_count;
    TimerHandle_t timer;
    const uint8_t *target_mac;
} pending_command_t;

pending_command_t pending_relay_cmd = {0};
pending_command_t pending_light_cmd[2] = {0};

// Auto-red timer and acknowledgment flags
#define GREEN_TIMEOUT 25000
static TimerHandle_t auto_red_timer;
static bool green_ack_1 = false;
static bool green_ack_2 = false;

// Function prototypes
void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
void keypad_task(void *pvParameter);
void send_command(command_t cmd, const uint8_t *mac_addr, pending_command_t *pending);
void timer_callback(TimerHandle_t xTimer);
void auto_red_timer_callback(TimerHandle_t xTimer);
void get_mac_address(void);
static void led_update_task(void* pvParameters);

// LED update task
static void led_update_task(void* pvParameters) {
    bool blink_state = false;
    while (true) {
        blink_state = !blink_state;
        switch (currentLedState) {
            case LED_IDLE:
                led_strip_set_pixel(led_strip, 0, 128, 128, 128); // Black
                break;
            case LED_CONNECTING_WIFI:
                led_strip_set_pixel(led_strip, 0, 0, 0, blink_state ? 128 : 0); // Yellow
                break;
            case LED_GREEN_LIGHT:
                led_strip_set_pixel(led_strip, 0, 0, 128, 0); // Green
                break;
            case LED_WIFI_FAILED:
                led_strip_set_pixel(led_strip, 0, blink_state ? 128 : 0, 0, blink_state ? 128 : 0); // Red
                break;
            case LED_RED_LIGHT:
                led_strip_set_pixel(led_strip, 0, 128, 0, 0); // Red
                break;
            case LED_REG_GREEN_LIGHT:
                led_strip_set_pixel(led_strip, 0, 128, 128, 0); // Yellow
                break;
        }
        led_strip_refresh(led_strip);
        taskYIELD();
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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
    xTaskCreate(led_update_task, "led_update_task", 2048, NULL, 1, NULL);

    currentLedState = LED_CONNECTING_WIFI;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(60));

    get_mac_address();

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    esp_now_peer_info_t peer_info = {};
    peer_info.channel = 0;
    peer_info.encrypt = false;

    memcpy(peer_info.peer_addr, weighbridge_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    memcpy(peer_info.peer_addr, light_control_mac_1, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    memcpy(peer_info.peer_addr, light_control_mac_2, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    // Initialize auto-red timer
    auto_red_timer = xTimerCreate("auto_red_timer", pdMS_TO_TICKS(GREEN_TIMEOUT), pdFALSE, NULL, auto_red_timer_callback);

    currentLedState = LED_IDLE;

    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_command(CMD_GET_STATE, weighbridge_mac, &pending_relay_cmd);

    xTaskCreate(keypad_task, "keypad_task", 4 * 2048, NULL, 5, NULL);
}

void keypad_task(void *pvParameter) {
    keypad_init();
    while (1) {
        char key = keypad_get_key();
        if (key != 0) {
            ESP_LOGI(TAG, "Key pressed: %c", key);
            switch (key) {
                case '*':
                    ESP_LOGI(TAG, "Executing command: %s", commands_name[CMD_TOGGLE_RED]);
                    send_command(CMD_TOGGLE_RED, weighbridge_mac, &pending_relay_cmd);
                    break;
                case '0':
                    ESP_LOGI(TAG, "Executing command: %s", commands_name[CMD_TOGGLE_GREEN]);
                    send_command(CMD_TOGGLE_GREEN, weighbridge_mac, &pending_relay_cmd);
                    break;
                case '#':
                    ESP_LOGI(TAG, "Executing command: %s", commands_name[CMD_TOGGLE_LIGHTS]);
                    send_command(CMD_TOGGLE_LIGHTS, weighbridge_mac, &pending_relay_cmd);
                    break;
                case '1':
                    ESP_LOGI(TAG, "Executing command: %s", commands_name[CMD_TOGGLE_TO_GREEN]);
                    send_command(CMD_TOGGLE_TO_GREEN, light_control_mac_1, &pending_light_cmd[0]);
                    send_command(CMD_TOGGLE_TO_GREEN, light_control_mac_2, &pending_light_cmd[1]);
                    break;
                case '2':
                    if (xTimerIsTimerActive(auto_red_timer)) {
                        xTimerStop(auto_red_timer, 0);
                    }
                    ESP_LOGI(TAG, "Executing command: %s", commands_name[CMD_TOGGLE_TO_RED]);
                    send_command(CMD_TOGGLE_TO_RED, light_control_mac_1, &pending_light_cmd[0]);
                    send_command(CMD_TOGGLE_TO_RED, light_control_mac_2, &pending_light_cmd[1]);
                    break;
                default:
                    ESP_LOGI(TAG, "No command was received");
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void send_command(command_t cmd, const uint8_t *mac_addr, pending_command_t *pending) {
    if (pending->cmd != 0) {
        ESP_LOGW(TAG, "Command pending for this target, ignoring new command");
        return;
    }
    pending->cmd = cmd;
    pending->retry_count = 0;
    pending->target_mac = mac_addr;
    uint8_t data[1] = {cmd};
    esp_err_t err = esp_now_send(mac_addr, data, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(err));
    }
    pending->timer = xTimerCreate("cmd_timer", pdMS_TO_TICKS(1000), pdFALSE, pending, timer_callback);
    xTimerStart(pending->timer, 0);
}

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "Send failed to " MACSTR, MAC2STR(mac_addr));
    }
}

void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (memcmp(recv_info->src_addr, weighbridge_mac, 6) == 0) {
        if (len == 2 && pending_relay_cmd.cmd != 0) {
            current_state.red_on = data[0];
            current_state.green_on = data[1];
            ESP_LOGI(TAG, "State updated - Red: %d, Green: %d", current_state.red_on, current_state.green_on);
            if (current_state.red_on && current_state.green_on)
                currentLedState = LED_REG_GREEN_LIGHT;
            else if (!current_state.red_on && current_state.green_on)
                currentLedState = LED_GREEN_LIGHT;
            else if (current_state.red_on && !current_state.green_on)
                currentLedState = LED_RED_LIGHT;
            else
                currentLedState = LED_WIFI_FAILED;
            xTimerStop(pending_relay_cmd.timer, 0);
            xTimerDelete(pending_relay_cmd.timer, 0);
            pending_relay_cmd.cmd = 0;
        }
    } else if (memcmp(recv_info->src_addr, light_control_mac_1, 6) == 0) {
        if (len == 1 && data[0] == 0xAA && pending_light_cmd[0].cmd != 0) {
            ESP_LOGI(TAG, "Acknowledgment received from light_control_esp_1");
            if (pending_light_cmd[0].cmd == CMD_TOGGLE_TO_GREEN) {
                green_ack_1 = true;
            }
            xTimerStop(pending_light_cmd[0].timer, 0);
            xTimerDelete(pending_light_cmd[0].timer, 0);
            pending_light_cmd[0].cmd = 0;
            xTimerReset(auto_red_timer, 0);
            // if (green_ack_1 && green_ack_2) {
            //     xTimerReset(auto_red_timer, 0);
            //     green_ack_1 = false;
            //     green_ack_2 = false;
            // }
        }
    } else if (memcmp(recv_info->src_addr, light_control_mac_2, 6) == 0) {
        if (len == 1 && data[0] == 0xAA && pending_light_cmd[1].cmd != 0) {
            ESP_LOGI(TAG, "Acknowledgment received from light_control_esp_2");
            if (pending_light_cmd[1].cmd == CMD_TOGGLE_TO_GREEN) {
                green_ack_2 = true;
            }
            xTimerStop(pending_light_cmd[1].timer, 0);
            xTimerDelete(pending_light_cmd[1].timer, 0);
            pending_light_cmd[1].cmd = 0;
            xTimerReset(auto_red_timer, 0);
            // if (green_ack_1 && green_ack_2) {
            //     xTimerReset(auto_red_timer, 0);
            //     green_ack_1 = false;
            //     green_ack_2 = false;
            // }
        }
    } else {
        ESP_LOGW(TAG, "Received packet from unknown MAC");
    }
}

void timer_callback(TimerHandle_t xTimer) {
    pending_command_t *pending = (pending_command_t *)pvTimerGetTimerID(xTimer);
    if (pending->retry_count < 3) {
        pending->retry_count++;
        ESP_LOGI(TAG, "Retrying command %d, attempt %d", pending->cmd, pending->retry_count);
        uint8_t data[1] = {pending->cmd};
        esp_now_send(pending->target_mac, data, 1);
        xTimerStart(pending->timer, 0);
    } else {
        ESP_LOGE(TAG, "Command %d failed after 3 retries", pending->cmd);
        pending->cmd = 0;
    }
}

void auto_red_timer_callback(TimerHandle_t xTimer) {
    if (pending_light_cmd[0].cmd == 0) {
        send_command(CMD_TOGGLE_TO_RED, light_control_mac_1, &pending_light_cmd[0]);
    } else {
        ESP_LOGW(TAG, "Pending command for light_control_esp_1, cannot send auto red");
    }
    if (pending_light_cmd[1].cmd == 0) {
        send_command(CMD_TOGGLE_TO_RED, light_control_mac_2, &pending_light_cmd[1]);
    } else {
        ESP_LOGW(TAG, "Pending command for light_control_esp_2, cannot send auto red");
    }
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