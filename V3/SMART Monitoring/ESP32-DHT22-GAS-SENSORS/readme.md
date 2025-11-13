# ESP32-S3 Multi-Gas Environmental Monitoring System (DHT22 + Gas Sensors)

This project implements a **real-time environmental monitoring system** using an **ESP32-S3 DEV KIT (WROOM1)**. It collects data from multiple sensors: DHT22 for temperature & humidity, and gas sensors for Methane (CH₄), Odor detection, and Smoke detection. The system calculates sensor resistances, converts them to PPM concentrations, and sends JSON-formatted data to a server over Wi-Fi at regular intervals.

---

## 📘 Features

- ⚡ Real-time environmental and gas detection.  
- 🌐 Wi-Fi connectivity with automatic reconnect only when disconnected.  
- 🧠 Automatic computation of sensor resistance (Rs) and PPM values.  
- 📡 Periodic JSON data transmission to a configurable HTTP server.  
- 🛡️ Watchdog Timer (WDT) protection for system reliability.  
- 🧾 Serial debug logging for monitoring values and connection status.  
- 💡 Modular design: Each sensor handled with separate processing functions.

---

## 🧱 Hardware Setup

### 🧩 Components

| Component | Description |
|-----------|-------------|
| ESP32-S3 DEV KIT (WROOM1) | Main microcontroller board |
| DHT22 | Temperature & Humidity sensor |
| Methane (CH₄) sensor | Gas sensor for methane detection |
| Odor detection sensor | Gas sensor for odor detection |
| Smoke detection sensor | Gas sensor for smoke detection |
| Connecting wires | Jumper cables |
| Power source | 3.3V regulated (from ESP32) |

### ⚙️ Pin Connections

| Sensor | ESP32 Pin | Notes |
|--------|-----------|------|
| DHT22 | GPIO 4 | Data pin |
| Methane (CH₄) | GPIO 2 | Analog output |
| Odor detection | GPIO 1 | Analog output |
| Smoke detection | GPIO 3 | Analog output |

> ⚠️ Use 3.3V for all sensors to ensure ESP32 ADC compatibility.

---

## 🧮 Sensor Calibration & PPM Calculations

Sensor resistance Rs and PPM concentrations are computed as follows:

```
Rs = RL * (Vin - Vout) / Vout
PPM = A * (Rs / Ro) ^ B
```

Where:  
- **Vin**: Input voltage (3300 mV)  
- **RL**: Load resistor (varies per sensor)  
- **Ro**: Sensor baseline resistance (varies per sensor)  
- **A, B**: Empirical constants (specific to each sensor)  

Each sensor’s Rs is computed from measured analog voltage, then converted to PPM using the sensor’s calibration constants.

---

## 🖥️ Software Setup

### Required Libraries

- `WiFi.h` (ESP32 core)  
- `HTTPClient.h`  
- `DHT.h`  
- `cJSON.h` (for JSON encoding)  
- `esp_task_wdt.h` (for watchdog timer)

### Configuration

Edit these parameters in the source code:

```cpp
const char* WIFI_SSID = "Homayoun";
const char* WIFI_PASS = "1q2w3e4r$@";
const char* RPI_HOST = "192.168.2.20";
const uint16_t RPI_PORT = 7500;
const uint32_t SAMPLE_EVERY_SEC = 60; // Interval in seconds
```

### Upload Instructions

1. Open the `.ino` file in Arduino IDE or PlatformIO.  
2. Select board: **ESP32S3 Dev Module**.  
3. Set the correct COM port.  
4. Compile and upload.  
5. Open Serial Monitor (115200 baud) for debugging.

---

## 📡 JSON Data Format

### DHT22 Example
```json
{
  "device_id": "B4:3A:45:3F:D4:58",
  "sensor_type": "dht22",
  "data": {
    "temperature": 25.3,
    "humidity": 48.2,
    "timestamp": 12345678,
    "type": "dht22_data"
  }
}
```

### Methane (CH₄) Example
```json
{
  "device_id": "B4:3A:45:3F:D4:58",
  "sensor_type": "ch4_gas",
  "data": {
    "voltage_mv": 950,
    "resistance_kohm": 2.5,
    "ppm_ch4": 1.23,
    "ppm_c3h8": 0.54,
    "timestamp": 12345678,
    "type": "ch4_data"
  }
}
```

### Odor Detection Example
```json
{
  "device_id": "B4:3A:45:3F:D4:58",
  "sensor_type": "odor_gas",
  "data": {
    "voltage_mv": 1020,
    "resistance_kohm": 3.0,
    "ppm_ammonia": 0.12,
    "ppm_alcohol": 0.05,
    "ppm_acetone": 0.07,
    "ppm_h2s": 0.01,
    "timestamp": 12345678,
    "type": "odor_data"
  }
}
```

### Smoke Detection Example
```json
{
  "device_id": "B4:3A:45:3F:D4:58",
  "sensor_type": "smoke_gas",
  "data": {
    "voltage_mv": 1100,
    "resistance_kohm": 4.7,
    "ppm_c2h5oh": 0.23,
    "timestamp": 12345678,
    "type": "smoke_data"
  }
}
```

---

## 🌐 Server Integration

Send HTTP POST requests to the server:  
```
http://<RPI_HOST>:<RPI_PORT>
```

Example Python Flask server:
```python
from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/', methods=['POST'])
def receive_data():
    data = request.get_json()
    print("Received JSON:", data)
    return jsonify({"status": "ok"}), 200

app.run(host='0.0.0.0', port=7500)
```

---

## 🧠 WiFi Reconnect & Watchdog

- WiFi reconnects **only when disconnected**, reducing unnecessary reconnect cycles.  
- **Watchdog timer (180 sec)** ensures system resets if stuck or delayed.  
- In `loop()`, `esp_task_wdt_reset()` is called periodically to prevent timeout.

```cpp
#define WDT_TIMEOUT_SECONDS 180
esp_task_wdt_init(&wdt_config);
esp_task_wdt_add(NULL);
```

---

## 🧾 Example Serial Output

```
=== FINAL v7.0: RECONNECT ON DISCONNECT ONLY ===
T=25.3°C H=48.2% | CH4=950 Odor=1020 Smoke=1100
Cycle Complete
```

---

## 🧩 Troubleshooting

| Problem | Possible Cause | Solution |
|---------|----------------|---------|
| WiFi not connecting | Wrong SSID/password | Check credentials and signal strength |
| Sensor PPM always 0 | Wrong wiring, sensor not heated | Verify analog pin & allow warm-up |
| Server not receiving data | Wrong IP/port | Check `RPI_HOST` and server setup |
| Random resets | Watchdog timeout | Reduce sampling interval or optimize code |

---

## 📜 License

MIT License – Free to use, modify, and extend for IoT and educational purposes.

---

## 👨‍💻 Author

**Developer:** Homayoun  
**Board:** ESP32-S3 DEV KIT (WROOM1)  
**Protocol:** Wi-Fi HTTP JSON  
**Version:** 1.0.0

