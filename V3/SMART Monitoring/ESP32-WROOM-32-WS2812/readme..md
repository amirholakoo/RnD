# WS2812 LED Examples Using ESP32 WROOM-32

This repository contains two example Arduino sketches demonstrating how to control WS2812 / NeoPixel LEDs using an **ESP32 WROOM‑32** board. The examples include:

- **A 4‑LED WS2812 arc/strip** wired in a daisy‑chain configuration
- **A 32‑LED WS2812 ring**

Both examples use the **Adafruit NeoPixel library** and follow a similar structure, setting all LEDs to a soft white color.

---

## 📌 Features
- Works with **ESP32 WROOM‑32**
- Uses **Adafruit NeoPixel** library
- Includes wiring explanation
- Simple example: sets all LEDs to white
- Ready for expansion (animations, effects, patterns)

---

## 🛠 Requirements
Before uploading the code, make sure you have the following:

### **Hardware**
- ESP32 WROOM‑32 module
- WS2812 / WS2812B LEDs
  - 4‑LED arc/strip (custom wired)
  - 32‑LED ring
- 5V power supply (recommended for longer strips)
- Common ground between LED strip and ESP32

### **Software**
- Arduino IDE
- ESP32 board package installed
  - `Tools → Board → ESP32 Arduino → ESP32 Dev Module`
- USB to UART driver (CP210x / CH340 depending on your board)
- Adafruit NeoPixel library:
  - `Sketch → Include Library → Manage Libraries → Adafruit NeoPixel`

---

## 🔌 Wiring Guide
### **WS2812 Arc (4 LEDs)**
The LEDs are wired in a daisy chain:
- **VCC pins of all 4 LEDs → connected together → 5V**
- **GND pins of all 4 LEDs → connected together → GND**
- **DO of LED1 → DI of LED2**
- **DO of LED2 → DI of LED3**
- **DO of LED3 → DI of LED4**
- **ESP32 GPIO13 → DI of LED1**

### **WS2812 Ring (32 LEDs)**
- **5V → VCC**
- **GND → GND**
- **GPIO13 → DIN**

> ⚠️ *Always use a common ground between ESP32 and LEDs.*

---

## 📄 Example Code
### **4‑LED WS2812 Arc**
```cpp
#include <Adafruit_NeoPixel.h>

#define LED_PIN 13
#define LED_COUNT 4

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.show();

  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(200, 200, 200));
  }
  strip.show();
}

void loop() {
}
```

### **32‑LED WS2812 Ring**
```cpp
#include <Adafruit_NeoPixel.h>

#define LED_PIN 13
#define LED_COUNT 32

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.show();

  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(200, 200, 200));
  }
  strip.show();
}

void loop() {
}
```

---

## ▶️ Uploading the Code
1. Connect the ESP32 to your computer.
2. Open Arduino IDE.
3. Select the correct board:
   - **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
4. Select the correct COM port.
5. Upload the example of your choice.

---

## 📝 Notes
- WS2812 LEDs can draw significant current. If you use many LEDs, power them with an external 5V supply.
- Use a **330–470 Ω resistor** between ESP32 GPIO and LED DIN for signal protection.
- Use a **1000 µF capacitor** across the LED power supply to smooth voltage spikes.

---

## 📚 License
This project is released under the MIT License. You are free to modify, distribute, and use it in your own projects.

---

## 🤝 Contributing
Contributions, improvements, and suggestions are welcome! Feel free to open issues or submit pull requests.

---

## ⭐ Support
If this project helped you, consider giving it a ⭐ on GitHub!

