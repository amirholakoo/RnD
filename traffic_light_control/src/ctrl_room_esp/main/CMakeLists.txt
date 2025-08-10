idf_component_register(SRCS "ctrl_room_esp.c" "lib/keypad.c"
                      INCLUDE_DIRS "." "lib"
                      REQUIRES "nvs_flash" "esp_wifi" "esp_driver_gpio" "led_strip")