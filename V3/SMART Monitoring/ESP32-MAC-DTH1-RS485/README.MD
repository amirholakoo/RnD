# MAC-DTH1 ESP32-S3 Monitoring System

This project provides firmware for **three ESP32-S3 boards**, each connected to a **MAC-DTH1 Modbus temperature and humidity sensor**.
Each device has a **unique MAC address**, but otherwise runs identical code.

The system reads data from the sensor (temperature, humidity, and dew point), displays it on an **OLED screen**, and periodically sends the data as JSON to a local server over Wi-Fi.

---

## 🧩 Features

* Reads **temperature**, **humidity**, and **dew point** via Modbus RTU
* Displays live data on a **128×64 OLED** screen:

  * Temperature (°C)
  * Humidity (%)
  * Dew Point (°C)
  * Device MAC address
  * Local IP address
* Sends sensor data to a **local HTTP server** (`http://192.168.2.20:7500`) every minute
* Handles Wi-Fi connection and automatic MAC fallback
* Works with multiple ESP32-S3 boards (only MAC addresses differ)

---

## 🛠️ Hardware Setup

| Component                        | Description                                              |
| -------------------------------- | -------------------------------------------------------- |
| **ESP32-S3**                     | Main controller (3 devices, each with unique MAC)        |
| **MAC-DTH1**                     | Modbus temperature & humidity sensor                     |
| **RS485 to TTL converter**       | Connects sensor to ESP32-S3 via UART                     |
| **Switching Power Supply (24V)** | Powers the sensor                                        |
| **5V Regulator**                 | Converts 24V from sensor power supply to 5V for ESP32-S3 |
| **OLED 128x64 (I2C)**            | Data display                                             |
| **Ground / Power lines**         | Ensure common GND between ESP32 and sensor circuit       |

### Connection Overview

1. **Power**

   * Sensor requires **24V DC** → provided by **switching power supply**
   * ESP32-S3 requires **5V** → use **5V regulator** from 24V supply
   * Make sure **common ground** is connected between sensor and ESP32

2. **Data (Modbus)**

   * Sensor RS485 → TTL converter → ESP32-S3 UART pins

     * TX (ESP32 GPIO 43) → DI (converter)
     * RX (ESP32 GPIO 44) → RO (converter)

3. **Display**

   * OLED connected via **I2C** (default SDA/SCL pins)

---

## 📡 Software Setup

### Required Libraries

Install these libraries from the Arduino Library Manager:

* **Adafruit GFX Library**
* **Adafruit SSD1306**
* **ModbusMaster**
* **WiFi** (built-in on ESP32)
* **HTTPClient** (built-in on ESP32)

---

## ⚙️ Configuration

Edit the following values at the top of the sketch if needed:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* serverUrl = "http://192.168.2.20:7500";
```

Each ESP32-S3 uses a different MAC address for identification:

| Device    | MAC Address       |
| --------- | ----------------- |
| Sensor #1 | D8:3B:DA:4A:13:30 |
| Sensor #2 | (your second MAC) |
| Sensor #3 | (your third MAC)  |

If Wi-Fi reports an invalid MAC (`00:00:00:00:00:00`), the firmware uses the fallback MAC defined as:

```cpp
const char* MAC_FALLBACK = "D8:3B:DA:4A:13:30";
```

---

## 🧠 How It Works

1. ESP32-S3 connects to the Wi-Fi network.
2. It reads Modbus registers (1000–1004) from the MAC-DTH1 sensor:

   * `1000–1001`: Temperature (float)
   * `1002–1003`: Humidity (float)
   * `1004–1005`: Dew Point (float)
3. Data is validated and displayed on the OLED.
4. Every 60 seconds, a JSON payload like this is sent to the local server:

```json
{
  "device_id": "D8:3B:DA:4A:13:30",
  "sensor_type": "DTH1",
  "data": {
    "temperature": 25.32,
    "humidity": 44.87,
    "dewpt": 10.21
  }
}
```

---

## 🧪 Example Output (Serial Monitor)

```
Starting MAC-DTH1 Modbus WiFi System...
Connecting to WiFi....
Connected to WiFi!
Device MAC: D8:3B:DA:4A:13:30
Temp: 25.34 °C  Hum: 45.12 %  DewPt: 10.11 °C
JSON -> {"device_id":"D8:3B:DA:4A:13:30","sensor_type":"DTH1","data":{"temperature":25.34,"humidity":45.12,"dewpt":10.11}}
HTTP Response code: 200
Server response: OK
```

---

## 🔄 Adding More Devices

To add a new ESP32-S3 node:

1. Copy the same code.
2. Update the fallback MAC address to match the new board.
3. Upload the sketch to the new board.
4. The device will report separately to the same server.

---

## ⚠️ Notes

* If the sensor is disconnected, the system sends **default values** (Temp=25°C, Hum=45%, DewPt=10°C).
* Ensure Modbus address of the DTH1 sensor is set to `100`.
* OLED I2C address: `0x3C`.
* Make sure all grounds are connected and voltage levels are correct (24V → sensor, 5V → ESP32).

---

## 📸 System Overview Diagram

```mermaid
graph LR
A[MAC-DTH1 Sensor] -- RS485 --> B[RS485 to TTL Converter]
B --> C[ESP32-S3]
C -->|I2C| D[OLED Display]
C -->|WiFi| E[Local Server (192.168.2.20:7500)]
```

---

### 🧑‍💻 Author

Developed by Homayoun IoT/RnD