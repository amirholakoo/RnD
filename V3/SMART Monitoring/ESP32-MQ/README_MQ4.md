# ESP32-S3 Methane Monitoring System (MQ-4 + WiFi JSON Sender)

This project implements a **real-time methane (CH₄) monitoring system** using an **ESP32-S3 DEV KIT NXR8 (WROOM1)** board and an **MQ-4 gas sensor**.  
The sensor reads methane concentration, computes ppm values, and sends JSON-formatted data to a Raspberry Pi (or any HTTP server) over Wi-Fi at regular intervals.

---

## 📘 Features

- ⚡ Real-time methane detection using MQ-4 analog sensor.  
- 🌐 Wi-Fi connectivity with auto-reconnect logic.  
- 🧠 Automatic computation of Rs (sensor resistance) and PPM using calibrated constants.  
- 📡 Periodic JSON data transmission to a configurable HTTP endpoint.  
- 🛡️ Watchdog Timer (WDT) protection for system reliability.  
- 🧩 Uses lightweight cJSON library for efficient memory use.  
- 🧾 Serial debug logging for monitoring values and connection states.

---

## 🧱 Hardware Setup

### 🧩 Components

| Component | Description |
|------------|-------------|
| ESP32-S3 DEV KIT NXR8 (WROOM1) | Main microcontroller board |
| MQ-4 Gas Sensor | Methane (CH₄) concentration sensor |
| Connecting wires | Jumper cables |
| Power source | 3.3V regulated (from ESP32) |

### ⚙️ Wiring

| MQ-4 Pin | Connects To | Description |
|-----------|-------------|-------------|
| **VCC** | 3.3V | Power |
| **GND** | GND | Ground |
| **AOUT** | GPIO 5 (PIN_MQ4) | Analog output |
| **DOUT** | *Not used* | Digital comparator output |

> ⚠️ The MQ-4 should be connected to **3.3V** (not 5V) for compatibility with ESP32’s ADC.

---

## 🧮 Sensor Calibration & Math

The system calculates the **sensor resistance (Rs)** and **methane concentration (ppm)** using the following formulas:

```
Rs = RL * (Vin - Vout) / Vout
PPM = A * (Rs / Ro) ^ B
```

Where:

| Symbol | Meaning | Value |
|---------|----------|--------|
| **Vin** | Input voltage | 3300 mV |
| **RL** | Load resistor | 3.0 kΩ |
| **Ro** | Sensor baseline resistance | 1.5 kΩ |
| **A** | Empirical constant | 84.2064 |
| **B** | Empirical exponent | -5.6602 |

---

## 🖥️ Software Setup

### 🧩 Required Libraries

Make sure these libraries are installed in your Arduino IDE or PlatformIO environment:

- `WiFi.h` (ESP32 core)
- `HTTPClient.h`
- `cJSON.h` (for JSON encoding)
- `esp_task_wdt.h` (for watchdog handling)

### ⚙️ Configuration

Edit the following parameters in the source code before uploading:

```cpp
const char* WIFI_SSID = "Homayoun";
const char* WIFI_PASS = "1q2w3e4r$@";
const char* RPI_HOST = "192.168.2.20";
const uint16_t RPI_PORT = 7500;
const uint32_t SAMPLE_EVERY_SEC = 60; // Send every 60s
```

### 🔧 Building and Flashing

1. Open the `.ino` file in Arduino IDE or VS Code (PlatformIO).  
2. Select board: **ESP32S3 Dev Module**.  
3. Set the correct COM port.  
4. Compile and upload the sketch.  
5. Open the Serial Monitor (115200 baud).

---

## 📡 JSON Data Format

Example of data sent to the server every 60 seconds:

```json
{
  "device_id": "B4:3A:45:3F:D4:58",
  "sensor_type": "MQ4",
  "data": {
    "ppm_methane": 125.432,
    "voltage": 1.020,
    "resistance": 2750.34,
    "timestamp": 14823000,
    "type": "mq4_data"
  }
}
```

---

## 🌐 Server Integration

The ESP32 sends data to:

```
http://<RPI_HOST>:<RPI_PORT>
```

### Example Python Flask endpoint (for Raspberry Pi)

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

## 🧠 Watchdog Timer (WDT)

The watchdog timer ensures that the ESP32 never freezes indefinitely.  
If the main loop or Wi-Fi reconnection stalls for more than **180 seconds**, the system automatically resets.

```cpp
#define WDT_TIMEOUT_SECONDS 180
esp_task_wdt_init(&wdt_config);
esp_task_wdt_add(NULL);
```

---

## 🧾 Example Serial Output

```
=== MQ-4 → SERVER (MQ4 FORMAT) ===
Device ID: B4:3A:45:3F:D4:58
Connecting to WiFi....
[WiFi] OK! IP: 192.168.2.101

[SENSOR] 1020 mV | 2.75 kΩ | 125.432 ppm

=== JSON SENT ===
{"device_id":"B4:3A:45:3F:D4:58","sensor_type":"MQ4","data":{"ppm_methane":125.432,"voltage":1.02,"resistance":2750.34,"timestamp":14823000,"type":"mq4_data"}}
=================
[HTTP] Response Code: 200
[HTTP] Response: {"status":"ok"}
Cycle Complete
```

---

## 🧩 Troubleshooting

| Problem | Possible Cause | Solution |
|----------|----------------|-----------|
| Wi-Fi not connecting | Wrong SSID/password | Check credentials and signal strength |
| PPM always 0 | Wrong wiring or sensor not heated | Verify analog pin & allow warm-up time |
| Server not receiving | Wrong IP/port | Check `RPI_HOST` and server setup |
| Random resets | Watchdog timeout | Reduce sampling delay or optimize code |

---

## 📜 License

This project is released under the **MIT License**.  
Feel free to modify, reuse, or extend for educational or IoT applications.

---

## 👨‍💻 Author

**Developed by:** Homayoun  
**Board:** ESP32-S3 DEV KIT NXR8 (WROOM1)  
**Sensor:** MQ-4 Methane Gas Sensor  
**Protocol:** Wi-Fi HTTP JSON