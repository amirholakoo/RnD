#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // <-- Added for JSON handling

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

// =================================================================
// >> > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > >
// >>  START: USER CONFIGURATION
// =================================================================

// -- WiFi Credentials --
const char* ssid = "Homayoun";
const char* password = "1q2w3e4r$@";

// -- Server Endpoint --
String serverName = "http://192.168.2.20:7500";

// =================================================================
// >>  END: USER CONFIGURATION
// =================================================================

#define EMMISIVITY 0.95
#define TA_SHIFT 8

const byte MLX90640_address = 0x33;
paramsMLX90640 mlx90640;
static float tempValues[32 * 24]; // 768 pixels

void setup() {
  Serial.begin(115200);

  // -- Connect to WiFi --
  WiFi.begin(ssid, password);

  delay(500);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);


  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // -- I2C and Sensor Setup --
  Wire.begin(); // Uses default I2C pins (SDA=8, SCL=9 for ESP32-S3)
  Wire.setClock(400000);

  Wire.beginTransmission((uint8_t)WIFI_POWER_15dBm);
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
}

void loop() {
  readTempValues();
  sendDataToServer();
  delay(5000); // Wait 5 seconds before next reading and send
}

void sendDataToServer() {
  if (WiFi.status() == WL_CONNECTED) {
    // Create an ArduinoJson document
    // The size is calculated to hold our data. Adjust if you add more.
    // https://arduinojson.org/v6/assistant/
    JsonDocument doc;

    // Add metadata
    doc["device_id"] = WiFi.macAddress();
    doc["sensor_type"] = "MLX90640";
    doc["type"] = "sensor_data";
    doc["timestamp"] = millis(); // Using milliseconds since boot as a simple timestamp

    // Create a nested object for the sensor data
    JsonObject data = doc.createNestedObject("data");

    // Add other useful info to the data object
    // (We read Vdd and Ta inside readTempValues, but we can re-calculate the last one)
    uint16_t mlx90640Frame[834]; // Dummy frame to get Ta
    MLX90640_GetFrameData(MLX90640_address, mlx90640Frame); // Quick frame grab
    data["ambient_temperature"] = MLX90640_GetTa(mlx90640Frame, &mlx90640);
    data["vdd"] = MLX90640_GetVdd(mlx90640Frame, &mlx90640);

    // Add the heatmap data (768 temperature values)
    JsonArray heatmap = data.createNestedArray("heatmap");
    for (int i = 0; i < 768; i++) {
      heatmap.add(tempValues[i]);
    }

    // Serialize the JSON document to a string
    String jsonString;
    serializeJson(doc, jsonString);

    // Create an HTTP client
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    // Send the POST request
    Serial.println("Sending data to server...");
    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.print("Response: ");
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end(); // Free resources
  } else {
    Serial.println("WiFi Disconnected. Cannot send data.");
  }
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
  // Optional: Print values to serial monitor for debugging
  // printTempValues();
}

void printTempValues() {
  Serial.println("\r\n=========================== MLX90640 Heatmap ===============================");
  for (int i = 0; i < 768; i++) {
    if (((i % 32) == 0) && (i != 0)) {
      Serial.println();
    }
    Serial.print(tempValues[i], 1); // Print with one decimal place
    Serial.print(" ");
  }
  Serial.println("\r\n=============================================================================");
}

// Device_Scan function is removed for brevity as it's not used in the main loop
// You can add it back if you need it for debugging.