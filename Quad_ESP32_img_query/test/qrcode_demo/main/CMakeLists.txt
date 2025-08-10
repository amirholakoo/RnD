idf_component_register(SRCS "qrcode_demo_main_wo_sending.c" 
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_timer vfs fatfs classifier
                       REQUIRES esp_wifi
                       REQUIRES nvs_flash
                       REQUIRES esp_http_client)

target_compile_options(${COMPONENT_LIB} PRIVATE -O3)

idf_component_get_property(quirc_lib_name espressif__quirc COMPONENT_LIB)
target_compile_options(${quirc_lib_name} PRIVATE -O3)
