/**
 * @file http_client.h
 * @brief Simple and robust HTTP client for sending JSON data
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "esp_err.h"
#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration structure */
typedef struct {
    char url[256];
    char auth_header[128];
    int timeout_ms;
    bool verify_ssl;
} http_client_config_t;

/* Response structure */
typedef struct {
    int status_code;
    char* response_data;
    size_t response_len;
} http_response_t;

/* Function prototypes */
esp_err_t http_client_init(const http_client_config_t* config);
esp_err_t http_client_send_json(const char* json_data, http_response_t* response);
void http_client_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_CLIENT_H */ 