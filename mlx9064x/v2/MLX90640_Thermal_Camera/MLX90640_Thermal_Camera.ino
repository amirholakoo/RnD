#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

// =================================================================
// START: USER CONFIGURATION
// =================================================================

// -- WiFi Credentials --
const char* ssid = "Homayoun";
const char* password = "1q2w3e4r$@";

// -- Static IP Configuration --
IPAddress staticIP(172, 16, 15, 21);     // Your desired static IP
IPAddress gateway(172, 16, 15, 1);      // Your router's IP
IPAddress subnet(255, 255, 255, 0);     // Your network's subnet mask
//IPAddress primaryDNS(8, 8, 8, 8);     // Optional: Google DNS
//IPAddress secondaryDNS(8, 8, 4, 4);   // Optional: Google DNS

// =================================================================
// END: USER CONFIGURATION
// =================================================================

#define EMMISIVITY 0.95
#define TA_SHIFT 8

const byte MLX90640_address = 0x33;
paramsMLX90640 mlx90640;
static float tempValues[32 * 24]; // 768 pixels

WebServer server(80); // Create a web server on port 80

// Global variable to store last read timestamp and frame data
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 500; // Read sensor every 500ms (2Hz)

// Function prototypes
void handleRoot();
void handleData();
void readTempValues();

// Function to handle the root URL "/"
void handleRoot() {
  server.send(200, "text/plain", "ESP32 Thermal Sensor Server. Request /data for JSON output.");
}

// Function to handle the "/data" endpoint
void handleData() {
  // Use a temporary JSON document to build the response
  JsonDocument doc;

  // Add metadata
  doc["device_id"] = WiFi.macAddress();
  doc["sensor_type"] = "MLX90640";
  doc["type"] = "sensor_data";
  doc["timestamp_ms"] = millis();

  // Create a nested object for the sensor data
  JsonObject data = doc.createNestedObject("data");

  // Read Vdd and Ta from a dummy frame to ensure fresh data
  uint16_t mlx90640Frame[834];
  MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
  data["ambient_temperature"] = MLX90640_GetTa(mlx90640Frame, &mlx90640);
  data["vdd"] = MLX90640_GetVdd(mlx90640Frame, &mlx90640);

  // Add the heatmap data (768 temperature values)
  JsonArray heatmap = data.createNestedArray("heatmap");
  for (int i = 0; i < 768; i++) {
    heatmap.add(tempValues[i]);
  }

  String jsonString;
  serializeJson(doc, jsonString);

  // Send the JSON response to the client
  server.send(200, "application/json", jsonString);
}

void setup() {
  Serial.begin(115200);

  // -- Connect to WiFi with Static IP --
  // Serial.println("Configuring static IP...");
  // if (!WiFi.config(staticIP, gateway, subnet)) {
  //   Serial.println("Failed to configure static IP!");
  // }
  WiFi.begin(ssid, password);
  
  delay(1500);
  // WiFi.setTxPower(WIFI_POWER_11dBm);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // -- I2C and Sensor Setup --
  Wire.begin();
  Wire.setClock(400000);

  Wire.beginTransmission((uint8_t)MLX90640_address);
  if (Wire.endTransmission() != 0) {
    Serial.println("MLX90640 not detected. Halting.");
    while (1);
  }
  Serial.println("MLX90640 online!");

  int status;
  uint16_t eeMLX90640[832];
  status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  if (status != 0) Serial.println("Failed to load system parameters");

  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0) Serial.println("Parameter extraction failed");

  MLX90640_SetRefreshRate(MLX90640_address, 0x05); // Set to 4Hz
  Wire.setClock(800000); // Increase I2C speed after setup

  // -- Server Setup --
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  // Check if it's time to read the sensor data
  if (millis() - lastReadTime >= READ_INTERVAL) {
    readTempValues();
    lastReadTime = millis();
  }

  // Handle incoming client requests
  server.handleClient();
}

void readTempValues() {
  for (byte x = 0; x < 2; x++) { // Read both subpages
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    if (status < 0) {
      Serial.print("GetFrame Error: ");
      Serial.println(status);
    }

    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
    float tr = Ta - TA_SHIFT; // Reflected temperature

    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, EMMISIVITY, tr, tempValues);
  }
}