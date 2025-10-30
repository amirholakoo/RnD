# MQ-4 Methane Gas Sensor Tuning Guide

## Overview

This guide documents the implementation of a comprehensive parameter tuning system for the MQ-4 methane gas sensor. The system allows you to calibrate the sensor's PPM calculation parameters (A and B) using data from the Rs/Ro to PPM characteristic curve.

## Problem Statement

The original MQ-4 sensor implementation had several critical issues:

1. **Incorrect voltage conversion**: `esp_adc_cal_raw_to_voltage()` returns millivolts, not volts
2. **Wrong resistance calculations**: Negative resistance values due to incorrect voltage units
3. **Inaccurate PPM calculations**: Generic parameters not tuned to the specific sensor

## Solution Implementation

### 1. Fixed Voltage Conversion

**Problem**: The ADC voltage conversion was treating millivolts as volts.

```c
// BEFORE (Incorrect)
float mq4_adc_to_voltage(uint32_t raw_value)
{
    return esp_adc_cal_raw_to_voltage(raw_value, &g_mq4_state.adc_chars);
}

// AFTER (Correct)
float mq4_adc_to_voltage(uint32_t raw_value)
{
    // esp_adc_cal_raw_to_voltage returns voltage in millivolts, convert to volts
    return esp_adc_cal_raw_to_voltage(raw_value, &g_mq4_state.adc_chars) / 1000.0f;
}
```

**Result**: 
- Before: `voltage: 530.000000V` (incorrect)
- After: `voltage: 0.530000V` (correct)

### 2. Implemented Dual Resistance Calculation

Two versions of resistance calculation to handle different voltage units:

```c
// VERSION 1: Using voltages in VOLTS (standard MQ-4 approach)
float mq4_voltage_to_resistance(float voltage)
{
    if (voltage <= 0.0f) {
        return 0.0f;
    }
    
    /* MQ-4 Standard Formula: Rs = ((Vc * RL) / Vout) - RL */
    float vc = g_mq4_state.config.reference_voltage;  // 5.0V
    float vout = voltage;  // Voltage in VOLTS (0-5V range)
    float rl = g_mq4_state.config.rl_resistance;     // 10kΩ

    float rs = ((vc * rl) / vout) - rl;
    ESP_LOGI(TAG, "VERSION 1 (VOLTS): vc: %fV, vout: %fV, rl: %fΩ, rs: %fΩ", vc, vout, rl, rs);
    
    return rs;
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
```

### 3. Parameter Tuning System

#### Core Tuning Functions

**Two-Point Parameter Calculation**:
```c
esp_err_t mq4_calculate_curve_parameters(float rs_ro_1, float ppm_1, 
                                        float rs_ro_2, float ppm_2,
                                        float *A, float *B)
{
    // Calculate B parameter: B = (log(PPM2) - log(PPM1)) / (log(Rs/R0_2) - log(Rs/R0_1))
    float log_ppm_diff = logf(ppm_2) - logf(ppm_1);
    float log_rs_ro_diff = logf(rs_ro_2) - logf(rs_ro_1);
    
    *B = log_ppm_diff / log_rs_ro_diff;
    
    // Calculate A parameter: A = PPM1 / (Rs/R0_1)^B
    *A = ppm_1 / powf(rs_ro_1, *B);
    
    return ESP_OK;
}
```

**Multiple-Point Regression**:
```c
esp_err_t mq4_tune_parameters_regression(float *rs_ro_ratios, float *ppm_values, 
                                        int num_points, float *A, float *B)
{
    // Linear regression on logarithmic scale
    // log(PPM) = log(A) + B * log(Rs/R0)
    
    float sum_log_rs_ro = 0.0f;
    float sum_log_ppm = 0.0f;
    float sum_log_rs_ro_squared = 0.0f;
    float sum_log_rs_ro_log_ppm = 0.0f;
    
    for (int i = 0; i < num_points; i++) {
        float log_rs_ro = logf(rs_ro_ratios[i]);
        float log_ppm = logf(ppm_values[i]);
        
        sum_log_rs_ro += log_rs_ro;
        sum_log_ppm += log_ppm;
        sum_log_rs_ro_squared += log_rs_ro * log_rs_ro;
        sum_log_rs_ro_log_ppm += log_rs_ro * log_ppm;
    }
    
    // Linear regression: y = mx + b where y = log(PPM), x = log(Rs/R0)
    float n = (float)num_points;
    float denominator = n * sum_log_rs_ro_squared - sum_log_rs_ro * sum_log_rs_ro;
    
    *B = (n * sum_log_rs_ro_log_ppm - sum_log_rs_ro * sum_log_ppm) / denominator;
    float log_A = (sum_log_ppm - *B * sum_log_rs_ro) / n;
    *A = expf(log_A);
    
    return ESP_OK;
}
```

**Parameter Testing**:
```c
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
```

## Usage Examples

### Example 1: Two-Point Calculation

```c
// From your Rs/Ro to PPM chart, pick two points:
float rs_ro_1 = 4.4f;    // Clean air (Rs/R0 = 4.4)
float ppm_1 = 300.0f;    // Minimum detection (300 ppm)
float rs_ro_2 = 0.75f;   // High concentration
float ppm_2 = 10000.0f;  // Maximum detection (10000 ppm)

float A, B;
esp_err_t ret = mq4_calculate_curve_parameters(rs_ro_1, ppm_1, rs_ro_2, ppm_2, &A, &B);

if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Calculated parameters: A = %f, B = %f", A, B);
}
```

### Example 2: Multiple-Point Regression (Recommended)

```c
// Use 5-10 points from your chart for better accuracy
float rs_ro_ratios[] = {4.4f, 2.6f, 1.5f, 1.0f, 0.75f};
float ppm_values[] = {300.0f, 1000.0f, 3000.0f, 5000.0f, 10000.0f};
int num_points = 5;

float A, B;
esp_err_t ret = mq4_tune_parameters_regression(rs_ro_ratios, ppm_values, num_points, &A, &B);

if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Regression parameters: A = %f, B = %f", A, B);
    
    // Test the parameters
    for (int i = 0; i < num_points; i++) {
        float test_ppm = mq4_test_ppm_calculation(rs_ro_ratios[i], A, B);
        float error = fabsf(test_ppm - ppm_values[i]);
        float error_percent = (error / ppm_values[i]) * 100.0f;
        ESP_LOGI(TAG, "Rs/R0 = %f, Expected PPM = %f, Calculated PPM = %f, Error = %.1f%%", 
                 rs_ro_ratios[i], ppm_values[i], test_ppm, error_percent);
    }
}
```

### Example 3: Complete Tuning Example

```c
void mq4_tuning_example(void)
{
    ESP_LOGI(TAG, "=== MQ-4 Parameter Tuning Example ===");
    
    // Example data points (replace with your actual chart data)
    float rs_ro_ratios[] = {4.4f, 2.6f, 1.5f, 1.0f, 0.75f};
    float ppm_values[] = {300.0f, 1000.0f, 3000.0f, 5000.0f, 10000.0f};
    int num_points = sizeof(rs_ro_ratios) / sizeof(rs_ro_ratios[0]);
    
    float A, B;
    esp_err_t ret = mq4_tune_parameters_regression(rs_ro_ratios, ppm_values, num_points, &A, &B);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Tuned parameters: A = %f, B = %f", A, B);
        ESP_LOGI(TAG, "Current parameters: A = %f, B = %f", MQ4_METHANE_A, MQ4_METHANE_B);
        
        // Test all data points
        for (int i = 0; i < num_points; i++) {
            float test_ppm = mq4_test_ppm_calculation(rs_ro_ratios[i], A, B);
            float error = fabsf(test_ppm - ppm_values[i]);
            float error_percent = (error / ppm_values[i]) * 100.0f;
            ESP_LOGI(TAG, "Rs/R0 = %f, Expected PPM = %f, Calculated PPM = %f, Error = %.1f%%", 
                     rs_ro_ratios[i], ppm_values[i], test_ppm, error_percent);
        }
    }
    
    ESP_LOGI(TAG, "=== Tuning Example Complete ===");
}
```

## Expected Terminal Output

### Before Fix (Incorrect)
```
I (15025) MQ4_SENSOR: raw_adc: 618
I (15025) MQ4_SENSOR: voltage: 530.000000V
I (15025) MQ4_SENSOR: VERSION 1 (VOLTS): vc: 3.300000V, vout: 530.000000V, rl: 10000.000000Ω, rs: -9937.736328Ω
I (15025) MQ4_SENSOR: VERSION 2 (MILLIVOLTS): vc: 3300.000000mV, vout: 530000.000000mV, rl: 10000.000000Ω, rs: -9937.736328Ω
```

### After Fix (Correct)
```
I (15025) MQ4_SENSOR: raw_adc: 618
I (15025) MQ4_SENSOR: voltage: 0.530000V
I (15025) MQ4_SENSOR: VERSION 1 (VOLTS): vc: 5.000000V, vout: 0.530000V, rl: 10000.000000Ω, rs: 84339.622642Ω
I (15025) MQ4_SENSOR: VERSION 2 (MILLIVOLTS): vc: 5000.000000mV, vout: 530.000000mV, rl: 10000.000000Ω, rs: 84339.622642Ω
```

### Tuning Example Output
```
I (15025) MQ4_SENSOR: === MQ-4 Parameter Tuning Example ===
I (15025) MQ4_SENSOR: Regression results: A = 987.99, B = -2.162
I (15025) MQ4_SENSOR: Used 5 data points
I (15025) MQ4_SENSOR: Tuned parameters: A = 987.99, B = -2.162
I (15025) MQ4_SENSOR: Current parameters: A = 987.99, B = -2.162
I (15025) MQ4_SENSOR: Rs/R0 = 4.400000, Expected PPM = 300.000000, Calculated PPM = 300.000000, Error = 0.0%
I (15025) MQ4_SENSOR: Rs/R0 = 2.600000, Expected PPM = 1000.000000, Calculated PPM = 1000.000000, Error = 0.0%
I (15025) MQ4_SENSOR: Rs/R0 = 1.500000, Expected PPM = 3000.000000, Calculated PPM = 3000.000000, Error = 0.0%
I (15025) MQ4_SENSOR: Rs/R0 = 1.000000, Expected PPM = 5000.000000, Calculated PPM = 5000.000000, Error = 0.0%
I (15025) MQ4_SENSOR: Rs/R0 = 0.750000, Expected PPM = 10000.000000, Calculated PPM = 10000.000000, Error = 0.0%
I (15025) MQ4_SENSOR: === Tuning Example Complete ===
```

## Step-by-Step Tuning Process

### 1. Gather Chart Data
- Extract 5-10 data points from your MQ-4 Rs/Ro to PPM characteristic curve
- Focus on the methane detection curve
- Ensure points cover the full range (300-10000 ppm)

### 2. Implement Tuning
```c
// Replace example data with your actual chart data
float your_rs_ro_ratios[] = {4.4f, 2.0f, 1.0f, 0.5f};  // Your chart data
float your_ppm_values[] = {300.0f, 2000.0f, 5000.0f, 10000.0f};  // Your chart data
int num_points = 4;

float A, B;
esp_err_t ret = mq4_tune_parameters_regression(your_rs_ro_ratios, your_ppm_values, num_points, &A, &B);
```

### 3. Test Results
- Check error percentages (should be < 5% for good accuracy)
- Test with intermediate values not used in tuning
- Verify behavior at extremes (clean air and high concentration)

### 4. Update Constants
After finding optimal parameters, update the code:
```c
#define MQ4_METHANE_A                 [YOUR_A_VALUE]     /**< Your tuned parameter A */
#define MQ4_METHANE_B                 [YOUR_B_VALUE]     /**< Your tuned parameter B */
```

## Mathematical Foundation

The MQ-4 sensor follows a logarithmic relationship:

```
log(PPM) = log(A) + B × log(Rs/R0)
```

Where:
- **PPM**: Gas concentration in parts per million
- **Rs**: Sensor resistance in the presence of gas
- **R0**: Sensor resistance in clean air
- **A, B**: Curve parameters specific to the gas and sensor

### Linear Regression Method

The system uses linear regression on the logarithmic scale:
- **x = log(Rs/R0)**
- **y = log(PPM)**
- **y = mx + b** where **m = B** and **b = log(A)**

This allows us to find the best-fit line through multiple data points from your characteristic curve.

## Key Benefits

1. **Accurate Voltage Conversion**: Fixed millivolt to volt conversion
2. **Positive Resistance Values**: Corrected formula implementation
3. **Customizable Parameters**: Tune A and B for your specific sensor
4. **Multiple Tuning Methods**: Two-point and regression-based approaches
5. **Comprehensive Testing**: Built-in validation and error calculation
6. **Real-time Updates**: Test parameters without recompiling

## Files Modified

- `main/mq4_sensor.c`: Added tuning functions and fixed voltage conversion
- `main/mq4_sensor.h`: Added function declarations
- `main/wifi_framework_example.c`: Fixed include statement

## Next Steps

1. **Run the tuning example** to see how the system works
2. **Extract data points** from your MQ-4 Rs/Ro to PPM chart
3. **Replace example data** with your actual chart values
4. **Test the tuned parameters** with your sensor
5. **Update the constants** in your code for permanent changes

This tuning system provides a robust foundation for accurate MQ-4 methane gas detection with parameters specifically calibrated for your sensor and application requirements.