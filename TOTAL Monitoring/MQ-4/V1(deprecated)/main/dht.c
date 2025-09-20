/**
 * @file dht.c
 * @brief Robust DHT22 Temperature and Humidity Sensor Driver for ESP32
 * 
 * This implementation is adapted from proven Arduino DHT library with
 * timing-critical sections preserved for reliable sensor communication.
 * Follows MISRA guidelines for infrastructure applications.
 * 
 * Compatible with ESP-IDF v5.4
 */

#include "dht.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "soc/gpio_reg.h"
#include "hal/gpio_hal.h"
#include "esp_clk_tree.h"

/* Logging tag */
static const char *TAG = "DHT22_DRIVER";

/* DHT22 Configuration Constants */
#define MIN_INTERVAL_MS         2000U         /**< Minimum interval between readings (ms) */
#define TIMEOUT_CYCLES          UINT32_MAX    /**< Timeout value for pulse reading */
#define DHT22_START_SIGNAL_US   1100U         /**< Start signal duration for DHT22 (us) */
#define DHT22_PULLUP_DELAY_US   55U           /**< Pull-up delay before reading (us) */
#define DHT_DATA_BITS           40U           /**< Number of data bits to read */
#define DHT_DATA_BYTES          5U            /**< Number of data bytes (40 bits / 8) */

/* Driver state structure */
typedef struct {
    gpio_num_t gpio_pin;                      /**< GPIO pin number */
    dht_type_t sensor_type;                   /**< Sensor type (DHT11/DHT22) */
    uint8_t data_buffer[DHT_DATA_BYTES];      /**< Raw data buffer */
    uint32_t max_cycles;                      /**< Maximum cycles for timeout */
    uint32_t last_read_time;                  /**< Last successful read time (ms) */
    bool last_result;                         /**< Result of last read operation */
    bool initialized;                         /**< Driver initialization status */
    float last_temperature;                   /**< Last valid temperature reading */
    float last_humidity;                      /**< Last valid humidity reading */
} dht_driver_state_t;

/* Global driver state */
static dht_driver_state_t g_dht_state = {
    .gpio_pin = GPIO_NUM_NC,
    .sensor_type = DHT_TYPE_DHT22,
    .data_buffer = {0},
    .max_cycles = 0U,
    .last_read_time = 0U,
    .last_result = false,
    .initialized = false,
    .last_temperature = NAN,
    .last_humidity = NAN
};

/* Function prototypes */
static uint32_t expect_pulse(bool level);
static bool perform_sensor_read(bool force_read);
static esp_err_t validate_parameters(float *temperature, float *humidity);

/**
 * @brief Wait for GPIO pin to reach expected level with timeout
 * 
 * @param level Expected GPIO level (0 or 1)
 * @return Number of cycles waited, or TIMEOUT_CYCLES on timeout
 */
static uint32_t expect_pulse(bool level)
{
    uint32_t count = 0U;
    
    /* Use direct GPIO level reading for timing precision */
    while (gpio_get_level(g_dht_state.gpio_pin) == (level ? 1 : 0)) {
        count++;
        if (count >= g_dht_state.max_cycles) {
            return TIMEOUT_CYCLES; /* Timeout occurred */
        }
    }
    
    return count;
}

/**
 * @brief Validate input parameters for temperature and humidity pointers
 * 
 * @param temperature Pointer to temperature storage
 * @param humidity Pointer to humidity storage
 * @return ESP_OK if valid, ESP_ERR_INVALID_ARG if invalid
 */
static esp_err_t validate_parameters(float *temperature, float *humidity)
{
    if ((temperature == NULL) || (humidity == NULL)) {
        ESP_LOGE(TAG, "Invalid parameters: temperature or humidity pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_dht_state.initialized) {
        ESP_LOGE(TAG, "DHT driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    return ESP_OK;
}

/**
 * @brief Perform the actual sensor reading with timing-critical protocol
 * 
 * @param force_read Force reading even if minimum interval hasn't elapsed
 * @return true if reading successful, false otherwise
 */
static bool perform_sensor_read(bool force_read)
{
    /* Check minimum interval between readings */
    uint32_t current_time = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (!force_read && ((current_time - g_dht_state.last_read_time) < MIN_INTERVAL_MS)) {
        return g_dht_state.last_result; /* Return last result if too soon */
    }
    g_dht_state.last_read_time = current_time;
    
    /* Clear data buffer */
    (void)memset(g_dht_state.data_buffer, 0, sizeof(g_dht_state.data_buffer));
    
    /* Initialize GPIO as input with pull-up */
    gpio_set_direction(g_dht_state.gpio_pin, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    /* Send start signal - set data line low */
    gpio_set_direction(g_dht_state.gpio_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(g_dht_state.gpio_pin, 0);
    ets_delay_us(DHT22_START_SIGNAL_US); /* 1100us for DHT22 */
    
    uint32_t cycles[DHT_DATA_BITS * 2U]; /* Store low and high pulse cycles */
    
    /* TIMING CRITICAL SECTION - NO INTERRUPTIONS */
    {
        /* End start signal by setting data line to input (pulled high) */
        gpio_set_direction(g_dht_state.gpio_pin, GPIO_MODE_INPUT);
        
        /* Brief delay to let sensor pull data line low */
        ets_delay_us(DHT22_PULLUP_DELAY_US);
        
        /* Disable interrupts for timing-critical section */
        portDISABLE_INTERRUPTS();
        
        /* Wait for sensor response - low pulse ~80us then high pulse ~80us */
        if (expect_pulse(false) == TIMEOUT_CYCLES) {
            ESP_LOGE(TAG, "Timeout waiting for start signal low pulse");
            g_dht_state.last_result = false;
            portENABLE_INTERRUPTS();
            return g_dht_state.last_result;
        }
        
        if (expect_pulse(true) == TIMEOUT_CYCLES) {
            ESP_LOGE(TAG, "Timeout waiting for start signal high pulse");
            g_dht_state.last_result = false;
            portENABLE_INTERRUPTS();
            return g_dht_state.last_result;
        }
        
        /* Read the 40 data bits */
        /* Each bit: 50us low pulse + variable high pulse (28us=0, 70us=1) */
        for (uint32_t i = 0U; i < (DHT_DATA_BITS * 2U); i += 2U) {
            cycles[i] = expect_pulse(false);      /* Low pulse */
            cycles[i + 1U] = expect_pulse(true);  /* High pulse */
        }
        
        portENABLE_INTERRUPTS();
    } /* End timing critical section */
    
    /* Process the received pulses */
    for (uint32_t i = 0U; i < DHT_DATA_BITS; i++) {
        uint32_t low_cycles = cycles[2U * i];
        uint32_t high_cycles = cycles[(2U * i) + 1U];
        
        if ((low_cycles == TIMEOUT_CYCLES) || (high_cycles == TIMEOUT_CYCLES)) {
            ESP_LOGE(TAG, "Timeout waiting for pulse at bit %lu", i);
            g_dht_state.last_result = false;
            return g_dht_state.last_result;
        }
        
        g_dht_state.data_buffer[i / 8U] <<= 1;
        /* Compare high vs low cycle times to determine bit value */
        if (high_cycles > low_cycles) {
            /* High cycles > low cycles = bit 1 */
            g_dht_state.data_buffer[i / 8U] |= 1U;
        }
        /* Otherwise bit 0 (no change needed) */
    }
    
    /* Verify checksum */
    uint8_t checksum = (g_dht_state.data_buffer[0] + g_dht_state.data_buffer[1] + 
                       g_dht_state.data_buffer[2] + g_dht_state.data_buffer[3]) & 0xFFU;
    
    if (g_dht_state.data_buffer[4] == checksum) {
        g_dht_state.last_result = true;
        
        /* Parse and store the readings */
        if (g_dht_state.sensor_type == DHT_TYPE_DHT22) {
            /* DHT22 temperature parsing */
            float temp = (float)(((uint16_t)(g_dht_state.data_buffer[2] & 0x7FU)) << 8 | 
                                g_dht_state.data_buffer[3]);
            temp *= 0.1f;
            if ((g_dht_state.data_buffer[2] & 0x80U) != 0U) {
                temp *= -1.0f;
            }
            g_dht_state.last_temperature = temp;
            
            /* DHT22 humidity parsing */
            float hum = (float)(((uint16_t)g_dht_state.data_buffer[0]) << 8 | 
                               g_dht_state.data_buffer[1]);
            hum *= 0.1f;
            g_dht_state.last_humidity = hum;
        } else {
            /* DHT11 parsing (for completeness) */
            g_dht_state.last_temperature = (float)g_dht_state.data_buffer[2];
            g_dht_state.last_humidity = (float)g_dht_state.data_buffer[0];
        }
        
        return g_dht_state.last_result;
    } else {
        ESP_LOGE(TAG, "Checksum failure! Expected: 0x%02X, Got: 0x%02X", 
                 checksum, g_dht_state.data_buffer[4]);
        g_dht_state.last_result = false;
        return g_dht_state.last_result;
    }
}

/* Public API Implementation */

esp_err_t dht_init(gpio_num_t gpio_num, dht_type_t type)
{
    /* Validate GPIO pin */
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        ESP_LOGE(TAG, "Invalid GPIO pin: %d", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Validate sensor type */
    if ((type != DHT_TYPE_DHT11) && (type != DHT_TYPE_DHT22)) {
        ESP_LOGE(TAG, "Invalid DHT sensor type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Configure GPIO as input with pull-up */
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << gpio_num),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Calculate max cycles for timeout - adapted from Arduino library */
    uint32_t cpu_freq_hz = 0U;
    ret = esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &cpu_freq_hz);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get CPU frequency: %s", esp_err_to_name(ret));
        return ret;
    }
    
    uint32_t max_cycles = cpu_freq_hz / 1000000UL; /* 1 microsecond worth of cycles */
    max_cycles *= 1000UL; /* 1 millisecond timeout */
    
    /* Initialize driver state */
    g_dht_state.gpio_pin = gpio_num;
    g_dht_state.sensor_type = type;
    g_dht_state.max_cycles = max_cycles;
    g_dht_state.last_read_time = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) - MIN_INTERVAL_MS;
    g_dht_state.last_result = false;
    g_dht_state.initialized = true;
    g_dht_state.last_temperature = NAN;
    g_dht_state.last_humidity = NAN;
    
    ESP_LOGI(TAG, "DHT%s initialized on GPIO %d, max_cycles: %lu", 
             (type == DHT_TYPE_DHT22) ? "22" : "11", gpio_num, max_cycles);
    
    return ESP_OK;
}

esp_err_t dht_read(float *temperature, float *humidity)
{
    /* Validate parameters */
    esp_err_t ret = validate_parameters(temperature, humidity);
    if (ret != ESP_OK) {
        *temperature = NAN;
        *humidity = NAN;
        return ret;
    }
    
    /* Perform sensor reading */
    bool success = perform_sensor_read(false);
    
    if (success) {
        *temperature = g_dht_state.last_temperature;
        *humidity = g_dht_state.last_humidity;
        return ESP_OK;
    } else {
        *temperature = NAN;
        *humidity = NAN;
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t dht_get_last_reading(float *temperature, float *humidity)
{
    /* Validate parameters */
    esp_err_t ret = validate_parameters(temperature, humidity);
    if (ret != ESP_OK) {
        *temperature = NAN;
        *humidity = NAN;
        return ret;
    }
    
    if (g_dht_state.last_result && !isnan(g_dht_state.last_temperature) && !isnan(g_dht_state.last_humidity)) {
        *temperature = g_dht_state.last_temperature;
        *humidity = g_dht_state.last_humidity;
        return ESP_OK;
    } else {
        *temperature = NAN;
        *humidity = NAN;
        return ESP_ERR_INVALID_STATE;
    }
}

float dht_convert_c_to_f(float celsius)
{
    return (celsius * 1.8f) + 32.0f;
}

float dht_convert_f_to_c(float fahrenheit)
{
    return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

float dht_compute_heat_index(float temperature, float humidity, bool is_fahrenheit)
{
    float hi;
    float temp_f = temperature;
    
    /* Convert to Fahrenheit if needed for calculation */
    if (!is_fahrenheit) {
        temp_f = dht_convert_c_to_f(temperature);
    }
    
    /* Using Rothfusz regression - initial approximation */
    hi = 0.5f * (temp_f + 61.0f + ((temp_f - 68.0f) * 1.2f) + (humidity * 0.094f));
    
    /* Use more accurate Steadman equation for higher temperatures */
    if (hi > 81.0f) {
        hi = -42.379f + 
             (2.04901523f * temp_f) + 
             (10.14333127f * humidity) +
             (-0.22475541f * temp_f * humidity) +
             (-0.00683783f * powf(temp_f, 2.0f)) +
             (-0.05481717f * powf(humidity, 2.0f)) +
             (0.00122874f * powf(temp_f, 2.0f) * humidity) +
             (0.00085282f * temp_f * powf(humidity, 2.0f)) +
             (-0.00000199f * powf(temp_f, 2.0f) * powf(humidity, 2.0f));
        
        /* Adjustments for specific conditions */
        if ((humidity < 13.0f) && (temp_f >= 80.0f) && (temp_f <= 112.0f)) {
            hi -= ((13.0f - humidity) * 0.25f) * 
                  sqrtf((17.0f - fabsf(temp_f - 95.0f)) * 0.05882f);
        } else if ((humidity > 85.0f) && (temp_f >= 80.0f) && (temp_f <= 87.0f)) {
            hi += ((humidity - 85.0f) * 0.1f) * ((87.0f - temp_f) * 0.2f);
        }
    }
    
    /* Return in requested units */
    return is_fahrenheit ? hi : dht_convert_f_to_c(hi);
}