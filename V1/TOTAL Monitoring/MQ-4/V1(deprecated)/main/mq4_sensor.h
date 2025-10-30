/**
 * @file mq4_sensor.h
 * @brief Robust MQ-4 Methane Gas Sensor Driver for ESP32
 * 
 * This driver provides a robust implementation for MQ-4 methane gas sensor
 * with ADC-based readings and proper calibration for infrastructure use.
 * 
 * Compatible with ESP-IDF v5.4
 * Follows MISRA guidelines for infrastructure applications
 */

#ifndef MQ4_SENSOR_H
#define MQ4_SENSOR_H

#include "esp_err.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MQ-4 sensor configuration structure
 */
typedef struct {
    adc_unit_t adc_unit;           /**< ADC unit (ADC_UNIT_1 or ADC_UNIT_2) */
    adc_channel_t adc_channel;     /**< ADC channel */
    gpio_num_t power_pin;          /**< GPIO pin for sensor power control (optional) */
    float reference_voltage;       /**< Reference voltage for ADC (typically 3.3V) */
    float rl_resistance;           /**< Load resistance in ohms (typically 10kΩ) */
    uint32_t warmup_time_ms;       /**< Sensor warmup time in milliseconds */
    uint32_t reading_interval_ms;  /**< Minimum interval between readings */
} mq4_config_t;

/**
 * @brief MQ-4 sensor reading structure
 */
typedef struct {
    uint32_t raw_adc_value;        /**< Raw ADC reading */
    float voltage;                 /**< Calculated voltage */
    float resistance;              /**< Calculated sensor resistance */
    float ppm_methane;             /**< Methane concentration in PPM */
    uint64_t timestamp;            /**< Reading timestamp */
    bool is_valid;                 /**< Reading validity flag */
} mq4_reading_t;

/**
 * @brief Initialize the MQ-4 sensor
 * 
 * This function initializes the MQ-4 sensor with the provided configuration.
 * It sets up the ADC, configures GPIO pins, and prepares the sensor for readings.
 *
 * @param config Pointer to MQ-4 configuration structure
 * @return esp_err_t ESP_OK on success, or an appropriate error code
 * 
 * @note The MQ-4 sensor requires a warmup period of at least 24 hours for accurate readings
 * @note A load resistor (typically 10kΩ) is required between sensor output and ground
 * @note The sensor operates at 5V but output is typically 3.3V compatible
 */
esp_err_t mq4_init(const mq4_config_t *config);

/**
 * @brief Read methane concentration from MQ-4 sensor
 * 
 * This function performs a complete reading cycle from the MQ-4 sensor,
 * including ADC sampling, voltage calculation, resistance calculation,
 * and PPM conversion using the sensor's characteristic curve.
 *
 * @param reading Pointer to store the sensor reading
 * @return esp_err_t ESP_OK on success, or an appropriate error code
 * 
 * @note This function includes automatic warmup time enforcement
 * @note Returns invalid reading if sensor is not properly warmed up
 * @note Includes built-in validation for reasonable reading ranges
 */
esp_err_t mq4_read(mq4_reading_t *reading);

/**
 * @brief Get the last successful reading without performing a new read
 * 
 * @param reading Pointer to store the last reading
 * @return esp_err_t ESP_OK if last reading was valid, ESP_ERR_INVALID_STATE otherwise
 */
esp_err_t mq4_get_last_reading(mq4_reading_t *reading);

/**
 * @brief Enable or disable sensor power
 * 
 * This function controls the power pin of the MQ-4 sensor if configured.
 * Useful for power management in battery-operated applications.
 *
 * @param enable true to enable power, false to disable
 * @return esp_err_t ESP_OK on success, or an appropriate error code
 */
esp_err_t mq4_set_power(bool enable);

/**
 * @brief Calibrate sensor in clean air
 * 
 * This function performs calibration of the MQ-4 sensor in clean air.
 * Should be called after sensor warmup in a clean air environment.
 *
 * @return esp_err_t ESP_OK on success, or an appropriate error code
 * 
 * @note Calibration should be performed in clean air (0 PPM methane)
 * @note Sensor must be fully warmed up before calibration
 */
esp_err_t mq4_calibrate_clean_air(void);

/**
 * @brief Convert raw ADC value to voltage
 * 
 * @param raw_value Raw ADC reading
 * @return Calculated voltage in volts
 */
float mq4_adc_to_voltage(uint32_t raw_value);

/**
 * @brief Convert voltage to sensor resistance
 * 
 * @param voltage Sensor output voltage
 * @return Calculated sensor resistance in ohms
 */
float mq4_voltage_to_resistance(float voltage);

/**
 * @brief Convert resistance ratio to PPM using MQ-4 characteristic curve
 * 
 * @param rs_ro_ratio Ratio of sensor resistance to clean air resistance
 * @return Methane concentration in PPM
 */
float mq4_resistance_to_ppm(float rs_ro_ratio);

/**
 * @brief Get default configuration for MQ-4 sensor
 * 
 * @param config Pointer to configuration structure to fill
 * @param adc_unit ADC unit to use
 * @param adc_channel ADC channel to use
 * @param power_pin GPIO pin for power control (GPIO_NUM_NC if not used)
 */
void mq4_get_default_config(mq4_config_t *config, adc_unit_t adc_unit, 
                           adc_channel_t adc_channel, gpio_num_t power_pin);

/**
 * @brief Deinitialize the MQ-4 sensor
 * 
 * This function cleans up resources used by the MQ-4 sensor driver.
 *
 * @return esp_err_t ESP_OK on success, or an appropriate error code
 */
esp_err_t mq4_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // MQ4_SENSOR_H