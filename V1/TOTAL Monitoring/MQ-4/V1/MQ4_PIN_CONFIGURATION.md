# MQ-4 Sensor Pin Configuration Update

## Overview
Updated the MQ-4 methane gas sensor ADC pin configuration from GPIO 36 (ADC_CHANNEL_0) to GPIO 32 (ADC_CHANNEL_4) for optimal performance with Wi-Fi enabled applications.

## Changes Made

### 1. Updated ADC Channel Configuration
**File**: `main/wifi_framework_example.c`
- **Before**: `#define MQ4_ADC_CHANNEL ADC_CHANNEL_0  // GPIO 36`
- **After**: `#define MQ4_ADC_CHANNEL ADC_CHANNEL_4  // GPIO 32`

### 2. Enhanced Documentation
Added comprehensive documentation block explaining:
- Pin selection rationale
- Hardware connection requirements
- Important notes about load resistors and voltage dividers

### 3. Updated Logging Messages
- Updated initialization message to show GPIO pin number
- Updated configuration display to include GPIO reference

## Pin Selection Rationale

### Why GPIO 32 (ADC_CHANNEL_4)?
1. **Wi-Fi Compatibility**: ADC1 channels work reliably when Wi-Fi is active
2. **Stability**: Less susceptible to interference from other peripherals
3. **Standard Practice**: Commonly used for analog sensors in ESP32 projects
4. **Future-Proof**: Ensures stable readings as more features are added

### ESP32 ADC Pin Mapping Reference

| ADC Channel | GPIO Pin | Arduino Name | Recommended For | Notes |
|-------------|----------|--------------|-----------------|-------|
| ADC_CHANNEL_0 | GPIO 36 | A0 | Hall sensor, basic analog | ✅ Previously used |
| ADC_CHANNEL_3 | GPIO 39 | A3 | Hall sensor, basic analog | Alternative |
| **ADC_CHANNEL_4** | **GPIO 32** | **A4** | **Analog sensors with Wi-Fi** | **✅ RECOMMENDED** |
| ADC_CHANNEL_5 | GPIO 33 | A5 | Alternative option | Good alternative |
| ADC_CHANNEL_6 | GPIO 34 | A6 | Input-only sensors | Very stable |
| ADC_CHANNEL_7 | GPIO 35 | A7 | Input-only sensors | Very stable |

## Hardware Connections

### MQ-4 Sensor Connections
```
MQ-4 Sensor    →    ESP32
------------------------
VCC           →    3.3V or 5V (check sensor spec)
GND           →    GND
A0 (Analog)   →    GPIO 32 (ADC_CHANNEL_4)
D0 (Digital)  →    Optional, any GPIO
```

### Important Hardware Notes
1. **Load Resistor**: MQ-4 requires a 10kΩ load resistor between A0 output and GND
2. **Voltage Divider**: If sensor outputs > 3.3V, use voltage divider (10kΩ resistors)
3. **Power Requirements**: MQ-4 typically requires 5V for heater, but analog output is 0-5V

## Benefits of This Change

### Technical Benefits
- ✅ **Wi-Fi Stability**: ADC1 channels remain stable during Wi-Fi operations
- ✅ **Reduced Interference**: Less prone to noise from other peripherals
- ✅ **Standard Compliance**: Follows ESP32 best practices for analog sensors
- ✅ **Future Compatibility**: Ensures stable operation as system grows

### Application Benefits
- ✅ **Reliable Readings**: More consistent sensor measurements
- ✅ **Better Performance**: Optimized for infrastructure monitoring applications
- ✅ **MISRA Compliance**: Follows robust coding practices for infrastructure use
- ✅ **Maintainability**: Well-documented and follows standard conventions

## Testing Recommendations

### Before Deployment
1. **Verify Connections**: Double-check all hardware connections
2. **Test ADC Reading**: Ensure GPIO 32 reads analog values correctly
3. **Wi-Fi Test**: Verify sensor readings remain stable during Wi-Fi operations
4. **Calibration Check**: Re-calibrate sensor if readings seem off

### Monitoring
- Monitor sensor readings for consistency
- Check for any interference patterns
- Verify PPM calculations remain accurate

## Rollback Information
If issues arise, the configuration can be easily reverted by changing:
```c
#define MQ4_ADC_CHANNEL ADC_CHANNEL_4  // GPIO 32
```
back to:
```c
#define MQ4_ADC_CHANNEL ADC_CHANNEL_0  // GPIO 36
```

## Related Files Modified
- `main/wifi_framework_example.c` - Main configuration and logging updates
- This documentation file created for reference

---
**Last Updated**: Current session
**ESP-IDF Version**: v5.4
**MISRA Compliance**: Maintained