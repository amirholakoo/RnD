# ESP32 MLX90640 Thermal Camera + Dual-Bus OLED + Web API

This project turns an **ESP32** into a fully network-enabled **thermal
imaging node** using the **MLX90640 32×24 infrared thermal camera**. It
also includes a secondary **SSD1306 OLED display**, a local **HTTP Web
API**, periodic **remote data posting**, and an optional **watchdog
timer** for reliability.

------------------------------------------------------------------------

# ⚠️ Critical Note: MLX90640 & OLED MUST NOT Share the Same I²C Bus

The **MLX90640 requires an 800 kHz I²C clock**, while the **SSD1306 OLED
displays cannot operate above 400 kHz**. If they share the same bus:

-   MLX90640 becomes unstable or returns corrupted data
-   OLED fails to refresh or freezes
-   Severe communication interference occurs

## ✅ Solution: Use Two Separate I²C Buses

  --------------------------------------------------------------------------
  Device           I²C Bus            SDA        SCL        Speed
  ---------------- ------------------ ---------- ---------- ----------------
  **MLX90640       Wire (default)     GPIO 8     GPIO 9     **800 kHz**
  Thermal Camera**                                          

  **SSD1306 OLED   Wire1 (secondary   GPIO 2     GPIO 1     **400 kHz**
  Display**        bus)                                     
  --------------------------------------------------------------------------

This dual-bus configuration completely eliminates interference and
ensures stable performance for both devices.

------------------------------------------------------------------------

# 🧩 Hardware Requirements

-   ESP32 Development Board (ESP32-WROOM recommended)
-   MLX90640 IR Thermal Camera
-   SSD1306 I²C OLED (128×64)
-   Jumper wires

------------------------------------------------------------------------

# 🔌 Wiring Overview

## MLX90640 (Primary I²C Bus)

  MLX90640   ESP32
  ---------- --------
  SDA        GPIO 8
  SCL        GPIO 9
  VCC        3.3V
  GND        GND

## OLED SSD1306 (Secondary I²C Bus)

  OLED   ESP32
  ------ --------
  SDA    GPIO 2
  SCL    GPIO 1
  VCC    3.3V
  GND    GND

------------------------------------------------------------------------

# 🚀 Features

### ✔ Dual-Bus I²C Support

Prevents conflicts between the high-speed thermal camera and low-speed
OLED.

### ✔ Real-Time IR Frame Acquisition

Reads temperature data from all 768 pixels (32×24 array).

### ✔ On-Device OLED Display

Shows: - device IP address - central pixel temperature

### ✔ Built-in Web Server (HTTP API)

Endpoints: - `/` -- System info - `/data` -- Thermal frame + metadata in
JSON - `/ip` -- Connection details

### ✔ Remote Server Upload (POST)

Periodically sends status and sensor data to a configurable remote
server.

### ✔ Hardware Watchdog (optional)

Prevents system freeze by resetting the ESP32 if needed.

------------------------------------------------------------------------

# 🌐 Web API Documentation

## **GET /**

Returns general device info.

## **GET /data**

Returns thermal sensor data in JSON.

## **GET /ip**

Returns Wi-Fi and network information.

------------------------------------------------------------------------

# ▶️ How to Run

1.  Install required libraries.
2.  Update Wi-Fi credentials.
3.  Upload to ESP32.
4.  Check Serial Monitor for IP address.
5.  Access the device via browser or Postman.

------------------------------------------------------------------------

# 📌 Important Notes

-   MLX90640 must run at **800 kHz**.
-   OLED cannot exceed **400 kHz**.
-   Two I²C buses are mandatory.
-   Watchdog can be enabled/disabled.
