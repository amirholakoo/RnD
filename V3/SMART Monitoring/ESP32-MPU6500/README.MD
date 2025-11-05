# MPU6500 + ESP32-S3 Dev Kit (WROOM-1)
### Smart Vibration Monitor with Wi‑Fi Data Logging & Status LED

This project implements a **vibration monitoring system** using an **ESP32-S3 Dev Kit (WROOM-1)** and an **MPU6500** accelerometer/gyroscope sensor. The system measures 3-axis acceleration, filters out gravity, detects abnormal vibration patterns, and sends JSON-formatted data to a remote HTTP server via Wi‑Fi.

An onboard **WS2812 RGB LED (GPIO38)** provides real-time visual feedback on system status, Wi‑Fi connectivity, and vibration activity.

---

## Key Features

- Real-time vibration monitoring with adaptive baseline learning
- Automatic drift and shock detection
- Wi‑Fi data upload to a configurable HTTP endpoint
- Non-blocking LED state machine for visual feedback
- Periodic health reporting (every 60 seconds)
- Watchdog timer (3 minutes) for reliability
- Configurable thresholds for shock, drift, and noise levels
- Low-pass filtering to remove gravity and sensor noise
- Efficient ring buffer averaging for adaptive calibration

---

## Hardware Components

| Component | Description | Connection |
|-----------|-------------|------------|
| **ESP32-S3 Dev Kit (WROOM-1)** | Main microcontroller with built-in WS2812 RGB LED | — |
| **MPU6500** | 3-axis accelerometer and gyroscope (I²C or SPI supported) | I²C (SDA=GPIO8, SCL=GPIO9) |
| **On-board WS2812 LED** | Status indication | GPIO38 |

> **Note:** The MPU6500 can communicate via **SPI** instead of I²C. Switching the MPU to SPI frees the I²C bus for other devices such as an **OLED display (SSD1306)** that can mirror the Serial Monitor output.

---

## Pin Configuration (Default: I²C)

| Signal | ESP32-S3 Pin | Description |
|--------|---------------|-------------|
| SDA | GPIO 8 | I²C data line |
| SCL | GPIO 9 | I²C clock line |
| WS2812 | GPIO 38 | Built-in RGB LED control |
| 3V3 | 3.3V | Sensor power |
| GND | GND | Common ground |

---

## LED Status Indicators

| LED Color | Mode | Meaning |
|-----------|------|---------|
| 🔵 Blue | Solid | Booting |
| 🟡 Yellow | Solid | Connecting to Wi‑Fi |
| 🟡 Yellow | Blinking | Wi‑Fi error (retrying) |
| ⚪ White | Dim solid | Normal operation |
| 🟢 Green | Two short pulses | Successful periodic data send |
| 🔴 Red | Fast blink | Active vibration detected |

---

## Network Configuration

The sketch requires you to set your Wi‑Fi credentials and server URL before uploading. Edit the following variables in the sketch:

```cpp
const char* ssid = "<YOUR_SSID>";
const char* password = "<YOUR_PASSWORD>";
const char* serverUrl = "http://<your-server-ip-or-domain>:<port>";
```

A fallback MAC address is used if the ESP32 cannot read its own MAC address.

---

## JSON Data Format

Example payload sent via HTTP POST:

```json
{
  "device_id": "xx:xx:xx:xx:xx:xx",
  "sensor_type": "MPU6500",
  "alarm": false,
  "data": {
    "Vx": 0.0041,
    "Vy": 0.0032,
    "Vz": 0.0027,
    "Vtotal": 0.0059
  }
}
```

- `"alarm": true` → vibration event
- `"alarm": false` → periodic transmission (normal condition)

---

## Detection Algorithm Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `baseline` | Adaptive quiet-level reference | 0.05 |
| `noiseFloor` | Minimum measurable vibration | 0.02 |
| `shockFactor` | Instant shock threshold | 1.8 × baseline |
| `driftFactor` | Average drift threshold | 1.3 × baseline |
| `learningRate` | Baseline adaptation rate | 0.002 (0.2%) |
| `vibTimeout` | Time to consider vibration stopped | 500 ms |
| `alarmCooldown` | Time between alarm sends | 2 s |

The system applies a **low-pass filter** (α = 0.9) to isolate vibration from gravity and maintains a **rolling average buffer** for drift detection.

---

## Timing Summary

| Action | Interval | Purpose |
|--------|----------|---------|
| Periodic send | 60 s | Regular data report |
| Alarm send | 2 s | While vibration active |
| Serial output | 2.5 s | Debug info |
| Wi‑Fi retry | 30 s | Reconnect attempt |
| Watchdog timeout | 180 s | System recovery |

---

## Watchdog

A 3‑minute watchdog timer (`esp_task_wdt`) ensures the system automatically resets if the loop stalls. The code calls `esp_task_wdt_reset()` on each loop iteration.

---

## Optional: SPI + OLED

If you prefer to use **SPI** for the MPU6500 to obtain higher throughput or to free the I²C bus, you can reconfigure the sensor to SPI and connect an **I²C OLED (SSD1306)** to display real‑time values. This allows mirroring everything that the Serial Monitor prints onto a compact display using `Adafruit_SSD1306` or `U8g2` libraries.

Example OLED snippet (conceptual):

```cpp
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void showStatus(float Vx, float Vy, float Vz, float Vtotal, float base) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Vx: %.3f Vy: %.3f", Vx, Vy);
  display.setCursor(0, 10);
  display.printf("Vz: %.3f V: %.3f", Vz, Vtotal);
  display.setCursor(0, 20);
  display.printf("Base: %.3f", base);
  display.display();
}
```

---

## Required Libraries

- `WiFi.h` (ESP32 core)
- `HTTPClient.h` (ESP32 core)
- `Wire.h`
- `Adafruit_NeoPixel`
- `esp_task_wdt.h`
- *(optional)* `Adafruit_SSD1306` (for OLED display)

---

## Build & Upload

1. Install **Arduino IDE 2.x** or **PlatformIO**.  
2. Install the **ESP32 board package** (Espressif).  
3. Select **ESP32-S3 Dev Module** board.  
4. Copy `mpu6500_esp32s3.ino` to your project folder and open it.  
5. Set your Wi‑Fi credentials and `serverUrl` in the sketch.  
6. Compile and upload.

---

## Project Structure

```
mpu6500_esp32s3/
├── mpu6500_esp32s3.ino
├── README.md
└── (libraries managed by the IDE)
```

---

## Example Serial Output

```
Calibrating baseline (2s) ...
✅ Baseline = 0.0483
✅ WiFi connected
Vx:0.0011 Vy:-0.0008 Vz:0.0022 | V:0.0035 | avg:0.0501 | base:0.0483
HTTP Response: 200
