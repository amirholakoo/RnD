#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

// ================== Pins ==================
const int sensorPin = 4;
const int ledPin    = LED_BUILTIN;

// ================== WiFi & Server ==================
const char* ssid     = "***";
const char* password = "********";
const char* serverUrl = "http://192.168.2.20:7500";
const char* DEVICE_MAC = "D8:3B:DA:4E:25:9C";

// ================== Non-volatile Storage ==================
Preferences prefs;
const uint8_t MAX_PREV = 9;       // Max number of previous detections to keep
uint32_t prevHistory[MAX_PREV];   // Array holding previous detections
uint8_t prevCount = 0;            // How many previous detections saved

// ================== Current Detection ==================
uint32_t currentTimestamp = 0;
bool unsavedDetection = false;

// ================== Debounce ==================
bool lastState = HIGH;
unsigned long lastTriggerTime = 0;
const unsigned long debounceDelay = 1000;  // Debounce interval (ms)

// ===================================
void setup() {
  Serial.begin(115200);
  pinMode(sensorPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  loadHistoryFromStorage();   // Load previous detections from memory
  loadCurrentTemp();          // Check if a previous unsent detection exists
  connectWiFi();              // Initialize WiFi connection

  Serial.printf("SN04-N Ready. %d previous events loaded.\n", prevCount);
  if (unsavedDetection) {
    Serial.println("Unsaved detection found from previous session.");
  }
}

// ===================================
void loop() {
  bool currentState = digitalRead(sensorPin);

  // --- Sensor detection with debounce ---
  if (lastState == HIGH && currentState == LOW) {
    unsigned long now = millis();
    if (now - lastTriggerTime >= debounceDelay) {
      lastTriggerTime = now;

      currentTimestamp = now;
      unsavedDetection = true;
      saveCurrentTemp(currentTimestamp);   // Save immediately to avoid losing it after reset

      addToPreviousHistory(currentTimestamp);

      digitalWrite(ledPin, HIGH);
      Serial.printf("DETECTED! Sending current + %d previous.\n", prevCount);

      if (WiFi.status() == WL_CONNECTED) {
        sendCurrentAndPrevious();
      } else {
        Serial.println("WiFi off. Saved for later.");
      }

      delay(100);
      digitalWrite(ledPin, LOW);
    }
  }

  lastState = currentState;

  // --- Check WiFi status every 10 seconds ---
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    } else if (unsavedDetection) {
      sendCurrentAndPrevious();
    }
  }

  delay(50);
}

// ===================================
// Add to previous history (max 9 entries)
void addToPreviousHistory(uint32_t newTimestamp) {
  if (prevCount == MAX_PREV) {
    // Shift array left to make room
    for (int i = 0; i < MAX_PREV - 1; i++) {
      prevHistory[i] = prevHistory[i + 1];
    }
    prevHistory[MAX_PREV - 1] = newTimestamp;
  } else {
    prevHistory[prevCount++] = newTimestamp;
  }
  saveHistoryToStorage();
}

// ===================================
// Load detection history from NVS
void loadHistoryFromStorage() {
  prefs.begin("sn04n", false);
  prevCount = prefs.getUChar("count", 0);
  if (prevCount > MAX_PREV) prevCount = MAX_PREV;
  prefs.getBytes("prev", prevHistory, prevCount * sizeof(uint32_t));
  prefs.end();
}

// ===================================
// Save detection history to NVS
void saveHistoryToStorage() {
  prefs.begin("sn04n", false);
  prefs.putUChar("count", prevCount);
  prefs.putBytes("prev", prevHistory, prevCount * sizeof(uint32_t));
  prefs.end();
}

// ===================================
// Save current detection (in case of reset)
void saveCurrentTemp(uint32_t timestamp) {
  prefs.begin("sn04n", false);
  prefs.putUInt("current_temp", timestamp);
  prefs.end();
}

// ===================================
// Load unsent detection after reset
void loadCurrentTemp() {
  prefs.begin("sn04n", false);
  currentTimestamp = prefs.getUInt("current_temp", 0);
  prefs.end();

  if (currentTimestamp > 0) {
    unsavedDetection = true;
  }
}

// ===================================
// Connect to WiFi
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("WiFi: Connecting...");
  WiFi.mode(WIFI_STA);

  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  const unsigned long timeout = 8000;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeout) {
    Serial.print(".");
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Failed. Will retry later.");
  }
}

// ===================================
// Send current + previous detections to server
void sendCurrentAndPrevious() {
  if (currentTimestamp == 0) return;

  HTTPClient http;
  if (!http.begin(serverUrl)) return;
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);

  String json = "{";
  json += "\"device_id\":\"" + String(DEVICE_MAC) + "\",";
  json += "\"sensor_type\":\"SN04-N\",";
  json += "\"data\":{";
  json += "\"detected\":true,";
  json += "\"current\":{\"timestamp\":" + String(currentTimestamp) + "},";
  json += "\"previous\":[";
  for (int i = 0; i < prevCount; i++) {
    if (i > 0) json += ",";
    json += "{\"timestamp\":" + String(prevHistory[i]) + "}";
  }
  json += "]}";
  json += "}";

  Serial.println("Sending JSON:");
  Serial.println(json);

  int code = http.POST(json);
  Serial.printf("HTTP: %d\n", code);
  if (code > 0) {
    Serial.println("Response: " + http.getString());
    currentTimestamp = 0;
    unsavedDetection = false;

    // Clear temporary saved value
    prefs.begin("sn04n", false);
    prefs.putUInt("current_temp", 0);
    prefs.end();
  }
  http.end();
}
