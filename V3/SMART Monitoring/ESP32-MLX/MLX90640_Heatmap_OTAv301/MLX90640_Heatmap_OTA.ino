/*
 * MLX90640 Thermal Camera with Interactive Heatmap
 * ESP32-C6-Touch-LCD-1.9 with OTA Updates
 * 
 * Features:
 * - Real-time thermal heatmap (32x24 pixels)
 * - 5 color schemes (touch buttons to switch)
 * - Touch interface: Heatmap, Info, Settings pages
 * - OTA wireless firmware updates
 * - WiFi with IP display
 * - Temperature stats and auto-scaling
 * 
 * OTA Usage:
 * 1. Upload via USB first time
 * 2. Note IP address on screen
 * 3. Tools → Port → Network (IP address)
 * 4. Upload wirelessly!
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Arduino_GFX_Library.h>

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

// WiFi Config
const char* WIFI_SSID = "VPN1";
const char* WIFI_PASS = "09126141426";
const char* OTA_HOSTNAME = "esp32-mlx-thermal";
const char* OTA_PASSWORD = "mlx90640";  // CHANGE THIS!

// MLX90640 Config
#define MLX_SDA 18
#define MLX_SCL 8
#define MLX90640_ADDR 0x33
#define MLX_I2C_SPEED 400000
#define MLX_REFRESH 0x05

// LCD Config  
#define LCD_DC 6
#define LCD_CS 7
#define LCD_SCK 5
#define LCD_MOSI 4
#define LCD_RST 14
#define LCD_BL 15
#define LCD_W 170
#define LCD_H 320

// Thermal
#define TH_W 32
#define TH_H 24
#define EMMISIVITY 0.95
#define TA_SHIFT 8

// Display layout - showing actual pixel size
#define PIXEL_W 5   // Width per thermal pixel (32 pixels * 5 = 160 fits screen)
#define PIXEL_H 5   // Height per thermal pixel (24 pixels * 5 = 120)
#define MAP_X 5     // X offset for centering
#define MAP_Y 50    // Y offset below title

// Define missing color
#define DARKGREY 0x7BEF  // RGB(128,128,128)

// Color Schemes
enum Scheme { IRONBOW, RAINBOW, GRAYSCALE, HOTCOLD, MEDICAL, SCHEME_CNT };
const char* schemeNames[] = {"Ironbow", "Rainbow", "Gray", "Hot/Cold", "Medical"};

// Pages
enum PageType { PG_HEATMAP, PG_INFO, PG_SETTINGS, PG_CNT };

// Globals
paramsMLX90640 mlx;
float temps[TH_W * TH_H];
float minT = 0, maxT = 100, avgT = 25, centerT = 25, ambientT = 25;
bool autoScale = true;
float manualMin = 20, manualMax = 40;

Scheme scheme = IRONBOW;
PageType page = PG_HEATMAP;
bool frozen = false;
unsigned long lastUpdate = 0;
unsigned long bootTime = 0;

String ip = "...";
bool wifiOK = false;
bool otaActive = false;
int otaPct = 0;
bool sensorErr = false;

Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, 0, LCD_W, LCD_H, 35, 0, 35, 0);

// Color mapping function
uint16_t getColor(float temp, float minT, float maxT) {
  if (isnan(temp)) return BLACK;
  
  float norm = constrain((temp - minT) / (maxT - minT), 0.0, 1.0);
  int idx = norm * 255;
  
  uint8_t r, g, b;
  
  switch(scheme) {
    case IRONBOW:  // Purple → Red → Yellow → White
      if (idx < 64) {
        r = idx * 4; g = 0; b = 128 + idx * 2;
      } else if (idx < 128) {
        r = 255; g = (idx - 64) * 4; b = 255 - (idx - 64) * 4;
      } else if (idx < 192) {
        r = 255; g = 255; b = (idx - 128) * 2;
      } else {
        r = 255; g = 255; b = 128 + (idx - 192) * 2;
      }
      break;
      
    case RAINBOW:  // Blue → Green → Yellow → Red
      if (idx < 85) {
        r = 0; g = idx * 3; b = 255 - idx * 3;
      } else if (idx < 170) {
        r = (idx - 85) * 3; g = 255 - (idx - 85) * 3; b = 0;
      } else {
        r = 255; g = (255 - idx) * 3; b = 0;
      }
      break;
      
    case GRAYSCALE:  // Black → White
      r = g = b = idx;
      break;
      
    case HOTCOLD:  // Blue → White → Red
      if (idx < 128) {
        r = idx * 2; g = idx * 2; b = 255;
      } else {
        r = 255; g = 255 - (idx - 128) * 2; b = 255 - (idx - 128) * 2;
      }
      break;
      
    case MEDICAL:  // Optimized for body temp 30-40°C
      if (idx < 100) {
        r = 0; g = idx * 2; b = 200;
      } else if (idx < 180) {
        r = (idx - 100) * 3; g = 200; b = 200 - (idx - 100) * 2;
      } else {
        r = 255; g = 200 - (idx - 180) * 2; b = 0;
      }
      break;
      
    default:
      r = g = b = idx;
  }
  
  return gfx->color565(r, g, b);
}

void initWiFi() {
  Serial.println("[WiFi] Connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiOK = true;
    ip = WiFi.localIP().toString();
    Serial.println("\n[WiFi] Connected: " + ip);
  } else {
    wifiOK = false;
    ip = "Failed";
    Serial.println("\n[WiFi] Failed");
  }
}

void initOTA() {
  if (!wifiOK) {
    Serial.println("[OTA] Skipped (no WiFi)");
    return;
  }
  
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  
  ArduinoOTA.onStart([]() {
    otaActive = true;
    otaPct = 0;
    Serial.println("[OTA] Start");
    gfx->fillScreen(BLACK);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(20, 100);
    gfx->println("OTA UPDATE");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    otaPct = (progress * 100) / total;
    Serial.printf("[OTA] Progress: %u%%\r", otaPct);
    
    gfx->fillRect(20, 140, 130, 20, BLACK);
    gfx->drawRect(20, 140, 130, 20, WHITE);
    gfx->fillRect(22, 142, (126 * otaPct) / 100, 16, GREEN);
    
    gfx->fillRect(20, 170, 130, 20, BLACK);
    gfx->setCursor(60, 173);
    gfx->setTextSize(2);
    gfx->print(otaPct);
    gfx->print("%");
  });
  
  ArduinoOTA.onEnd([]() {
    otaActive = false;
    Serial.println("\n[OTA] Complete!");
    gfx->fillRect(20, 200, 130, 20, BLACK);
    gfx->setTextColor(GREEN);
    gfx->setCursor(30, 203);
    gfx->println("SUCCESS!");
    delay(1000);
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    otaActive = false;
    Serial.printf("[OTA] Error[%u]: ", error);
    gfx->fillRect(20, 200, 130, 20, BLACK);
    gfx->setTextColor(RED);
    gfx->setCursor(40, 203);
    gfx->println("FAILED");
  });
  
  ArduinoOTA.begin();
  Serial.println("[OTA] Ready");
  Serial.println("[OTA] Password: " + String(OTA_PASSWORD));
}

void initMLX() {
  Serial.println("[MLX] Init...");
  Wire.begin(MLX_SDA, MLX_SCL);
  Wire.setClock(MLX_I2C_SPEED);
  delay(100);
  
  Wire.beginTransmission(MLX90640_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("[MLX] Not found!");
    sensorErr = true;
    return;
  }
  
  uint16_t ee[832];
  if (MLX90640_DumpEE(MLX90640_ADDR, ee) != 0) {
    Serial.println("[MLX] EEPROM fail");
    sensorErr = true;
    return;
  }
  
  if (MLX90640_ExtractParameters(ee, &mlx) != 0) {
    Serial.println("[MLX] Param fail");
    sensorErr = true;
    return;
  }
  
  MLX90640_SetRefreshRate(MLX90640_ADDR, MLX_REFRESH);
  Serial.println("[MLX] Ready");
  sensorErr = false;
}

void initLCD() {
  Serial.println("[LCD] Init...");
  
  if (!gfx->begin()) {
    Serial.println("[LCD] Failed!");
    return;
  }
  
  gfx->fillScreen(BLACK);
  
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, LOW);  // LOW = ON
  
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 10);
  gfx->println("MLX90640");
  gfx->println("Thermal");
  gfx->setTextSize(1);
  gfx->println("");
  gfx->println("Heatmap Camera");
  gfx->println("Initializing...");
  
  Serial.println("[LCD] Ready");
}

void readThermal() {
  if (sensorErr || frozen) return;
  
  for (int i = 0; i < 2; i++) {
    uint16_t frame[834];
    if (MLX90640_GetFrameData(MLX90640_ADDR, frame) < 0) {
      Serial.println("[MLX] Frame error");
      return;
    }
    
    float ta = MLX90640_GetTa(frame, &mlx);
    float tr = ta - TA_SHIFT;
    MLX90640_CalculateTo(frame, &mlx, EMMISIVITY, tr, temps);
    ambientT = ta;
  }
}

void calcStats() {
  if (sensorErr) return;
  
  float sum = 0;
  int valid = 0;
  minT = 999;
  maxT = -999;
  
  for (int i = 0; i < TH_W * TH_H; i++) {
    if (!isnan(temps[i]) && !isinf(temps[i])) {
      if (temps[i] < minT) minT = temps[i];
      if (temps[i] > maxT) maxT = temps[i];
      sum += temps[i];
      valid++;
    }
  }
  
  if (valid > 0) {
    avgT = sum / valid;
    centerT = temps[(TH_H / 2) * TH_W + (TH_W / 2)];
  }
}

void drawHeatmap() {
  gfx->fillScreen(BLACK);
  
  // Title
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(5, 5);
  gfx->print("Thermal Camera - ");
  gfx->println(schemeNames[scheme]);
  
  gfx->setCursor(5, 18);
  if (frozen) {
    gfx->setTextColor(YELLOW);
    gfx->print("FROZEN  ");
  }
  gfx->setTextColor(WHITE);
  gfx->print("IP: ");
  gfx->println(ip);
  
  // Draw heatmap
  float useMin = autoScale ? minT : manualMin;
  float useMax = autoScale ? maxT : manualMax;
  
  for (int y = 0; y < TH_H; y++) {
    for (int x = 0; x < TH_W; x++) {
      float t = temps[y * TH_W + x];
      uint16_t c = getColor(t, useMin, useMax);
      gfx->fillRect(MAP_X + x * PIXEL_W, MAP_Y + y * PIXEL_H, PIXEL_W, PIXEL_H, c);
    }
  }
  
  // Stats
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(5, 295);
  gfx->print("Min:");
  gfx->print(minT, 1);
  gfx->print(" Avg:");
  gfx->print(avgT, 1);
  gfx->print(" Max:");
  gfx->print(maxT, 1);
  
  // Buttons at bottom
  drawButtons();
}

void drawButtons() {
  int btnY = 305;
  int btnH = 12;
  
  // Page buttons
  gfx->fillRect(2, btnY, 30, btnH, page == PG_HEATMAP ? GREEN : DARKGREY);
  gfx->fillRect(35, btnY, 30, btnH, page == PG_INFO ? GREEN : DARKGREY);
  gfx->fillRect(68, btnY, 30, btnH, page == PG_SETTINGS ? GREEN : DARKGREY);
  
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(6, btnY + 2);
  gfx->print("Heat");
  gfx->setCursor(39, btnY + 2);
  gfx->print("Info");
  gfx->setCursor(72, btnY + 2);
  gfx->print("Set");
  
  // Color scheme button
  gfx->fillRect(105, btnY, 60, btnH, BLUE);
  gfx->setCursor(109, btnY + 2);
  gfx->print("Color");
}

void drawInfoPage() {
  gfx->fillScreen(BLACK);
  
  gfx->setTextSize(2);
  gfx->setTextColor(CYAN);
  gfx->setCursor(10, 10);
  gfx->println("Device Info");
  
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(5, 40);
  
  gfx->println("");
  gfx->print("WiFi: ");
  gfx->setTextColor(wifiOK ? GREEN : RED);
  gfx->println(wifiOK ? "Connected" : "Disconnected");
  
  gfx->setTextColor(WHITE);
  gfx->print("IP: ");
  gfx->println(ip);
  
  gfx->println("");
  gfx->print("OTA Host: ");
  gfx->println(OTA_HOSTNAME);
  
  gfx->println("");
  gfx->print("Sensor: ");
  gfx->setTextColor(sensorErr ? RED : GREEN);
  gfx->println(sensorErr ? "ERROR" : "OK");
  
  gfx->setTextColor(WHITE);
  gfx->print("Ambient: ");
  gfx->print(ambientT, 1);
  gfx->println(" C");
  
  gfx->println("");
  gfx->print("Uptime: ");
  unsigned long sec = (millis() - bootTime) / 1000;
  if (sec > 3600) {
    gfx->print(sec / 3600);
    gfx->print("h ");
  }
  gfx->print((sec % 3600) / 60);
  gfx->print("m");
  
  gfx->println("");
  gfx->print("Free RAM: ");
  gfx->print(ESP.getFreeHeap() / 1024);
  gfx->println(" KB");
  
  drawButtons();
}

void drawSettingsPage() {
  gfx->fillScreen(BLACK);
  
  gfx->setTextSize(2);
  gfx->setTextColor(MAGENTA);
  gfx->setCursor(10, 10);
  gfx->println("Settings");
  
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(5, 40);
  
  gfx->println("");
  gfx->println("Color Schemes:");
  
  for (int i = 0; i < SCHEME_CNT; i++) {
    gfx->print(scheme == i ? ">" : " ");
    gfx->print(" ");
    gfx->setTextColor(scheme == i ? YELLOW : WHITE);
    gfx->println(schemeNames[i]);
    gfx->setTextColor(WHITE);
  }
  
  gfx->println("");
  gfx->print("Auto-Scale: ");
  gfx->setTextColor(autoScale ? GREEN : RED);
  gfx->println(autoScale ? "ON" : "OFF");
  
  gfx->setTextColor(WHITE);
  if (!autoScale) {
    gfx->print("Range: ");
    gfx->print(manualMin, 1);
    gfx->print(" - ");
    gfx->print(manualMax, 1);
    gfx->println(" C");
  }
  
  gfx->println("");
  gfx->print("Frozen: ");
  gfx->setTextColor(frozen ? YELLOW : GREEN);
  gfx->println(frozen ? "YES" : "NO");
  
  drawButtons();
}

void handleTouch() {
  // Simple touch detection - would need FT3168 library
  // For now, stub for future implementation
  // You'd read touch coordinates and check button areas
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  bootTime = millis();
  
  Serial.println("\n=== MLX90640 Thermal Heatmap ===");
  Serial.println("=== With OTA Updates ===");
  
  initLCD();
  delay(2000);
  
  initWiFi();
  initOTA();
  initMLX();
  
  Serial.println("\n[READY] System initialized");
  if (wifiOK) {
    Serial.println("[OTA] Upload via: " + ip);
  }
  
  delay(1000);
}

void loop() {
  ArduinoOTA.handle();
  if (otaActive) return;
  
  unsigned long now = millis();
  
  if (now - lastUpdate >= 250) {
    readThermal();
    calcStats();
    
    switch(page) {
      case PG_HEATMAP:
        drawHeatmap();
        break;
      case PG_INFO:
        drawInfoPage();
        break;
      case PG_SETTINGS:
        drawSettingsPage();
        break;
    }
    
    lastUpdate = now;
  }
  
  // Boot button (GPIO 9) - cycles through modes
  static bool btnPressed = false;
  static unsigned long firstPressTime = 0;
  static int btnPressCount = 0;
  
  if (digitalRead(9) == LOW && !btnPressed) {  // Boot button pressed
    btnPressed = true;
    
    if (btnPressCount == 0) {
      firstPressTime = now;  // Start timing on first press
    }
    btnPressCount++;
    
    // Single press: cycle color schemes
    scheme = (Scheme)((scheme + 1) % SCHEME_CNT);
    Serial.println("Color Scheme: " + String(schemeNames[scheme]));
    
    // Double press: also cycle pages
    if (btnPressCount >= 2 && (now - firstPressTime) < 500) {
      page = (PageType)((page + 1) % PG_CNT);
      Serial.println("Page: " + String(page));
      btnPressCount = 0;
    }
    
    delay(200);
  } else if (digitalRead(9) == HIGH) {
    btnPressed = false;
    if (now - firstPressTime > 1000) {
      btnPressCount = 0;  // Reset counter after 1 second
    }
  }
  
  delay(10);
}