/*
  Paper-mill friendly ESP32 architecture (skeleton)
  - Read sensors every 1s
  - Batch + send every 10s
  - WiFi: IOT then Homayoun
  - If no WiFi: ESP-NOW to relay
  - Ask server for OTA every 5 minutes (only enter OTA when told)
  - Watchdog + safe reboot
*/

#include <WiFi.h>
#include <HTTPClient.h>

// ---------- Settings you will customize per device ----------
const char* DEVICE_ID = "ESP32-BCDDC2";
const char* LOCATION  = "Dryer_Section_Above_18";

const char* WIFI1_SSID = "IOT";
const char* WIFI1_PASS = "xxxxxx";

const char* WIFI2_SSID = "Homayoun";
const char* WIFI2_PASS = "xxxxxx";

const char* SERVER_BASE = "http://192.168.10.10:5000"; // your server IP

// Timing
const unsigned long SAMPLE_EVERY_MS   = 1000;   // read sensors every 1s
const unsigned long SEND_EVERY_MS     = 10000;  // send batch every 10s
const unsigned long OTA_CHECK_EVERY_MS= 300000; // check OTA every 5 min

// Recovery policy
const unsigned long WIFI_TRY_TIMEOUT_MS   = 12000;   // 12s per SSID
const unsigned long NO_DELIVERY_RESTART_MS= 300000;  // 5 min -> restart WiFi
const unsigned long NO_DELIVERY_REBOOT_MS = 900000;  // 15 min -> reboot

// ---------- Simple in-memory buffer ----------
struct Sample {
  uint32_t ts;
  float t;
  float rh;
  int ch4;
  int smoke;
};

const int MAX_SAMPLES = 30; // 30 seconds stored (adjust)
Sample samples[MAX_SAMPLES];
int sampleCount = 0;

unsigned long lastSampleMs = 0;
unsigned long lastSendMs = 0;
unsigned long lastOtaCheckMs = 0;

unsigned long lastSuccessfulDeliveryMs = 0;

// ---------- Helper: connect WiFi with timeout ----------
bool connectWiFi(const char* ssid, const char* pass, unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
  }
  return (WiFi.status() == WL_CONNECTED);
}

// ---------- Helper: try both WiFi networks ----------
bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  // Try primary network
  if (connectWiFi(WIFI1_SSID, WIFI1_PASS, WIFI_TRY_TIMEOUT_MS)) return true;

  // Try backup network
  if (connectWiFi(WIFI2_SSID, WIFI2_PASS, WIFI_TRY_TIMEOUT_MS)) return true;

  return false;
}

// ---------- Send HELLO once WiFi is connected ----------
void sendHello() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(SERVER_BASE) + "/api/iot/hello";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Build JSON manually (simple)
  String payload = "{";
  payload += "\"type\":\"hello\",";
  payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"location\":\"" + String(LOCATION) + "\",";
  payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"ssid\":\"" + WiFi.SSID() + "\"";
  payload += "}";

  int code = http.POST(payload);
  http.end();
}

// ---------- Read sensors (replace with your real reads) ----------
Sample readSensors() {
  Sample s;
  s.ts = (uint32_t)time(nullptr); // if you have NTP; otherwise use millis/1000
  s.t = 33.2;     // TODO: replace with DHT temp
  s.rh = 54.1;    // TODO: replace with DHT humidity
  s.ch4 = 12;     // TODO: replace with CH4 sensor
  s.smoke = 0;    // TODO: replace with smoke sensor
  return s;
}

// ---------- Send buffered samples via HTTP ----------
bool sendBatchHTTP() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (sampleCount == 0) return true;

  HTTPClient http;
  String url = String(SERVER_BASE) + "/api/iot/data";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload (keep it small)
  String payload = "{";
  payload += "\"type\":\"data\",";
  payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"location\":\"" + String(LOCATION) + "\",";
  payload += "\"samples\":[";

  for (int i = 0; i < sampleCount; i++) {
    payload += "{";
    payload += "\"ts\":" + String(samples[i].ts) + ",";
    payload += "\"t\":" + String(samples[i].t, 2) + ",";
    payload += "\"rh\":" + String(samples[i].rh, 2) + ",";
    payload += "\"ch4\":" + String(samples[i].ch4) + ",";
    payload += "\"smoke\":" + String(samples[i].smoke);
    payload += "}";
    if (i < sampleCount - 1) payload += ",";
  }
  payload += "]}";

  int code = http.POST(payload);
  http.end();

  // Success if 200..299
  if (code >= 200 && code < 300) {
    sampleCount = 0; // clear buffer only on success
    lastSuccessfulDeliveryMs = millis();
    return true;
  }
  return false;
}

// ---------- OTA check (server decides) ----------
void checkOTA() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(SERVER_BASE) + "/api/iot/ota_check?device_id=" + DEVICE_ID;
  http.begin(url);

  int code = http.GET();
  // TODO: parse response JSON:
  // if {"ota":true,"window_s":180,"url":"..."} then start OTA
  http.end();
}

void setup() {
  Serial.begin(115200);

  // Start by trying WiFi
  if (ensureWiFi()) {
    sendHello();
    lastSuccessfulDeliveryMs = millis(); // assume okay at start
  } else {
    // TODO: init ESP-NOW here as fallback
  }
}

void loop() {
  unsigned long now = millis();

  // 1) Sample sensors every second
  if (now - lastSampleMs >= SAMPLE_EVERY_MS) {
    lastSampleMs = now;

    Sample s = readSensors();
    if (sampleCount < MAX_SAMPLES) {
      samples[sampleCount++] = s;
    }
    // If buffer full, you can either drop oldest or force send
  }

  // 2) Every SEND_EVERY_MS try to deliver
  if (now - lastSendMs >= SEND_EVERY_MS) {
    lastSendMs = now;

    if (ensureWiFi()) {
      // Send via WiFi
      sendBatchHTTP();
    } else {
      // TODO: send batch via ESP-NOW relay
      // If relay not available, keep buffering
    }
  }

  // 3) OTA check every 5 minutes (only enter OTA if server says so)
  if (now - lastOtaCheckMs >= OTA_CHECK_EVERY_MS) {
    lastOtaCheckMs = now;
    if (ensureWiFi()) checkOTA();
  }

  // 4) Recovery policy
  unsigned long sinceOk = now - lastSuccessfulDeliveryMs;

  // If no delivery for a while -> restart WiFi stack
  if (sinceOk > NO_DELIVERY_RESTART_MS && sinceOk < NO_DELIVERY_REBOOT_MS) {
    WiFi.disconnect(true);
    delay(200);
  }

  // If no delivery too long -> reboot
  if (sinceOk >= NO_DELIVERY_REBOOT_MS) {
    ESP.restart();
  }

  delay(5); // keep loop responsive (avoid long delays)
}
