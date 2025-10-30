/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_timer.h"

#include "nvs_flash.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

/* ESP-DSP includes for FFT processing */
#include "esp_dsp.h"
#include "dsps_fft2r.h"
#include "dsps_wind.h"
#include "dsps_view.h"
#include "dsps_dotprod.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "adxl345_driver.h"

/* ============================================================================
 * CONFIGURATION SECTION - MODIFY THESE VALUES AS NEEDED
 * ============================================================================ */

/* ADXL345 Communication Interface Configuration */
#define ADXL345_USE_SPI         1       /* Set to 1 for SPI, 0 for I2C */

#if ADXL345_USE_SPI
    #define ADXL345_INTERFACE       ADXL345_INTERFACE_SPI
    #define ADXL345_SPI_CS_PIN      5    /* GPIO 5 for CS */
    #define ADXL345_SPI_SCLK_PIN    18   /* GPIO 18 for SCLK */
    #define ADXL345_SPI_MOSI_PIN    23   /* GPIO 23 for MOSI */
    #define ADXL345_SPI_MISO_PIN    19   /* GPIO 19 for MISO */
    #define ADXL345_SPI_SPEED       5000000  /* 5 MHz SPI clock */
#else
    #define ADXL345_INTERFACE       ADXL345_INTERFACE_I2C
    #define ADXL345_I2C_SDA_PIN     21   /* GPIO 21 for SDA */
    #define ADXL345_I2C_SCL_PIN     22   /* GPIO 22 for SCL */
    #define ADXL345_I2C_SPEED       400000   /* 400 kHz I2C clock */
    #define ADXL345_I2C_ADDR        ADXL345_I2C_ADDR_ALT_LOW  /* 0x53 */
#endif

/* ADXL345 Sensor Configuration */
#define ADXL345_RANGE           ADXL345_RANGE_16G    /* +/- 16g range */
#define ADXL345_DATARATE        ADXL345_DATARATE_3200HZ  /* 3200 Hz - Maximum */
#define ADXL345_FULL_RESOLUTION 1       /* 1 = enabled, 0 = disabled */
#define ADXL345_USE_FIFO        1       /* 1 = enabled, 0 = disabled */

/* FFT Configuration */
#define FFT_SIZE                1024    /* FFT size (must be power of 2) */
#define SAMPLE_BUFFER_SIZE      2048    /* Sample buffer size */
#define FFT_OVERLAP_PERCENT     50      /* Overlap percentage (0-75) */
#define FFT_OVERLAP_SAMPLES     ((FFT_SIZE * FFT_OVERLAP_PERCENT) / 100U)

/* FFT Window Configuration */
#define FFT_WINDOW_HANN         1       /* 1 = Hann window, 0 = no window */
#define FFT_WINDOW_HAMMING      0       /* 1 = Hamming window */
#define FFT_WINDOW_BLACKMAN     0       /* 1 = Blackman window */

/* FFT Output Configuration */
#define FFT_DC_REMOVE           1       /* 1 = remove DC component, 0 = keep */
#define FFT_OUTPUT_MAGNITUDE    1       /* 1 = output magnitude, 0 = disable */
#define FFT_OUTPUT_PHASE        0       /* 1 = output phase, 0 = disable */
#define FFT_LOG_SCALE           1       /* 1 = dB scale, 0 = linear scale */

/* WiFi Configuration */
#define WIFI_SSID               "your_wifi_ssid"     /* Your WiFi network name */
#define WIFI_PASSWORD           "your_wifi_password" /* Your WiFi password */
#define WIFI_MAXIMUM_RETRY      5       /* Maximum WiFi connection retries */

/* HTTP Server Configuration */
#define HTTP_SERVER_URL         "http://192.168.2.34:8003/api/data/"  /* Update this */
#define HTTP_TIMEOUT_MS         10000
#define HTTP_MAX_DATA_SIZE      8192
#define DATA_SEND_INTERVAL_MS   1000

/* Task priorities and stack sizes */
#define SENSOR_TASK_PRIORITY    (5U)
#define FFT_TASK_PRIORITY       (4U)
#define WIFI_TASK_PRIORITY      (3U)
#define SENSOR_TASK_STACK_SIZE  (4096U)
#define FFT_TASK_STACK_SIZE     (8192U)
#define WIFI_TASK_STACK_SIZE    (4096U)

/* Timing constants */
#define SAMPLE_PERIOD_US        (1000000U / 3200U)  /* 3200 Hz sampling */
#define MAX_RECONNECT_ATTEMPTS  (5U)

static const char *TAG = "ADXL345_FFT";

/* Device structures */
typedef struct {
    adxl345_handle_t handle;
    bool initialized;
    adxl345_range_t range;
    bool full_resolution;
    uint32_t sample_rate_hz;
} adxl345_device_t;

typedef struct {
    float *real_buffer;
    float *imag_buffer;
    float *magnitude;
    float *phase;
    float *window;
    uint16_t size;
    bool magnitude_valid;
    bool phase_valid;
} fft_result_t;

typedef struct {
    adxl345_accel_data_t *data_buffer;
    uint16_t buffer_size;
    uint16_t write_index;
    uint16_t read_index;
    bool buffer_full;
    SemaphoreHandle_t mutex;
} sample_buffer_t;

typedef struct {
    float sample_rate;
    uint32_t timestamp_ms;
    uint16_t fft_size;
    float freq_resolution;
    float *magnitude_x;
    float *magnitude_y;
    float *magnitude_z;
    float *phase_x;
    float *phase_y;
    float *phase_z;
} fft_data_packet_t;

/* Global variables */
static adxl345_device_t g_adxl345_dev = {0};
static sample_buffer_t g_sample_buffer = {0};
static fft_result_t g_fft_result_x = {0};
static fft_result_t g_fft_result_y = {0};
static fft_result_t g_fft_result_z = {0};

static QueueHandle_t g_fft_queue = NULL;
static QueueHandle_t g_wifi_queue = NULL;
static SemaphoreHandle_t g_wifi_ready_semaphore = NULL;

static esp_timer_handle_t g_sample_timer = NULL;
static bool g_system_running = false;

/* WiFi event handler */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                              int32_t event_id, void* event_data)
{
    static int retry_num = 0;
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            ESP_LOGE(TAG, "connect to the AP fail");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xSemaphoreGive(g_wifi_ready_semaphore);
    }
}

/* Initialize WiFi */
static esp_err_t wifi_init_sta(void)
{
    esp_err_t ret = ESP_OK;
    
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_event_handler,
                                            NULL,
                                            &instance_any_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            &wifi_event_handler,
                                            NULL,
                                            &instance_got_ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi initialization completed");
    return ESP_OK;
}

/* Initialize sample buffer */
static esp_err_t sample_buffer_init(sample_buffer_t *buffer, uint16_t size)
{
    if (buffer == NULL || size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    buffer->data_buffer = calloc(size, sizeof(adxl345_accel_data_t));
    if (buffer->data_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate sample buffer");
        return ESP_ERR_NO_MEM;
    }

    buffer->buffer_size = size;
    buffer->write_index = 0U;
    buffer->read_index = 0U;
    buffer->buffer_full = false;

    buffer->mutex = xSemaphoreCreateMutex();
    if (buffer->mutex == NULL) {
        free(buffer->data_buffer);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/* Add sample to buffer */
static esp_err_t sample_buffer_add(sample_buffer_t *buffer, const adxl345_accel_data_t *sample)
{
    if (buffer == NULL || sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(buffer->mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    buffer->data_buffer[buffer->write_index] = *sample;
    buffer->write_index = (buffer->write_index + 1U) % buffer->buffer_size;

    if (buffer->write_index == buffer->read_index) {
        buffer->buffer_full = true;
        buffer->read_index = (buffer->read_index + 1U) % buffer->buffer_size;
    }

    xSemaphoreGive(buffer->mutex);
    return ESP_OK;
}

/* Get samples from buffer */
static esp_err_t sample_buffer_get(sample_buffer_t *buffer, adxl345_accel_data_t *samples, 
                                  uint16_t count, uint16_t *actual_count)
{
    if (buffer == NULL || samples == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(buffer->mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint16_t available = 0U;
    if (buffer->buffer_full) {
        available = buffer->buffer_size;
    } else if (buffer->write_index >= buffer->read_index) {
        available = buffer->write_index - buffer->read_index;
    } else {
        available = buffer->buffer_size - buffer->read_index + buffer->write_index;
    }

    uint16_t to_read = MIN(count, available);
    *actual_count = to_read;

    for (uint16_t i = 0U; i < to_read; i++) {
        samples[i] = buffer->data_buffer[buffer->read_index];
        buffer->read_index = (buffer->read_index + 1U) % buffer->buffer_size;
    }

    if (to_read > 0U) {
        buffer->buffer_full = false;
    }

    xSemaphoreGive(buffer->mutex);
    return ESP_OK;
}

/* Initialize FFT result structure */
static esp_err_t fft_result_init(fft_result_t *result, uint16_t size)
{
    if (result == NULL || size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    result->real_buffer = calloc(size, sizeof(float));
    result->imag_buffer = calloc(size, sizeof(float));
    result->window = calloc(size, sizeof(float));
    
    if (result->real_buffer == NULL || result->imag_buffer == NULL || result->window == NULL) {
        ESP_LOGE(TAG, "Failed to allocate FFT buffers");
        free(result->real_buffer);
        free(result->imag_buffer);
        free(result->window);
        return ESP_ERR_NO_MEM;
    }

#if FFT_OUTPUT_MAGNITUDE
    result->magnitude = calloc(size / 2U, sizeof(float));
    if (result->magnitude == NULL) {
        ESP_LOGE(TAG, "Failed to allocate magnitude buffer");
        free(result->real_buffer);
        free(result->imag_buffer);
        free(result->window);
        return ESP_ERR_NO_MEM;
    }
#endif

#if FFT_OUTPUT_PHASE
    result->phase = calloc(size / 2U, sizeof(float));
    if (result->phase == NULL) {
        ESP_LOGE(TAG, "Failed to allocate phase buffer");
        free(result->real_buffer);
        free(result->imag_buffer);
        free(result->window);
        free(result->magnitude);
        return ESP_ERR_NO_MEM;
    }
#endif

    result->size = size;
    result->magnitude_valid = false;
    result->phase_valid = false;

    /* Initialize window function */
#if FFT_WINDOW_HANN
    dsps_wind_hann_f32(result->window, size);
#elif FFT_WINDOW_HAMMING
    dsps_wind_hamming_f32(result->window, size);
#elif FFT_WINDOW_BLACKMAN
    dsps_wind_blackman_f32(result->window, size);
#else
    /* No window - fill with ones */
    for (uint16_t i = 0U; i < size; i++) {
        result->window[i] = 1.0f;
    }
#endif

    ESP_LOGI(TAG, "FFT result initialized with size %u", size);
    return ESP_OK;
}

/* Apply windowing and remove DC component */
static void apply_preprocessing(float *data, const float *window, uint16_t size)
{
    if (data == NULL || window == NULL || size == 0U) {
        return;
    }

#if FFT_DC_REMOVE
    /* Calculate and remove DC component */
    float dc_sum = 0.0f;
    for (uint16_t i = 0U; i < size; i++) {
        dc_sum += data[i];
    }
    float dc_avg = dc_sum / (float)size;
    
    for (uint16_t i = 0U; i < size; i++) {
        data[i] -= dc_avg;
    }
#endif

    /* Apply windowing function */
    for (uint16_t i = 0U; i < size; i++) {
        data[i] *= window[i];
    }
}

/* Perform FFT on single axis data */
static esp_err_t perform_fft(fft_result_t *result, const float *input_data)
{
    if (result == NULL || input_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Copy input data to real buffer */
    memcpy(result->real_buffer, input_data, result->size * sizeof(float));
    
    /* Clear imaginary buffer */
    memset(result->imag_buffer, 0, result->size * sizeof(float));

    /* Apply preprocessing */
    apply_preprocessing(result->real_buffer, result->window, result->size);

    /* Prepare complex array for FFT (interleaved real and imaginary) */
    float *fft_buffer = calloc(result->size * 2U, sizeof(float));
    if (fft_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate FFT buffer");
        return ESP_ERR_NO_MEM;
    }

    for (uint16_t i = 0U; i < result->size; i++) {
        fft_buffer[i * 2U] = result->real_buffer[i];      /* Real part */
        fft_buffer[i * 2U + 1U] = 0.0f;                   /* Imaginary part */
    }

    /* Perform FFT */
    esp_err_t ret = dsps_fft2r_fc32_ansi(fft_buffer, result->size, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FFT computation failed: %s", esp_err_to_name(ret));
        free(fft_buffer);
        return ret;
    }

    /* Bit reverse the result */
    ret = dsps_bit_rev_fc32_ansi(fft_buffer, result->size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bit reverse failed: %s", esp_err_to_name(ret));
        free(fft_buffer);
        return ret;
    }

    /* Extract real and imaginary parts */
    for (uint16_t i = 0U; i < result->size; i++) {
        result->real_buffer[i] = fft_buffer[i * 2U];
        result->imag_buffer[i] = fft_buffer[i * 2U + 1U];
    }

    /* Calculate magnitude spectrum */
#if FFT_OUTPUT_MAGNITUDE
    if (result->magnitude != NULL) {
        for (uint16_t i = 0U; i < (result->size / 2U); i++) {
            float real = result->real_buffer[i];
            float imag = result->imag_buffer[i];
            result->magnitude[i] = sqrtf(real * real + imag * imag);
            
#if FFT_LOG_SCALE
            /* Convert to dB scale */
            if (result->magnitude[i] > 0.0f) {
                result->magnitude[i] = 20.0f * log10f(result->magnitude[i]);
            } else {
                result->magnitude[i] = -INFINITY;
            }
#endif
        }
        result->magnitude_valid = true;
    }
#endif

    /* Calculate phase spectrum */
#if FFT_OUTPUT_PHASE
    if (result->phase != NULL) {
        for (uint16_t i = 0U; i < (result->size / 2U); i++) {
            result->phase[i] = atan2f(result->imag_buffer[i], result->real_buffer[i]);
        }
        result->phase_valid = true;
    }
#endif

    free(fft_buffer);
    ESP_LOGD(TAG, "FFT computation completed successfully");
    return ESP_OK;
}

/* Convert acceleration data to float and scale */
static void convert_accel_to_float(const adxl345_accel_data_t *accel_data, 
                                  float *x_data, float *y_data, float *z_data, 
                                  uint16_t count)
{
    if (accel_data == NULL || x_data == NULL || y_data == NULL || z_data == NULL) {
        return;
    }

    for (uint16_t i = 0U; i < count; i++) {
        x_data[i] = adxl345_convert_to_mg(accel_data[i].x, g_adxl345_dev.range, g_adxl345_dev.full_resolution);
        y_data[i] = adxl345_convert_to_mg(accel_data[i].y, g_adxl345_dev.range, g_adxl345_dev.full_resolution);
        z_data[i] = adxl345_convert_to_mg(accel_data[i].z, g_adxl345_dev.range, g_adxl345_dev.full_resolution);
    }
}

/* Create JSON data packet for HTTP transmission */
static char* create_json_packet(const fft_data_packet_t *packet)
{
    if (packet == NULL) {
        return NULL;
    }

    /* Calculate required buffer size */
    size_t json_size = 1024U + (packet->fft_size * 6U * 32U); /* Conservative estimate */
    char *json_buffer = calloc(json_size, sizeof(char));
    if (json_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate JSON buffer");
        return NULL;
    }

    /* Build JSON string */
    int offset = snprintf(json_buffer, json_size,
        "{"
        "\"timestamp\":%lu,"
        "\"sample_rate\":%.2f,"
        "\"fft_size\":%u,"
        "\"freq_resolution\":%.4f",
        packet->timestamp_ms,
        packet->sample_rate,
        packet->fft_size,
        packet->freq_resolution);

    /* Add magnitude data */
#if FFT_OUTPUT_MAGNITUDE
    if (packet->magnitude_x != NULL) {
        offset += snprintf(json_buffer + offset, json_size - offset, ",\"magnitude_x\":[");
        for (uint16_t i = 0U; i < (packet->fft_size / 2U); i++) {
            offset += snprintf(json_buffer + offset, json_size - offset, 
                             "%s%.3f", (i > 0U) ? "," : "", packet->magnitude_x[i]);
        }
        offset += snprintf(json_buffer + offset, json_size - offset, "]");
    }

    if (packet->magnitude_y != NULL) {
        offset += snprintf(json_buffer + offset, json_size - offset, ",\"magnitude_y\":[");
        for (uint16_t i = 0U; i < (packet->fft_size / 2U); i++) {
            offset += snprintf(json_buffer + offset, json_size - offset, 
                             "%s%.3f", (i > 0U) ? "," : "", packet->magnitude_y[i]);
        }
        offset += snprintf(json_buffer + offset, json_size - offset, "]");
    }

    if (packet->magnitude_z != NULL) {
        offset += snprintf(json_buffer + offset, json_size - offset, ",\"magnitude_z\":[");
        for (uint16_t i = 0U; i < (packet->fft_size / 2U); i++) {
            offset += snprintf(json_buffer + offset, json_size - offset, 
                             "%s%.3f", (i > 0U) ? "," : "", packet->magnitude_z[i]);
        }
        offset += snprintf(json_buffer + offset, json_size - offset, "]");
    }
#endif

    /* Add phase data */
#if FFT_OUTPUT_PHASE
    if (packet->phase_x != NULL) {
        offset += snprintf(json_buffer + offset, json_size - offset, ",\"phase_x\":[");
        for (uint16_t i = 0U; i < (packet->fft_size / 2U); i++) {
            offset += snprintf(json_buffer + offset, json_size - offset, 
                             "%s%.6f", (i > 0U) ? "," : "", packet->phase_x[i]);
        }
        offset += snprintf(json_buffer + offset, json_size - offset, "]");
    }

    if (packet->phase_y != NULL) {
        offset += snprintf(json_buffer + offset, json_size - offset, ",\"phase_y\":[");
        for (uint16_t i = 0U; i < (packet->fft_size / 2U); i++) {
            offset += snprintf(json_buffer + offset, json_size - offset, 
                             "%s%.6f", (i > 0U) ? "," : "", packet->phase_y[i]);
        }
        offset += snprintf(json_buffer + offset, json_size - offset, "]");
    }

    if (packet->phase_z != NULL) {
        offset += snprintf(json_buffer + offset, json_size - offset, ",\"phase_z\":[");
        for (uint16_t i = 0U; i < (packet->fft_size / 2U); i++) {
            offset += snprintf(json_buffer + offset, json_size - offset, 
                             "%s%.6f", (i > 0U) ? "," : "", packet->phase_z[i]);
        }
        offset += snprintf(json_buffer + offset, json_size - offset, "]");
    }
#endif

    snprintf(json_buffer + offset, json_size - offset, "}");

    ESP_LOGD(TAG, "JSON packet created, size: %d bytes", strlen(json_buffer));
    return json_buffer;
}

/* HTTP event handler */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* Send data via HTTP POST */
static esp_err_t send_http_data(const char *json_data)
{
    if (json_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t config = {
        .url = HTTP_SERVER_URL,
        .event_handler = http_event_handler,
        .timeout_ms = HTTP_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set HTTP method: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        return ret;
    }

    ret = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set HTTP header: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        return ret;
    }

    ret = esp_http_client_set_post_field(client, json_data, strlen(json_data));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set POST data: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        return ret;
    }

    ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                status_code, esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(ret));
    }

    esp_http_client_cleanup(client);
    return ret;
}

/* Sample timer callback */
static void IRAM_ATTR sample_timer_callback(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (g_system_running && g_adxl345_dev.initialized) {
        /* Trigger sensor reading from timer */
        /* Note: In ISR context, we need to defer actual I2C/SPI operations */
        xTaskNotifyFromISR((TaskHandle_t)arg, 0x01, eSetBits, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Sensor reading task */
static void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started");
    
    uint32_t notification_value;
    adxl345_accel_data_t sample;
    
    while (g_system_running) {
        /* Wait for timer notification */
        if (xTaskNotifyWait(0, ULONG_MAX, &notification_value, portMAX_DELAY) == pdTRUE) {
            if (g_adxl345_dev.initialized) {
                /* Read acceleration data */
                esp_err_t ret = adxl345_read_accel(g_adxl345_dev.handle, &sample);
                if (ret == ESP_OK) {
                    /* Add sample to buffer */
                    sample_buffer_add(&g_sample_buffer, &sample);
                } else {
                    ESP_LOGW(TAG, "Failed to read acceleration data: %s", esp_err_to_name(ret));
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "Sensor task ended");
    vTaskDelete(NULL);
}

/* FFT processing task */
static void fft_task(void *pvParameters)
{
    ESP_LOGI(TAG, "FFT task started");
    
    adxl345_accel_data_t *sample_chunk = calloc(FFT_SIZE, sizeof(adxl345_accel_data_t));
    float *x_data = calloc(FFT_SIZE, sizeof(float));
    float *y_data = calloc(FFT_SIZE, sizeof(float));
    float *z_data = calloc(FFT_SIZE, sizeof(float));
    
    if (sample_chunk == NULL || x_data == NULL || y_data == NULL || z_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate FFT processing buffers");
        free(sample_chunk);
        free(x_data);
        free(y_data);
        free(z_data);
        vTaskDelete(NULL);
        return;
    }
    
    while (g_system_running) {
        uint16_t samples_read = 0;
        
        /* Get samples from buffer */
        esp_err_t ret = sample_buffer_get(&g_sample_buffer, sample_chunk, FFT_SIZE, &samples_read);
        
        if (ret == ESP_OK && samples_read >= FFT_SIZE) {
            /* Convert to float arrays */
            convert_accel_to_float(sample_chunk, x_data, y_data, z_data, FFT_SIZE);
            
            /* Perform FFT on each axis */
            ret = perform_fft(&g_fft_result_x, x_data);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "FFT failed for X-axis: %s", esp_err_to_name(ret));
                continue;
            }
            
            ret = perform_fft(&g_fft_result_y, y_data);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "FFT failed for Y-axis: %s", esp_err_to_name(ret));
                continue;
            }
            
            ret = perform_fft(&g_fft_result_z, z_data);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "FFT failed for Z-axis: %s", esp_err_to_name(ret));
                continue;
            }
            
            /* Prepare data packet for transmission */
            fft_data_packet_t packet = {
                .sample_rate = (float)g_adxl345_dev.sample_rate_hz,
                .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL),
                .fft_size = FFT_SIZE,
                .freq_resolution = (float)g_adxl345_dev.sample_rate_hz / (float)FFT_SIZE,
#if FFT_OUTPUT_MAGNITUDE
                .magnitude_x = g_fft_result_x.magnitude,
                .magnitude_y = g_fft_result_y.magnitude,
                .magnitude_z = g_fft_result_z.magnitude,
#endif
#if FFT_OUTPUT_PHASE
                .phase_x = g_fft_result_x.phase,
                .phase_y = g_fft_result_y.phase,
                .phase_z = g_fft_result_z.phase,
#endif
            };
            
            /* Send to WiFi task */
            if (xQueueSend(g_wifi_queue, &packet, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Failed to queue FFT data for transmission");
            }
            
            ESP_LOGI(TAG, "FFT processing completed, queued for transmission");
        } else {
            /* Not enough samples, wait a bit */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    free(sample_chunk);
    free(x_data);
    free(y_data);
    free(z_data);
    
    ESP_LOGI(TAG, "FFT task ended");
    vTaskDelete(NULL);
}

/* WiFi data transmission task */
static void wifi_task(void *pvParameters)
{
    ESP_LOGI(TAG, "WiFi task started");
    
    fft_data_packet_t packet;
    
    /* Wait for WiFi connection */
    if (xSemaphoreTake(g_wifi_ready_semaphore, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to wait for WiFi connection");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "WiFi connected, starting data transmission");
    
    while (g_system_running) {
        /* Wait for FFT data */
        if (xQueueReceive(g_wifi_queue, &packet, portMAX_DELAY) == pdTRUE) {
            /* Create JSON packet */
            char *json_data = create_json_packet(&packet);
            if (json_data != NULL) {
                /* Send data via HTTP */
                esp_err_t ret = send_http_data(json_data);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Data sent successfully");
                } else {
                    ESP_LOGW(TAG, "Failed to send data: %s", esp_err_to_name(ret));
                }
                
                free(json_data);
            } else {
                ESP_LOGE(TAG, "Failed to create JSON packet");
            }
            
            /* Rate limiting */
            vTaskDelay(pdMS_TO_TICKS(DATA_SEND_INTERVAL_MS));
        }
    }
    
    ESP_LOGI(TAG, "WiFi task ended");
    vTaskDelete(NULL);
}

/* Initialize ADXL345 device */
static esp_err_t init_adxl345(void)
{
    ESP_LOGI(TAG, "Initializing ADXL345...");
    
    adxl345_config_t config = {
        .interface = ADXL345_INTERFACE,
        .range = ADXL345_RANGE,
        .datarate = ADXL345_DATARATE,
        .full_resolution = (ADXL345_FULL_RESOLUTION != 0),
    };

    /* Set sample rate based on datarate */
    switch (ADXL345_DATARATE) {
        case ADXL345_DATARATE_25HZ:
            g_adxl345_dev.sample_rate_hz = 25U;
            break;
        case ADXL345_DATARATE_50HZ:
            g_adxl345_dev.sample_rate_hz = 50U;
            break;
        case ADXL345_DATARATE_100HZ:
            g_adxl345_dev.sample_rate_hz = 100U;
            break;
        case ADXL345_DATARATE_200HZ:
            g_adxl345_dev.sample_rate_hz = 200U;
            break;
        case ADXL345_DATARATE_400HZ:
            g_adxl345_dev.sample_rate_hz = 400U;
            break;
        case ADXL345_DATARATE_800HZ:
            g_adxl345_dev.sample_rate_hz = 800U;
            break;
        case ADXL345_DATARATE_1600HZ:
            g_adxl345_dev.sample_rate_hz = 1600U;
            break;
        case ADXL345_DATARATE_3200HZ:
        default:
            g_adxl345_dev.sample_rate_hz = 3200U;
            break;
    }

#if ADXL345_USE_SPI
    config.comm_config.spi.host_id = SPI2_HOST;
    config.comm_config.spi.cs_gpio = ADXL345_SPI_CS_PIN;
    config.comm_config.spi.sclk_gpio = ADXL345_SPI_SCLK_PIN;
    config.comm_config.spi.mosi_gpio = ADXL345_SPI_MOSI_PIN;
    config.comm_config.spi.miso_gpio = ADXL345_SPI_MISO_PIN;
    config.comm_config.spi.clock_speed_hz = ADXL345_SPI_SPEED;
#else
    config.comm_config.i2c.port = I2C_NUM_0;
    config.comm_config.i2c.sda_gpio = ADXL345_I2C_SDA_PIN;
    config.comm_config.i2c.scl_gpio = ADXL345_I2C_SCL_PIN;
    config.comm_config.i2c.clock_speed_hz = ADXL345_I2C_SPEED;
    config.comm_config.i2c.device_address = ADXL345_I2C_ADDR;
#endif

    esp_err_t ret = adxl345_init(&config, &g_adxl345_dev.handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADXL345: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Verify device ID */
    uint8_t device_id = 0;
    ret = adxl345_read_device_id(g_adxl345_dev.handle, &device_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID: %s", esp_err_to_name(ret));
        return ret;
    }

    if (device_id != ADXL345_DEVICE_ID) {
        ESP_LOGE(TAG, "Invalid device ID: 0x%02X (expected 0x%02X)", device_id, ADXL345_DEVICE_ID);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "ADXL345 device ID verified: 0x%02X", device_id);

    /* Configure FIFO if enabled */
#if ADXL345_USE_FIFO
    ret = adxl345_configure_fifo(g_adxl345_dev.handle, ADXL345_FIFO_MODE_STREAM, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to configure FIFO: %s", esp_err_to_name(ret));
    }
#endif

    /* Start measurement */
    ret = adxl345_start_measurement(g_adxl345_dev.handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start measurement: %s", esp_err_to_name(ret));
        return ret;
    }

    g_adxl345_dev.initialized = true;
    g_adxl345_dev.range = config.range;
    g_adxl345_dev.full_resolution = config.full_resolution;

    ESP_LOGI(TAG, "ADXL345 initialized successfully");
    ESP_LOGI(TAG, "Sample rate: %lu Hz, Range: %dg, Full resolution: %s", 
             g_adxl345_dev.sample_rate_hz,
             (1 << (config.range + 1)),
             config.full_resolution ? "enabled" : "disabled");

    return ESP_OK;
}

/* Application main function */
void app_main(void)
{
    ESP_LOGI(TAG, "ADXL345 FFT Analysis System Starting...");
    
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize ESP-DSP */
    ret = dsps_fft2r_init_fc32(NULL, 4096);  /* Initialize for max 4096-point FFT */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP-DSP: %s", esp_err_to_name(ret));
        return;
    }

    /* Initialize sample buffer */
    ret = sample_buffer_init(&g_sample_buffer, SAMPLE_BUFFER_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sample buffer: %s", esp_err_to_name(ret));
        return;
    }

    /* Initialize FFT result structures */
    ret = fft_result_init(&g_fft_result_x, FFT_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize X-axis FFT result: %s", esp_err_to_name(ret));
        return;
    }

    ret = fft_result_init(&g_fft_result_y, FFT_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Y-axis FFT result: %s", esp_err_to_name(ret));
        return;
    }

    ret = fft_result_init(&g_fft_result_z, FFT_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Z-axis FFT result: %s", esp_err_to_name(ret));
        return;
    }

    /* Create queues and semaphores */
    g_fft_queue = xQueueCreate(2, sizeof(fft_data_packet_t));
    g_wifi_queue = xQueueCreate(2, sizeof(fft_data_packet_t));
    g_wifi_ready_semaphore = xSemaphoreCreateBinary();

    if (g_fft_queue == NULL || g_wifi_queue == NULL || g_wifi_ready_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create queues or semaphores");
        return;
    }

    /* Initialize ADXL345 */
    ret = init_adxl345();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADXL345 initialization failed");
        return;
    }

    /* Initialize WiFi */
    ret = wifi_init_sta();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi initialization failed");
        return;
    }

    /* Mark system as running */
    g_system_running = true;

    /* Create tasks */
    TaskHandle_t sensor_task_handle = NULL;
    
    ret = xTaskCreate(sensor_task, "sensor_task", SENSOR_TASK_STACK_SIZE, 
                     NULL, SENSOR_TASK_PRIORITY, &sensor_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return;
    }

    ret = xTaskCreate(fft_task, "fft_task", FFT_TASK_STACK_SIZE, 
                     NULL, FFT_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create FFT task");
        return;
    }

    ret = xTaskCreate(wifi_task, "wifi_task", WIFI_TASK_STACK_SIZE, 
                     NULL, WIFI_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi task");
        return;
    }

    /* Create and start sample timer */
    esp_timer_create_args_t timer_args = {
        .callback = sample_timer_callback,
        .arg = sensor_task_handle,
        .name = "sample_timer"
    };

    ret = esp_timer_create(&timer_args, &g_sample_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create sample timer: %s", esp_err_to_name(ret));
        return;
    }

    /* Start periodic timer for sampling */
    uint64_t timer_period_us = 1000000ULL / g_adxl345_dev.sample_rate_hz;
    ret = esp_timer_start_periodic(g_sample_timer, timer_period_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start sample timer: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "System initialization completed successfully");
    ESP_LOGI(TAG, "Sampling at %lu Hz with %d-point FFT", 
             g_adxl345_dev.sample_rate_hz, FFT_SIZE);
    ESP_LOGI(TAG, "Data will be sent to: %s", HTTP_SERVER_URL);

    /* Main loop - system monitoring */
    while (g_system_running) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        /* Print system status */
        ESP_LOGI(TAG, "System running - Free heap: %lu bytes", esp_get_free_heap_size());
        
        /* Monitor sample buffer usage */
        uint16_t buffer_usage = 0;
        if (g_sample_buffer.buffer_full) {
            buffer_usage = 100;
        } else if (g_sample_buffer.write_index >= g_sample_buffer.read_index) {
            buffer_usage = (100 * (g_sample_buffer.write_index - g_sample_buffer.read_index)) / g_sample_buffer.buffer_size;
        } else {
            buffer_usage = (100 * (g_sample_buffer.buffer_size - g_sample_buffer.read_index + g_sample_buffer.write_index)) / g_sample_buffer.buffer_size;
        }
        ESP_LOGI(TAG, "Sample buffer usage: %u%%", buffer_usage);
    }
}