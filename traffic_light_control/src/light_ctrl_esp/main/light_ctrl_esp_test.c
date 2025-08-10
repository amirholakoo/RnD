#include <stdio.h>
#include <string.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#define pin1  GPIO_NUM_4
#define pin2  GPIO_NUM_5
#define pin3  GPIO_NUM_6
#define pin4  GPIO_NUM_7
static const char *TAG = "GPIO_TEST";

void app_main(void) {

    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin1) | (1ULL << pin2)
         | (1ULL << pin3) | (1ULL << pin4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(err));
        return;
    }

    // Set GPIO high
    for(int i = 0; i<4; i++){
        gpio_set_drive_capability(i+4, GPIO_DRIVE_CAP_3);
        err = gpio_set_level(i+4, 1);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set GPIO level: %s", esp_err_to_name(err));
            return;
        }
    }
    

    // Keep the program running
    while (1) {
        for(int i = 0; i<4; i++){
            err = gpio_set_level(i+4, 1);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set GPIO level: %s", esp_err_to_name(err));
                return;
            }
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);

        for(int i = 0; i<4; i++){
            err = gpio_set_level(i+4, 0);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set GPIO level: %s", esp_err_to_name(err));
                return;
            }
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}