// === ESP32-S3 -> Raspberry Pi HTTP Ingest (DHT + 3x analog gas) ===
// Version 4.0.0 - Rearchitected for multi-gas PPM calculation per sensor.
//
// What it does:
//  - Replaced generic gas sensor function with specific functions for each sensor.
//  - Each sensor function calculates PPM for all relevant gases it can detect.
//  - Each gas has its own 'A' and 'B' coefficients for accurate calculation.
//  - The JSON payload for each sensor now includes PPM values for all its target gases.
//  - Continues to use a dedicated FreeRTOS task for non-blocking LED alerts.

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include "cJSON.h"
#include <Adafruit_NeoPixel.h>

// ----------- USER SETTINGS (EDIT THESE) -----------
const char* WIFI_SSID = "Homayoun";
const char* WIFI_PASS = "1q2w3e4r$@";
const char* RPI_HOST = "192.168.2.20";
const uint16_t RPI_PORT = 7500;
const uint32_t SAMPLE_EVERY_SEC = 60;
#define USE_DEEP_SLEEP false

// -- RGB LED & Pin Settings --
#define NEOPIXEL_PIN 48
#define NEOPIXEL_COUNT 1
#define PIN_DHT   4
#define PIN_CH4   2
#define PIN_ODOR  1
#define PIN_SMOKE 3

// =================================================================================
// --- GAS SENSOR CALCULATION & CALIBRATION SETTINGS (CUSTOMIZE THESE VALUES) ---
// =================================================================================
const float VOLTAGE_IN = 3300.0; // System voltage in mV (3.3V for ESP32)

// --- 1. CH4 Sensor (e.g., MQ-4) ---
const float CH4_LOAD_RESISTOR = 3.0; 
const float CH4_RO = 1.5;            
// Coefficients for CH4 (Methane)
const float CH4_CH4_PARAM_A = 84.2064;
const float CH4_CH4_PARAM_B = -5.6602;
// Coefficients for C3H8 (Propane)
const float CH4_C3H8_PARAM_A = 29.8046; 
const float CH4_C3H8_PARAM_B = -4.4417;  

// --- 2. Odor Sensor (e.g., MQ-135) ---
const float ODOR_LOAD_RESISTOR = 3.0; 
const float ODOR_RO = 3.0;            
// Coefficients for NH3 (Ammonia)
const float ODOR_AMMONIA_PARAM_A = 0.5328;;
const float ODOR_AMMONIA_PARAM_B = -2.7671;
// Coefficients for Alcohol
const float ODOR_ALCOHOL_PARAM_A = 0.9991; 
const float ODOR_ALCOHOL_PARAM_B = -4.2704; 
// Coefficients for Acetone
const float ODOR_ACETONE_PARAM_A = 1.2348; 
const float ODOR_ACETONE_PARAM_B = -2.1610; 
// Coefficients for Hydrogen Sulfide (H2S)
const float ODOR_H2S_PARAM_A = 0.7095;   
const float ODOR_H2S_PARAM_B = -2.2430;   

// --- 3. Smoke Sensor (e.g., MQ-2) ---
const float SMOKE_LOAD_RESISTOR = 4.7; 
const float SMOKE_RO = 20.0;            
// Coefficients for C2H5OH (Ethanol / Alcohol)
const float SMOKE_C2H5OH_PARAM_A = 0.4313;
const float SMOKE_C2H5OH_PARAM_B = -2.4307;

// -- Sensor Alert Thresholds (ADJUST THESE VALUES) --
const float TEMP_THRESHOLD_C = 50.0;
const float HUMIDITY_THRESHOLD_PCT = 70.0;
const int CH4_THRESHOLD_MV = 2000;
const int ODOR_THRESHOLD_MV = 1500;
const int SMOKE_THRESHOLD_MV = 800;
// =================================================================================

// ---- Sensor & LED Setup ----
DHT dht(PIN_DHT, DHT22);
Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ---- Global & RTOS Variables ----
String device_id_str;
TaskHandle_t ledTaskHandle = NULL;
volatile bool alert_active = false;

// --- FreeRTOS LED Task ---
void ledAlertTask(void *pvParameters) {
  Serial.println("LED Alert Task started on Core 1.");
  for (;;) {
    if (alert_active) {
      pixels.setPixelColor(0, pixels.Color(120, 0, 0)); pixels.show(); vTaskDelay(pdMS_TO_TICKS(250));
      pixels.clear(); pixels.show(); vTaskDelay(pdMS_TO_TICKS(250));
    } else {
      pixels.clear(); pixels.show(); vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

// Helper function to calculate sensor resistance (Rs)
float calculate_rs(float v_out, float load_resistor) {
  if (v_out <= 0) return -1.0;
  return load_resistor * (VOLTAGE_IN - v_out) / v_out;
}

// Helper function to calculate gas PPM
float calculate_ppm(float rs, float ro, float a, float b) {
  if (rs <= 0 || ro <= 0) return 0.0;
  return a * pow(rs / ro, b);
}

// Function to send a JSON string to the server
void sendJsonToServer(const char* json_string, const char* sensor_name) {
  if (WiFi.status() != WL_CONNECTED) { return; }
  HTTPClient http;
  String url = String("http://") + RPI_HOST + ":" + String(RPI_PORT);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int http_code = http.POST(json_string);
  if (http_code <= 0) {
    Serial.printf("[%s] HTTP POST failed, error: %s\n", sensor_name, http.errorToString(http_code).c_str());
  }
  http.end();
}

// --- SENSOR-SPECIFIC PROCESSING FUNCTIONS ---

void processDhtSensor(const char* device_id, float temperature, float humidity) {
    if (isnan(temperature) || isnan(humidity)) { return; }
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "sensor_type", "dht22");
    cJSON* data_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(data_obj, "temperature", temperature);
    cJSON_AddNumberToObject(data_obj, "humidity", humidity);
    cJSON_AddNumberToObject(data_obj, "timestamp", millis());
    cJSON_AddStringToObject(data_obj, "type", "dht22_data");
    cJSON_AddItemToObject(root, "data", data_obj);
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string) { sendJsonToServer(json_string, "DHT22"); cJSON_free(json_string); }
    cJSON_Delete(root);
}

void processCh4Sensor(const char* device_id, int millivolts) {
    float resistance_kohm = calculate_rs((float)millivolts, CH4_LOAD_RESISTOR);
    float ppm_ch4 = calculate_ppm(resistance_kohm, CH4_RO, CH4_CH4_PARAM_A, CH4_CH4_PARAM_B);
    float ppm_c3h8 = calculate_ppm(resistance_kohm, CH4_RO, CH4_C3H8_PARAM_A, CH4_C3H8_PARAM_B);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "sensor_type", "ch4_gas");
    cJSON* data_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(data_obj, "voltage_mv", millivolts);
    cJSON_AddNumberToObject(data_obj, "resistance_kohm", resistance_kohm);
    cJSON_AddNumberToObject(data_obj, "ppm_ch4", ppm_ch4);
    cJSON_AddNumberToObject(data_obj, "ppm_c3h8", ppm_c3h8);
    cJSON_AddNumberToObject(data_obj, "timestamp", millis());
    cJSON_AddStringToObject(data_obj, "type", "ch4_data");
    cJSON_AddItemToObject(root, "data", data_obj);
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string) { sendJsonToServer(json_string, "CH4"); cJSON_free(json_string); }
    cJSON_Delete(root);
}

void processOdorSensor(const char* device_id, int millivolts) {
    float resistance_kohm = calculate_rs((float)millivolts, ODOR_LOAD_RESISTOR);
    float ppm_ammonia = calculate_ppm(resistance_kohm, ODOR_RO, ODOR_AMMONIA_PARAM_A, ODOR_AMMONIA_PARAM_B);
    float ppm_alcohol = calculate_ppm(resistance_kohm, ODOR_RO, ODOR_ALCOHOL_PARAM_A, ODOR_ALCOHOL_PARAM_B);
    float ppm_acetone = calculate_ppm(resistance_kohm, ODOR_RO, ODOR_ACETONE_PARAM_A, ODOR_ACETONE_PARAM_B);
    float ppm_h2s = calculate_ppm(resistance_kohm, ODOR_RO, ODOR_H2S_PARAM_A, ODOR_H2S_PARAM_B);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "sensor_type", "odor_gas");
    cJSON* data_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(data_obj, "voltage_mv", millivolts);
    cJSON_AddNumberToObject(data_obj, "resistance_kohm", resistance_kohm);
    cJSON_AddNumberToObject(data_obj, "ppm_ammonia", ppm_ammonia);
    cJSON_AddNumberToObject(data_obj, "ppm_alcohol", ppm_alcohol);
    cJSON_AddNumberToObject(data_obj, "ppm_acetone", ppm_acetone);
    cJSON_AddNumberToObject(data_obj, "ppm_h2s", ppm_h2s);
    cJSON_AddNumberToObject(data_obj, "timestamp", millis());
    cJSON_AddStringToObject(data_obj, "type", "odor_data");
    cJSON_AddItemToObject(root, "data", data_obj);
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string) { sendJsonToServer(json_string, "Odor"); cJSON_free(json_string); }
    cJSON_Delete(root);
}

void processSmokeSensor(const char* device_id, int millivolts) {
    float resistance_kohm = calculate_rs((float)millivolts, SMOKE_LOAD_RESISTOR);
    float ppm_c2h5oh = calculate_ppm(resistance_kohm, SMOKE_RO, SMOKE_C2H5OH_PARAM_A, SMOKE_C2H5OH_PARAM_B);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "sensor_type", "smoke_gas");
    cJSON* data_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(data_obj, "voltage_mv", millivolts);
    cJSON_AddNumberToObject(data_obj, "resistance_kohm", resistance_kohm);
    cJSON_AddNumberToObject(data_obj, "ppm_c2h5oh", ppm_c2h5oh);
    cJSON_AddNumberToObject(data_obj, "timestamp", millis());
    cJSON_AddStringToObject(data_obj, "type", "smoke_data");
    cJSON_AddItemToObject(root, "data", data_obj);
    char* json_string = cJSON_PrintUnformatted(root);
    if (json_string) { sendJsonToServer(json_string, "Smoke"); cJSON_free(json_string); }
    cJSON_Delete(root);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi OK. IP: "); Serial.println(WiFi.localIP());
  } else { Serial.println("\nWiFi failed."); }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nESP32-S3 Multi-Gas Ingest v4.0.0");
  
  pixels.begin(); pixels.clear(); pixels.show();
  dht.begin();
  analogSetPinAttenuation(PIN_CH4, ADC_11db);
  analogSetPinAttenuation(PIN_ODOR, ADC_11db);
  analogSetPinAttenuation(PIN_SMOKE, ADC_11db);

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    uint8_t mac[6]; WiFi.macAddress(mac);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    device_id_str = String(buf);
    Serial.print("Device ID: "); Serial.println(device_id_str);
  }

  xTaskCreatePinnedToCore(ledAlertTask, "LEDAlertTask", 2048, NULL, 1, &ledTaskHandle, 1);
}

void loop() {
  // --- 1) Read all sensors ---
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int ch4_mv   = analogReadMilliVolts(PIN_CH4);
  int odor_mv  = analogReadMilliVolts(PIN_ODOR);
  int smoke_mv = analogReadMilliVolts(PIN_SMOKE);

  Serial.println("\n--- Starting Sensor Read and Send Cycle ---");
  Serial.printf("DHT: T=%.1f C, RH=%.1f %% | CH4=%d mV, Odor=%d mV, Smoke=%d mV\n", t, h, ch4_mv, odor_mv, smoke_mv);

  // --- 2) Check for threshold alerts and set the shared flag ---
  alert_active = (!isnan(t) && t > TEMP_THRESHOLD_C) ||
                 (!isnan(h) && h > HUMIDITY_THRESHOLD_PCT) ||
                 (ch4_mv > CH4_THRESHOLD_MV) ||
                 (odor_mv > ODOR_THRESHOLD_MV) ||
                 (smoke_mv > SMOKE_THRESHOLD_MV);

  // --- 3) Process and send data for each sensor separately ---
  const char* dev_id = device_id_str.c_str();
  processDhtSensor(dev_id, t, h);
  delay(200);
  processCh4Sensor(dev_id, ch4_mv);
  delay(200);
  processOdorSensor(dev_id, odor_mv);
  delay(200);
  processSmokeSensor(dev_id, smoke_mv);

  Serial.println("--- Cycle Complete ---");

  // --- 4) Wait or Deep Sleep ---
  if (USE_DEEP_SLEEP) {
    esp_sleep_enable_timer_wakeup((uint64_t)SAMPLE_EVERY_SEC * 1000000ULL);
    Serial.printf("Entering deep sleep for %d seconds...\n", SAMPLE_EVERY_SEC);
    delay(100); esp_deep_sleep_start();
  } else {
    Serial.printf("Waiting for %d seconds before next cycle...\n", SAMPLE_EVERY_SEC);
    delay(SAMPLE_EVERY_SEC * 1000);
  }
}
