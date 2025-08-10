#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "RASM";      
const char* password = "1234qwert"; 


const char* serverUrl = "http://192.168.215.219:5000/test";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Send HTTP GET request
  HTTPClient http;
  http.begin(serverUrl);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  } else {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  http.end();
}

void loop() {

}