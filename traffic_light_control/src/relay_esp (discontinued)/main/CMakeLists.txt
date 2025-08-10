idf_component_register(SRCS "relay_esp_main.c"
                    PRIV_REQUIRES spi_flash
                    INCLUDE_DIRS ""
                    REQUIRES "nvs_flash" "esp_wifi" "esp_driver_gpio" "led_strip" "esp_driver_spi")
