#include <WiFi.h>

void setup() {
  // Initialize serial communication at 115200 baud rate
  Serial.begin(115200);
  
  // Set WiFi to station mode and disconnect from any previous connections
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  Serial.println("Setup complete");
}

void loop() {
  Serial.println("Starting WiFi scan...");

  // Scan for nearby networks
  int networkCount = WiFi.scanNetworks();
  
  Serial.println("Scan complete");
  
  // Check if any networks were found
  if (networkCount == 0) {
    Serial.println("No networks found");
  } else {
    Serial.print(networkCount);
    Serial.println(" networks found:");
    
    // Print each network's SSID
    for (int i = 0; i < networkCount; i++) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(WiFi.SSID(i));
    }
  }
  
  Serial.println("------------------------");
  
  // Wait 5 seconds before scanning again
  delay(5000);
}