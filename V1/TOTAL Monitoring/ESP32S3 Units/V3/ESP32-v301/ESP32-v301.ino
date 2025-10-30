// === ESP32-S3 -> Raspberry Pi HTTP Ingest (DHT + 3x analog gas) ===
// Version 3.0.4 - Offloaded LED alerting to a dedicated FreeRTOS task.
//
// What it does:
//  - Creates a separate FreeRTOS task on Core 1 to handle LED status.
//  - Connects to Wi-Fi.
//  - Reads DHT22 (Temp °C, RH %).
//  - Reads 3 analog gas channels as millivolts (mV).
//  - For each sensor, it creates and sends a separate JSON object to the server.
//  - The main loop checks sensor values against thresholds and sets a flag.
//  - The LED task independently blinks the RGB LED red if the alert flag is set.
//
// Required libs:
//  - DHT sensor library (by Adafruit)
//  - cJSON (included with ESP-IDF, available for Arduino)
//  - Adafruit NeoPixel (for the onboard RGB LED)

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include "cJSON.h"
#include <Adafruit_NeoPixel.h>

// ----------- USER SETTINGS (EDIT THESE) -----------
const char* WIFI_SSID = "Homayoun";
const char* WIFI_PASS = "1q2w3e4r$@";

// Your Raspberry Pi address and port
const char* RPI_HOST = "192.168.2.20";
const uint16_t RPI_PORT = 7500;

// How often to send data (seconds)
const uint32_t SAMPLE_EVERY_SEC = 60;

// Set to true for battery-powered operation.
#define USE_DEEP_SLEEP false

// -- RGB LED Settings (for ESP32-S3 Dev Board) --
#define NEOPIXEL_PIN 48
#define NEOPIXEL_COUNT 1

// -- Sensor Alert Thresholds (ADJUST THESE VALUES) --
const float TEMP_THRESHOLD_C = 50.0;
const float HUMIDITY_THRESHOLD_PCT = 70.0;
const int CH4_THRESHOLD_MV = 2000;
const int ODOR_THRESHOLD_MV = 1500;
const int SMOKE_THRESHOLD_MV = 800;
// --------------------------------------------------

// ---- Pin map ----
#define PIN_DHT   4
#define PIN_CH4   2
#define PIN_ODOR  1
#define PIN_SMOKE 3

// ---- Sensor & LED Setup ----
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);
Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ---- Global & RTOS Variables ----
String device_id_str;
TaskHandle_t ledTaskHandle = NULL;
// 'volatile' is crucial as this is shared between tasks
volatile bool alert_active = false;

// --- FreeRTOS LED Task ---
// This function runs independently on a separate core.
void ledAlertTask(void *pvParameters) {
  Serial.println("LED Alert Task started on Core 1.");
  for (;;) { // Infinite loop for the task
    if (alert_active) {
      // Blink red while the alert is active
      pixels.setPixelColor(0, pixels.Color(120, 0, 0)); // Red
      pixels.show();
      vTaskDelay(pdMS_TO_TICKS(250)); // FreeRTOS-safe delay

      pixels.clear();
      pixels.show();
      vTaskDelay(pdMS_TO_TICKS(250));
    } else {
      // If no alert, ensure LED is off and wait.
      pixels.clear();
      pixels.show();
      vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for a second when inactive
    }
  }
}

// Helper function to get the device ID from MAC address
void initializeDeviceId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  device_id_str = String(buf);
  Serial.print("Device ID: ");
  Serial.println(device_id_str);
}


// Function to send a JSON string to the server
void sendJsonToServer(const char* json_string, const char* sensor_name) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection. Skipping send.");
    return;
  }
  HTTPClient http;
  String url = String("http://") + RPI_HOST + ":" + String(RPI_PORT);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int http_code = http.POST(json_string);
  if (http_code > 0) {
    // String response = http.getString(); // Uncomment for debugging
  } else {
    Serial.printf("[%s] HTTP POST failed, error: %s\n", sensor_name, http.errorToString(http_code).c_str());
  }
  http.end();
}


// Creates and sends JSON for the DHT22 sensor
void processDhtSensor(const char* device_id, float temperature, float humidity) {
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("DHT read failed (NaN). Skipping send for DHT22.");
        return;
    }
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "sensor_type", "dht22");
    cJSON* data_obj = cJSON_CreateObject();
    unsigned long now = millis();
    cJSON_AddNumberToObject(data_obj, "temperature", temperature);
    cJSON_AddNumberToObject(data_obj, "humidity", humidity);
    cJSON_AddNumberToObject(data_obj, "timestamp", now);
    cJSON_AddStringToObject(data_obj, "type", "dht22_data");
    cJSON_AddItemToObject(root, "data", data_obj);
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL) {
        sendJsonToServer(json_string, "DHT22");
        cJSON_free(json_string);
    }
    cJSON_Delete(root);
}


// Creates and sends JSON for a generic analog gas sensor
void processGasSensor(const char* device_id, const char* sensor_name, int millivolts) {
    cJSON* root = cJSON_CreateObject();
    char sensor_type[16];
    snprintf(sensor_type, sizeof(sensor_type), "%s_gas", sensor_name);
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "sensor_type", sensor_type);
    cJSON* data_obj = cJSON_CreateObject();
    unsigned long now = millis();
    char data_type[16];
    snprintf(data_type, sizeof(data_type), "%s_data", sensor_name);
    cJSON_AddNumberToObject(data_obj, "voltage_mv", millivolts);
    cJSON_AddNumberToObject(data_obj, "timestamp", now);
    cJSON_AddStringToObject(data_obj, "type", data_type);
    cJSON_AddItemToObject(root, "data", data_obj);
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL) {
        sendJsonToServer(json_string, sensor_name);
        cJSON_free(json_string);
    }
    cJSON_Delete(root);
}


void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");

  delay(250);

  if (WiFi.setTxPower(WIFI_POWER_17dBm)) {
    Serial.println("WiFi Tx Power set to MAX (17dBm)");
  } else {
    Serial.println("Failed to set WiFi Tx Power.");
  }
  // Available levels:
  // WIFI_POWER_19_5dBm (Max)
  // WIFI_POWER_19dBm
  // WIFI_POWER_18_5dBm
  // WIFI_POWER_17dBm
  // WIFI_POWER_15dBm
  // WIFI_POWER_13dBm
  // WIFI_POWER_11dBm
  // WIFI_POWER_8_5dBm
  // WIFI_POWER_7dBm
  // WIFI_POWER_5dBm
  // WIFI_POWER_2dBm (Min)


  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed.");
  }
}

void setupAdc() {
  analogSetPinAttenuation(PIN_CH4, ADC_11db);
  analogSetPinAttenuation(PIN_ODOR, ADC_11db);
  analogSetPinAttenuation(PIN_SMOKE, ADC_11db);
}

int readMilliVolts(uint8_t pin) {
  return analogReadMilliVolts(pin);
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nESP32-S3 IoT Ingest - FreeRTOS LED Task");
  
  pixels.begin();
  pixels.clear();
  pixels.show();

  dht.begin();
  setupAdc();
  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    initializeDeviceId();
  }

  // Create the LED task and pin it to the second core (Core 1)
  xTaskCreatePinnedToCore(
      ledAlertTask,     // Task function
      "LEDAlertTask",   // Name of the task
      2048,             // Stack size in words
      NULL,             // Task input parameter
      1,                // Priority of the task
      &ledTaskHandle,   // Task handle
      1);               // Core ID (0 or 1)
}

void loop() {
  // --- 1) Read all sensors ---
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int ch4_mv   = readMilliVolts(PIN_CH4);
  int odor_mv  = readMilliVolts(PIN_ODOR);
  int smoke_mv = readMilliVolts(PIN_SMOKE);

  Serial.println("\n--- Starting Sensor Read and Send Cycle ---");
  Serial.printf("DHT: T=%.1f C, RH=%.1f %% | CH4=%d mV, Odor=%d mV, Smoke=%d mV\n",
                t, h, ch4_mv, odor_mv, smoke_mv); 

  // --- 2) Check for threshold alerts and set the shared flag ---
  bool any_alert = false;
  if (!isnan(t) && t > TEMP_THRESHOLD_C) any_alert = true;
  if (!isnan(h) && h > HUMIDITY_THRESHOLD_PCT) any_alert = true;
  if (ch4_mv > CH4_THRESHOLD_MV) any_alert = true;
  if (odor_mv > ODOR_THRESHOLD_MV) any_alert = true;
  if (smoke_mv > SMOKE_THRESHOLD_MV) any_alert = true;

  // This is the only interaction with the LED task
  alert_active = any_alert;

  // --- 3) Process and send data for each sensor separately ---
  const char* dev_id = device_id_str.c_str();
  processDhtSensor(dev_id, t, h);
  delay(200);
  processGasSensor(dev_id, "ch4", ch4_mv);
  delay(200);
  processGasSensor(dev_id, "odor", odor_mv);
  delay(200);
  processGasSensor(dev_id, "smoke", smoke_mv);

  Serial.println("--- Cycle Complete ---");

  // --- 4) Wait or Deep Sleep ---
  if (USE_DEEP_SLEEP) {
    esp_sleep_enable_timer_wakeup((uint64_t)SAMPLE_EVERY_SEC * 1000000ULL);
    Serial.printf("Entering deep sleep for %d seconds...\n", SAMPLE_EVERY_SEC);
    delay(100);
    esp_deep_sleep_start();
  } else {
    Serial.printf("Waiting for %d seconds before next cycle...\n", SAMPLE_EVERY_SEC);
    delay(SAMPLE_EVERY_SEC * 1000);
  }
}
