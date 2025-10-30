/**
 * @file data_sender.h
 * @brief Simple data sender for HTTP JSON transmission
 */

#ifndef DATA_SENDER_H
#define DATA_SENDER_H

#include "esp_err.h"
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function prototypes */
esp_err_t data_sender_init(const char* server_url, const char* auth_token);
esp_err_t data_sender_send_sensor_data(const char* device_id, float temp, float humidity);
esp_err_t data_sender_send_dht22_data(const char* device_id, const char* sensor_type, float temperature, float humidity, uint64_t timestamp);
esp_err_t data_sender_send_status(const char* device_id, const char* status);
void data_sender_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* DATA_SENDER_H */ 