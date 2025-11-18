# SN04-N ESP32 Magnetic Proximity Sensor

A robust ESP32-based system for detecting events using an **SN04-N inductive proximity sensor**, storing detection history in non-volatile memory, and sending detection data to **two different HTTP servers**.

This version also includes a built-in **web dashboard** for viewing device status and history via your browser.

---

## ✨ Features

### 🔹 Sensor & Detection
- Reads SN04-N proximity sensor on GPIO 4  
- Debounced trigger detection  
- LED indicator on GPIO 38  
- Stores the last **9 previous detections**  
- Saves unsent detections across resets  

### 🔹 Networking
- Auto-connects to WiFi with retry logic  
- Sends JSON detection packets to:
  - **Primary server** (`serverUrl`)
  - **Secondary server** (`serverUrl2`) ← *new feature*  

### 🔹 Data Persistence
Uses ESP32 **Preferences (NVS)** to store:
- Previous detection timestamps  
- Last unsent timestamp (`current_temp`)  

### 🔹 Embedded Web Server (New)
Provides the following endpoints:

| Endpoint | Description |
|----------|-------------|
| `/` | Human-readable device info |
| `/info` | JSON summary of status & history |
| `/ping` | Returns **OK** for health checks |

---

# 📁 Project Structure
This project consists of a single Arduino sketch containing:
- Sensor logic  
- WiFi connection manager  
- JSON REST client  
- Local web server  
- NVS storage logic  

---

## 🧩 How the System Works

### 1. On Startup
- Loads detection history from NVS  
- Loads unsent detection value  
- Connects to WiFi  
- Starts a web server  

---

### 2. In the Main Loop
- Handles HTTP requests to the device  
- Monitors the SN04-N sensor input  
- On trigger detection:
  - Saves timestamp  
  - Updates history queue  
  - Sends JSON to both servers  
  - Blinks LED  

---

### 3. Detection JSON Format

```json
{
  "device_id": "D8:3B:DA:4E:25:9C",
  "sensor_type": "SN04-N",
  "data": {
    "detected": true,
    "current": { "timestamp": 123456 },
    "previous": [
      { "timestamp": 123000 },
      { "timestamp": 122500 }
    ]
  }
}
```

---

# 📡 New Additions in This Version

## ✅ 1. Embedded Web Server

Added using:

```cpp
#include <WebServer.h>
WebServer server(80);
```

### New Endpoints

#### `/`
Human-readable summary including:
- MAC address  
- IP address  
- Unsaved detection state  
- Previous detection count  

---

#### `/info`
Returns full device state in JSON:

```json
{
  "device_id": "...",
  "ip": "...",
  "unsaved_detection": false,
  "current_timestamp": 0,
  "previous_count": 3,
  "previous": [1111,2222,3333]
}
```

---

#### `/ping`
Simple OK response for system monitoring.

---

## ✅ 2. Secondary Server Support

New configuration line:

```cpp
const char* serverUrl2 = "http://192.168.2.22:6002";
```

The device now sends detection packets to **two separate servers**, improving:
- Redundancy  
- Multi-system compatibility  
- Logging reliability  

Implementation:

```cpp
HTTPClient http2;
if (http2.begin(serverUrl2)) {
    http2.addHeader("Content-Type", "application/json");
    http2.setTimeout(10000);
    int code2 = http2.POST(json);
}
```

---

# 🛠️ Hardware Setup

| Component | Pin |
|----------|-----|
| SN04-N Signal | GPIO 4 |
| LED Indicator | GPIO 38 |
| Power | 5V / GND |

Ensure SN04-N is connected in **NPN/NO mode** and configured to work with `INPUT_PULLUP`.

---

# 📦 Installation

### Requirements
- Arduino IDE OR PlatformIO  
- ESP32 board package  
- Built-in libraries:
  - WiFi  
  - WebServer  
  - Preferences  
  - HTTPClient  

### Steps
1. Open the `.ino` file  
2. Update WiFi SSID & password  
3. Edit server addresses if needed  
4. Upload to ESP32  

---

# 🌐 Accessing the Device Dashboard

After WiFi connects, visit:

```
http://<ESP-IP>/
```

Examples:

```
http://192.168.2.50/
http://192.168.1.99/info
http://192.168.1.99/ping
```

---

# 📖 Optional Future Improvements

- NTP time sync instead of `millis()` timestamps  
- OTA firmware updates  
- `/history/clear` endpoint  
- Pretty-formatted HTML dashboard  

---
