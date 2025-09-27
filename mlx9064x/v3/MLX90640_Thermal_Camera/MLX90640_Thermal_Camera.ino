#include <Arduino.h>
#include <ElegantOTA.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

// =================================================================
// START: CONFIGURATION & SETUP
// =================================================================

// -- WiFi Credentials --
const char* ssid = "XXX";
const char* password = "XXX";

#define EEPROM_SIZE 512
#define CONFIG_VERSION "v1.0" // Change this to force a reset of EEPROM settings

// Structure to hold all configurable settings
struct AppSettings {
  char version[10];
  int detection_row;
  float high_threshold;
  float low_threshold;
  int num_points_to_extract;
  float emissivity;
  float ta_shift;
};

// Global settings object, accessible by all functions
AppSettings settings;

// =================================================================
// END: CONFIGURATION & SETUP
// =================================================================

const byte MLX90640_address = 0x33;
paramsMLX90640 mlx90640;
static float tempValues[32 * 24]; // 768 pixels

WebServer server(80);

unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 500;

struct DetectionResult {
  bool found;
  int start_col;
  int end_col;
};

// Forward declarations for all functions
void saveSettings();
void loadSettings();
void handleRoot();
void handleConfig();
void handleData();
void handleDetection();
void handleDebugRow();
DetectionResult detectHotSheet();
void readTempValues();

const int WIFI_MAX_RETRIES = 3;
// Timeout for each connection attempt in milliseconds
const unsigned long WIFI_TIMEOUT_MS = 4000; // 3 seconds

void connectToWifi() {
  Serial.println("Connecting to WiFi...");
  
  // Set a lower WiFi transmit power to save energy and reduce interference.
  // Options range from WIFI_POWER_MINUS_1dBm to WIFI_POWER_19_5dBm.
  // WIFI_POWER_11dBm is a good, moderate choice.
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  // Set WiFi mode to Station mode (client)
  WiFi.mode(WIFI_STA);

  for (int i = 1; i <= WIFI_MAX_RETRIES; i++) {
    Serial.printf("Attempt %d of %d... ", i, WIFI_MAX_RETRIES);
    
    WiFi.begin(ssid, password);
    unsigned long startTime = millis();

    // Wait for connection or timeout
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - startTime > WIFI_TIMEOUT_MS) {
        Serial.println("Timeout.");
        break; // Exit the while loop
      }
      Serial.print(".");
      delay(500);
    }

    // If we are connected, print the IP and exit the function
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      return; // Success!
    }
    
    // If not connected after timeout, disconnect and prepare for next retry
    WiFi.disconnect(true);
    delay(1000); // Wait a second before retrying
  }

  // If the code reaches here, all retries have failed
  Serial.println("\nFailed to connect to WiFi. Restarting ESP32...");
  delay(1000);
  ESP.restart();
}



void setup() {
  Serial.begin(115200);
  delay(100); // Allow serial monitor to connect
  
  // -- Initialize EEPROM and load settings --
  EEPROM.begin(EEPROM_SIZE);
  loadSettings(); // Load settings from EEPROM or use defaults if invalid

  // -- Connect to WiFi --

  // Call the robust connection function
  connectToWifi();

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

  uint16_t eeMLX90640[832];
  MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  MLX90640_SetRefreshRate(MLX90640_address, 0x05);
  Wire.setClock(800000);

  // -- Web Server Setup --
  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_GET, handleConfig);    // GET to read config
  server.on("/config", HTTP_POST, handleConfig);   // POST to update config
  server.on("/data", HTTP_GET, handleData);
  server.on("/detect", HTTP_GET, handleDetection);
  server.on("/debug_row", HTTP_GET, handleDebugRow);

  ElegantOTA.begin(&server);

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  if (millis() - lastReadTime >= READ_INTERVAL) {
    readTempValues();
    lastReadTime = millis();
  }
  server.handleClient();
  ElegantOTA.loop();
}

/**
 * @brief Serves the main HTML configuration page.
 * The page has a form to edit settings and uses JavaScript to POST them to /config.
 * UPDATED: Now includes a section with links to all API endpoints.
 */
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Thermal Sensor Config</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; }
        .container { max-width: 600px; margin: auto; background: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1, h2 { color: #333; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input[type='number'] { width: calc(100% - 22px); padding: 10px; border: 1px solid #ddd; border-radius: 4px; }
        button { background-color: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        button:hover { background-color: #0056b3; }
        .status { margin-top: 15px; padding: 10px; border-radius: 4px; display: none; text-align: center; }
        .success { background-color: #d4edda; color: #155724; }
        .error { background-color: #f8d7da; color: #721c24; }
        .api-links { margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; }
        .api-links h2 { margin-top: 0; }
        .api-links ul { list-style: none; padding: 0; }
        .api-links li { margin-bottom: 10px; }
        .api-links a { text-decoration: none; color: #007bff; font-family: monospace; font-size: 1.1em; }
        .api-links a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Thermal Sensor Configuration</h1>
        <form id="configForm">
            <div class="form-group">
                <label for="detection_row">Detection Row (0-23)</label>
                <input type="number" id="detection_row" name="detection_row" min="0" max="23" value="%DETECTION_ROW%">
            </div>
            <div class="form-group">
                <label for="high_threshold">High Temp Threshold (&deg;C)</label>
                <input type="number" id="high_threshold" name="high_threshold" step="0.1" value="%HIGH_THRESHOLD%">
            </div>
            <div class="form-group">
                <label for="low_threshold">Low Temp Threshold (&deg;C)</label>
                <input type="number" id="low_threshold" name="low_threshold" step="0.1" value="%LOW_THRESHOLD%">
            </div>
            <div class="form-group">
                <label for="num_points_to_extract">Points to Extract</label>
                <input type="number" id="num_points_to_extract" name="num_points_to_extract" min="2" max="20" value="%NUM_POINTS%">
            </div>
            <div class="form-group">
                <label for="emissivity">Object Emissivity</label>
                <input type="number" id="emissivity" name="emissivity" step="0.01" min="0.1" max="1.0" value="%EMISSIVITY%">
            </div>
            <div class="form-group">
                <label for="ta_shift">TA Shift (Compensation)</label>
                <input type="number" id="ta_shift" name="ta_shift" step="1" value="%TA_SHIFT%">
            </div>
            <button type="submit">Save Settings</button>
        </form>
        <div id="status" class="status"></div>

        <!-- ===== NEW SECTION WITH API LINKS ===== -->
        <div class="api-links">
            <h2>API Endpoints</h2>
            <p>View raw JSON data from the device (opens in new tab):</p>
            <ul>
                <li><a href="/data" target="_blank">/data (Live Heatmap)</a></li>
                <li><a href="/detect" target="_blank">/detect (Hot Sheet Detection)</a></li>
                <li><a href="/debug_row" target="_blank">/debug_row (Raw Row Data)</a></li>
                <li><a href="/config" target="_blank">/config (Current Settings)</a></li>
                <li><a href="/update" target="_blank">/update (ElegantOTA panel))</a></li>
            </ul>
        </div>
        <!-- ======================================= -->

    </div>
    <script>
        document.getElementById('configForm').addEventListener('submit', function(e) {
            e.preventDefault();
            const formData = new FormData(this);
            const data = {};
            formData.forEach((value, key) => data[key] = parseFloat(value));
            
            const statusDiv = document.getElementById('status');
            statusDiv.style.display = 'none';

            fetch('/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            })
            .then(response => response.json())
            .then(result => {
                if (result.status === 'success') {
                    statusDiv.textContent = 'Settings saved successfully!';
                    statusDiv.className = 'status success';
                } else {
                    statusDiv.textContent = 'Error: ' + (result.message || 'Could not save settings.');
                    statusDiv.className = 'status error';
                }
                statusDiv.style.display = 'block';
            })
            .catch(err => {
                statusDiv.textContent = 'Network error. Please try again.';
                statusDiv.className = 'status error';
                statusDiv.style.display = 'block';
            });
        });
    </script>
</body>
</html>
)rawliteral";
  
  // Replace placeholders with current values from the settings struct
  html.replace("%DETECTION_ROW%", String(settings.detection_row));
  html.replace("%HIGH_THRESHOLD%", String(settings.high_threshold));
  html.replace("%LOW_THRESHOLD%", String(settings.low_threshold));
  html.replace("%NUM_POINTS%", String(settings.num_points_to_extract));
  html.replace("%EMISSIVITY%", String(settings.emissivity));
  html.replace("%TA_SHIFT%", String(settings.ta_shift));
  
  server.send(200, "text/html", html);
}

/**
 * @brief Handles reading (GET) and writing (POST) the configuration via JSON API.
 */
void handleConfig() {
  if (server.method() == HTTP_GET) {
    // Client is requesting the current configuration
    JsonDocument doc;
    doc["detection_row"] = settings.detection_row;
    doc["high_threshold"] = settings.high_threshold;
    doc["low_threshold"] = settings.low_threshold;
    doc["num_points_to_extract"] = settings.num_points_to_extract;
    doc["emissivity"] = settings.emissivity;
    doc["ta_shift"] = settings.ta_shift;
    
    String jsonString;
    serializeJson(doc, jsonString);
    server.send(200, "application/json", jsonString);

  } else if (server.method() == HTTP_POST) {
    // Client is sending new configuration
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
      server.send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
      return;
    }

    // Update settings object with values from JSON, if they exist
    settings.detection_row = doc["detection_row"] | settings.detection_row;
    settings.high_threshold = doc["high_threshold"] | settings.high_threshold;
    settings.low_threshold = doc["low_threshold"] | settings.low_threshold;
    settings.num_points_to_extract = doc["num_points_to_extract"] | settings.num_points_to_extract;
    settings.emissivity = doc["emissivity"] | settings.emissivity;
    settings.ta_shift = doc["ta_shift"] | settings.ta_shift;
    
    saveSettings(); // Persist the new settings to EEPROM
    server.send(200, "application/json", "{\"status\":\"success\", \"message\":\"Settings saved\"}");
  }
}

void handleData() {
  JsonDocument doc;
  doc["timestamp_ms"] = millis();
  JsonArray heatmap = doc.createNestedArray("heatmap");
  for (int i = 0; i < 768; i++) {
    heatmap.add(tempValues[i]);
  }
  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}

void handleDetection() {
  DetectionResult result = detectHotSheet();
  JsonDocument doc;

  if (!result.found) {
    doc["status"] = "not_found";
    doc["message"] = "No hot sheet detected based on current thresholds.";
  } else {
    doc["status"] = "success";
    JsonObject detection = doc.createNestedObject("detection");
    detection["start_column"] = result.start_col;
    detection["end_column"] = result.end_col;

    JsonArray points = doc.createNestedArray("calculated_points");
    int sheet_width = result.end_col - result.start_col;
    
    if (sheet_width == 0 && settings.num_points_to_extract > 1) sheet_width = 1;

    for (int i = 0; i < settings.num_points_to_extract; i++) {
      int col = result.start_col;
      if (settings.num_points_to_extract > 1) {
          col += (i * sheet_width / (settings.num_points_to_extract - 1));
      }
      if(col > result.end_col) col = result.end_col;
      
      int temp_index = settings.detection_row * 32 + col;
      float temperature = tempValues[temp_index];

      // Placeholder formula
      // -0.01632 * temp * temp + 1.30838 * temp - 17.71
      float formula_result = ((-0.01632) * temperature * temperature) + (1.30838 * temperature) - 17.71;

      JsonObject point = points.createNestedObject();
      point["column"] = col;
      point["temperature"] = temperature;
      point["formula_result"] = formula_result;
    }
  }
  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}

void handleDebugRow() {
    JsonDocument doc;
    doc["detection_row"] = settings.detection_row;
    JsonArray temps = doc.createNestedArray("temperatures");
    int row_start_index = settings.detection_row * 32;
    for(int i = 0; i < 32; i++) {
        temps.add(tempValues[row_start_index + i]);
    }
    String jsonString;
    serializeJson(doc, jsonString);
    server.send(200, "application/json", jsonString);
}

/**
 * @brief Reads sensor data using the global settings for emissivity and ta_shift.
 */
void readTempValues() {
  for (byte x = 0; x < 2; x++) {
    uint16_t mlx90640Frame[834];
    MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
    float tr = Ta - settings.ta_shift;
    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, settings.emissivity, tr, tempValues);
  }
}

/**
 * @brief Performs hot sheet detection using the global settings.
 */
DetectionResult detectHotSheet() {
  DetectionResult result = {false, -1, -1};
  int row_start_index = settings.detection_row * 32;
  bool is_hot = false;

  for (int i = 0; i < 32; i++) {
    float temp = tempValues[row_start_index + i];
    if (!is_hot && temp > settings.high_threshold) {
      is_hot = true;
      result.start_col = i;
    } else if (is_hot && temp < settings.low_threshold) {
      is_hot = false;
      result.end_col = i - 1;
      break;
    }
  }

  if (is_hot && result.end_col == -1) result.end_col = 31;
  if (result.start_col != -1 && result.end_col != -1) result.found = true;

  return result;
}

/**
 * @brief Saves the current settings struct to EEPROM.
 */
void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
  Serial.println("Settings saved to EEPROM.");
}

/**
 * @brief Loads settings from EEPROM. If invalid or version mismatch, loads defaults.
 */
void loadSettings() {
  EEPROM.get(0, settings);
  // Check if the loaded version matches the code version.
  if (String(settings.version) != String(CONFIG_VERSION)) {
    Serial.println("Invalid/old settings in EEPROM. Loading defaults.");
    // Load default values
    strcpy(settings.version, CONFIG_VERSION);
    settings.detection_row = 12;
    settings.high_threshold = 45.0;
    settings.low_threshold = 40.0;
    settings.num_points_to_extract = 10;
    settings.emissivity = 0.95;
    settings.ta_shift = 8.0;
    saveSettings(); // Save the new default settings to EEPROM
  } else {
    Serial.println("Valid settings loaded from EEPROM.");
  }
}

