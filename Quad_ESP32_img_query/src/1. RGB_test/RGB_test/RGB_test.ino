#include <FastLED.h>

#define NUM_LEDS 1      // Onboard RGB LED (single WS2812B)
#define DATA_PIN 48     // GPIO48 for ESP32-S3-DevKitC-1 onboard LED
#define LED_TYPE WS2812 // WS2812B LED type
#define COLOR_ORDER GRB // Color order for WS2812B
#define BRIGHTNESS 50   // Brightness (0-255, 50 is safe for onboard LED)
#define HUE_STEP 1      // Hue increment per step (1 for smooth, higher for faster)
#define DELAY_MS 10     // Delay between steps (ms, lower for faster transitions)

CRGB leds[NUM_LEDS];
uint8_t hue = 0;        // Starting hue (0-255)

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  Serial.println("ESP32-S3 RGB LED Spectrum Started");
}

void loop() {
  // Set LED color using HSV (hue cycles through spectrum)
  leds[0] = CHSV(hue, 255, 255); // Full saturation and value for vibrant colors
  FastLED.show();
  
  // Increment hue for next color
  hue += HUE_STEP;
  // No need to wrap hue (CHSV handles 0-255 overflow automatically)
  
  delay(DELAY_MS); // Control transition speed
}