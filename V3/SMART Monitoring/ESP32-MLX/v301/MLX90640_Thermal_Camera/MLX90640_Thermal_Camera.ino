#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

// =================================================================
// START: USER CONFIGURATION
// =================================================================

// -- Multiple WiFi Networks (will try in order) --
struct WiFiNetwork {
  const char* ssid;
  const char* password;
};

WiFiNetwork wifiNetworks[] = {
  {"VPN1", "XXX"},
  {"Homayoun", "XXX"},
  // Add more networks as needed
};
const int numNetworks = sizeof(wifiNetworks) / sizeof(wifiNetworks[0]);

// -- Remote Server Configuration --
const char* remoteServer = "192.168.2.20";
//const char* remoteServer = "81.163.7.71";
const int remotePort = 7500;
const bool enableRemoteLogging = true; // Set to false to disable remote POST

//const unsigned long REMOTE_POST_INTERVAL = 5000; // Post every 5 seconds
const unsigned long REMOTE_POST_INTERVAL = 60000; // Post every 1 minutes
//const unsigned long REMOTE_POST_INTERVAL = 300000; // Post every 5 minutes

// -- Watchdog Timer Configuration --
const bool enableWatchdog = true; // Set to false to disable watchdog
const int WATCHDOG_TIMEOUT = 2400; // Watchdog timeout in seconds (40 minutes)

// -- Static IP Configuration (optional) --
//IPAddress staticIP(172, 16, 15, 21);     // Your desired static IP
//IPAddress gateway(172, 16, 15, 1);      // Your router's IP
//IPAddress subnet(255, 255, 255, 0);     // Your network's subnet mask
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

unsigned long lastRemotePostTime = 0;
String connectedSSID = "";

// Function prototypes
void connectToWiFi();
void handleRoot();
void handleData();
void readTempValues();
void postToRemoteServer();

// WiFi connection function with fallback support
void connectToWiFi() {
  bool connected = false;
  
  for (int i = 0; i < numNetworks && !connected; i++) {
    Serial.print("Attempting to connect to: ");
    Serial.println(wifiNetworks[i].ssid);
    
    WiFi.begin(wifiNetworks[i].ssid, wifiNetworks[i].password);
    
    // Try for 10 seconds
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      connectedSSID = String(wifiNetworks[i].ssid);
      //Serial.println("\nConnected to WiFi: " + connectedSSID);
      //Serial.print("IP Address: ");
      //Serial.println(WiFi.localIP());
    } else {
      //Serial.println("\nFailed to connect to: " + String(wifiNetworks[i].ssid));
      WiFi.disconnect();
    }
  }
  
  if (!connected) {
    //Serial.println("Failed to connect to any WiFi network. Halting.");
    while (1) {
      delay(1000);
    }
  }
}

// Function to handle the root URL "/"
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
  
  server.send(200, "text/plain", message);
}

// Function to handle the "/data" endpoint
void handleData() {
  // Use a temporary JSON document to build the response
  JsonDocument doc;

  // Add metadata
  doc["device_id"] = WiFi.macAddress();
  doc["ip_address"] = WiFi.localIP().toString();
  doc["sensor_type"] = "MLX90640";
  doc["type"] = "sensor_data";
  doc["timestamp_ms"] = millis();

  // Create a nested object for the sensor data
  JsonObject data = doc["data"].to<JsonObject>();

  // Read Vdd and Ta from a dummy frame to ensure fresh data
  uint16_t mlx90640Frame[834];
  MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
  data["ambient_temperature"] = MLX90640_GetTa(mlx90640Frame, &mlx90640);
  data["vdd"] = MLX90640_GetVdd(mlx90640Frame, &mlx90640);

  // Add the heatmap data (768 temperature values)
  JsonArray heatmap = data["heatmap"].to<JsonArray>();
  for (int i = 0; i < 768; i++) {
    heatmap.add(tempValues[i]);
  }

  String jsonString;
  serializeJson(doc, jsonString);

  // Send the JSON response to the client
  server.send(200, "application/json", jsonString);
}

// Function to POST data to remote server
void postToRemoteServer() {
  if (WiFi.status() != WL_CONNECTED) {
    //Serial.println("WiFi not connected. Skipping remote post.");
    return;
  }

  HTTPClient http;
  String url = "http://" + String(remoteServer) + ":" + String(remotePort);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload
  JsonDocument doc;
  doc["device_id"] = WiFi.macAddress();
  doc["ip_address"] = WiFi.localIP().toString();
  doc["sensor_type"] = "MLX90640";
  
  JsonObject data = doc["data"].to<JsonObject>();
  data["status"] = 200;
  data["timestamp_ms"] = millis();
  data["connected_ssid"] = connectedSSID;
  data["rssi"] = WiFi.RSSI();
  
  // Add latest temperature readings
  uint16_t mlx90640Frame[834];
  MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
  data["ambient_temperature"] = MLX90640_GetTa(mlx90640Frame, &mlx90640);
  data["vdd"] = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
  
  // Optionally add heatmap (warning: large payload ~3KB)
  // JsonArray heatmap = data["heatmap"].to<JsonArray>();
  // for (int i = 0; i < 768; i++) {
  //   heatmap.add(tempValues[i]);
  // }

  String jsonString;
  serializeJson(doc, jsonString);

  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    //Serial.print("Remote POST response code: ");
    //Serial.println(httpResponseCode);
  } else {
    //Serial.print("Remote POST error: ");
    //Serial.println(http.errorToString(httpResponseCode));
  }
  
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // -- Configure Watchdog Timer --
  if (enableWatchdog) {
    //Serial.print("Configuring watchdog timer for ");
    //Serial.print(WATCHDOG_TIMEOUT);
    //Serial.println(" seconds...");
    
    // Configure watchdog for ESP32 Arduino Core 3.x
    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WATCHDOG_TIMEOUT * 1000,  // Convert seconds to milliseconds
      .idle_core_mask = 0,                     // Not watching idle tasks
      .trigger_panic = true                    // Enable panic so ESP32 restarts
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL); // Add current thread to WDT watch
    //Serial.println("Watchdog enabled - Device will auto-reset if frozen");
  }

  // -- Connect to WiFi with fallback support --
  // Uncomment below for static IP configuration
  // Serial.println("Configuring static IP...");
  // if (!WiFi.config(staticIP, gateway, subnet)) {
  //   Serial.println("Failed to configure static IP!");
  // }
  
  connectToWiFi();

  // -- I2C and Sensor Setup --
  Wire.begin();
  Wire.setClock(400000);

  Wire.beginTransmission((uint8_t)MLX90640_address);
  if (Wire.endTransmission() != 0) {
    //Serial.println("MLX90640 not detected. Halting.");
    while (1);
  }
  //Serial.println("MLX90640 online!");

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
  //Serial.println("HTTP server started.");
}

void loop() {
  // Reset watchdog timer to prevent device reset
  if (enableWatchdog) {
    esp_task_wdt_reset();
  }

  // Check if it's time to read the sensor data
  if (millis() - lastReadTime >= READ_INTERVAL) {
    readTempValues();
    lastReadTime = millis();
  }

  // Check if it's time to post to remote server
  if (enableRemoteLogging && (millis() - lastRemotePostTime >= REMOTE_POST_INTERVAL)) {
    postToRemoteServer();
    lastRemotePostTime = millis();
  }

  // Handle incoming client requests
  server.handleClient();
}

void readTempValues() {
  for (byte x = 0; x < 2; x++) { // Read both subpages
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    if (status < 0) {
      //Serial.print("GetFrame Error: ");
      //Serial.println(status);
    }

    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
    float tr = Ta - TA_SHIFT; // Reflected temperature

    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, EMMISIVITY, tr, tempValues);
  }
}
