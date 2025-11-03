#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ModbusMaster.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define RX_PIN 44
#define TX_PIN 43

ModbusMaster node;

// ---------------------- OLED ------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------------- TIME ------------------------
unsigned long lastRead = 0;
const unsigned long interval = 60000; // 1 minute interval

// ---------------------- SENSOR ----------------------
float lastTemp     = NAN;
float lastHum      = NAN;
float lastDewPoint = NAN;

// ---------------------- WiFi ------------------------
const char* ssid = "*******";
const char* password = "********";

// Updated fallback MAC address
const char* MAC_FALLBACK = "D8:3B:DA:4A:13:30";

// ---------------------- SERVER ----------------------
const char* serverUrl = "http://192.168.2.20:7500";

// ====================================================
// ======================= SETUP =======================
// ====================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Starting MAC-DTH1 Modbus WiFi System...");

  // ---- RS485 ----
  Serial2.begin(38400, SERIAL_8N1, RX_PIN, TX_PIN);
  node.begin(100, Serial2);  // Modbus slave ID = 100

  // ---- OLED ----
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();
  delay(1000);
  display.clearDisplay();

  // ---- WiFi ----
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000UL) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
  } else {
    Serial.println("\nWiFi not connected (continuing anyway).");
  }

  // ---- MAC Address ----
  String mac = WiFi.macAddress();
  if (mac == "00:00:00:00:00:00") {
    mac = MAC_FALLBACK;
  }
  Serial.print("Device MAC: ");
  Serial.println(mac);

  // ---- Display WiFi info ----
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Connected!");
  display.println(mac);
  display.display();
  delay(1500);
}

// ====================================================
// ======================= LOOP =======================
// ====================================================
void loop() {
  if (millis() - lastRead >= interval) {
    lastRead = millis();

    // ---- Read Modbus registers ----
    float temperature = readFloatRegs(1000);
    float humidity    = readFloatRegs(1002);
    float dewPoint    = readFloatRegs(1004);

    if (!isnan(temperature)) lastTemp = temperature;
    if (!isnan(humidity))    lastHum  = humidity;
    if (!isnan(dewPoint))    lastDewPoint = dewPoint;

    // ---- Print to Serial ----
    Serial.print("Temp: "); Serial.print(lastTemp, 2);
    Serial.print(" °C  Hum: "); Serial.print(lastHum, 2);
    Serial.print(" %  DewPt: "); Serial.print(lastDewPoint, 2);
    Serial.println(" °C");

    // ---- Display on OLED ----
    display.clearDisplay();
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.print("IP: ");
    display.println(WiFi.localIP());

    display.setCursor(0, 18);
    display.print("MAC:");
    display.println(WiFi.macAddress());

    display.setCursor(0, 35);
    display.print("Temp: "); display.print(lastTemp, 2); display.println(" C");

    display.setCursor(0, 45);
    display.print("Hum:  "); display.print(lastHum, 2); display.println(" %");

    display.setCursor(0, 55);
    display.print("DewPt: "); display.print(lastDewPoint, 2); display.println(" C");

    display.display();

    // ====================================================
    // Send to server (even if sensor is not connected)
    // ====================================================
    String deviceId = WiFi.macAddress();
    if (deviceId == "00:00:00:00:00:00") deviceId = String(MAC_FALLBACK);

    // Use dummy values if sensor data not available
    if (isnan(lastTemp)) lastTemp = 25.0;
    if (isnan(lastHum))  lastHum  = 45.0;
    if (isnan(lastDewPoint)) lastDewPoint = 10.0;

    // Create JSON payload
    String jsonData = "{";
    jsonData += "\"device_id\": \"" + deviceId + "\",";
    jsonData += "\"sensor_type\": \"DTH1\",";
    jsonData += "\"data\": {";
    jsonData += "\"temperature\": " + String(lastTemp, 2) + ",";
    jsonData += "\"humidity\": " + String(lastHum, 2) + ",";
    jsonData += "\"dewpt\": " + String(lastDewPoint, 2);
    jsonData += "}}";

    Serial.print("JSON -> ");
    Serial.println(jsonData);

    // ---- Send HTTP POST ----
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      Serial.print("Connecting to server: ");
      Serial.println(serverUrl);

      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonData);
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      if (httpResponseCode > 0) {
        String resp = http.getString();
        Serial.print("Server response: ");
        Serial.println(resp);
      } else {
        Serial.print("HTTP error: ");
        Serial.println(httpResponseCode);
        Serial.print("Error detail: ");
        Serial.println(http.errorToString(httpResponseCode));
      }
      http.end();
    } else {
      Serial.println("WiFi not connected, skipped HTTP POST.");
    }
  }
}

// ====================================================
// ============ Read float values from Modbus ==========
// ====================================================
float readFloatRegs(uint16_t addr) {
  uint8_t result = node.readHoldingRegisters(addr, 2);
  if (result == node.ku8MBSuccess) {
    uint16_t r1 = node.getResponseBuffer(0);
    uint16_t r2 = node.getResponseBuffer(1);
    uint32_t raw = ((uint32_t)r2 << 16) | r1;
    float val;
    memcpy(&val, &raw, 4);

    // Basic validation
    if (addr == 1000 && (val < -50.0f || val > 80.0f)) return NAN;
    if (addr == 1002 && (val < 0.0f   || val > 100.0f)) return NAN;
    if (addr == 1004 && (val < -50.0f || val > 80.0f)) return NAN;

    return val;
  } else {
    return NAN;
  }
}
