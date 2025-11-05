/*
   LED status meaning:
   - Boot: blue solid
   - WiFi connecting: yellow solid
   - WiFi error: yellow blink (keeps retrying)
   - Normal: soft white solid
   - Periodic send (60s): two green blinks (unless vibration active)
   - Vibration: red fast blink; send every 2s while vibration active
*/

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>
#include "esp_task_wdt.h"

// ---------- Hardware configuration ----------
#define MPU_ADDR 0x68     // I2C address of MPU6500
#define SDA_PIN 8         // I2C SDA pin
#define SCL_PIN 9         // I2C SCL pin

// WS2812 LED
#define LED_PIN 38
#define LED_COUNT 1
Adafruit_NeoPixel neo(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------- WiFi & server configuration ----------
const char* ssid = "*******";
const char* password = "********";
const char* serverUrl = "http://192.168.2.20:7500";
const char* MAC_FALLBACK = "94:26:4E:DA:3B:D8"; // backup MAC if WiFi.macAddress() fails

// ---------- MPU constants ----------
#define ACCEL_SCALE 16384.0  // ±2g full-scale range

// ---------- Signal filtering ----------
float ax_f = 0, ay_f = 0, az_f = 0;          // filtered acceleration values
float Vx_max = 0, Vy_max = 0, Vz_max = 0;    // peak values
float Vtotal_max = 0;                        // peak of total vibration magnitude

// ---------- Industrial vibration parameters ----------
float baseline = 0.05;       // baseline vibration level
const float noiseFloor = 0.02;
const float minBaseline = 0.03;
const float shockFactor = 1.8;   // threshold multiplier for sudden vibration
const float driftFactor = 1.3;   // threshold multiplier for gradual drift
const float learningRate = 0.002; // baseline learning rate (0.2% per update)

// ---------- Rolling buffer for vibration drift detection ----------
#define BUF_SIZE 12
float vibBuffer[BUF_SIZE];
int bufIndex = 0;

// ---------- Timers ----------
unsigned long lastSend = 0;
unsigned long lastDisplay = 0;
const unsigned long normalInterval = 60000;   // periodic send every 60s
const unsigned long displayInterval = 2500;   // serial print interval
const unsigned long wifiReconnectTimeout = 180000; // WiFi connect timeout (3 min)

// ---------- Alarm timing ----------
unsigned long lastAlarmTime = 0;
unsigned long alarmCooldown = 2000;    // allow vibration send every 2s
unsigned long lastVibSend = 0;
unsigned long lastVibDetected = 0;
const unsigned long vibTimeout = 500;  // ms threshold to declare vibration stopped

// ---------- LED colors ----------
uint32_t COLOR_BOOT   = neo.Color(0, 0, 255);    // blue
uint32_t COLOR_WIFI   = neo.Color(255, 255, 0);  // yellow
uint32_t COLOR_NORMAL = neo.Color(20, 20, 20);   // soft white
uint32_t COLOR_SEND   = neo.Color(255, 0, 0);    // green
uint32_t COLOR_VIB    = neo.Color(0, 255, 0);    // red
uint32_t COLOR_OFF    = neo.Color(0, 0, 0);

// ---------- LED blinking control ----------
bool ledBlinkEnabled = false;
uint32_t ledBlinkColor = COLOR_OFF;
unsigned long ledBlinkInterval = 200;
unsigned long lastLedToggle = 0;
bool ledOn = false;

// ---------- WiFi states ----------
bool wifiConnected = false;
bool wifiError = false;
unsigned long wifiAttemptStart = 0;
unsigned long lastWifiRetry = 0;
const unsigned long wifiRetryInterval = 30000; // retry WiFi every 30s

// ---------- Vibration state ----------
bool vibrationActive = false;

// Low-pass filter coefficient
const float alpha = 0.9;

// ---------- Watchdog (3 minutes timeout) ----------
esp_task_wdt_config_t wdt_cfg = {
  .timeout_ms = 180000,
  .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
  .trigger_panic = true
};

// =====================================================
//                 LED Utility Functions
// =====================================================

// Set LED to a solid color
void ledSetSolid(uint32_t color, uint8_t brightness = 150) {
  ledBlinkEnabled = false;
  neo.setBrightness(brightness);
  neo.setPixelColor(0, color);
  neo.show();
  ledOn = (color != COLOR_OFF);
}

// Start blinking LED with specific color & interval (non-blocking)
void ledStartBlink(uint32_t color, unsigned long interval_ms, uint8_t brightness = 200) {
  ledBlinkEnabled = true;
  ledBlinkColor = color;
  ledBlinkInterval = interval_ms;
  neo.setBrightness(brightness);
  lastLedToggle = millis();
  ledOn = true;
  neo.setPixelColor(0, ledBlinkColor);
  neo.show();
}

// Handle blinking logic (called in loop)
void ledHandleBlink() {
  if (!ledBlinkEnabled) return;
  unsigned long now = millis();
  if (now - lastLedToggle >= ledBlinkInterval) {
    lastLedToggle = now;
    ledOn = !ledOn;
    neo.setPixelColor(0, ledOn ? ledBlinkColor : COLOR_OFF);
    neo.show();
  }
}

// =====================================================
//                 Green Double Pulse
// =====================================================

enum GreenPulseState { GP_IDLE, GP_START, GP_PULSE1_ON, GP_PULSE1_OFF, GP_PULSE2_ON, GP_PULSE2_OFF, GP_DONE };
GreenPulseState gpState = GP_IDLE;
unsigned long gpTimer = 0;
const unsigned long gpPulseMs = 150;

// Trigger two short green flashes
void startGreenDoublePulse() {
  if (vibrationActive) return; // do not pulse green if vibration active
  gpState = GP_START;
  gpTimer = millis();
}

// Handle green pulse timing (non-blocking)
void handleGreenDoublePulse() {
  unsigned long now = millis();
  switch (gpState) {
    case GP_IDLE: return;
    case GP_START:
      gpState = GP_PULSE1_ON;
      gpTimer = now;
      neo.setBrightness(255);
      neo.setPixelColor(0, COLOR_SEND);
      neo.show();
      break;
    case GP_PULSE1_ON:
      if (now - gpTimer >= gpPulseMs) {
        gpState = GP_PULSE1_OFF;
        gpTimer = now;
        neo.setPixelColor(0, COLOR_OFF);
        neo.show();
      }
      break;
    case GP_PULSE1_OFF:
      if (now - gpTimer >= gpPulseMs) {
        gpState = GP_PULSE2_ON;
        gpTimer = now;
        neo.setPixelColor(0, COLOR_SEND);
        neo.show();
      }
      break;
    case GP_PULSE2_ON:
      if (now - gpTimer >= gpPulseMs) {
        gpState = GP_PULSE2_OFF;
        gpTimer = now;
        neo.setPixelColor(0, COLOR_OFF);
        neo.show();
      }
      break;
    case GP_PULSE2_OFF:
      if (now - gpTimer >= gpPulseMs) {
        gpState = GP_DONE;
        gpTimer = now;
      }
      break;
    case GP_DONE:
      gpState = GP_IDLE;
      if (!vibrationActive) ledSetSolid(COLOR_NORMAL, 20);
      break;
  }
}

// =====================================================
//                 MPU I2C Utilities
// =====================================================

// Read 16-bit register from MPU6500
int16_t read16(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 2);
  int16_t hi = Wire.read();
  int16_t lo = Wire.read();
  return (hi << 8) | lo;
}

// =====================================================
//                 HTTP Communication
// =====================================================

// Send HTTP POST request with JSON data
void sendJSON(const String &jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(jsonData);
    Serial.print("HTTP Response: ");
    Serial.println(code);
    http.end();
  } else {
    Serial.println("⚠️ Not connected when sending");
  }
}

// Prepare JSON payload and send
void sendToServerPayload(float Vx, float Vy, float Vz, float Vtotal, bool isAlarm) {
  String deviceId = WiFi.macAddress();
  if (deviceId == "00:00:00:00:00:00") deviceId = String(MAC_FALLBACK);

  String json = "{";
  json += "\"device_id\":\"" + deviceId + "\",";
  json += "\"sensor_type\":\"MPU6500\",";
  json += "\"alarm\":"; json += (isAlarm ? "true" : "false"); json += ",";
  json += "\"data\":{";
  json += "\"Vx\":" + String(Vx, 4) + ",";
  json += "\"Vy\":" + String(Vy, 4) + ",";
  json += "\"Vz\":" + String(Vz, 4) + ",";
  json += "\"Vtotal\":" + String(Vtotal, 4);
  json += "}}";
  Serial.print("Sending JSON -> ");
  Serial.println(json);
  sendJSON(json);
}

// =====================================================
//                 WiFi Management
// =====================================================

// Start WiFi connection (yellow solid)
void startWiFiConnect() {
  wifiConnected = false;
  wifiError = false;
  wifiAttemptStart = millis();
  ledSetSolid(COLOR_WIFI, 200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

// Handle WiFi connection, LED state, and retries
void handleWiFi() {
  unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      wifiError = false;
      Serial.println("✅ WiFi connected");
      ledSetSolid(COLOR_NORMAL, 20);
    }
    return;
  } else {
    // Not connected or lost
    if (!wifiConnected && (now - wifiAttemptStart < wifiReconnectTimeout)) return;
    wifiConnected = false;
    wifiError = true;
    // Start yellow blinking if not already
    if (!ledBlinkEnabled || ledBlinkColor != COLOR_WIFI) {
      ledStartBlink(COLOR_WIFI, 400, 200);
    }
    // Try reconnect every 30s
    if (now - lastWifiRetry > wifiRetryInterval) {
      lastWifiRetry = now;
      Serial.println("🔁 WiFi retry...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }
}

// =====================================================
//                 Setup
// =====================================================

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // Initialize watchdog
  esp_task_wdt_init(&wdt_cfg);
  esp_task_wdt_add(NULL);

  // Boot LED (blue)
  neo.begin();
  neo.show();
  ledSetSolid(COLOR_BOOT, 255);
  delay(1200);

  // Start WiFi
  startWiFiConnect();

  // Initialize MPU6500
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission();
  delay(50);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission();
  delay(50);

  // 2-second baseline calibration
  Serial.println("Calibrating baseline (2s) ...");
  float sum = 0; int samples = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 2000) {
    float ax = read16(0x3B) / ACCEL_SCALE;
    float ay = read16(0x3D) / ACCEL_SCALE;
    float az = read16(0x3F) / ACCEL_SCALE;
    float V = sqrt(ax * ax + ay * ay + az * az);
    sum += V; samples++;
    delay(40);
  }
  if (samples > 0) baseline = sum / samples;
  baseline = max(baseline, minBaseline);
  for (int i = 0; i < BUF_SIZE; i++) vibBuffer[i] = baseline;

  Serial.print("✅ Baseline = ");
  Serial.println(baseline, 4);
}

// =====================================================
//                 Main Loop
// =====================================================

void loop() {
  esp_task_wdt_reset();
  unsigned long now = millis();

  // 1. Manage WiFi and LED
  handleWiFi();

  // 2. Read MPU accelerometer
  float ax = read16(0x3B) / ACCEL_SCALE;
  float ay = read16(0x3D) / ACCEL_SCALE;
  float az = read16(0x3F) / ACCEL_SCALE;

  // Apply low-pass filter to remove gravity
  ax_f = alpha * ax_f + (1 - alpha) * ax;
  ay_f = alpha * ay_f + (1 - alpha) * ay;
  az_f = alpha * az_f + (1 - alpha) * az;

  float Vx = ax - ax_f;
  float Vy = ay - ay_f;
  float Vz = az - az_f;
  float Vtotal = sqrt(Vx * Vx + Vy * Vy + Vz * Vz);
  Vtotal = max(Vtotal, noiseFloor);

  // 3. Rolling average for drift detection
  vibBuffer[bufIndex] = Vtotal;
  bufIndex = (bufIndex + 1) % BUF_SIZE;
  float avg = 0;
  for (int i = 0; i < BUF_SIZE; i++) avg += vibBuffer[i];
  avg /= BUF_SIZE;

  // 4. Detect vibration or drift
  bool shockEvent = (Vtotal > baseline * shockFactor);
  bool driftEvent = (avg > baseline * driftFactor);
  bool isAlarmCandidate = (shockEvent || driftEvent);

  // 5. Alarm gating (cooldown-controlled)
  bool alarmNow = false;
  if (isAlarmCandidate) {
    lastVibDetected = now;
    if (now - lastAlarmTime > alarmCooldown) {
      alarmNow = true;
      lastAlarmTime = now;
    }
  }

  // 6. Determine vibration state
  vibrationActive = (now - lastVibDetected <= vibTimeout);

  // 7. Adaptive baseline update
  if (!isAlarmCandidate && avg < baseline * 1.1)
    baseline = baseline * (1.0 - learningRate) + avg * learningRate;
  baseline = max(baseline, minBaseline);

  // 8. Track peaks
  if (fabs(Vx) > fabs(Vx_max)) Vx_max = Vx;
  if (fabs(Vy) > fabs(Vy_max)) Vy_max = Vy;
  if (fabs(Vz) > fabs(Vz_max)) Vz_max = Vz;
  if (Vtotal > Vtotal_max) Vtotal_max = Vtotal;

  // 9. LED logic priority:
  // vibrationActive > WiFi error > normal
  ledHandleBlink();
  handleGreenDoublePulse();

  if (vibrationActive) {
    gpState = GP_IDLE;
    if (!ledBlinkEnabled || ledBlinkColor != COLOR_VIB)
      ledStartBlink(COLOR_VIB, 180, 200);
  } else if (wifiError) {
    // already handled by WiFi handler
  } else {
    if (gpState == GP_IDLE && (!ledBlinkEnabled || ledBlinkColor != COLOR_NORMAL))
      ledSetSolid(COLOR_NORMAL, 20);
  }

  // 10. Serial debug output
  if (now - lastDisplay >= displayInterval) {
    lastDisplay = now;
    Serial.printf("Vx:%.4f Vy:%.4f Vz:%.4f | V:%.4f | avg:%.4f | base:%.4f\n",
                  Vx, Vy, Vz, Vtotal, avg, baseline);
  }

  // 11. Data sending logic
  bool shouldSend = false;
  bool sendIsAlarm = false;

  if (now - lastSend >= normalInterval) {
    shouldSend = true;
    sendIsAlarm = false;
  }

  if (alarmNow) {
    shouldSend = true;
    sendIsAlarm = true;
  }

  if (shouldSend) {
    lastSend = now;
    if (sendIsAlarm)
      sendToServerPayload(Vx, Vy, Vz, Vtotal, true);
    else {
      sendToServerPayload(Vx_max, Vy_max, Vz_max, Vtotal_max, false);
      if (!vibrationActive) startGreenDoublePulse();
    }
    Vx_max = Vy_max = Vz_max = Vtotal_max = 0;
  }

  delay(20); // prevent high CPU load
}
