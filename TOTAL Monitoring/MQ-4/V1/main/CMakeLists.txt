idf_component_register(SRCS   wifi_framework.c
                              wifi_framework_example.c
                              http_client.c
                              data_sender.c
                              dht.c
                              mq4_sensor.c
                       INCLUDE_DIRS "."
                       REQUIRES esp_http_client 
                                json
                                esp_wifi
                                nvs_flash
                                esp_timer
                                bt
                                driver
                                esp_adc)
