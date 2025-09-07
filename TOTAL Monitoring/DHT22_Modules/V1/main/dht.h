/**
 * @file dht.h
 * @brief Robust DHT22 Temperature and Humidity Sensor Driver for ESP32
 * 
 * This driver provides a robust implementation of DHT22 sensor communication
 * with timing-critical sections preserved from proven Arduino implementations.
 * 
 * Compatible with ESP-IDF v5.4
 */

#ifndef DHT_H
#define DHT_H

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DHT sensor types
 */
typedef enum {
    DHT_TYPE_DHT11,     /**< DHT11 sensor type */
    DHT_TYPE_DHT22      /**< DHT22 sensor type */
} dht_type_t;

/**
 * @brief Initialize the DHT sensor
 * 
 * This function initializes the DHT sensor on the specified GPIO pin.
 * It configures the GPIO, calculates timing parameters, and prepares
 * the sensor for readings.
 *
 * @param gpio_num GPIO pin connected to the DHT sensor data line
 * @param type Type of DHT sensor (DHT11 or DHT22)
 * @return esp_err_t ESP_OK on success, or an appropriate error code
 * 
 * @note The DHT22 data line requires a 4.7kΩ pull-up resistor to 3.3V
 * @note Minimum 2 seconds between readings for DHT22
 */
esp_err_t dht_init(gpio_num_t gpio_num, dht_type_t type);

/**
 * @brief Read temperature and humidity from the DHT sensor
 * 
 * This function performs a complete read cycle from the DHT sensor,
 * including timing-critical communication protocol, data validation,
 * and checksum verification.
 *
 * @param temperature Pointer to store the temperature value in Celsius
 * @param humidity Pointer to store the humidity percentage (0-100%)
 * @return esp_err_t ESP_OK on success, or an appropriate error code
 * 
 * @note This function includes timing-critical sections with interrupts disabled
 * @note Automatic rate limiting prevents readings faster than sensor minimum interval
 * @note Returns NAN values through pointers if reading fails
 */
esp_err_t dht_read(float *temperature, float *humidity);

/**
 * @brief Get the last successful reading without performing a new read
 * 
 * @param temperature Pointer to store the last temperature value in Celsius
 * @param humidity Pointer to store the last humidity percentage
 * @return esp_err_t ESP_OK if last reading was valid, ESP_ERR_INVALID_STATE otherwise
 */
esp_err_t dht_get_last_reading(float *temperature, float *humidity);

/**
 * @brief Convert Celsius to Fahrenheit
 * 
 * @param celsius Temperature in Celsius
 * @return Temperature in Fahrenheit
 */
float dht_convert_c_to_f(float celsius);

/**
 * @brief Convert Fahrenheit to Celsius
 * 
 * @param fahrenheit Temperature in Fahrenheit  
 * @return Temperature in Celsius
 */
float dht_convert_f_to_c(float fahrenheit);

/**
 * @brief Compute Heat Index
 * 
 * Calculates the heat index (apparent temperature) based on temperature
 * and humidity using the Rothfusz regression and Steadman equation.
 * 
 * @param temperature Temperature value
 * @param humidity Relative humidity percentage (0-100%)
 * @param is_fahrenheit True if temperature is in Fahrenheit, false for Celsius
 * @return Heat index in the same units as input temperature
 */
float dht_compute_heat_index(float temperature, float humidity, bool is_fahrenheit);

#ifdef __cplusplus
}
#endif

#endif // DHT_H