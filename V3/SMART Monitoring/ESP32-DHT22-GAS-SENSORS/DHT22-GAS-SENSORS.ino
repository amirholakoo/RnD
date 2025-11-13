#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include "cJSON.h"
#include "esp_task_wdt.h"

// =========== SETTINGS ===========
const char* WIFI_SSID = "********";
const char* WIFI_PASS = "********";
const char* RPI_HOST = "192.168.2.20";
const uint16_t RPI_PORT = 7500;
const uint32_t SAMPLE_EVERY_SEC = 60;

// =========== PINS ===========
#define PIN_DHT   4
#define PIN_CH4   2
#define PIN_ODOR  1
#define PIN_SMOKE 3

// =========== CALIBRATION ===========
const float VOLTAGE_IN = 3300.0;

// CH4 
const float CH4_LOAD_RESISTOR = 3.0;
const float CH4_RO = 1.5;
const float CH4_CH4_PARAM_A = 84.2064, CH4_CH4_PARAM_B = -5.6602;
const float CH4_C3H8_PARAM_A = 29.8046, CH4_C3H8_PARAM_B = -4.4417;

// Odor 
const float ODOR_LOAD_RESISTOR = 3.0;
const float ODOR_RO = 3.0;
const float ODOR_AMMONIA_PARAM_A = 0.5328, ODOR_AMMONIA_PARAM_B = -2.7671;
const float ODOR_ALCOHOL_PARAM_A = 0.9991, ODOR_ALCOHOL_PARAM_B = -4.2704;
const float ODOR_ACETONE_PARAM_A = 1.2348, ODOR_ACETONE_PARAM_B = -2.1610;
const float ODOR_H2S_PARAM_A = 0.7095, ODOR_H2S_PARAM_B = -2.2430;

// Smoke 
const float SMOKE_LOAD_RESISTOR = 4.7;
const float SMOKE_RO = 20.0;
const float SMOKE_C2H5OH_PARAM_A = 0.4313, SMOKE_C2H5OH_PARAM_B = -2.4307;

// =========== OBJECTS ===========
DHT dht(PIN_DHT, DHT22);
String device_id_str;

// =========== WATCHDOG ===========
#define WDT_TIMEOUT_SECONDS 180  // 3 minutes

// =========== WiFi RECONNECT ONLY WHEN DISCONNECTED ===========
void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("WiFi LOST! Reconnecting...");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
    esp_task_wdt_reset();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi Reconnected! IP: "); Serial.println(WiFi.localIP());
    if (device_id_str.isEmpty()) {
      uint8_t mac[6]; WiFi.macAddress(mac);
      char buf[18];
      snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      device_id_str = String(buf);
      Serial.print("Device ID: "); Serial.println(device_id_str);
    }
  } else {
    Serial.println("\nWiFi reconnect FAILED! Will retry next cycle...");
  }
}

// =========== HELPER FUNCTIONS ===========
float calculate_rs(float v_out, float load_resistor) {
  if (v_out <= 0) return -1.0;
  return load_resistor * (VOLTAGE_IN - v_out) / v_out;
}

float calculate_ppm(float rs, float ro, float a, float b) {
  if (rs <= 0 || ro <= 0) return 0.0;
  return a * pow(rs / ro, b);
}

void sendJsonToServer(const char* json_string, const char* sensor_name) {
  ensureWiFiConnected();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi, skipping send.");
    return;
  }

  HTTPClient http;
  String url = "http://" + String(RPI_HOST) + ":" + String(RPI_PORT);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);

  int code = http.POST(json_string);
  Serial.printf("[%s] -> %d\n", sensor_name, code);
  http.end();

  esp_task_wdt_reset();
}

// =========== SENSOR PROCESSING ===========
void processDhtSensor(const char* device_id, float t, float h) {
  if (isnan(t) || isnan(h)) return;
  cJSON* root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "device_id", device_id);
  cJSON_AddStringToObject(root, "sensor_type", "dht22");
  cJSON* data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "temperature", t);
  cJSON_AddNumberToObject(data, "humidity", h);
  cJSON_AddNumberToObject(data, "timestamp", millis());
  cJSON_AddStringToObject(data, "type", "dht22_data");
  cJSON_AddItemToObject(root, "data", data);
  char* json = cJSON_PrintUnformatted(root);
  if (json) { sendJsonToServer(json, "DHT22"); cJSON_free(json); }
  cJSON_Delete(root);
}

void processCh4Sensor(const char* device_id, int mv) {
  float rs = calculate_rs(mv, CH4_LOAD_RESISTOR);
  float ppm_ch4 = calculate_ppm(rs, CH4_RO, CH4_CH4_PARAM_A, CH4_CH4_PARAM_B);
  float ppm_c3h8 = calculate_ppm(rs, CH4_RO, CH4_C3H8_PARAM_A, CH4_C3H8_PARAM_B);
  cJSON* root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "device_id", device_id);
  cJSON_AddStringToObject(root, "sensor_type", "ch4_gas");
  cJSON* data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "voltage_mv", mv);
  cJSON_AddNumberToObject(data, "resistance_kohm", rs);
  cJSON_AddNumberToObject(data, "ppm_ch4", ppm_ch4);
  cJSON_AddNumberToObject(data, "ppm_c3h8", ppm_c3h8);
  cJSON_AddNumberToObject(data, "timestamp", millis());
  cJSON_AddStringToObject(data, "type", "ch4_data");
  cJSON_AddItemToObject(root, "data", data);
  char* json = cJSON_PrintUnformatted(root);
  if (json) { sendJsonToServer(json, "CH4"); cJSON_free(json); }
  cJSON_Delete(root);
}

void processOdorSensor(const char* device_id, int mv) {
  float rs = calculate_rs(mv, ODOR_LOAD_RESISTOR);
  float nh3 = calculate_ppm(rs, ODOR_RO, ODOR_AMMONIA_PARAM_A, ODOR_AMMONIA_PARAM_B);
  float alc = calculate_ppm(rs, ODOR_RO, ODOR_ALCOHOL_PARAM_A, ODOR_ALCOHOL_PARAM_B);
  float ace = calculate_ppm(rs, ODOR_RO, ODOR_ACETONE_PARAM_A, ODOR_ACETONE_PARAM_B);
  float h2s = calculate_ppm(rs, ODOR_RO, ODOR_H2S_PARAM_A, ODOR_H2S_PARAM_B);
  cJSON* root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "device_id", device_id);
  cJSON_AddStringToObject(root, "sensor_type", "odor_gas");
  cJSON* data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "voltage_mv", mv);
  cJSON_AddNumberToObject(data, "resistance_kohm", rs);
  cJSON_AddNumberToObject(data, "ppm_ammonia", nh3);
  cJSON_AddNumberToObject(data, "ppm_alcohol", alc);
  cJSON_AddNumberToObject(data, "ppm_acetone", ace);
  cJSON_AddNumberToObject(data, "ppm_h2s", h2s);
  cJSON_AddNumberToObject(data, "timestamp", millis());
  cJSON_AddStringToObject(data, "type", "odor_data");
  cJSON_AddItemToObject(root, "data", data);
  char* json = cJSON_PrintUnformatted(root);
  if (json) { sendJsonToServer(json, "Odor"); cJSON_free(json); }
  cJSON_Delete(root);
}

void processSmokeSensor(const char* device_id, int mv) {
  float rs = calculate_rs(mv, SMOKE_LOAD_RESISTOR);
  float ppm = calculate_ppm(rs, SMOKE_RO, SMOKE_C2H5OH_PARAM_A, SMOKE_C2H5OH_PARAM_B);
  cJSON* root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "device_id", device_id);
  cJSON_AddStringToObject(root, "sensor_type", "smoke_gas");
  cJSON* data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "voltage_mv", mv);
  cJSON_AddNumberToObject(data, "resistance_kohm", rs);
  cJSON_AddNumberToObject(data, "ppm_c2h5oh", ppm);
  cJSON_AddNumberToObject(data, "timestamp", millis());
  cJSON_AddStringToObject(data, "type", "smoke_data");
  cJSON_AddItemToObject(root, "data", data);
  char* json = cJSON_PrintUnformatted(root);
  if (json) { sendJsonToServer(json, "Smoke"); cJSON_free(json); }
  cJSON_Delete(root);
}

// =========== SETUP ===========
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== FINAL v7.0: RECONNECT ON DISCONNECT ONLY ===");

  dht.begin();
  analogSetPinAttenuation(PIN_CH4, ADC_11db);
  analogSetPinAttenuation(PIN_ODOR, ADC_11db);
  analogSetPinAttenuation(PIN_SMOKE, ADC_11db);

  // Initial WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi OK. IP: "); Serial.println(WiFi.localIP());
    uint8_t mac[6]; WiFi.macAddress(mac);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    device_id_str = String(buf);
    Serial.print("Device ID: "); Serial.println(device_id_str);
  } else {
    Serial.println("\nWiFi failed. Will retry on disconnect...");
  }

  // Watchdog (ESP32-S3 compatible)
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms    = WDT_TIMEOUT_SECONDS * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  Serial.printf("Watchdog enabled: %d sec\n", WDT_TIMEOUT_SECONDS);
}

// =========== LOOP ===========
unsigned long lastSampleTime = 0;
bool wasConnected = false;  // Previous WiFi state

void loop() {
  unsigned long now = millis();

  // --- Reconnect only when disconnected ---
  bool currentlyConnected = (WiFi.status() == WL_CONNECTED);
  if (!currentlyConnected && wasConnected) {
    Serial.println("WiFi DISCONNECTED! Reconnecting...");
    ensureWiFiConnected();
  }
  wasConnected = currentlyConnected;

  // --- Send data every 60 seconds ---
  if (now - lastSampleTime >= SAMPLE_EVERY_SEC * 1000UL) {
    lastSampleTime = now;

    // Ensure WiFi before sending
    ensureWiFiConnected();

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int ch4_mv = analogReadMilliVolts(PIN_CH4);
    int odor_mv = analogReadMilliVolts(PIN_ODOR);
    int smoke_mv = analogReadMilliVolts(PIN_SMOKE);

    Serial.printf("\nT=%.1f°C H=%.1f%% | CH4=%d Odor=%d Smoke=%d\n", t, h, ch4_mv, odor_mv, smoke_mv);

    const char* dev_id = device_id_str.c_str();
    processDhtSensor(dev_id, t, h);
    processCh4Sensor(dev_id, ch4_mv);
    processOdorSensor(dev_id, odor_mv);
    processSmokeSensor(dev_id, smoke_mv);

    Serial.println("Cycle Complete");
    esp_task_wdt_reset();
  }

  delay(100);
  esp_task_wdt_reset();
}