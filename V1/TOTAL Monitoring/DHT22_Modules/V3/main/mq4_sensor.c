/**
 * @file mq4_sensor.c
 * @brief Robust MQ-4 Methane Gas Sensor Driver for ESP32
 * 
 * This implementation provides robust MQ-4 methane gas sensor functionality
 * with ADC-based readings, proper calibration, and PPM conversion.
 * Follows MISRA guidelines for infrastructure applications.
 * 
 * Compatible with ESP-IDF v5.4
 */

#include "mq4_sensor.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_adc_cal.h"

/* Logging tag */
static const char *TAG = "MQ4_SENSOR";

/* MQ-4 Configuration Constants */
#define MQ4_DEFAULT_WARMUP_TIME_MS    86400000UL  /**< 24 hours warmup time */
#define MQ4_DEFAULT_READING_INTERVAL  1000UL      /**< 1 second between readings */
#define MQ4_DEFAULT_RL_RESISTANCE     10000.0f    /**< 10kΩ load resistance */
#define MQ4_DEFAULT_REF_VOLTAGE       3.3f        /**< 3.3V reference voltage */
#define MQ4_ADC_SAMPLES               64U         /**< Number of ADC samples for averaging */
#define MQ4_ADC_ATTEN                 ADC_ATTEN_DB_12  /**< ADC attenuation for 0-3.3V range */
#define MQ4_ADC_WIDTH                 ADC_WIDTH_BIT_12 /**< 12-bit ADC resolution */

/* MQ-4 Characteristic Curve Constants for Methane */
#define MQ4_METHANE_A                 272.67f     /**< Curve parameter A */
#define MQ4_METHANE_B                 -0.84f     /**< Curve parameter B (negative because Rs decreases with concentration) */

/* Driver state structure */
typedef struct {
    mq4_config_t config;                    /**< Sensor configuration */
    mq4_reading_t last_reading;             /**< Last successful reading */
    uint64_t init_time;                     /**< Initialization timestamp */
    uint64_t last_read_time;                /**< Last read timestamp */
    float ro_clean_air;                     /**< Clean air resistance (calibrated) */
    bool initialized;                       /**< Driver initialization status */
    bool calibrated;                        /**< Calibration status */
    esp_adc_cal_characteristics_t adc_chars; /**< ADC calibration characteristics */
} mq4_driver_state_t;

/* Global driver state */
static mq4_driver_state_t g_mq4_state = {
    .config = {0},
    .last_reading = {0},
    .init_time = 0ULL,
    .last_read_time = 0ULL,
    .ro_clean_air = 50000.0f,
    .initialized = false,
    .calibrated = true,
    .adc_chars = {0}
};

/* Function prototypes */
static esp_err_t validate_config(const mq4_config_t *config);
static esp_err_t setup_adc(void);
static esp_err_t setup_gpio(void);
static uint32_t read_adc_averaged(void);
static bool is_sensor_warmed_up(void);
static esp_err_t validate_reading(mq4_reading_t *reading);

/**
 * @brief Validate MQ-4 configuration parameters
 * 
 * @param config Pointer to configuration structure
 * @return ESP_OK if valid, ESP_ERR_INVALID_ARG if invalid
 */
static esp_err_t validate_config(const mq4_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Configuration pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Validate ADC unit */
    if ((config->adc_unit != ADC_UNIT_1) && (config->adc_unit != ADC_UNIT_2)) {
        ESP_LOGE(TAG, "Invalid ADC unit: %d", config->adc_unit);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Validate ADC channel */
    if ((config->adc_channel < ADC_CHANNEL_0) || (config->adc_channel > ADC_CHANNEL_9)) {
        ESP_LOGE(TAG, "Invalid ADC channel: %d", config->adc_channel);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Validate reference voltage */
    if ((config->reference_voltage <= 0.0f) || (config->reference_voltage > 5.0f)) {
        ESP_LOGE(TAG, "Invalid reference voltage: %.2fV", config->reference_voltage);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Validate load resistance */
    if ((config->rl_resistance <= 0.0f) || (config->rl_resistance > 100000.0f)) {
        ESP_LOGE(TAG, "Invalid load resistance: %.2fΩ", config->rl_resistance);
        return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

/**
 * @brief Setup ADC for MQ-4 sensor reading
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t setup_adc(void)
{
    esp_err_t ret;
    
    /* Configure ADC width and attenuation */
    ret = adc1_config_width(MQ4_ADC_WIDTH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC width: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = adc1_config_channel_atten(g_mq4_state.config.adc_channel, MQ4_ADC_ATTEN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel attenuation: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Characterize ADC for voltage conversion */
    esp_adc_cal_characterize(g_mq4_state.config.adc_unit, MQ4_ADC_ATTEN, 
                            MQ4_ADC_WIDTH, g_mq4_state.config.reference_voltage, 
                            &g_mq4_state.adc_chars);
    
    ESP_LOGI(TAG, "ADC configured for channel %d with %d-bit resolution", 
             g_mq4_state.config.adc_channel, MQ4_ADC_WIDTH);
    
    return ESP_OK;
}

/**
 * @brief Setup GPIO for MQ-4 sensor power control
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t setup_gpio(void)
{
    if (g_mq4_state.config.power_pin != GPIO_NUM_NC) {
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << g_mq4_state.config.power_pin),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE,
        };
        
        esp_err_t ret = gpio_config(&io_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure power GPIO: %s", esp_err_to_name(ret));
            return ret;
        }
        
        /* Enable sensor power by default */
        gpio_set_level(g_mq4_state.config.power_pin, 1);
        ESP_LOGI(TAG, "Power GPIO %d configured and enabled", g_mq4_state.config.power_pin);
    }
    
    return ESP_OK;
}

/**
 * @brief Read ADC with averaging for noise reduction
 * 
 * @return Averaged ADC reading
 */
static uint32_t read_adc_averaged(void)
{
    uint32_t adc_sum = 0U;
    
    for (uint32_t i = 0U; i < MQ4_ADC_SAMPLES; i++) {
        adc_sum += adc1_get_raw(g_mq4_state.config.adc_channel);
        vTaskDelay(pdMS_TO_TICKS(1U)); /* Small delay between samples */
    }
    
    return adc_sum / MQ4_ADC_SAMPLES;
}

/**
 * @brief Check if sensor has been warmed up for sufficient time
 * 
 * @return true if warmed up, false otherwise
 */
static bool is_sensor_warmed_up(void)
{
    uint64_t current_time = esp_timer_get_time() / 1000ULL; /* Convert to milliseconds */
    uint64_t elapsed_time = current_time - g_mq4_state.init_time;
    
    return (elapsed_time >= g_mq4_state.config.warmup_time_ms);
}

/**
 * @brief Validate sensor reading for reasonable ranges
 * 
 * @param reading Pointer to reading structure
 * @return ESP_OK if valid, ESP_ERR_INVALID_STATE if invalid
 */
static esp_err_t validate_reading(mq4_reading_t *reading)
{
    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Check for reasonable voltage range (0-3.3V) */
    if ((reading->voltage < 0.0f) || (reading->voltage > 3300.0f)) {
        ESP_LOGW(TAG, "Voltage out of range: %.3fV", reading->voltage);
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Check for reasonable resistance range (1Ω to 1MΩ) */
    if ((reading->resistance < 1.0f) || (reading->resistance > 1000000.0f)) {
        ESP_LOGW(TAG, "Resistance out of range: %.2fΩ", reading->resistance);
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Check for reasonable PPM range (0-10000 PPM) */
    if ((reading->ppm_methane < 0.0f) || (reading->ppm_methane > 10000.0f)) {
        ESP_LOGW(TAG, "PPM out of range: %.2f PPM", reading->ppm_methane);
        return ESP_ERR_INVALID_STATE;
    }
    
    return ESP_OK;
}

/* Public API Implementation */

esp_err_t mq4_init(const mq4_config_t *config)
{
    /* Validate configuration */
    esp_err_t ret = validate_config(config);
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Copy configuration */
    (void)memcpy(&g_mq4_state.config, config, sizeof(mq4_config_t));
    
    /* Setup ADC */
    ret = setup_adc();
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Setup GPIO */
    ret = setup_gpio();
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Initialize state */
    g_mq4_state.init_time = esp_timer_get_time() / 1000ULL; /* Convert to milliseconds */
    g_mq4_state.last_read_time = 0ULL;
    g_mq4_state.ro_clean_air = 50000.0f;
    g_mq4_state.initialized = true;
    g_mq4_state.calibrated = true;
    
    /* Clear last reading */
    (void)memset(&g_mq4_state.last_reading, 0, sizeof(mq4_reading_t));
    g_mq4_state.last_reading.is_valid = false;
    
    ESP_LOGI(TAG, "MQ-4 sensor initialized on ADC%d_CH%d", 
             (g_mq4_state.config.adc_unit == ADC_UNIT_1) ? 1 : 2,
             g_mq4_state.config.adc_channel);
    ESP_LOGI(TAG, "Warmup time: %lu ms, Reading interval: %lu ms", 
             g_mq4_state.config.warmup_time_ms, g_mq4_state.config.reading_interval_ms);
    
    return ESP_OK;
}

// VERSION 2: Using voltages in MILLIVOLTS (alternative approach)
float mq4_voltage_to_resistance_mv(float voltage_mv)
{
    if (voltage_mv <= 0.0f) {
        return 0.0f;
    }
    
    /* MQ-4 Formula with millivolts: Rs = ((Vc_mv * RL) / Vout_mv) - RL */
    float vc_mv = g_mq4_state.config.reference_voltage * 1000.0f;  // 5000mV
    float vout_mv = voltage_mv;  // Voltage in MILLIVOLTS
    float rl = g_mq4_state.config.rl_resistance;                  // 10kΩ

    float rs = ((vc_mv * rl) / vout_mv) - rl;
    ESP_LOGI(TAG, "VERSION 2 (MILLIVOLTS): vc: %fmV, vout: %fmV, rl: %fΩ, rs: %fΩ", vc_mv, vout_mv, rl, rs);
    
    return rs;
}

esp_err_t mq4_read(mq4_reading_t *reading)
{
    if (reading == NULL) {
        ESP_LOGE(TAG, "Reading pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_mq4_state.initialized) {
        ESP_LOGE(TAG, "MQ-4 sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Check minimum reading interval */
    uint64_t current_time = esp_timer_get_time() / 1000ULL; /* Convert to milliseconds */
    if ((current_time - g_mq4_state.last_read_time) < g_mq4_state.config.reading_interval_ms) {
        /* Return last reading if too soon */
        if (g_mq4_state.last_reading.is_valid) {
            (void)memcpy(reading, &g_mq4_state.last_reading, sizeof(mq4_reading_t));
            return ESP_OK;
        } else {
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    /* Check if sensor is warmed up */
    if (!is_sensor_warmed_up()) {
        ESP_LOGW(TAG, "Sensor not warmed up yet. Elapsed: %llu ms, Required: %lu ms",
                 current_time - g_mq4_state.init_time, g_mq4_state.config.warmup_time_ms);
        reading->is_valid = false;
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Read ADC value */
    uint32_t raw_adc = read_adc_averaged();
    
    /* Convert to voltage */
    ESP_LOGI(TAG, "raw_adc: %"PRIu32, raw_adc);
    float voltage = mq4_adc_to_voltage(raw_adc);
    ESP_LOGI(TAG, "voltage: %fV", voltage);
    
    /* Test both versions as requested */
    float resistance_v1 = mq4_voltage_to_resistance(voltage);
    float resistance_v2 = mq4_voltage_to_resistance_mv(voltage * 1000.0f);  // Convert to mV for V2

    ESP_LOGI(TAG, "Resistance V1 (volts): %fΩ", resistance_v1);
    ESP_LOGI(TAG, "Resistance V2 (millivolts): %fΩ", resistance_v2);
    
    /* Calculate PPM using both versions */
    float ppm_v1 = 0.0f, ppm_v2 = 0.0f;
    if (g_mq4_state.ro_clean_air > 0.0f) {
        float ratio_v1 = resistance_v1 / g_mq4_state.ro_clean_air;
        float ratio_v2 = resistance_v2 / g_mq4_state.ro_clean_air;
        
        ppm_v1 = mq4_resistance_to_ppm(ratio_v1);
        ppm_v2 = mq4_resistance_to_ppm(ratio_v2);
        
        ESP_LOGI(TAG, "PPM V1: %f, PPM V2: %f", ppm_v1, ppm_v2);
    } else {
        ESP_LOGW(TAG, "R0 not calibrated. Run calibration first.");
    }
    
    /* Fill reading structure */
    reading->raw_adc_value = raw_adc;
    reading->voltage = voltage;
    reading->resistance = resistance_v1; // Use V1 resistance for now
    reading->ppm_methane = ppm_v1; // Use V1 PPM for now
    reading->timestamp = current_time;
    reading->is_valid = true;
    
    /* Validate reading */
    esp_err_t ret = validate_reading(reading);
    if (ret != ESP_OK) {
        reading->is_valid = false;
        return ret;
    }
    
    /* Update state */
    g_mq4_state.last_read_time = current_time;
    (void)memcpy(&g_mq4_state.last_reading, reading, sizeof(mq4_reading_t));
    
    ESP_LOGI(TAG, "MQ-4 Reading: ADC=%"PRIu32", Voltage=%.3fV, Resistance=%.2fΩ, PPM=%.2f",
             raw_adc, voltage, resistance_v1, ppm_v1);
    
    return ESP_OK;
}

esp_err_t mq4_get_last_reading(mq4_reading_t *reading)
{
    if (reading == NULL) {
        ESP_LOGE(TAG, "Reading pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_mq4_state.initialized) {
        ESP_LOGE(TAG, "MQ-4 sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_mq4_state.last_reading.is_valid) {
        (void)memcpy(reading, &g_mq4_state.last_reading, sizeof(mq4_reading_t));
        return ESP_OK;
    } else {
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t mq4_set_power(bool enable)
{
    if (!g_mq4_state.initialized) {
        ESP_LOGE(TAG, "MQ-4 sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_mq4_state.config.power_pin != GPIO_NUM_NC) {
        gpio_set_level(g_mq4_state.config.power_pin, enable ? 1 : 0);
        ESP_LOGI(TAG, "MQ-4 power %s", enable ? "enabled" : "disabled");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Power control not configured");
        return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t mq4_calibrate_clean_air(void)
{
    if (!g_mq4_state.initialized) {
        ESP_LOGE(TAG, "MQ-4 sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!is_sensor_warmed_up()) {
        ESP_LOGE(TAG, "Sensor must be warmed up before calibration");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting clean air calibration...");
    
    /* Take multiple readings and average */
    float total_resistance = 0.0f;
    uint32_t valid_readings = 0U;
    
    for (uint32_t i = 0U; i < 10U; i++) {
        mq4_reading_t reading;
        esp_err_t ret = mq4_read(&reading);
        
        if ((ret == ESP_OK) && reading.is_valid) {
            total_resistance += reading.resistance;
            valid_readings++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000U)); /* 1 second between readings */
    }
    
    // if (valid_readings > 0U) {
    //     g_mq4_state.ro_clean_air = total_resistance / (float)valid_readings;
    //     g_mq4_state.calibrated = true;
        
    //     ESP_LOGI(TAG, "Clean air calibration completed. Ro = %.2fΩ", g_mq4_state.ro_clean_air);
    //     return ESP_OK;
    // } else {
    //     ESP_LOGE(TAG, "Failed to get valid readings for calibration");
    //     return ESP_ERR_TIMEOUT;
    // }

    return ESP_OK;
}

float mq4_adc_to_voltage(uint32_t raw_value)
{
    // esp_adc_cal_raw_to_voltage returns voltage in millivolts, convert to volts
    return esp_adc_cal_raw_to_voltage(raw_value, &g_mq4_state.adc_chars) / 1000.0f;
}

// VERSION 1: Using voltages in VOLTS (standard MQ-4 approach)
float mq4_voltage_to_resistance(float voltage)
{
    if (voltage <= 0.0f) {
        return 0.0f;
    }
    
    /* MQ-4 Standard Formula: Rs = ((Vc * RL) / Vout) - RL */
    /* Where Vc is the reference voltage (5V), Vout is measured voltage, RL is load resistance */
    float vc = g_mq4_state.config.reference_voltage;  // 5.0V
    float vout = voltage;  // Voltage in VOLTS (0-5V range)
    float rl = g_mq4_state.config.rl_resistance;     // 10kΩ

    float rs = ((vc * rl) / vout) - rl;
    ESP_LOGI(TAG, "VERSION 1 (VOLTS): vc: %fV, vout: %fV, rl: %fΩ, rs: %fΩ", vc, vout, rl, rs);
    
    return rs;
}


float mq4_resistance_to_ppm(float rs_ro_ratio)
{
    if (rs_ro_ratio <= 0.0f) {
        return 0.0f;
    }
    
    /* MQ-4 methane characteristic curve from datasheet */
    /* For methane: Rs/R0 = 4.4 in clean air, decreases with gas concentration */
    /* Using logarithmic relationship: log(PPM) = log(A) + B * log(Rs/R0) */
    /* Where A and B are curve parameters for methane */
    
    float log_ppm = logf(MQ4_METHANE_A) + MQ4_METHANE_B * logf(rs_ro_ratio);
    float ppm = expf(log_ppm);
    
    /* Ensure reasonable range for methane (300-10000 ppm) */
    if (ppm < 300.0f) {
        ppm = 300.0f;
    } else if (ppm > 10000.0f) {
        ppm = 10000.0f;
    }
    
    ESP_LOGI(TAG, "Rs/R0 ratio: %f, calculated PPM: %f", rs_ro_ratio, ppm);
    
    return ppm;
}

// MQ-4 PPM Tuning Functions
// These functions help you tune the A and B parameters using your Rs/Ro to PPM chart

/**
 * @brief Calculate curve parameters A and B from two data points
 * 
 * Use this function with two known points from your Rs/Ro to PPM chart
 * Formula: log(PPM) = log(A) + B * log(Rs/R0)
 * 
 * @param rs_ro_1 Rs/R0 ratio for first point
 * @param ppm_1 PPM value for first point
 * @param rs_ro_2 Rs/R0 ratio for second point  
 * @param ppm_2 PPM value for second point
 * @param A Output parameter A
 * @param B Output parameter B
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mq4_calculate_curve_parameters(float rs_ro_1, float ppm_1, 
                                        float rs_ro_2, float ppm_2,
                                        float *A, float *B)
{
    if (A == NULL || B == NULL) {
        ESP_LOGE(TAG, "Output parameters cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (rs_ro_1 <= 0.0f || rs_ro_2 <= 0.0f || ppm_1 <= 0.0f || ppm_2 <= 0.0f) {
        ESP_LOGE(TAG, "All input values must be positive");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (rs_ro_1 == rs_ro_2) {
        ESP_LOGE(TAG, "Rs/R0 ratios must be different");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calculate B parameter: B = (log(PPM2) - log(PPM1)) / (log(Rs/R0_2) - log(Rs/R0_1))
    float log_ppm_diff = logf(ppm_2) - logf(ppm_1);
    float log_rs_ro_diff = logf(rs_ro_2) - logf(rs_ro_1);
    
    *B = log_ppm_diff / log_rs_ro_diff;
    
    // Calculate A parameter: A = PPM1 / (Rs/R0_1)^B
    *A = ppm_1 / powf(rs_ro_1, *B);
    
    ESP_LOGI(TAG, "Calculated parameters: A = %f, B = %f", *A, *B);
    ESP_LOGI(TAG, "Point 1: Rs/R0 = %f, PPM = %f", rs_ro_1, ppm_1);
    ESP_LOGI(TAG, "Point 2: Rs/R0 = %f, PPM = %f", rs_ro_2, ppm_2);
    
    return ESP_OK;
}

/**
 * @brief Test PPM calculation with custom parameters
 * 
 * @param rs_ro_ratio Rs/R0 ratio to test
 * @param A Parameter A
 * @param B Parameter B
 * @return float Calculated PPM
 */
float mq4_test_ppm_calculation(float rs_ro_ratio, float A, float B)
{
    if (rs_ro_ratio <= 0.0f) {
        return 0.0f;
    }
    
    float log_ppm = logf(A) + B * logf(rs_ro_ratio);
    float ppm = expf(log_ppm);
    
    ESP_LOGI(TAG, "Test: Rs/R0 = %f, A = %f, B = %f, PPM = %f", 
             rs_ro_ratio, A, B, ppm);
    
    return ppm;
}

/**
 * @brief Tune parameters using multiple data points from your chart
 * 
 * This function uses linear regression to find the best fit line
 * through multiple data points from your Rs/Ro to PPM chart
 * 
 * @param rs_ro_ratios Array of Rs/R0 ratios
 * @param ppm_values Array of corresponding PPM values
 * @param num_points Number of data points
 * @param A Output parameter A
 * @param B Output parameter B
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mq4_tune_parameters_regression(float *rs_ro_ratios, float *ppm_values, 
                                        int num_points, float *A, float *B)
{
    if (rs_ro_ratios == NULL || ppm_values == NULL || A == NULL || B == NULL) {
        ESP_LOGE(TAG, "Input parameters cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (num_points < 2) {
        ESP_LOGE(TAG, "Need at least 2 data points");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Convert to logarithmic scale for linear regression
    float sum_log_rs_ro = 0.0f;
    float sum_log_ppm = 0.0f;
    float sum_log_rs_ro_squared = 0.0f;
    float sum_log_rs_ro_log_ppm = 0.0f;
    
    for (int i = 0; i < num_points; i++) {
        if (rs_ro_ratios[i] <= 0.0f || ppm_values[i] <= 0.0f) {
            ESP_LOGE(TAG, "All values must be positive (point %d)", i);
            return ESP_ERR_INVALID_ARG;
        }
        
        float log_rs_ro = logf(rs_ro_ratios[i]);
        float log_ppm = logf(ppm_values[i]);
        
        sum_log_rs_ro += log_rs_ro;
        sum_log_ppm += log_ppm;
        sum_log_rs_ro_squared += log_rs_ro * log_rs_ro;
        sum_log_rs_ro_log_ppm += log_rs_ro * log_ppm;
    }
    
    // Linear regression: y = mx + b where y = log(PPM), x = log(Rs/R0)
    // B = m, log(A) = b, so A = e^b
    float n = (float)num_points;
    float denominator = n * sum_log_rs_ro_squared - sum_log_rs_ro * sum_log_rs_ro;
    
    if (fabsf(denominator) < 1e-6f) {
        ESP_LOGE(TAG, "Cannot perform regression - points are collinear");
        return ESP_ERR_INVALID_ARG;
    }
    
    *B = (n * sum_log_rs_ro_log_ppm - sum_log_rs_ro * sum_log_ppm) / denominator;
    float log_A = (sum_log_ppm - *B * sum_log_rs_ro) / n;
    *A = expf(log_A);
    
    ESP_LOGI(TAG, "Regression results: A = %f, B = %f", *A, *B);
    ESP_LOGI(TAG, "Used %d data points", num_points);
    
    return ESP_OK;
}

/**
 * @brief Update the MQ-4 curve parameters
 * 
 * @param A New parameter A
 * @param B New parameter B
 */
void mq4_update_curve_parameters(float A, float B)
{
    ESP_LOGI(TAG, "Updating curve parameters: A = %f, B = %f", A, B);
    ESP_LOGI(TAG, "Previous values: A = %f, B = %f", MQ4_METHANE_A, MQ4_METHANE_B);
    
    // Note: You'll need to update the #define values in the code
    // or use global variables instead of #define for runtime updates
    ESP_LOGW(TAG, "To make this permanent, update MQ4_METHANE_A and MQ4_METHANE_B in the code");
}

/**
 * @brief Example function to demonstrate parameter tuning
 * 
 * Call this function to test the tuning system with your chart data
 */
void mq4_tuning_example(void)
{
    ESP_LOGI(TAG, "=== MQ-4 Parameter Tuning Example ===");
    
    // Example 1: Calculate parameters from two points
    // Replace these with actual values from your Rs/Ro to PPM chart
    float rs_ro_1 = 4.4f;    // Clean air (Rs/R0 = 4.4)
    float ppm_1 = 300.0f;    // Minimum detection (300 ppm)
    float rs_ro_2 = 0.75f;   // High concentration
    float ppm_2 = 10000.0f;  // Maximum detection (10000 ppm)
    
    float A, B;
    esp_err_t ret = mq4_calculate_curve_parameters(rs_ro_1, ppm_1, rs_ro_2, ppm_2, &A, &B);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calculated parameters: A = %f, B = %f", A, B);
        
        // Test the parameters with different Rs/R0 ratios
        float test_ratios[] = {4.4f, 2.0f, 1.0f, 0.75f};
        int num_tests = sizeof(test_ratios) / sizeof(test_ratios[0]);
        
        ESP_LOGI(TAG, "Testing calculated parameters:");
        for (int i = 0; i < num_tests; i++) {
            float test_ppm = mq4_test_ppm_calculation(test_ratios[i], A, B);
            ESP_LOGI(TAG, "Rs/R0 = %f -> PPM = %f", test_ratios[i], test_ppm);
        }
    }
    
    // Example 2: Use multiple data points for better accuracy
    // Replace these arrays with actual data from your chart
    float rs_ro_ratios[] = {4.4f, 2.6f, 1.5f, 1.0f, 0.75f};
    float ppm_values[] = {300.0f, 1000.0f, 3000.0f, 5000.0f, 10000.0f};
    int num_points = sizeof(rs_ro_ratios) / sizeof(rs_ro_ratios[0]);
    
    float A_reg, B_reg;
    ret = mq4_tune_parameters_regression(rs_ro_ratios, ppm_values, num_points, &A_reg, &B_reg);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Regression parameters: A = %f, B = %f", A_reg, B_reg);
        
        // Compare with current parameters
        ESP_LOGI(TAG, "Current parameters: A = %f, B = %f", MQ4_METHANE_A, MQ4_METHANE_B);
        
        // Test regression parameters
        ESP_LOGI(TAG, "Testing regression parameters:");
        for (int i = 0; i < num_points; i++) {
            float test_ppm = mq4_test_ppm_calculation(rs_ro_ratios[i], A_reg, B_reg);
            float error = fabsf(test_ppm - ppm_values[i]);
            float error_percent = (error / ppm_values[i]) * 100.0f;
            ESP_LOGI(TAG, "Rs/R0 = %f, Expected PPM = %f, Calculated PPM = %f, Error = %.1f%%", 
                     rs_ro_ratios[i], ppm_values[i], test_ppm, error_percent);
        }
    }
    
    ESP_LOGI(TAG, "=== Tuning Example Complete ===");
}

// Add R0 calibration function for clean air baseline
float mq4_calibrate_ro_clean_air(void)
{
    ESP_LOGI(TAG, "Starting R0 calibration in clean air...");
    
    // Take multiple readings in clean air
    float total_rs = 0.0f;
    int samples = 100;
    
    for (int i = 0; i < samples; i++) {
        uint32_t raw_adc = read_adc_averaged();
        float voltage = mq4_adc_to_voltage(raw_adc);
        float rs = mq4_voltage_to_resistance(voltage);
        total_rs += rs;
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay between samples
    }
    
    float avg_rs = total_rs / samples;
    float ro = avg_rs / 4.4f;  // Rs/R0 = 4.4 in clean air for MQ-4
    
    ESP_LOGI(TAG, "R0 calibration complete. Average Rs: %fΩ, R0: %fΩ", avg_rs, ro);
    
    return ro;
}

void mq4_get_default_config(mq4_config_t *config, adc_unit_t adc_unit, 
                           adc_channel_t adc_channel, gpio_num_t power_pin)
{
    if (config == NULL) {
        return;
    }
    
    config->adc_unit = adc_unit;
    config->adc_channel = adc_channel;
    config->power_pin = power_pin;
    config->reference_voltage = MQ4_DEFAULT_REF_VOLTAGE;
    config->rl_resistance = MQ4_DEFAULT_RL_RESISTANCE;
    config->warmup_time_ms = MQ4_DEFAULT_WARMUP_TIME_MS;
    config->reading_interval_ms = MQ4_DEFAULT_READING_INTERVAL;
}

esp_err_t mq4_deinit(void)
{
    if (!g_mq4_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Disable power if configured */
    if (g_mq4_state.config.power_pin != GPIO_NUM_NC) {
        gpio_set_level(g_mq4_state.config.power_pin, 0);
    }
    
    /* Reset state */
    (void)memset(&g_mq4_state, 0, sizeof(mq4_driver_state_t));
    
    ESP_LOGI(TAG, "MQ-4 sensor deinitialized");
    return ESP_OK;
}