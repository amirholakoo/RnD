#include "keypad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const gpio_num_t rows[] = {KEYPAD_ROW_1, KEYPAD_ROW_2, KEYPAD_ROW_3, KEYPAD_ROW_4};
static const gpio_num_t cols[] = {KEYPAD_COL_1, KEYPAD_COL_2, KEYPAD_COL_3};

void keypad_init(void) {
    // Configure rows as outputs
    for (int i = 0; i < 4; i++) {
        gpio_set_direction(rows[i], GPIO_MODE_OUTPUT);
        gpio_set_level(rows[i], 1); // Set high initially
    }
    // Configure columns as inputs with pull-ups
    for (int i = 0; i < 3; i++) {
        gpio_set_direction(cols[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(cols[i], GPIO_PULLUP_ONLY);
    }
}

char keypad_get_key(void) {
    for (int row = 0; row < 4; row++) {
        // Set current row low
        gpio_set_level(rows[row], 0);
        // Check each column
        for (int col = 0; col < 3; col++) {
            if (gpio_get_level(cols[col]) == 0) {
                // Wait for key release (simple debouncing)
                while (gpio_get_level(cols[col]) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                // Set row back to high
                gpio_set_level(rows[row], 1);
                return keypad_layout[row][col];
            }
        }
        // Set row back to high
        gpio_set_level(rows[row], 1);
    }
    return 0; // No key pressed
}