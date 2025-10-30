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
#include "driver/gpio.h"
#include "led_strip.h"
#include "driver/spi_master.h"

#define RED_RELAY_PIN   GPIO_NUM_11
#define GREEN_RELAY_PIN GPIO_NUM_13
#define NVS_NAMESPACE   "relay_state"
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
static const char *TAG = "WEIGHBRIDGE";

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

// Command types
typedef enum {
    CMD_BLANK,
    CMD_TOGGLE_RED,
    CMD_TOGGLE_GREEN,
    CMD_TOGGLE_LIGHTS,
    CMD_GET_STATE
} command_t;

// Control Room ESP32 MAC address
static uint8_t control_room_mac[6] = {0xb4, 0x3a, 0x45, 0x3f, 0x3c, 0xf4};

// Relay states
bool red_on = false;
bool green_on = false;

// Matrix LED configuration
#define NUM_MODULES 4
#define PIN_MOSI 9  
#define PIN_CLK  12 
#define PIN_CS   10 
static spi_device_handle_t spi;

// Font data for 'S', 'T', 'O', 'P', 'M', 'V', 'E' (5x8, column-major)
const uint8_t font[7][5] = {
    // 'S' (index 0)
    {0b11110001, 0b10010001, 0b10010001, 0b10010001, 0b10011111},
    // 'T' (index 1)
    {0b10000000, 0b10000000, 0b11111111, 0b10000000, 0b10000000},
    // 'O' (index 2)
    {0b11111111, 0b10000001, 0b10000001, 0b10000001, 0b11111111},
    // 'P' (index 3)
    {0b11111111, 0b10010000, 0b10010000, 0b10010000, 0b01100000},
    // 'M' (index 4)
    {0b11111111, 0b00100000, 0b00010000, 0b00100000, 0b11111111},
    // 'V' (index 5)
    {0b11100000, 0b00011100, 0b00000011, 0b00011100, 0b11100000},
    // 'E' (index 6)
    {0b11111111, 0b10010001, 0b10010001, 0b10010001, 0b10010001}
};

// Display states
typedef enum {
    DISPLAY_OFF,
    DISPLAY_STOP,
    DISPLAY_MOVE
} display_state_t;
static display_state_t current_display = DISPLAY_OFF;

// Function prototypes
void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
void send_ack(void);
void load_state(void);
void save_state(void);
void get_mac_address(void);
void max7219_send(uint8_t reg, uint8_t data);
void max7219_send_row(uint8_t row, const uint8_t data[4]);
void max7219_init(void);
void update_matrix_display(void);


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
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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
    xTaskCreate(led_update_task, "led_update_task", 2048, NULL, 1, NULL);

    // Load previous state
    load_state();

    currentLedState = LED_CONNECTING_WIFI;

    // Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Get and log MAC address
    get_mac_address();

    if (red_on && green_on) currentLedState = LED_REG_GREEN_LIGHT;
    else if (!red_on && green_on) currentLedState = LED_GREEN_LIGHT;
    else if (red_on && !green_on) currentLedState = LED_RED_LIGHT;
    else currentLedState = LED_WIFI_FAILED;

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    // Add Control Room ESP32 as peer
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, control_room_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    // Initialize relay GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RED_RELAY_PIN) | (1ULL << GREEN_RELAY_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Initialize matrix LED
    max7219_init();

    // Set initial relay states and update display
    gpio_set_level(RED_RELAY_PIN, red_on);
    gpio_set_level(GREEN_RELAY_PIN, green_on);
    update_matrix_display();
}

void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == 1) {
        command_t cmd = (command_t)data[0];
        ESP_LOGI(TAG, "Received command: %d", cmd);
        switch (cmd) {
            case CMD_TOGGLE_RED:
                red_on = !red_on;
                gpio_set_level(RED_RELAY_PIN, red_on);
                save_state();
                send_ack();
                break;
            case CMD_TOGGLE_GREEN:
                green_on = !green_on;
                gpio_set_level(GREEN_RELAY_PIN, green_on);
                save_state();
                send_ack();
                break;
            case CMD_TOGGLE_LIGHTS:
                if (red_on && !green_on) {
                    red_on = false;
                    green_on = true;
                } else if (!red_on && green_on) {
                    red_on = true;
                    green_on = false;
                } else {
                    red_on = true;
                    green_on = false; // Default to red if both off or both on
                }
                gpio_set_level(RED_RELAY_PIN, red_on);
                gpio_set_level(GREEN_RELAY_PIN, green_on);
                save_state();
                send_ack();
                break;
            case CMD_GET_STATE:
                send_ack();
                break;
            default:
                ESP_LOGW(TAG, "Unknown command: %d", cmd);
                break;
        }

        if (red_on && green_on) currentLedState = LED_REG_GREEN_LIGHT;
        else if (!red_on && green_on) currentLedState = LED_GREEN_LIGHT;
        else if (red_on && !green_on) currentLedState = LED_RED_LIGHT;
        else currentLedState = LED_WIFI_FAILED;

        // Update matrix display after state change
        update_matrix_display();
    }
}

void send_ack(void) {
    uint8_t state_data[2] = {red_on, green_on};
    uint8_t try_again = 0;
    while (try_again < 2) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        esp_err_t err = esp_now_send(control_room_mac, state_data, 2);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Ack send failed: %s", esp_err_to_name(err));
        }
        try_again++;
    }
}

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "Ack send failed to " MACSTR, MAC2STR(mac_addr));
    }
}

void load_state(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        uint8_t state[2];
        size_t size = sizeof(state);
        err = nvs_get_blob(nvs_handle, "state", state, &size);
        if (err == ESP_OK) {
            red_on = state[0];
            green_on = state[1];
            ESP_LOGI(TAG, "Loaded state - Red: %d, Green: %d", red_on, green_on);
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGW(TAG, "No previous state found");
    }
}

void save_state(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        uint8_t state[2] = {red_on, green_on};
        err = nvs_set_blob(nvs_handle, "state", state, sizeof(state));
        if (err == ESP_OK) {
            nvs_commit(nvs_handle);
        }
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Saved state - Red: %d, Green: %d", red_on, green_on);
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

// Matrix LED functions
void max7219_send(uint8_t reg, uint8_t data) {
    uint8_t tx[NUM_MODULES * 2];
    for (int i = 0; i < NUM_MODULES; i++) {
        tx[2 * i] = reg;
        tx[2 * i + 1] = data;
    }
    spi_transaction_t t = {
        .length = NUM_MODULES * 16,  // bits
        .tx_buffer = tx,
    };
    esp_err_t ret = spi_device_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_device_transmit failed: %d", ret);
    }
}

void max7219_send_row(uint8_t row, const uint8_t data[4]) {
    uint8_t tx[8]; // 4 modules * 2 bytes
    for (int i = 0; i < 4; i++) {
        tx[2 * i] = row;
        tx[2 * i + 1] = data[3 - i]; // Data for module 3, 2, 1, 0
    }
    spi_transaction_t t = {
        .length = 4 * 16,  // bits
        .tx_buffer = tx,
    };
    esp_err_t ret = spi_device_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_device_transmit failed: %d", ret);
    }
}

void max7219_init(void) {
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = NUM_MODULES * 2
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000, // 2 MHz
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi));

    max7219_send(0x0F, 0x00);  // Display test OFF
    max7219_send(0x09, 0x00);  // Decode mode OFF
    max7219_send(0x0B, 0x07);  // Scan limit = all 8 rows
    max7219_send(0x0C, 0x01);  // Shutdown = normal operation
    max7219_send(0x0A, 0x08);  // Brightness = mid (0–15)

    for (uint8_t row = 1; row <= 8; row++) {
        uint8_t data[4] = {0, 0, 0, 0};
        max7219_send_row(row, data);
    }
}

void update_matrix_display(void) {
    display_state_t desired_display;
    if (red_on && !green_on) {
        desired_display = DISPLAY_STOP;
    } else if (!red_on && green_on) {
        desired_display = DISPLAY_MOVE;
    } else {
        desired_display = DISPLAY_OFF;
    }

    if (desired_display != current_display) {
        current_display = desired_display;
        if (desired_display == DISPLAY_OFF) {
            for (uint8_t row = 1; row <= 8; row++) {
                uint8_t data[4] = {0, 0, 0, 0};
                max7219_send_row(row, data);
            }
        } else {
            uint8_t text[4];
            if (desired_display == DISPLAY_STOP) {
                text[0] = 0; // 'S'
                text[1] = 1; // 'T'
                text[2] = 2; // 'O'
                text[3] = 3; // 'P'
            } else { // DISPLAY_MOVE
                text[0] = 4; // 'M'
                text[1] = 2; // 'O'
                text[2] = 5; // 'V'
                text[3] = 6; // 'E'
            }
            for (uint8_t row = 1; row <= 8; row++) {
                uint8_t data[4];
                for (int m = 0; m < 4; m++) {
                    data[m] = 0;
                    for (int col = 0; col < 5; col++) {
                        uint8_t char_index = text[m];
                        uint8_t bit_value = (font[char_index][col] >> (row - 1)) & 1;
                        data[m] |= (bit_value << col);
                    }
                }
                max7219_send_row(row, data);
            }
        }
    }
}