// === ESP32-S3 -> Raspberry Pi HTTP Ingest (DHT + 3x analog gas) ===
// Version 3.0.2 - Modified to send separate JSON for each sensor using cJSON library.
//
// What it does:
//  - Connects to Wi-Fi.
//  - Synchronizes time with an NTP server for timestamps.
//  - Reads DHT22 (Temp °C, RH %).
//  - Reads 3 analog gas channels as millivolts (mV).
//  - For each sensor, it creates a separate JSON object with a device_id, sensor_type, and a data sub-object.
//  - Sends each JSON object to the server via an HTTP POST request.
//  - Waits for the specified interval or enters deep sleep.
//
// Required libs:
//  - DHT sensor library (by Adafruit)
//  - cJSON (included with ESP-IDF, available for Arduino)
//    -> You might need to add this to your project's lib folder if not present.
//
// Board: ESP32S3 Dev Module

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include "cJSON.h"

// ----------- USER SETTINGS (EDIT THESE) -----------
const char* WIFI_SSID = "Homayoun";
const char* WIFI_PASS = "1q2w3e4r$@";

// Your Raspberry Pi address and port
const char* RPI_HOST = "192.168.2.20";
const uint16_t RPI_PORT = 7500;

// How often to send data (seconds)
const uint32_t SAMPLE_EVERY_SEC = 60;

// Set to true for battery-powered operation to conserve power.
// Keep false while debugging via Serial to keep the device awake.
#define USE_DEEP_SLEEP false
// --------------------------------------------------

// ---- Pin map ----
#define PIN_DHT   4
#define PIN_CH4   2
#define PIN_ODOR  1
#define PIN_SMOKE 3

// ---- DHT Sensor Setup ----
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

// ---- Global Variables ----
String device_id_str; // Store device ID globally

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

  Serial.printf("Sending %s data...\n", sensor_name);
  Serial.println(json_string);

  int http_code = http.POST(json_string);

  if (http_code > 0) {
    String response = http.getString();
    Serial.printf("[%s] HTTP Response code: %d\n", sensor_name, http_code);
    Serial.println(response);
  } else {
    Serial.printf("[%s] HTTP POST failed, error: %s\n", sensor_name, http.errorToString(http_code).c_str());
  }

  http.end();
}


// --- JSON Generation and Sending Functions ---

// Creates and sends JSON for the DHT22 sensor
void processDhtSensor(const char* device_id, float temperature, float humidity) {
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("DHT read failed (NaN). Skipping send for DHT22.");
        return;
    }

    cJSON* root = cJSON_CreateObject();
    if (root == NULL) {
        Serial.println("Failed to create JSON object for DHT22");
        return;
    }

    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "sensor_type", "dht22");

    cJSON* data_obj = cJSON_CreateObject();
    if (data_obj == NULL) {
        Serial.println("Failed to create data object for DHT22");
        cJSON_Delete(root);
        return;
    }

    unsigned long now = millis();

    cJSON_AddNumberToObject(data_obj, "temperature", temperature);
    cJSON_AddNumberToObject(data_obj, "humidity", humidity);
    cJSON_AddNumberToObject(data_obj, "timestamp", now);
    cJSON_AddStringToObject(data_obj, "type", "dht22_data");
    cJSON_AddItemToObject(root, "data", data_obj);

    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        Serial.println("Failed to print JSON for DHT22");
    } else {
        sendJsonToServer(json_string, "DHT22");
        cJSON_free(json_string); // Free the string memory
    }

    cJSON_Delete(root); // Free the cJSON object
}


// Creates and sends JSON for a generic analog gas sensor
void processGasSensor(const char* device_id, const char* sensor_name, int millivolts) {
    cJSON* root = cJSON_CreateObject();
    if (root == NULL) {
        printf("Failed to create JSON object for %s\n", sensor_name);
        return;
    }
    
    char sensor_type[16];
    snprintf(sensor_type, sizeof(sensor_type), "%s_gas", sensor_name);
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "sensor_type", sensor_type);

    cJSON* data_obj = cJSON_CreateObject();
    if (data_obj == NULL) {
        printf("Failed to create data object for %s\n", sensor_name);
        cJSON_Delete(root);
        return;
    }
    
    unsigned long now = millis();

    char data_type[16];
    snprintf(data_type, sizeof(data_type), "%s_data", sensor_name);

    cJSON_AddNumberToObject(data_obj, "voltage_mv", millivolts);
    cJSON_AddNumberToObject(data_obj, "timestamp", now);
    cJSON_AddStringToObject(data_obj, "type", data_type);
    cJSON_AddItemToObject(root, "data", data_obj);

    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        printf("Failed to print JSON for %s\n", sensor_name);
    } else {
        sendJsonToServer(json_string, sensor_name);
        cJSON_free(json_string); // Free the string memory
    }

    cJSON_Delete(root); // Free the cJSON object
}


// Connect to Wi-Fi with a simple retry loop
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

// Setup ADC for full-scale reading near 3.3V
void setupAdc() {
  analogSetPinAttenuation(PIN_CH4, ADC_11db);
  analogSetPinAttenuation(PIN_ODOR, ADC_11db);
  analogSetPinAttenuation(PIN_SMOKE, ADC_11db);
}

// Read one analog channel in millivolts
int readMilliVolts(uint8_t pin) {
  return analogReadMilliVolts(pin);
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nESP32-S3 IoT Ingest - Separate JSON per Sensor");

  dht.begin();
  setupAdc();
  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    initializeDeviceId();
  }
}

void loop() {
  // --- 1) Read all sensors ---
  float t = dht.readTemperature();  // °C
  float h = dht.readHumidity();     // %

  int ch4_mv   = readMilliVolts(PIN_CH4);
  int odor_mv  = readMilliVolts(PIN_ODOR);
  int smoke_mv = readMilliVolts(PIN_SMOKE);

  Serial.println("\n--- Starting Sensor Read and Send Cycle ---");
  Serial.printf("DHT: T=%.1f C, RH=%.1f %% | CH4=%d mV, Odor=%d mV, Smoke=%d mV\n",
                t, h, ch4_mv, odor_mv, smoke_mv);

  // --- 2) Process and send data for each sensor separately ---
  const char* dev_id = device_id_str.c_str();

  // Process and send DHT22 data
  processDhtSensor(dev_id, t, h);
  delay(200); // Small delay between requests

  // Process and send CH4 data
  processGasSensor(dev_id, "ch4", ch4_mv);
  delay(200);

  // Process and send Odor data
  processGasSensor(dev_id, "odor", odor_mv);
  delay(200);
  
  // Process and send Smoke data
  processGasSensor(dev_id, "smoke", smoke_mv);

  Serial.println("--- Cycle Complete ---");

  // --- 3) Wait or Deep Sleep ---
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

