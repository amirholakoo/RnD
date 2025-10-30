#include <WiFi.h>


const char *ssid = "RASM";
const char *password = "1234qwert";


void setup() {

  // Initialize serial communication for debugging
  Serial.begin(115200);
  // Step 1: Fully disconnect and turn off WiFi to clear everything
  WiFi.persistent(false);  // Optional: Prevent settings from being saved
  WiFi.disconnect(true);   // Disconnects from any network and turns off WiFi
  delay(1000);             // Optional delay to ensure WiFi is fully off

  // Step 2: Set WiFi to station mode (turns WiFi back on in client mode)
  WiFi.mode(WIFI_STA);
  delay(1000);

  // Step 3: Start a fresh connection to the new network
  WiFi.begin(ssid);

  // Wait for the connection to establish with a timeout
  Serial.print("Connecting to WiFi");
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
  } else {
    Serial.println("\nConnection failed!");
  }

  // Connection successful
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // put your main code here, to run repeatedly:
}
