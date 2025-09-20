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
#define MQ4_ADC_ATTEN                 ADC_ATTEN_DB_11  /**< ADC attenuation for 0-3.3V range */
#define MQ4_ADC_WIDTH                 ADC_WIDTH_BIT_12 /**< 12-bit ADC resolution */

/* MQ-4 Characteristic Curve Constants for Methane */
#define MQ4_METHANE_A                 987.99f     /**< Curve parameter A */
#define MQ4_METHANE_B                 2.162f      /**< Curve parameter B */

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
    .ro_clean_air = 0.0f,
    .initialized = false,
    .calibrated = false,
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
    if ((reading->voltage < 0.0f) || (reading->voltage > 3.3f)) {
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
    g_mq4_state.ro_clean_air = 0.0f;
    g_mq4_state.initialized = true;
    g_mq4_state.calibrated = false;
    
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
    float voltage = mq4_adc_to_voltage(raw_adc);
    
    /* Convert to resistance */
    float resistance = mq4_voltage_to_resistance(voltage);
    
    /* Convert to PPM */
    float ppm = 0.0f;
    if (g_mq4_state.calibrated && (g_mq4_state.ro_clean_air > 0.0f)) {
        float rs_ro_ratio = resistance / g_mq4_state.ro_clean_air;
        ppm = mq4_resistance_to_ppm(rs_ro_ratio);
    } else {
        ESP_LOGW(TAG, "Sensor not calibrated, cannot convert to PPM");
    }
    
    /* Fill reading structure */
    reading->raw_adc_value = raw_adc;
    reading->voltage = voltage;
    reading->resistance = resistance;
    reading->ppm_methane = ppm;
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
    
    ESP_LOGI(TAG, "MQ-4 Reading: ADC=%lu, Voltage=%.3fV, Resistance=%.2fΩ, PPM=%.2f",
             raw_adc, voltage, resistance, ppm);
    
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
    
    if (valid_readings > 0U) {
        g_mq4_state.ro_clean_air = total_resistance / (float)valid_readings;
        g_mq4_state.calibrated = true;
        
        ESP_LOGI(TAG, "Clean air calibration completed. Ro = %.2fΩ", g_mq4_state.ro_clean_air);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get valid readings for calibration");
        return ESP_ERR_TIMEOUT;
    }
}

float mq4_adc_to_voltage(uint32_t raw_value)
{
    return esp_adc_cal_raw_to_voltage(raw_value, &g_mq4_state.adc_chars);
}

float mq4_voltage_to_resistance(float voltage)
{
    if (voltage <= 0.0f) {
        return 0.0f;
    }
    
    /* Rs = (Vc - Vout) * RL / Vout */
    /* Where Vc is the reference voltage, Vout is the measured voltage, RL is load resistance */
    float vc = g_mq4_state.config.reference_voltage;
    float vout = voltage;
    float rl = g_mq4_state.config.rl_resistance;
    
    return ((vc - vout) * rl) / vout;
}

float mq4_resistance_to_ppm(float rs_ro_ratio)
{
    if (rs_ro_ratio <= 0.0f) {
        return 0.0f;
    }
    
    /* MQ-4 methane characteristic curve: PPM = A * (Rs/Ro)^B */
    /* Using logarithmic form: log(PPM) = log(A) + B * log(Rs/Ro) */
    float log_ppm = logf(MQ4_METHANE_A) + MQ4_METHANE_B * logf(rs_ro_ratio);
    float ppm = expf(log_ppm);
    
    /* Ensure reasonable range */
    if (ppm < 0.0f) {
        ppm = 0.0f;
    } else if (ppm > 10000.0f) {
        ppm = 10000.0f;
    }
    
    return ppm;
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