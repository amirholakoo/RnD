#include <WiFi.h>
#include <HTTPClient.h>
#include <FastLED.h>

const char* ssid = "RASM";      
const char* password = "1234qwert"; 

const char* serverUrl = "http://192.168.215.219:5000/test";

// Configuration
const int wifiRetryLimit = 5;          // Max WiFi connection retries
const int httpRetryLimit = 3;          // Max HTTP request retries
const int wifiTimeoutMs = 10000;       // WiFi connection timeout (10s)
const int httpTimeoutMs = 5000;        // HTTP request timeout (5s)
const int loopDelayMs = 5000;          // Base delay between HTTP requests (5s)
const int randomDelayMs = 2000;        // Random delay range (0-2s)

// LED Definitions
#define NUM_LEDS 1
#define DATA_PIN 48
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define BRIGHTNESS 50
CRGB leds[NUM_LEDS];


enum LedState {
  LED_IDLE,
  LED_CONNECTING_WIFI,
  LED_WIFI_CONNECTED,
  LED_WIFI_FAILED,
  LED_HTTP_SENDING,
  LED_HTTP_SUCCESS,
  LED_HTTP_FAILED
};
LedState currentLedState = LED_IDLE;


TaskHandle_t ledTaskHandle = NULL;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting ESP32 WiFi and HTTP test...");

  // Initialize LED
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  currentLedState = LED_IDLE;

  // Create LED update task
  xTaskCreate(ledTask, "LED Task", 2048, NULL, 1, &ledTaskHandle);

  connectWiFi();

  randomSeed(analogRead(0));
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    connectWiFi();
  }

  int randomDelay = random(randomDelayMs);
  Serial.printf("Waiting for %d ms before sending request...\n", randomDelay);
  delay(randomDelay);

  sendHttpRequest();

  delay(loopDelayMs);
}

void connectWiFi() {
  int retries = 0;
  currentLedState = LED_CONNECTING_WIFI;

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    delay(100);
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && retries < wifiRetryLimit && millis() - startTime < wifiTimeoutMs) {
    delay(1000);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentLedState = LED_WIFI_CONNECTED;
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    currentLedState = LED_WIFI_FAILED;
    Serial.println("\nFailed to connect to WiFi");
  }
}

bool sendHttpRequest() {
  HTTPClient http;
  int retries = 0;
  bool success = false;
  currentLedState = LED_HTTP_SENDING;

  String macAddress = WiFi.macAddress();
  Serial.println("MAC Address: " + macAddress);

  http.setTimeout(httpTimeoutMs);
  http.begin(serverUrl);
  http.addHeader("X-Device-ID", macAddress);

  while (retries < httpRetryLimit && !success) {
    Serial.println("Sending HTTP GET request...");
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.printf("HTTP Response code: %d\n", httpResponseCode);
      String response = http.getString();
      Serial.println("Response: " + response);
      success = true;
      currentLedState = LED_HTTP_SUCCESS;
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
      retries++;
      if (retries < httpRetryLimit) {
        Serial.printf("Retrying (%d/%d)...\n", retries, httpRetryLimit);
        delay(1000 * retries);
      }
    }
  }

  if (!success) {
    currentLedState = LED_HTTP_FAILED;
    Serial.println("Max HTTP retries reached, request failed");
  }

  http.end();
  return success;
}

// LED Task to handle updates
void ledTask(void *parameter) {
  const TickType_t xFrequency = 100 / portTICK_PERIOD_MS; // Update every 100ms
  TickType_t xLastWakeTime = xTaskGetTickCount();
  bool blinkState = false;

  while (1) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    updateLed(blinkState);
    blinkState = !blinkState; // Toggle for blinking
  }
}

// Update LED based on current state
void updateLed(bool blinkState) {
  switch (currentLedState) {
    case LED_IDLE:
      leds[0] = CRGB::Black; // Off
      break;
    case LED_CONNECTING_WIFI:
      leds[0] = blinkState ? CRGB::Yellow : CRGB::Black; // Blinking yellow
      break;
    case LED_WIFI_CONNECTED:
      leds[0] = CRGB::Green; // Solid green
      break;
    case LED_WIFI_FAILED:
      leds[0] = CRGB::Red; // Solid red
      break;
    case LED_HTTP_SENDING:
      leds[0] = CRGB::Blue; // Solid blue
      break;
    case LED_HTTP_SUCCESS:
      leds[0] = CRGB::Green; // Solid green
      break;
    case LED_HTTP_FAILED:
      leds[0] = blinkState ? CRGB::Red : CRGB::Black; // Blinking red
      break;
  }
  FastLED.show();
}