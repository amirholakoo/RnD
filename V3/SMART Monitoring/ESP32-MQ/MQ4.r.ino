#include <WiFi.h>
#include <HTTPClient.h>
#include "cJSON.h"
#include "esp_task_wdt.h"

// =========== setting ===========
const char* WIFI_SSID = "*********";
const char* WIFI_PASS = "********";
const char* RPI_HOST = "192.168.2.20";
const uint16_t RPI_PORT = 7500;
const uint32_t SAMPLE_EVERY_SEC = 60;

// =========== pin MQ-4 ===========
#define PIN_MQ4 5

// =========== callibracion ===========
const float VOLTAGE_IN = 3300.0;       // mV
const float LOAD_RESISTOR = 3.0;       // kΩ
const float RO = 1.5;                  // kΩ
const float A = 84.2064;
const float B = -5.6602;

// =========== Device ID ===========
String device_id_str = "B4:3A:45:3F:D4:58";

// =========== whatchdog ===========
#define WDT_TIMEOUT_SECONDS 180

// =========== wifi ===========
void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("\n[WiFi] LOST! Reconnecting...");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500); Serial.print("."); tries++;
    esp_task_wdt_reset();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\n[WiFi] OK! IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] FAILED!");
  }
}

// =========== math ===========
float calculate_rs(float v_out_mv) {
  if (v_out_mv <= 0) return -1.0;
  return LOAD_RESISTOR * (VOLTAGE_IN - v_out_mv) / v_out_mv;
}

float calculate_ppm(float rs_kohm) {
  if (rs_kohm <= 0 || RO <= 0) return 0.0;
  return A * pow(rs_kohm / RO, B);
}

// =========== json ===========
void sendMQ4Data(float ppm, float voltage_mv, float rs_kohm) {
  ensureWiFiConnected();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] No WiFi!");
    return;
  }

  float voltage_v = voltage_mv / 1000.0;          // mv→ v
  float resistance_ohm = rs_kohm * 1000.0;       // kΩ → Ω

  cJSON* root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "device_id", device_id_str.c_str());
  cJSON_AddStringToObject(root, "sensor_type", "MQ4");  // Optional

  cJSON* data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "ppm_methane", ppm);
  cJSON_AddNumberToObject(data, "voltage", voltage_v);
  cJSON_AddNumberToObject(data, "resistance", resistance_ohm);
  cJSON_AddNumberToObject(data, "timestamp", millis());
  cJSON_AddStringToObject(data, "type", "mq4_data");
  cJSON_AddItemToObject(root, "data", data);

  char* json = cJSON_PrintUnformatted(root);
  if (!json) {
    Serial.println("[ERROR] JSON failed!");
    cJSON_Delete(root);
    return;
  }

  Serial.println("\n=== JSON SENT ===");
  Serial.println(json);
  Serial.println("=================");

  HTTPClient http;
  String url = "http://" + String(RPI_HOST) + ":" + String(RPI_PORT);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  int code = http.POST(json);
  Serial.printf("[HTTP] Response Code: %d\n", code);
  if (code > 0) {
    String payload = http.getString();
    Serial.printf("[HTTP] Response: %s\n", payload.c_str());
  } else {
    Serial.printf("[ERROR] HTTP Error: %s\n", http.errorToString(code).c_str());
  }

  http.end();
  cJSON_free(json);
  cJSON_Delete(root);
  esp_task_wdt_reset();
}

// =========== Setup ===========
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== MQ-4 → SERVER (MQ4 FORMAT) ===");
  Serial.print("Device ID: "); Serial.println(device_id_str);

  analogSetPinAttenuation(PIN_MQ4, ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500); Serial.print("."); tries++;
    esp_task_wdt_reset();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\n[WiFi] OK! IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] FAILED!");
  }

  esp_task_wdt_config_t wdt_config = { .timeout_ms = WDT_TIMEOUT_SECONDS * 1000, .trigger_panic = true };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
}

// =========== Loop ===========
unsigned long lastSampleTime = 0;
bool wasConnected = false;

void loop() {
  unsigned long now = millis();

  bool currentlyConnected = (WiFi.status() == WL_CONNECTED);
  if (!currentlyConnected && wasConnected) {
    Serial.println("\n[WiFi] DISCONNECTED! Reconnecting...");
    ensureWiFiConnected();
  }
  wasConnected = currentlyConnected;

  if (now - lastSampleTime >= SAMPLE_EVERY_SEC * 1000UL) {
    lastSampleTime = now;
    ensureWiFiConnected();

    int mv = analogReadMilliVolts(PIN_MQ4);
    float rs_kohm = calculate_rs(mv);
    float ppm = calculate_ppm(rs_kohm);

    Serial.printf("\n[SENSOR] %d mV | %.1f kΩ | %.3f ppm\n", mv, rs_kohm, ppm);

    sendMQ4Data(ppm, mv, rs_kohm);

    Serial.println("Cycle Complete");
    esp_task_wdt_reset();
  }

  delay(100);
  esp_task_wdt_reset();
}
