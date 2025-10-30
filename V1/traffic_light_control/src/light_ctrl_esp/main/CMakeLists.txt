idf_component_register(SRCS "light_ctrl_esp.c"
                    PRIV_REQUIRES spi_flash
                    INCLUDE_DIRS ""
                    REQUIRES "nvs_flash" "esp_wifi" "esp_driver_gpio"
                    )
