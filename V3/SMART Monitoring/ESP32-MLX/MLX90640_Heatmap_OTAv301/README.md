# MLX90640 Thermal Heatmap Camera with OTA

Interactive thermal camera with real-time heatmap visualization, multiple color schemes, and Over-The-Air (OTA) wireless firmware updates.

## Features

### 🎨 Visual Features
- **Real-time Heatmap**: 32x24 thermal sensor displayed at actual pixel size (5x5 pixels per thermal point = 160x120 display)
- **5 Color Schemes**:
  - **Ironbow**: Classic thermal imaging (purple → red → yellow → white)
  - **Rainbow**: Full spectrum (blue → green → yellow → red)
  - **Grayscale**: Simple black to white gradient
  - **Hot/Cold**: Blue to white to red gradient
  - **Medical**: Optimized for body temperature detection (30-40°C)
- **Auto-scaling**: Automatic temperature range adjustment
- **Temperature Statistics**: Min, Max, Average, Center point, Ambient

### 📱 User Interface
- **Page 1 - Heatmap**: Live thermal visualization
- **Page 2 - Info**: Device info, WiFi status, IP address, uptime, memory
- **Page 3 - Settings**: Color scheme selection, scaling options
- **On-screen buttons** for navigation (Heat/Info/Set/Color)

### 🔄 OTA Updates (Wireless Programming)
- Upload new firmware over WiFi - **No USB cable needed!**
- Password protected for security
- Visual progress bar on LCD during update
- Much faster than USB uploads

### 📡 Connectivity
- WiFi connection with IP display
- mDNS hostname: `esp32-mlx-thermal.local`
- Network discovery for easy OTA access

## Hardware Requirements

- ESP32-C6 with built-in LCD (170x320 ST7789)
- MLX90640 32x24 thermal sensor
- WiFi network

## Pin Connections

```
MLX90640:
  SDA → GPIO 18
  SCL → GPIO 8
  VCC → 3.3V
  GND → GND

LCD (Built-in):
  DC  → GPIO 6
  CS  → GPIO 7
  SCK → GPIO 5
  MOSI→ GPIO 4
  RST → GPIO 14
  BL  → GPIO 15
```

## Installation

### 1. Arduino IDE Setup

1. Install ESP32 board support in Arduino IDE
2. Install required libraries:
   ```
   - Arduino_GFX_Library (built-in with examples)
   - ArduinoOTA (comes with ESP32 core)
   - WiFi (comes with ESP32 core)
   ```

### 2. Configure WiFi

Edit the sketch to add your WiFi credentials:

```cpp
const char* WIFI_SSID = "YourWiFiName";     // Change this
const char* WIFI_PASS = "YourPassword";      // Change this
const char* OTA_PASSWORD = "mlx90640";       // Change for security!
```

### 3. First Upload (USB)

1. Connect ESP32-C6 via USB
2. Select board: `ESP32C6 Dev Module`
3. Select correct COM port
4. Click Upload
5. Open Serial Monitor (115200 baud)
6. **Note the IP address** displayed (very important!)

## Using OTA (Wireless Updates)

### Quick Start

After the first USB upload:

1. **Find the IP address**:
   - Check Serial Monitor
   - Look at LCD screen (shows IP)
   - Check Page 2 (Info page)

2. **In Arduino IDE**:
   - Go to `Tools` → `Port`
   - Look for network port: `esp32-mlx-thermal at [IP_ADDRESS]`
   - Select this network port

3. **Upload wirelessly**:
   - Make code changes
   - Click Upload
   - Enter password when prompted: `mlx90640` (or your custom password)
   - Watch LCD for progress bar!

### OTA Upload Process

```
1. Click Upload in Arduino IDE
2. IDE connects to device over WiFi
3. LCD shows "OTA UPDATE" with progress bar
4. Upload completes in ~30 seconds
5. Device automatically restarts
6. Done! No USB cable needed!
```

### Troubleshooting OTA

**Network port not showing?**
- Ensure computer and ESP32 are on same WiFi network
- Check firewall isn't blocking port 3232
- Try restarting Arduino IDE
- Verify device is powered on and connected

**Upload fails?**
- Check OTA password is correct
- Ensure device isn't already updating
- Try USB upload if OTA repeatedly fails
- Check Serial Monitor for error messages

**Can't find IP address?**
- Check Serial Monitor after boot
- Look at LCD Info page
- Check your router's connected devices list
- Try USB upload and check Serial Monitor

## Usage

### Controls

**Boot Button (GPIO 9)**:
- **Single press**: Cycle through color schemes (Ironbow → Rainbow → Grayscale → Hot/Cold → Medical)
- **Double press** (within 0.5s): Cycle through pages (Heatmap → Info → Settings)
- Current scheme shown in title bar

**Note**: The physical reset button will reboot the entire device (this is normal hardware behavior). Use the boot button to change modes without rebooting.

**Future Touch Controls** (to be implemented):
- Tap "Heat" button → Heatmap page
- Tap "Info" button → Device info page
- Tap "Set" button → Settings page
- Tap "Color" button → Change color scheme

### Pages

#### Page 1: Heatmap View
- Real-time thermal visualization
- Color-coded temperature display
- Min/Max/Avg temperature statistics
- Current color scheme name
- IP address display

#### Page 2: Device Info
- WiFi connection status
- IP address for OTA
- OTA hostname
- Sensor status
- Ambient temperature
- System uptime
- Free RAM

#### Page 3: Settings
- Color scheme list (5 options)
- Auto-scaling toggle
- Manual range settings
- Freeze frame option

## Color Scheme Guide

### Ironbow (Default)
Best for: General thermal imaging
- Purple/Blue: Cold
- Red: Medium
- Yellow: Hot
- White: Very hot

### Rainbow
Best for: Maximum color differentiation
- Blue: Cold
- Green: Cool
- Yellow: Warm
- Red: Hot

### Grayscale
Best for: Simple visualization, printing
- Black: Cold
- Gray: Medium
- White: Hot

### Hot/Cold
Best for: Detecting hot spots
- Blue: Cold areas
- White: Neutral
- Red: Hot areas

### Medical
Best for: Body temperature (30-40°C)
- Blue/Green: Below body temp
- Yellow/Orange: Normal body temp
- Red: Fever range

## Technical Specifications

- **Sensor**: MLX90640 32x24 (768 pixels)
- **Display**: ST7789 170x320 LCD
- **Refresh Rate**: ~4 Hz (250ms update)
- **Temperature Range**: -40°C to +300°C (sensor capability)
- **Accuracy**: ±1°C (typical)
- **Pixel Size**: 5x5 pixels per thermal point (shows actual sensor resolution)
- **WiFi**: 2.4GHz 802.11 b/g/n
- **OTA Port**: 3232 (default)

## Performance Tips

### For Best Thermal Images:
1. Allow sensor to warm up for 30 seconds
2. Keep sensor steady (no rapid movements)
3. Ideal distance: 0.5m to 3m from target
4. Avoid reflective surfaces
5. Use Medical scheme for body temperature

### For Fastest OTA Updates:
1. Stay close to WiFi router
2. Use 2.4GHz WiFi (not 5GHz)
3. Minimize other network traffic
4. Typical upload time: 20-40 seconds

## Customization

### Change OTA Hostname

```cpp
const char* OTA_HOSTNAME = "your-custom-name";  // Changes to your-custom-name.local
```

### Adjust Update Rate

```cpp
#define MLX_REFRESH 0x05  // 4Hz
// Options: 0x00=0.5Hz, 0x02=2Hz, 0x05=4Hz, 0x06=8Hz
```

### Change Pixel Size

```cpp
#define PIXEL_W 5   // Pixel width (5 = actual size, fits 32x5=160 on screen)
#define PIXEL_H 5   // Pixel height (5 = actual size, 24x5=120 pixels)
// Current setting shows actual 32x24 thermal sensor resolution
// Larger values = bigger pixels but same 32x24 data
```

## Advanced Features (Future)

- [ ] Touch screen integration for button controls
- [ ] Data logging to SD card
- [ ] Temperature alerts/alarms
- [ ] WiFi configuration portal
- [ ] Web server for remote viewing
- [ ] Bluetooth thermal data streaming
- [ ] Custom color palette creation

## Security Notes

⚠️ **Important**: Change the OTA password before deploying!

```cpp
const char* OTA_PASSWORD = "your-secure-password";  // CHANGE THIS!
```

The default password is `mlx90640`. Anyone on your network could update your device if you don't change it!

## Troubleshooting

### Black Screen
- Check backlight is on (LCD_BL = LOW)
- Verify LCD connections
- Try USB upload with Serial Monitor

### No WiFi Connection
- Verify SSID and password
- Check WiFi is 2.4GHz (not 5GHz only)
- Ensure WiFi router is in range
- Try restarting device

### Sensor Error
- Check I2C wiring (SDA/SCL)
- Verify 3.3V power supply
- Try different I2C address if needed
- Check Serial Monitor for error messages

### Slow Updates
- Normal: ~4 updates per second
- If slower: Check sensor communication
- Reduce other CPU tasks if needed

## Serial Monitor Output

Expected startup sequence:

```
=== MLX90640 Thermal Heatmap ===
=== With OTA Updates ===
[LCD] Init...
[LCD] Ready
[WiFi] Connecting...
[WiFi] Connected: 192.168.1.100
[OTA] Ready
[OTA] Password: mlx90640
[MLX] Init...
[MLX] Ready
[READY] System initialized
[OTA] Upload via: 192.168.1.100
```

## Support

For issues or questions:
1. Check Serial Monitor for error messages
2. Verify hardware connections
3. Try USB upload first
4. Check WiFi signal strength

## License

Open source - feel free to modify and share!

---

**Enjoy your wireless thermal camera! 🌡️📡**
