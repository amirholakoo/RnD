# MLX90640 Thermal Camera Heatmap Project

## Overview
Create an interactive thermal camera display with touch controls, OTA updates, and professional features.

## Features to Implement

### 1. Core Thermal Camera
- MLX90640 32x24 thermal sensor integration
- Real-time heatmap rendering on 170x320 LCD
- Optimized frame rate (4Hz refresh)
- Temperature range detection and display

### 2. Color Schemes (Touch Selectable)
- **Ironbow** - Classic thermal (purple→red→yellow→white)
- **Rainbow** - Full spectrum (blue→green→yellow→red)
- **Grayscale** - Black and white thermal
- **Hot/Cold** - Blue to red gradient
- **Medical** - Optimized for body temperature

### 3. Touch Interface Pages
- **Page 1: Heatmap View** - Full screen thermal visualization
- **Page 2: Device Info** - IP, WiFi, uptime, memory
- **Page 3: Settings** - Color scheme selector, calibration
- Swipe/tap navigation between pages

### 4. OTA (Over-The-Air) Updates
- ArduinoOTA library integration
- WiFi-based firmware updates
- Password protection
- Status display during update
- Automatic rollback on failure

### 5. Display Features
- Min/max temperature markers
- Center point temperature
- Auto-scaling temperature range
- FPS counter
- Color legend/scale bar

### 6. Additional Features
- Temperature statistics (avg, min, max)
- Freeze frame capability (touch hold)
- Screenshot/data logging capability
- WiFi reconnection handling
- Low memory warning

## Technical Implementation

### Pin Configuration
- MLX90640: I2C (SDA=18, SCL=8)
- LCD: SPI (DC=6, CS=7, SCK=5, MOSI=4, RST=14, BL=15)
- Touch: I2C (shared with MLX, address different)

### Color Scheme Algorithm
For each pixel temperature:
1. Normalize to 0-1 range based on min/max
2. Map to color palette (256 colors)
3. Render as 5x10 pixel blocks (32x24 → 160x240)
4. Leave space for UI elements

### OTA Setup Instructions
1. Upload initial firmware via USB
2. Connect to WiFi (display shows IP)
3. In Arduino IDE: Tools → Port → Network Port (IP)
4. Upload code wirelessly (much faster!)

### Memory Optimization
- Use PROGMEM for color palettes
- Double buffering for smooth rendering
- Efficient thermal data storage

## File Structure
```
MLX90640_Heatmap_OTA/
├── MLX90640_Heatmap_OTA.ino (main file)
├── MLX90640_API.cpp/h (sensor library)
├── MLX90640_I2C_Driver.cpp/h (I2C driver)
├── ColorSchemes.h (color palette definitions)
└── README.md (setup instructions)
```

## OTA Usage Guide

### First Time Setup
1. Upload via USB cable
2. Open Serial Monitor
3. Note the IP address displayed
4. Wait for "OTA Ready" message

### Wireless Upload
1. In Arduino IDE: Tools → Port
2. Select network port: "esp32-mlx at [IP]"
3. Click Upload
4. Watch LCD for update progress
5. Device auto-restarts when done

### Troubleshooting
- If OTA port not visible: Check same WiFi network
- If upload fails: Try USB upload again
- Check firewall settings if needed

## Next Steps
1. Create main sketch with all features
2. Implement color schemes
3. Add touch handling
4. Test OTA functionality
5. Optimize performance