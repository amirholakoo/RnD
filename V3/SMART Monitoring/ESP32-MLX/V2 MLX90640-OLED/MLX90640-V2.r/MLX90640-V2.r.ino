#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

// ---------------- OLED second bus ----------------
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// OLED I2C
#define OLED_SDA  2   // IO2 → SDA
#define OLED_SCL  1   // IO1 → SCL (SCK)

// FOR SECOND BUS OLED
TwoWire WireOLED = TwoWire(1);  // Wire1 = SECOND BUS

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &WireOLED, -1);
// ------------------------------------------------

// =================================================================
// USER CONFIGURATION
// =================================================================
const char* WIFI_SSID = "***"; 
const char* WIFI_PASS = "********";

const char* remoteServer = "192.168.2.20";
const int remotePort = 7500;
const bool enableRemoteLogging = true;
const unsigned long REMOTE_POST_INTERVAL = 60000;

const bool enableWatchdog = true;
const int WATCHDOG_TIMEOUT = 2400;

// MLX90640 Fist bus
#define MLX_SDA 8
#define MLX_SCL 9
// =================================================================

#define EMMISIVITY 0.95
#define TA_SHIFT 8

const byte MLX90640_address = 0x33;
paramsMLX90640 mlx90640;
static float tempValues[32 * 24];

WebServer server(80);

unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 500;
unsigned long lastRemotePostTime = 0;
String connectedSSID = "";

// Function prototypes
void connectToWiFi();
void handleRoot();
void handleData();
void handleIP();
void readTempValues();
void postToRemoteServer();
void updateOLED();

// ----------------------------------------------------------------

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    connectedSSID = String(WIFI_SSID);
    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi. Halting.");
    while (1) delay(1000);
  }
}

// Handle root URL
void handleRoot() {
  String message = "ESP32 Thermal Sensor Server\n\n";
  message += "Device ID (MAC): " + WiFi.macAddress() + "\n";
  message += "IP Address: " + WiFi.localIP().toString() + "\n";
  message += "Connected to: " + connectedSSID + "\n";
  message += "Signal Strength: " + String(WiFi.RSSI()) + " dBm\n";
  message += "Uptime: " + String(millis() / 1000) + " seconds\n";
  if (enableWatchdog) {
    message += "Watchdog: Enabled (" + String(WATCHDOG_TIMEOUT) + "s timeout)\n";
  }
  message += "\nAvailable Endpoints:\n";
  message += "  GET  /data  - Get thermal sensor data in JSON format\n";
  message += "  GET  /ip    - Get current IP in JSON format\n";
  server.send(200, "text/plain", message);
}

// Handle /data endpoint
void handleData() {
  StaticJsonDocument<8192> doc;

  doc["device_id"] = WiFi.macAddress();
  doc["ip_address"] = WiFi.localIP().toString();
  doc["sensor_type"] = "MLX90640-CAM";
  doc["type"] = "sensor_data";
  doc["timestamp_ms"] = millis();

  JsonObject data = doc.createNestedObject("data");

  uint16_t mlx90640Frame[834];
  MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
  data["ambient_temperature"] = MLX90640_GetTa(mlx90640Frame, &mlx90640);
  data["vdd"] = MLX90640_GetVdd(mlx90640Frame, &mlx90640);

  JsonArray heatmap = data.createNestedArray("heatmap");
  for (int i = 0; i < 768; i++) heatmap.add(tempValues[i]);

  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}
// Handle /ip endpoint
void handleIP() {
  StaticJsonDocument<256> doc;
  doc["device_id"] = WiFi.macAddress();
  doc["ip_address"] = WiFi.localIP().toString();
  doc["connected_ssid"] = connectedSSID;
  doc["rssi"] = WiFi.RSSI();

  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}

// POST data to remote server
void postToRemoteServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://" + String(remoteServer) + ":" + String(remotePort);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<4096> doc;
  doc["device_id"] = WiFi.macAddress();
  doc["ip_address"] = WiFi.localIP().toString();
  doc["sensor_type"] = "MLX90640-CAM";

  JsonObject data = doc.createNestedObject("data");
  data["status"] = 200;
  data["timestamp_ms"] = millis();
  data["connected_ssid"] = connectedSSID;
  data["rssi"] = WiFi.RSSI();

  uint16_t mlx90640Frame[834];
  MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
  data["ambient_temperature"] = MLX90640_GetTa(mlx90640Frame, &mlx90640);
  data["vdd"] = MLX90640_GetVdd(mlx90640Frame, &mlx90640);

  String jsonString;
  serializeJson(doc, jsonString);

  int httpResponseCode = http.POST(jsonString);
  http.end();
}

// Read MLX90640 data
void readTempValues() {
  for (byte x = 0; x < 2; x++) {
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    if (status < 0) continue;

    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
    float tr = Ta - TA_SHIFT;

    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, EMMISIVITY, tr, tempValues);
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.setCursor(0, 16);
  display.print("Temp: ");
  display.print(tempValues[384]);
  display.print(" C");
  display.display();
}

// ----------------------------------------------------------------
// SETUP 
// ----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1.  MLX90640 : 
  Wire.begin(MLX_SDA, MLX_SCL);       // GPIO8 SDA, GPIO9 SCL
  Wire.setClock(800000);              // 800kHz → perfect for MLX90640

  // 2.  OLED :
  WireOLED.begin(OLED_SDA, OLED_SCL); // IO2 SDA, IO1 SCL
  WireOLED.setClock(400000);          // 400kHz → perfect for OLED 

  // initialize OLED 
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED failed on IO1/IO2!");
  } else {
    display.clearDisplay();
    display.display();
    Serial.println("OLED OK on IO1/IO2");
  }

  // Watchdog
  if (enableWatchdog) {
    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WATCHDOG_TIMEOUT * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
  }
  connectToWiFi();

  Wire.beginTransmission(MLX90640_address);
  if (Wire.endTransmission() != 0) {
    Serial.println("MLX90640 not found!");
    while (1) delay(100);
   }


  int status;
  uint16_t eeMLX90640[832];
  status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  status |= MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  MLX90640_SetRefreshRate(MLX90640_address, 0x05); // 4Hz
  Wire.setClock(800000);

   // Web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/ip", HTTP_GET, handleIP);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  if (enableWatchdog) esp_task_wdt_reset();
  updateOLED();

  if (millis() - lastReadTime >= READ_INTERVAL) {
    readTempValues();
    lastReadTime = millis();
  }

  if (enableRemoteLogging && millis() - lastRemotePostTime >= REMOTE_POST_INTERVAL) {
    postToRemoteServer();
    lastRemotePostTime = millis();
  }

  server.handleClient();
}