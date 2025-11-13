# 🔥 ESP32-S3 Smart Sensor Node  
### Multi-Sensor Node with WiFi + HTTP + OLED Display  

This project implements a **WiFi-enabled sensor node** based on the **ESP32-S3 DEV KIT NXR8 (WROOM1)**.  
It simultaneously reads data from **MPU6500 (accelerometer)** and **MLX90614 (infrared temperature sensor)**, displays live values on an **OLED SSD1306**, and periodically sends JSON data to an HTTP server.

---

## 🧠 Features

- 📶 Connects to WiFi and sends data via HTTP POST  
- 🌡️ Reads ambient and object temperature using **MLX90614**  
- 📈 Measures vibration and acceleration using **MPU6500**  
- 🖥️ Displays live data on an **OLED SSD1306 (128x64)**  
- 🧮 Uses a low-pass filter to remove gravity from acceleration data  
- ⏱️ Scheduled data sending and OLED updates  
- 💾 JSON format compatible with REST APIs  

---

## 🧩 Hardware Requirements

| Component | Model | Description |
|------------|--------|-------------|
| MCU | **ESP32-S3 DEV KIT NXR8 (WROOM1)** | Main WiFi + processing unit |
| Temperature Sensor | **MLX90614** | Infrared thermometer (I2C) |
| Accelerometer | **MPU6500** | 3-axis accelerometer/gyro |
| Display | **SSD1306 OLED 0.96"** | 128x64 OLED screen |
| Jumper wires | - | For I2C connections |

---

## ⚙️ Wiring

| Module | Pin | ESP32-S3 |
|---------|-----|-----------|
| **MLX90614** | SDA | GPIO 8 |
|               | SCL | GPIO 9 |
| **MPU6500** | SDA | GPIO 8 |
|              | SCL | GPIO 9 |
| **OLED SSD1306** | SDA | GPIO 8 |
|                   | SCL | GPIO 9 |
| **Power** | VCC | 3.3V |
|            | GND | GND |

> All modules share the same I2C bus.

---

## 🧰 Required Libraries

Before uploading, install the following libraries in **Arduino IDE**:

- `Adafruit MLX90614 Library`
- `Adafruit SSD1306`
- `Adafruit GFX`
- `HTTPClient`
- `WiFi`
- `Wire`

**Install via:**  
`Sketch → Include Library → Manage Libraries…`

---

## 🚀 Getting Started

1. Open the Arduino sketch (e.g., `main.ino`).
2. Update WiFi and server credentials:
   ```cpp
   const char* ssid = "Your_SSID";
   const char* password = "Your_PASSWORD";
   const char* serverUrl = "http://your-server-ip:port";
   ```
3. Select **ESP32-S3 DEV KIT NXR8 (WROOM1)** from *Tools → Board*.
4. Connect the board, choose the correct COM port, and upload the code.
5. On boot, OLED should display `✅ Setup done!`.
6. Open Serial Monitor to view real-time logs and server responses.

---

## 🌡️ Temperature Data (MLX90614)

The **MLX90614** provides both ambient and object temperatures:

```json
{
  "device_id": "94:26:4E:DA:3B:D8",
  "sensor_type": "MLX90614",
  "data": {
    "Ambient Temp": 25.43,
    "Object Temp": 36.12
  }
}
```

---

## 📈 Vibration Data (MPU6500)

Acceleration values (X, Y, Z) are filtered to remove gravity and calculate total vibration magnitude:

```json
{
  "device_id": "94:26:4E:DA:3B:D8",
  "sensor_type": "MPU6500",
  "data": {
    "Vx": 0.0123,
    "Vy": 0.0045,
    "Vz": 0.0067,
    "Vtotal": 0.0158
  }
}
```

- `Vx`, `Vy`, `Vz`: filtered acceleration in each axis  
- `Vtotal`: overall vibration magnitude (RMS)

---

## 🖥️ OLED Interface

The display updates every **2 seconds** showing IP address, temperatures, and vibration data:

```
IP: 192.168.2.45

Ta:25.43 | To:36.12 C
---------------------
Vx:0.0023   Vy:0.0004
Vz:0.0008   Vt:0.0026
```

---

## 🧮 Low-pass Filter

To reduce noise and gravity, a simple low-pass filter is applied:

```
filtered = α * filtered + (1 - α) * raw
```
where `α = 0.9`.  
Higher α means smoother but slower response.

---

## ⏱️ Timing Summary

| Action | Interval |
|--------|-----------|
| OLED refresh | 2 seconds |
| HTTP data send | 60 seconds |
| Sensor sampling | 50 ms |

---

## 🌐 HTTP Communication

Data is sent via HTTP POST request:

```
POST http://<server-ip>:7500
Content-Type: application/json
```

Serial output example:
```
📡 Sending MLX90614 data...
📡 Server response: 200
```

---

## ⚠️ Notes & Recommendations

- If WiFi disconnects, data sending will be skipped but local operation continues.  
- Consider adding automatic **WiFi reconnection** logic.  
- Adjust filter α between `0.85`–`0.95` for noise vs responsiveness balance.  
- Recommended backend: Node.js or Python Flask REST API.  

---

## 📊 System Diagram

```
     ┌───────────────────────────┐
     │         ESP32-S3          │
     │                           │
     │   ┌──────────────┐        │
     │   │  MLX90614    │ Temp   │
     │   └──────────────┘        │
     │   ┌──────────────┐        │
     │   │  MPU6500     │ Accel  │
     │   └──────────────┘        │
     │   ┌──────────────┐        │
     │   │ OLED SSD1306 │ Display│
     │   └──────────────┘        │
     │           │               │
     │        WiFi (HTTP)        │
     │           │               │
     └──────────►Server◄─────────┘
```

---

## 👨‍💻 Author & License

**Author:** Homayoun  
**Maintainer:** [Your GitHub Username]  
**Board Tested:** ESP32-S3 DEV KIT NXR8 (WROOM1)  
**License:** MIT License  

---

## ❤️ Support