idf_component_register(SRCS   wifi_framework.c
                              wifi_framework_example.c
                              http_client.c
                              data_sender.c
                       INCLUDE_DIRS "."
                       REQUIRES esp_http_client 
                                esp32-dht
                                json
                                esp_wifi
                                nvs_flash
                                esp_timer)
