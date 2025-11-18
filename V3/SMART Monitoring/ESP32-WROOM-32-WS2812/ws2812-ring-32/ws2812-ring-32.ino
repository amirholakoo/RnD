#include <Adafruit_NeoPixel.h>

#define LED_PIN 13           // DIN connected to GPIO4
#define LED_COUNT 32        // ENTER THE NUMBER OF LEDs on your ring

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Set all LEDs to white
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(200, 200, 200));  // White (R,G,B)
  }
  strip.show();
}

void loop() {
  // Nothing here, LEDs stay on
}