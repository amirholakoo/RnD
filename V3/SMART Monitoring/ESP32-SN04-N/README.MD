# 📌 README --- SN04-N ESP32 Detection Logger

## 📖 Overview

This project uses an **ESP32** with an **SN04-N inductive sensor** to
detect object presence and send detection logs to a server over WiFi.\
Each detection is stored locally using **Preferences (NVS)** so that no
data is lost after power reset.\
When WiFi becomes available, the ESP32 automatically sends the **current
detection** along with **up to 9 previous events** to the server.

## 🚀 Features

-   ✔️ Real-time detection using SN04-N sensor\
-   ✔️ Debounce system to avoid false triggers\
-   ✔️ Automatic WiFi reconnect\
-   ✔️ Local storage of last unsent detection and up to 9 previous
    timestamps\
-   ✔️ Automatic resend when WiFi reconnects\
-   ✔️ JSON API for sending data to server\
-   ✔️ LED indicator for detection

## 🛠 Hardware Requirements

-   ESP32 Development Board\
-   SN04-N NPN Inductive Proximity Sensor\
-   Power supply (5V)\
-   Jumper wires\
-   (Optional) LED debugging indicator

### 🧩 Pin Connections

  Component              ESP32 Pin
  ---------------------- --------------
  SN04-N Signal Output   GPIO 4
  LED Indicator          Built-in LED

## 📡 JSON Data Format (POST)

``` json
{
  "device_id": "D8:3B:DA:4E:25:9C",
  "sensor_type": "SN04-N",
  "data": {
    "detected": true,
    "current": { "timestamp": 12345678 },
    "previous": [
      { "timestamp": 12345000 },
      { "timestamp": 12344000 }
    ]
  }
}
```

## 📂 Project Structure

    /project-root
    │── main.ino
    │── README.md

## ⚙️ Configuration

Edit WiFi and server details inside code.

## 🧠 Storage System

Uses Preferences.h to store: - Previous detections (max 9) - Last unsent
detection

## 🔧 How It Works

1.  ESP32 starts, loads previous logs\
2.  Attempts WiFi connection\
3.  Reads SN04-N sensor\
4.  On detection → saves + sends\
5.  If WiFi unavailable → data saved for later

## ▶️ Getting Started

1.  Install Arduino IDE + ESP32 boards\
2.  Upload code\
3.  Open Serial Monitor (115200 baud)

## 📬 Server Requirements

-   HTTP POST
-   JSON body
-   Respond with status 200 to clear memory

## 🤝 Contributing

Pull requests are welcome.