#include <DHT.h>
#include <DHT_U.h>

// === ESP32-S3 -> Raspberry Pi HTTP Ingest (DHT + 3x analog gas) ===
// Pins you gave:
//  DHT on GPIO 4        (digital)
//  CH4 on GPIO 2        (ADC1_CH1)
//  Odor on GPIO 1       (ADC1_CH0)
//  Smoke on GPIO 3      (ADC1_CH2)
//
// What it does:
//  - Connects to Wi-Fi
//  - Reads DHT (Temp °C, RH %)
//  - Reads 3 analog gas channels as millivolts (mV)
//  - Builds JSON payload matching your /ingest API
//  - HTTP POST to http://<RPi_IP>:8000/ingest
//  - Waits SAMPLE_EVERY_SEC (or goes to deep-sleep if enabled)
//
// NOTE: Gas sensors are sent as millivolts ("unit":"mV").
//       Later, we can add calibration to convert mV -> ppm.
//
// Required libs: DHT sensor library (Adafruit)
// Board: ESP32S3 Dev Module (USB CDC on Boot: Enabled)

#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"

// ----------- USER SETTINGS (EDIT THESE) -----------
const char* WIFI_SSID = "VPN1";
const char* WIFI_PASS = "09126141426";

// Your Raspberry Pi address (static) and port
const char* RPI_HOST = "192.168.1.236";   // <-- change to your Pi's IP
const uint16_t RPI_PORT = 8000;

// How often to send data (seconds)
const uint32_t SAMPLE_EVERY_SEC = 60;

// If you plan to run on battery later, set this true to use deep sleep.
// While debugging via Serial, keep false (so the device stays awake).
#define USE_DEEP_SLEEP false
// --------------------------------------------------

// ---- Pin map (from your message) ----
#define PIN_DHT   4
#define PIN_CH4   2
#define PIN_ODOR  1
#define PIN_SMOKE 3

// Choose your DHT type: DHT22 or DHT11
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

// Helper: build a device_id from MAC (looks like esp32s3_AABBCCDDEEFF)
String deviceIdFromMac() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "esp32s3_%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// We’ll use ADC attenuation so the ADC range covers ~0–3.3V
// (ESP32 Arduino core handles this; S3 uses 12-bit ADC by default)
void setupAdc() {

  // Make sure these pins are configured as analog inputs
  pinMode(PIN_CH4,   INPUT);
  pinMode(PIN_ODOR,  INPUT); 
  pinMode(PIN_SMOKE, INPUT);
  

  // Give the ADC full-scale near 3.3V
  analogSetPinAttenuation(PIN_CH4,   ADC_11db);
  analogSetPinAttenuation(PIN_ODOR,  ADC_11db);
  analogSetPinAttenuation(PIN_SMOKE, ADC_11db);
}

// Read one analog channel in millivolts (uses ESP32 ADC calibration if available)
int readMilliVolts(uint8_t pin) {
  // On recent ESP32 Arduino core, analogReadMilliVolts() returns calibrated mV
  // If your core is older, you can approximate: mv = analogRead(pin) * (3300.0 / 4095.0)
  int mv = analogReadMilliVolts(pin);
  if (mv <= 0) {
    // Fallback approximation
    int raw = analogRead(pin);
    mv = (int)(raw * (3300.0 / 4095.0));
  }
  return mv;
}

// Connect to Wi-Fi with simple retry loop
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) { // ~20s timeout
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed. Will try again next cycle.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nESP32-S3 IoT Ingest (DHT + 3x analog gas)");

  dht.begin();
  setupAdc();
  connectWiFi();
}

void postToServer(float tempC, float rhPct, int ch4_mV, int odor_mV, int smoke_mV) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Skip POST (no WiFi).");
      return;
    }
  }

  HTTPClient http;
  String url = String("http://") + RPI_HOST + ":" + String(RPI_PORT) + "/ingest";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Compose JSON payload matching your backend ingestor
  // We include unit for gas channels = "mV" so sensor_types will store that.
  String devId = deviceIdFromMac();
  String ipStr = WiFi.localIP().toString();

  String payload = "{";
  payload += "\"device_id\":\"" + devId + "\",";
  payload += "\"firmware_version\":\"v1.0.0\",";
  payload += "\"ip\":\"" + ipStr + "\",";
  payload += "\"sample_interval_sec\":" + String(SAMPLE_EVERY_SEC) + ",";
  payload += "\"sensors\":[";
  // DHT Temperature
  payload += "{\"type\":\"temperature\",\"name\":\"DHT_T\",\"value\":" + String(tempC, 1) + "},";
  // DHT Humidity
  payload += "{\"type\":\"humidity\",\"name\":\"DHT_RH\",\"value\":" + String(rhPct, 1) + "},";
  // CH4 analog (millivolts)
  payload += "{\"type\":\"ch4\",\"name\":\"CH4_mV\",\"unit\":\"mV\",\"value\":" + String(ch4_mV) + "},";
  // Odor analog (millivolts)
  payload += "{\"type\":\"odor\",\"name\":\"Odor_mV\",\"unit\":\"mV\",\"value\":" + String(odor_mV) + "},";
  // Smoke analog (millivolts)
  payload += "{\"type\":\"smoke\",\"name\":\"Smoke_mV\",\"unit\":\"mV\",\"value\":" + String(smoke_mV) + "}";
  payload += "]}";

  Serial.println("POST " + url);
  Serial.println("Payload: " + payload);

  int code = http.POST(payload);
  Serial.printf("HTTP status: %d\n", code);
  if (code > 0) {
    String resp = http.getString();
    Serial.println("Resp: " + resp);
  } else {
    Serial.println("POST failed.");
  }
  http.end();
}

void loop() {
  // --- 1) Read sensors ---
  float t = dht.readTemperature();  // °C
  float h = dht.readHumidity();     // %
  // DHT may return NaN if timing off; we’ll guard
  if (isnan(t) || isnan(h)) {
    Serial.println("DHT read failed (NaN). Will retry next cycle.");
    t = -127.0;  // send a sentinel value (or skip)
    h = -1.0;
  }

  // Read gas analogs in mV
  int ch4_mv   = readMilliVolts(PIN_CH4);
  int odor_mv  = readMilliVolts(PIN_ODOR);
  int smoke_mv = readMilliVolts(PIN_SMOKE);

  Serial.printf("DHT: T=%.1f C, RH=%.1f %% | CH4=%d mV, Odor=%d mV, Smoke=%d mV\n",
                t, h, ch4_mv, odor_mv, smoke_mv);

  // --- 2) Send to Raspberry Pi ---
  postToServer(t, h, ch4_mv, odor_mv, smoke_mv);

  // --- 3) Wait or Deep Sleep ---
  if (USE_DEEP_SLEEP) {
    // Deep sleep is great for battery units.
    // The chip will wake up after SAMPLE_EVERY_SEC automatically.
    esp_sleep_enable_timer_wakeup((uint64_t)SAMPLE_EVERY_SEC * 1000000ULL);
    Serial.println("Deep sleeping...");
    delay(50);
    esp_deep_sleep_start();
  } else {
    // Keep running (easier for debugging)
    for (uint32_t i = 0; i < SAMPLE_EVERY_SEC; ++i) delay(1000);
  }
}
