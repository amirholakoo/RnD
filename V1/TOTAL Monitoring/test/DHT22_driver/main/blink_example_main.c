/* DHT22 Temperature and Humidity Sensor Driver for ESP32-S3
 * 
 * Adapted from Adafruit DHT library for ESP-IDF
 * Timing-critical sections preserved from original Arduino implementation
 */

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

static const char *TAG = "DHT22";

/* DHT22 Configuration - Based on Adafruit library */
#define DHT22_GPIO_PIN          GPIO_NUM_4    /* Change this to your desired GPIO pin */
#define DHT22_TYPE              22            /* DHT22 sensor type */
#define MIN_INTERVAL_MS         2000          /* Minimum interval between readings */
#define TIMEOUT_CYCLES          UINT32_MAX    /* Timeout value for pulse reading */

/* Timing constants from Adafruit library */
#define DHT22_START_SIGNAL_US   1100          /* Start signal duration for DHT22 */
#define DHT22_PULLUP_DELAY_US   55            /* Pull-up delay before reading */

/* Global variables */
static uint8_t dht_data[5];
static uint32_t max_cycles;
static uint32_t last_read_time = 0;
static bool last_result = false;

/* Function prototypes */
static esp_err_t dht22_init(void);
static bool dht22_read(bool force);
static uint32_t expect_pulse(bool level);
static float read_temperature(bool fahrenheit);
static float read_humidity(void);
static float convert_c_to_f(float celsius);
static float convert_f_to_c(float fahrenheit);
static float compute_heat_index(float temperature, float percent_humidity, bool is_fahrenheit);

static esp_err_t dht22_init(void)
{
    /* Configure GPIO as input with pull-up */
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << DHT22_GPIO_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Calculate max cycles for 1ms timeout - adapted from Arduino library */
    uint32_t cpu_freq_hz = 0;
    esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &cpu_freq_hz);
    max_cycles = cpu_freq_hz / 1000000UL; /* 1 microsecond worth of cycles */
    max_cycles *= 1000UL; /* 1 millisecond timeout */
    
    /* Initialize timing */
    last_read_time = xTaskGetTickCount() * portTICK_PERIOD_MS - MIN_INTERVAL_MS;
    
    ESP_LOGI(TAG, "DHT22 initialized on GPIO %d, max_cycles: %lu", DHT22_GPIO_PIN, max_cycles);
    return ESP_OK;
}

/* Expect pulse function - directly adapted from Arduino library */
static uint32_t expect_pulse(bool level)
{
    uint32_t count = 0;
    
    /* Use direct GPIO register access for timing precision */
    while (gpio_get_level(DHT22_GPIO_PIN) == level) {
        if (count++ >= max_cycles) {
            return TIMEOUT_CYCLES; /* Timeout */
        }
    }
    
    return count;
}

/* Main read function - adapted from Arduino DHT library */
static bool dht22_read(bool force)
{
    /* Check if sensor was read less than 2 seconds ago */
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (!force && ((current_time - last_read_time) < MIN_INTERVAL_MS)) {
        return last_result; /* Return last correct measurement */
    }
    last_read_time = current_time;
    
    /* Reset 40 bits of received data to zero */
    dht_data[0] = dht_data[1] = dht_data[2] = dht_data[3] = dht_data[4] = 0;
    
    /* Send start signal - exactly like Arduino library */
    gpio_set_direction(DHT22_GPIO_PIN, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    /* Set data line low for start signal */
    gpio_set_direction(DHT22_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT22_GPIO_PIN, 0);
    ets_delay_us(DHT22_START_SIGNAL_US); /* 1100us for DHT22 */
    
    uint32_t cycles[80];
    
    /* TIMING CRITICAL SECTION - NO INTERRUPTIONS */
    {
        /* End start signal by setting data line high */
        gpio_set_direction(DHT22_GPIO_PIN, GPIO_MODE_INPUT);
        
        /* Delay to let sensor pull data line low */
        ets_delay_us(DHT22_PULLUP_DELAY_US);
        
        /* Disable interrupts for timing-critical section */
        portDISABLE_INTERRUPTS();
        
        /* Wait for sensor response - low pulse ~80us then high pulse ~80us */
        if (expect_pulse(0) == TIMEOUT_CYCLES) {
            ESP_LOGE(TAG, "DHT timeout waiting for start signal low pulse");
            last_result = false;
            portENABLE_INTERRUPTS();
            return last_result;
        }
        if (expect_pulse(1) == TIMEOUT_CYCLES) {
            ESP_LOGE(TAG, "DHT timeout waiting for start signal high pulse");
            last_result = false;
            portENABLE_INTERRUPTS();
            return last_result;
        }
        
        /* Read the 40 bits sent by the sensor */
        /* Each bit: 50us low pulse + variable high pulse (28us=0, 70us=1) */
        for (int i = 0; i < 80; i += 2) {
            cycles[i] = expect_pulse(0);     /* Low pulse */
            cycles[i + 1] = expect_pulse(1); /* High pulse */
        }
        
        portENABLE_INTERRUPTS();
    } /* End timing critical code */
    
    /* Process the received pulses */
    for (int i = 0; i < 40; ++i) {
        uint32_t low_cycles = cycles[2 * i];
        uint32_t high_cycles = cycles[2 * i + 1];
        
        if ((low_cycles == TIMEOUT_CYCLES) || (high_cycles == TIMEOUT_CYCLES)) {
            ESP_LOGE(TAG, "DHT timeout waiting for pulse at bit %d", i);
            last_result = false;
            return last_result;
        }
        
        dht_data[i / 8] <<= 1;
        /* Compare high vs low cycle times to determine bit value */
        if (high_cycles > low_cycles) {
            /* High cycles > low cycles = bit 1 */
            dht_data[i / 8] |= 1;
        }
        /* Otherwise bit 0 (no change needed) */
    }
    
    /* Verify checksum */
    if (dht_data[4] == ((dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3]) & 0xFF)) {
        last_result = true;
        return last_result;
    } else {
        ESP_LOGE(TAG, "DHT checksum failure!");
        last_result = false;
        return last_result;
    }
}

/* Read temperature - adapted from Arduino library */
static float read_temperature(bool fahrenheit)
{
    float temp = NAN;
    
    if (dht22_read(false)) {
        /* DHT22 temperature parsing */
        temp = ((uint16_t)(dht_data[2] & 0x7F)) << 8 | dht_data[3];
        temp *= 0.1f;
        if (dht_data[2] & 0x80) {
            temp *= -1.0f;
        }
        if (fahrenheit) {
            temp = convert_c_to_f(temp);
        }
    }
    return temp;
}

/* Read humidity - adapted from Arduino library */
static float read_humidity(void)
{
    float humidity = NAN;
    
    if (dht22_read(false)) {
        /* DHT22 humidity parsing */
        humidity = ((uint16_t)dht_data[0]) << 8 | dht_data[1];
        humidity *= 0.1f;
    }
    return humidity;
}

/* Convert Celsius to Fahrenheit */
static float convert_c_to_f(float celsius)
{
    return celsius * 1.8f + 32.0f;
}

/* Convert Fahrenheit to Celsius */
static float convert_f_to_c(float fahrenheit)
{
    return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

/* Compute Heat Index - adapted from Arduino DHT library */
static float compute_heat_index(float temperature, float percent_humidity, bool is_fahrenheit)
{
    float hi;
    float temp_f = temperature;
    
    /* Convert to Fahrenheit if needed for calculation */
    if (!is_fahrenheit) {
        temp_f = convert_c_to_f(temperature);
    }
    

    /* Using Rothfusz regression - initial approximation */
    ESP_LOGI(TAG, "Using Rothfusz equation");
    hi = 0.5f * (temp_f + 61.0f + ((temp_f - 68.0f) * 1.2f) + 
                (percent_humidity * 0.094f));
    
    /* Use more accurate Steadman equation for higher temperatures */
    if (hi > 81.0f) {
        ESP_LOGI(TAG, "Using Steadman equation");
        hi = -42.379f + 
             2.04901523f * temp_f + 
             10.14333127f * percent_humidity +
             -0.22475541f * temp_f * percent_humidity +
             -0.00683783f * pow(temp_f, 2.0f) +
             -0.05481717f * pow(percent_humidity, 2.0f) +
             0.00122874f * pow(temp_f, 2.0f) * percent_humidity +
             0.00085282f * temp_f * pow(percent_humidity, 2.0f) +
             -0.00000199f * pow(temp_f, 2.0f) * pow(percent_humidity, 2.0f);
        
        /* Adjustments for specific conditions */
        if ((percent_humidity < 13.0f) && (temp_f >= 80.0f) && (temp_f <= 112.0f)) {
            hi -= ((13.0f - percent_humidity) * 0.25f) * 
                  sqrtf((17.0f - fabsf(temp_f - 95.0f)) * 0.05882f);
        } else if ((percent_humidity > 85.0f) && (temp_f >= 80.0f) && (temp_f <= 87.0f)) {
            hi += ((percent_humidity - 85.0f) * 0.1f) * ((87.0f - temp_f) * 0.2f);
        }
    }
    
    /* Return in requested units */
    return is_fahrenheit ? hi : convert_f_to_c(hi);
}

void app_main(void)
{
    float temperature, humidity, heat_index_c, heat_index_f;
    
    ESP_LOGI(TAG, "DHT22 Driver Starting...");
    
    /* Initialize DHT22 */
    esp_err_t ret = dht22_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DHT22 initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    
    /* Wait for sensor to stabilize */
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (1) {
        /* Read sensor data */
        temperature = read_temperature(false); /* Read in Celsius */
        humidity = read_humidity();
        
        if (!isnan(temperature) && !isnan(humidity)) {
            float temp_f = convert_c_to_f(temperature);
            
            /* Calculate heat index - pass temperature in Celsius, function will convert */
            heat_index_c = compute_heat_index(temperature, humidity, false);
            heat_index_f = compute_heat_index(temp_f, humidity, true);
            
            ESP_LOGI(TAG, "Temperature: %.1f°C (%.1f°F)", temperature, temp_f);
            ESP_LOGI(TAG, "Humidity: %.1f%%", humidity);
            ESP_LOGI(TAG, "Heat Index: %.1f°C (%.1f°F)", heat_index_c, heat_index_f);
            ESP_LOGI(TAG, "---");
        } else {
            ESP_LOGE(TAG, "Failed to read DHT22 sensor");
        }
        
        /* Wait 2 seconds between readings (DHT22 minimum interval) */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}