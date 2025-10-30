# Sensor Build Configuration Guide

This ESP32 project supports multiple sensor types with a single codebase. You can choose which sensor implementation to build using CMake configuration.

## Supported Sensors

### MQ-135 Air Quality Sensor
- **File**: `main/blink_example_main.c`
- **Purpose**: Air quality monitoring, CO2 detection
- **Measurements**: CO2 PPM, Air Quality Index
- **Default**: This is the default build target

### MQ-2 Gas Sensor  
- **File**: `main/mq2_sensor_main.c`
- **Purpose**: Combustible gas detection
- **Measurements**: LPG, Methane, Alcohol concentrations with safety status
- **Features**: Multi-gas detection with danger/warning thresholds

## Build Instructions

### Option 1: Using idf.py (Recommended)

#### Build for MQ-135 (Default)
```bash
idf.py build
```

#### Build for MQ-2
```bash
idf.py -D SENSOR_TYPE=MQ2 build
```

### Option 2: Using CMake directly

#### Build for MQ-135
```bash
idf.py -D SENSOR_TYPE=MQ135 build
```

#### Build for MQ-2
```bash
idf.py -D SENSOR_TYPE=MQ2 build
```

### Option 3: Set in sdkconfig

You can also add the following line to your `sdkconfig` file:
```
CONFIG_SENSOR_TYPE="MQ2"
```

## Flashing and Monitoring

After building, flash and monitor as usual:
```bash
idf.py flash monitor
```

## Configuration Parameters

Both sensor implementations share similar configuration parameters that you can modify in their respective source files:

- **WiFi Settings**: Update `WIFI_SSID` and `WIFI_PASS`
- **Server URL**: Update `SERVER_URL` for your data collection server  
- **GPIO Pins**: Modify `ADC_CHANNEL` for your hardware setup
- **Timing**: Adjust `SENSOR_READ_INTERVAL_MS` and warmup times
- **Calibration**: Update sensor-specific calibration parameters

## Hardware Connections

### MQ-135 (Air Quality)
- VCC → 3.3V or 5V
- GND → GND  
- AO → GPIO36 (ADC1_CH0) - configurable

### MQ-2 (Gas Detection)
- VCC → 3.3V or 5V
- GND → GND
- AO → GPIO36 (ADC1_CH0) - configurable

## Data Format

Both sensors send JSON data to the configured server with device identification and sensor-specific measurements.

## MISRA-C Compliance

Both implementations follow MISRA-C guidelines for robust infrastructure applications:
- Proper error handling with return codes
- Bounded operations and safe calculations  
- Static memory allocation where possible
- Comprehensive input validation