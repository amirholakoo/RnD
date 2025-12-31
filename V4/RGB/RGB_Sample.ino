/*
  RGB Status Helper for ESP32-S3 boards with addressable RGB LED (WS2812 / NeoPixel)

  - Non-blocking patterns (does NOT freeze your loop)
  - Call ledUpdate() frequently in loop()
  - Use setLedMode(...) whenever your state changes

  NOTE:
  - Change LED_PIN and LED_COUNT for your board
  - Many ESP32-S3 dev boards have 1 NeoPixel LED
*/

#include <Adafruit_NeoPixel.h>

// ---------- Board-specific settings ----------
#ifndef LED_PIN
  #define LED_PIN 48      // Common for some ESP32-S3 boards (change if needed)
#endif

#ifndef LED_COUNT
  #define LED_COUNT 1
#endif

Adafruit_NeoPixel rgb(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------- Color helper ----------
struct RGB { uint8_t r, g, b; };

RGB C_OFF     = {0, 0, 0};
RGB C_RED     = {255, 0, 0};
RGB C_GREEN   = {0, 255, 0};
RGB C_BLUE    = {0, 0, 255};
RGB C_CYAN    = {0, 255, 255};
RGB C_YELLOW  = {255, 255, 0};
RGB C_ORANGE  = {255, 80, 0};
RGB C_PURPLE  = {160, 0, 255};
RGB C_MAGENTA = {255, 0, 255};
RGB C_WHITE   = {255, 255, 255};

// ---------- Patterns ----------
enum LedMode {
  LED_OFF = 0,
  LED_SOLID,
  LED_BLINK,
  LED_DOUBLE_BLINK,
  LED_BREATH,
  LED_PULSE
};

struct LedState {
  LedMode mode = LED_OFF;
  RGB color = C_OFF;

  // Blink timing
  uint16_t onMs = 500;
  uint16_t offMs = 500;

  // Double blink timing
  uint16_t gapMs = 1200;     // gap after two blinks
  uint16_t dblOnMs = 120;
  uint16_t dblOffMs = 120;

  // Breath timing
  uint16_t breathPeriodMs = 2000;

  // Pulse timing (short flash)
  uint16_t pulseMs = 80;

  // Internal
  unsigned long lastMs = 0;
  int step = 0;
  bool isOn = false;
};

LedState led;

// ---------- Low-level: set physical LED ----------
void ledShowColor(RGB c, uint8_t brightness = 50) {
  rgb.setBrightness(brightness);
  for (int i = 0; i < LED_COUNT; i++) {
    rgb.setPixelColor(i, rgb.Color(c.r, c.g, c.b));
  }
  rgb.show();
}

// ---------- Public: init ----------
void ledBegin() {
  rgb.begin();
  ledShowColor(C_OFF);
}

// ---------- Public: set modes ----------

void setLedOff() {
  led.mode = LED_OFF;
  ledShowColor(C_OFF);
}

// Solid color (optionally with brightness)
void setLedSolid(RGB c, uint8_t brightness = 50) {
  led.mode = LED_SOLID;
  led.color = c;
  ledShowColor(c, brightness);
}

// Blink color (non-blocking)
void setLedBlink(RGB c, uint16_t onMs, uint16_t offMs, uint8_t brightness = 50) {
  led.mode = LED_BLINK;
  led.color = c;
  led.onMs = onMs;
  led.offMs = offMs;
  led.lastMs = millis();
  led.isOn = true;
  ledShowColor(c, brightness);
}

// Double blink pattern: blink-blink-gap
void setLedDoubleBlink(RGB c, uint16_t blinkOnMs, uint16_t blinkOffMs, uint16_t gapMs, uint8_t brightness = 50) {
  led.mode = LED_DOUBLE_BLINK;
  led.color = c;
  led.dblOnMs = blinkOnMs;
  led.dblOffMs = blinkOffMs;
  led.gapMs = gapMs;
  led.lastMs = millis();
  led.step = 0;        // 0:on1,1:off1,2:on2,3:off2,4:gap
  ledShowColor(c, brightness);
}

// Breath effect (fade in/out)
void setLedBreath(RGB c, uint16_t periodMs, uint8_t maxBrightness = 50) {
  led.mode = LED_BREATH;
  led.color = c;
  led.breathPeriodMs = periodMs;
  led.lastMs = millis();
  // Start at low brightness
  ledShowColor(c, 1);
}

// Pulse once (short flash) then return to OFF automatically
void setLedPulse(RGB c, uint16_t pulseMs, uint8_t brightness = 80) {
  led.mode = LED_PULSE;
  led.color = c;
  led.pulseMs = pulseMs;
  led.lastMs = millis();
  led.isOn = true;
  ledShowColor(c, brightness);
}

// ---------- Public: must be called in loop() ----------
void ledUpdate() {
  unsigned long now = millis();

  if (led.mode == LED_OFF) return;
  if (led.mode == LED_SOLID) return;

  if (led.mode == LED_BLINK) {
    if (led.isOn && (now - led.lastMs >= led.onMs)) {
      led.isOn = false;
      led.lastMs = now;
      ledShowColor(C_OFF);
    } else if (!led.isOn && (now - led.lastMs >= led.offMs)) {
      led.isOn = true;
      led.lastMs = now;
      ledShowColor(led.color);
    }
    return;
  }

  if (led.mode == LED_DOUBLE_BLINK) {
    // step timeline: on1 -> off1 -> on2 -> off2 -> gap -> repeat
    if (led.step == 0 && (now - led.lastMs >= led.dblOnMs)) { // on1 done
      led.step = 1; led.lastMs = now; ledShowColor(C_OFF); return;
    }
    if (led.step == 1 && (now - led.lastMs >= led.dblOffMs)) { // off1 done
      led.step = 2; led.lastMs = now; ledShowColor(led.color); return;
    }
    if (led.step == 2 && (now - led.lastMs >= led.dblOnMs)) { // on2 done
      led.step = 3; led.lastMs = now; ledShowColor(C_OFF); return;
    }
    if (led.step == 3 && (now - led.lastMs >= led.dblOffMs)) { // off2 done
      led.step = 4; led.lastMs = now; /* stay off */ return;
    }
    if (led.step == 4 && (now - led.lastMs >= led.gapMs)) { // gap done
      led.step = 0; led.lastMs = now; ledShowColor(led.color); return;
    }
    return;
  }

  if (led.mode == LED_BREATH) {
    // Compute brightness based on sine wave over period
    float t = (float)((now - led.lastMs) % led.breathPeriodMs) / (float)led.breathPeriodMs; // 0..1
    // 0..1..0 shape (smooth)
    float b = 0.5f - 0.5f * cosf(2.0f * 3.14159f * t);
    uint8_t brightness = (uint8_t)(1 + b * 60); // max brightness ~61 (adjust)
    ledShowColor(led.color, brightness);
    return;
  }

  if (led.mode == LED_PULSE) {
    if (led.isOn && (now - led.lastMs >= led.pulseMs)) {
      led.isOn = false;
      setLedOff(); // pulse ends, go OFF
    }
    return;
  }
}
