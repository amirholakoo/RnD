#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ---------- I2C configuration ----------
#define SDA_PIN 8
#define SCL_PIN 9
#define MPU_ADDR 0x68
#define OLED_ADDR 0x3C

// ---------- WiFi & server ----------
const char* ssid = "********";
const char* password = "********";
const char* serverUrl = "http://192.168.2.20:7500";      //Server address
const char* MAC_FALLBACK = "94:26:4E:DA:3B:D8";          //Device id

// ---------- Sensor objects ----------
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ---------- Timing ----------
unsigned long lastSend = 0;
const unsigned long sendInterval = 60000; // send every 60s
unsigned long lastDisplay = 0;
const unsigned long displayInterval = 2000; // OLED update every 2s

// ---------- MPU variables ----------
float ax_f = 0, ay_f = 0, az_f = 0; // low-pass filtered
float alpha = 0.9;                  // low-pass coefficient
float Vx=0, Vy=0, Vz=0, Vtotal=0;

// ---------- MLX variables ----------
float ambientTemp = 0, objectTemp = 0;

// ---------- MPU read ----------
int16_t read16(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 2);
  int16_t hi = Wire.read();
  int16_t lo = Wire.read();
  return (hi << 8) | lo;
}

void readMPU() {
  float ax = read16(0x3B) / 16384.0;
  float ay = read16(0x3D) / 16384.0;
  float az = read16(0x3F) / 16384.0;

  // Low-pass filter to remove gravity
  ax_f = alpha * ax_f + (1 - alpha) * ax;
  ay_f = alpha * ay_f + (1 - alpha) * ay;
  az_f = alpha * az_f + (1 - alpha) * az;

  Vx = ax - ax_f;
  Vy = ay - ay_f;
  Vz = az - az_f;
  Vtotal = sqrt(Vx*Vx + Vy*Vy + Vz*Vz);
}

// ---------- WiFi ----------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("✅ Connected! IP: ");
  Serial.println(WiFi.localIP());
}

// ---------- HTTP send ----------
void sendJSON(const String &jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(jsonData);
    Serial.print("📡 Server response: ");
    Serial.println(code);
    http.end();
  } else {
    Serial.println("⚠️ WiFi not connected, skipping send.");
  }
}

void sendData() {
  String mac = WiFi.macAddress();
  if (mac == "00:00:00:00:00:00") mac = MAC_FALLBACK;

  // MLX data
  String json1 = "{";
  json1 += "\"device_id\":\"" + mac + "\",";
  json1 += "\"sensor_type\":\"MLX90614\",";
  json1 += "\"data\":{";
  json1 += "\"Ambient Temp\":" + String(ambientTemp, 2) + ",";
  json1 += "\"Object Temp\":" + String(objectTemp, 2);
  json1 += "}}";
  Serial.println("📡 Sending MLX90614 data...");
  Serial.println(json1);
  sendJSON(json1);

  // MPU data
  String json2 = "{";
  json2 += "\"device_id\":\"" + mac + "\",";
  json2 += "\"sensor_type\":\"MPU6500\",";
  json2 += "\"data\":{";
  json2 += "\"Vx\":" + String(Vx, 4) + ",";
  json2 += "\"Vy\":" + String(Vy, 4) + ",";
  json2 += "\"Vz\":" + String(Vz, 4) + ",";
  json2 += "\"Vtotal\":" + String(Vtotal, 4);
  json2 += "}}";
  Serial.println("📡 Sending MPU6500 data...");
  Serial.println(json2);
  sendJSON(json2);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  delay(500);

  Serial.println("\n--- Booting ---");

  // WiFi
  connectWiFi();

  // MLX90614
  if (!mlx.begin()) {
    Serial.println("❌ MLX90614 not detected!");
    while (true) delay(1000);
  }

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("❌ SSD1306 not found!");
    while (true) delay(1000);
  }

  // MPU6500
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("✅ Setup done!");
  display.display();
  delay(1000);
}

// ---------- Loop ----------
void loop() {
  unsigned long now = millis();

  // Read sensors
  readMPU();
  ambientTemp = mlx.readAmbientTempC();
  objectTemp = mlx.readObjectTempC();

  // --- Update OLED every 2s ---
if (now - lastDisplay >= displayInterval) {
    lastDisplay = now;
    display.clearDisplay();

    // IP
    display.setCursor(0, 0);
    display.print("IP: ");
    display.println(WiFi.localIP());

    // 
    display.setCursor(0, 10);
    display.println();

    // temp
    display.setCursor(0, 18);
    display.printf("Ta:%.2f | To:%.2f C", ambientTemp, objectTemp);

    // 
    display.setCursor(0, 28);
    display.println("---------------------");

    // vibration data
    display.setCursor(0, 40);
    display.printf("Vx:%.4f", Vx);
    display.setCursor(66, 40);
    display.printf("Vy:%.4f", Vy);
    display.setCursor(0, 50);
    display.printf("Vz:%.4f", Vz);
    display.setCursor(66, 50);
    display.printf("Vt:%.4f", Vtotal);

    display.display();

    // Serial debug
    Serial.println("===============================");
    Serial.printf("🌡️  Ambient: %.2f°C | Object: %.2f°C\n", ambientTemp, objectTemp);
    Serial.printf("📈 Vx: %.4f Vy: %.4f Vz: %.4f | Vtotal: %.4f\n", Vx, Vy, Vz, Vtotal);
  }

  // --- Send to server every 60s ---
  if (now - lastSend >= sendInterval) {
    lastSend = now;
    sendData();
  }

  delay(50);
}
