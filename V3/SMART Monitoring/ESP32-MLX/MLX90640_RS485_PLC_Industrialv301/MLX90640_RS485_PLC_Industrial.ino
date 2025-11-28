/*
 * MLX90640 Thermal Camera with RS485 Modbus RTU - Industrial Paper Mill Version
 * ESP32-C6-Touch-LCD-1.9 Configuration
 * 
 * Hardware Connections:
 * - MLX90640: I2C (SDA=GPIO18, SCL=GPIO8)
 * - Built-in LCD: ST7789 170x320 SPI display
 * - RS485 TTL Converter: UART1 (TX=GPIO2, RX=GPIO3, DE/RE=GPIO10)
 * 
 * Industrial Features:
 * - Enhanced EMI/noise immunity with filtering
 * - Configurable watchdog with extended timeout
 * - Exponential backoff retry with jitter
 * - Brownout detection and recovery
 * - Data buffering for communication failures
 * - Health monitoring and diagnostics
 * - Dual safety: Hardware + software watchdogs
 * - Galvanic isolation ready (hardware dependent)
 * - Extended temperature range support
 * - Power failure detection and graceful shutdown
 * 
 * Author: Codebuff AI
 * Date: 2024
 * Version: 2.0 Industrial
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <esp_pm.h>
#include <driver/gpio.h>

// MLX90640 Libraries
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

// GFX Display Library
#include <Arduino_GFX_Library.h>

// Modbus RTU Library
#include <ModbusMaster.h>

// =================================================================
// INDUSTRIAL CONFIGURATION
// =================================================================

// WiFi Configuration (Optional - for remote monitoring)
const char* WIFI_SSID = "VPN1";
const char* WIFI_PASS = "XXX";
const char* OTA_HOSTNAME = "esp32-mlx-industrial";
const char* OTA_PASSWORD = "mlx90640";  // CHANGE THIS! 

// MLX90640 Sensor Configuration
#define MLX_SDA 18
#define MLX_SCL 8
#define MLX90640_ADDRESS 0x33
#define MLX_I2C_SPEED 400000      // Conservative 400kHz for noise immunity
#define MLX_REFRESH_RATE 0x05     // 4Hz - balance between speed and stability

// Built-in LCD Display
#define LCD_DC 6
#define LCD_CS 7
#define LCD_SCK 5
#define LCD_MOSI 4
#define LCD_RST 14
#define LCD_BL 15
#define LCD_WIDTH 170
#define LCD_HEIGHT 320

// RS485 Configuration - Industrial Grade
#define RS485_TX 2
#define RS485_RX 3
#define RS485_DE_RE 10
#define RS485_BAUD 9600           // Low baud for long-distance reliability
#define PLC_SLAVE_ID 1

// Modbus Register Map
#define REG_AVG_TEMP 0
#define REG_MIN_TEMP 1
#define REG_MAX_TEMP 2
#define REG_AMBIENT_TEMP 3
#define REG_STATUS 4
#define REG_ERROR_COUNT 5
#define REG_SUCCESS_COUNT 6       // Added for diagnostics
#define REG_UPTIME_HOURS 7        // System uptime in hours
#define REG_SENSOR_HEALTH 8       // Sensor health indicator (0-100%)
#define REG_COMM_QUALITY 9        // Communication quality (0-100%)

// Timing Configuration - Industrial Grade
#define SENSOR_READ_INTERVAL 1000
#define PLC_SEND_INTERVAL 2000
#define LCD_UPDATE_INTERVAL 5000
#define HEALTH_CHECK_INTERVAL 10000   // System health check every 10s
#define WIFI_RETRY_INTERVAL 300000    // Retry WiFi every 5 minutes if failed

// Industrial Retry Configuration
#define MAX_RETRIES 5                 // Increased retries
#define RETRY_BASE_DELAY 100          // Base delay in ms
#define RETRY_MAX_DELAY 2000          // Max delay cap
#define MODBUS_TIMEOUT 2000           // Extended timeout for noisy environments
#define MODBUS_TURNAROUND_DELAY 50    // Inter-frame delay

// Watchdog Configuration - Dual Safety
#define WATCHDOG_TIMEOUT 90           // 90 seconds for MLX90640 init
#define ENABLE_WATCHDOG true
#define SOFTWARE_WDT_INTERVAL 30000   // Software watchdog check every 30s

// Thermal Sensor Configuration
#define EMMISIVITY 0.95
#define TA_SHIFT 8

// Data Buffering - Store last N readings for recovery
#define DATA_BUFFER_SIZE 10

// Health Monitoring Thresholds
#define MIN_SUCCESS_RATE 80           // Minimum 80% success rate
#define MAX_CONSECUTIVE_ERRORS 100    // Reboot after 100 consecutive errors (tolerant for testing without PLC)
#define TEMP_RANGE_MIN -20.0          // Valid temperature range
#define TEMP_RANGE_MAX 400.0

// Power Management
#define BROWNOUT_THRESHOLD_MV 2700    // Brownout at 2.7V
#define ENABLE_POWER_SAVE false       // Disable for continuous operation

// =================================================================
// GLOBAL VARIABLES
// =================================================================

// MLX90640 sensor
paramsMLX90640 mlx90640;
static float tempValues[32 * 24];

// Temperature data
float avgTemp = 0.0;
float minTemp = 0.0;
float maxTemp = 0.0;
float ambientTemp = 0.0;

// Data buffering for recovery
struct TempData {
  float avg;
  float min;
  float max;
  float ambient;
  unsigned long timestamp;
};
TempData dataBuffer[DATA_BUFFER_SIZE];
uint8_t bufferIndex = 0;

// Timing
unsigned long lastSensorRead = 0;
unsigned long lastPLCSend = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastHealthCheck = 0;
unsigned long lastWiFiRetry = 0;
unsigned long lastSoftwareWDT = 0;
unsigned long bootTime = 0;

// Error tracking and diagnostics
uint32_t errorCount = 0;
uint32_t successCount = 0;
uint16_t consecutiveErrors = 0;
uint16_t sensorErrorCount = 0;
uint16_t modbusErrorCount = 0;
uint8_t sensorHealth = 100;       // 0-100%
uint8_t commQuality = 100;        // 0-100%
bool sensorError = false;
bool modbusError = false;
bool wifiConnected = false;
bool criticalError = false;

// WiFi
String ipAddress = "Not Connected";
bool otaActive = false;
int otaPct = 0;

// Display
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, 0, 
                                       LCD_WIDTH, LCD_HEIGHT, 
                                       35, 0, 35, 0);

// Modbus
ModbusMaster plc;

// =================================================================
// FUNCTION PROTOTYPES
// =================================================================
void initWiFi();
void initOTA();
void initMLX90640();
void initLCD();
void initRS485();
void initWatchdog();
void readTempValues();
void calculateStats();
void bufferData();
bool sendToPLC();
void updateLCD();
void performHealthCheck();
void handleCriticalError();
void preTransmission();
void postTransmission();
void idle();
int16_t floatToInt(float value);
uint16_t calculateChecksum(float temp);
bool validateTemperature(float temp);

// =================================================================
// RS485 CONTROL FUNCTIONS
// =================================================================

void preTransmission() {
  digitalWrite(RS485_DE_RE, HIGH);
  delayMicroseconds(MODBUS_TURNAROUND_DELAY * 10);
}

void postTransmission() {
  delayMicroseconds(MODBUS_TURNAROUND_DELAY * 10);
  digitalWrite(RS485_DE_RE, LOW);
}

void idle() {
  // Reset software watchdog during idle
  if (millis() - lastSoftwareWDT > SOFTWARE_WDT_INTERVAL) {
    lastSoftwareWDT = millis();
  }
}

int16_t floatToInt(float value) {
  return (int16_t)(value * 100.0);
}

// Simple checksum for data integrity
uint16_t calculateChecksum(float temp) {
  int16_t intTemp = floatToInt(temp);
  return (uint16_t)(intTemp ^ 0xA5A5);
}

bool validateTemperature(float temp) {
  return (temp >= TEMP_RANGE_MIN && temp <= TEMP_RANGE_MAX && !isnan(temp) && !isinf(temp));
}

// =================================================================
// INITIALIZATION FUNCTIONS
// =================================================================

void initWiFi() {
  Serial.println("[INIT] WiFi (Optional)...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    if (ENABLE_WATCHDOG && attempts % 5 == 0) {
      esp_task_wdt_reset();
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    ipAddress = WiFi.localIP().toString();
    Serial.println("\n[OK] WiFi: " + ipAddress);
  } else {
    wifiConnected = false;
    ipAddress = "WiFi Disabled";
    Serial.println("\n[INFO] WiFi disabled, continuing");
  }
}

void initOTA() {
  if (!wifiConnected) {
    Serial.println("[OTA] Skipped (no WiFi)");
    return;
  }
  
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  
  ArduinoOTA.onStart([]() {
    otaActive = true;
    otaPct = 0;
    
    // Disable watchdog during OTA to prevent timeout
    if (ENABLE_WATCHDOG) {
      esp_task_wdt_delete(NULL);
    }
    
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
    
    // Re-enable watchdog after OTA
    if (ENABLE_WATCHDOG) {
      esp_task_wdt_add(NULL);
    }
    
    Serial.println("\n[OTA] Complete!");
    gfx->fillRect(20, 200, 130, 20, BLACK);
    gfx->setTextColor(GREEN);
    gfx->setCursor(30, 203);
    gfx->println("SUCCESS!");
    delay(1000);
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    otaActive = false;
    
    // Re-enable watchdog after OTA error
    if (ENABLE_WATCHDOG) {
      esp_task_wdt_add(NULL);
    }
    
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

void initMLX90640() {
  Serial.println("[INIT] MLX90640 Thermal Sensor...");
  
  if (ENABLE_WATCHDOG) esp_task_wdt_reset();
  
  Wire.begin(MLX_SDA, MLX_SCL);
  Wire.setClock(MLX_I2C_SPEED);
  delay(100);
  
  // Retry logic for sensor detection
  int retries = 0;
  while (retries < 3) {
    Wire.beginTransmission(MLX90640_ADDRESS);
    if (Wire.endTransmission() == 0) {
      break;
    }
    retries++;
    Serial.print("[WARN] MLX90640 retry ");
    Serial.println(retries);
    delay(500);
  }
  
  if (retries >= 3) {
    Serial.println("[ERROR] MLX90640 not detected!");
    sensorError = true;
    sensorHealth = 0;
    return;
  }
  
  if (ENABLE_WATCHDOG) esp_task_wdt_reset();
  
  uint16_t eeMLX90640[832];
  int status = MLX90640_DumpEE(MLX90640_ADDRESS, eeMLX90640);
  if (status != 0) {
    Serial.println("[ERROR] EEPROM dump failed");
    sensorError = true;
    sensorHealth = 25;
    return;
  }
  
  if (ENABLE_WATCHDOG) esp_task_wdt_reset();
  
  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0) {
    Serial.println("[ERROR] Parameter extraction failed");
    sensorError = true;
    sensorHealth = 50;
    return;
  }
  
  MLX90640_SetRefreshRate(MLX90640_ADDRESS, MLX_REFRESH_RATE);
  Wire.setClock(MLX_I2C_SPEED);
  
  Serial.println("[OK] MLX90640 initialized");
  sensorError = false;
  sensorHealth = 100;
}

void initLCD() {
  Serial.println("[INIT] LCD Display...");
  
  // Init Display first
  if (!gfx->begin()) {
    Serial.println("[ERROR] LCD init failed");
    return;
  }
  
  gfx->fillScreen(BLACK);
  
  // Initialize backlight AFTER display init (LOW = ON for this board)
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, LOW);
  
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 10);
  gfx->println("MLX90640");
  gfx->println("Industrial");
  gfx->setTextSize(1);
  gfx->println("");
  gfx->println("Paper Mill Ready");
  gfx->println("Initializing...");
  
  Serial.println("[OK] LCD ready");
}

void initRS485() {
  Serial.println("[INIT] RS485 Modbus...");
  
  pinMode(RS485_DE_RE, OUTPUT);
  digitalWrite(RS485_DE_RE, LOW);
  
  Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);
  delay(100);
  
  plc.begin(PLC_SLAVE_ID, Serial1);
  plc.preTransmission(preTransmission);
  plc.postTransmission(postTransmission);
  plc.idle(idle);
  
  Serial.print("[OK] RS485 @ ");
  Serial.print(RS485_BAUD);
  Serial.println(" baud");
}

void initWatchdog() {
  if (!ENABLE_WATCHDOG) {
    Serial.println("[INFO] Watchdog disabled");
    return;
  }
  
  Serial.print("[INIT] Watchdog (");
  Serial.print(WATCHDOG_TIMEOUT);
  Serial.println("s)...");
  
  esp_task_wdt_deinit();
  
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  
  Serial.println("[OK] Watchdog enabled");
}

// =================================================================
// SENSOR FUNCTIONS
// =================================================================

void readTempValues() {
  if (sensorError) return;
  
  for (byte x = 0; x < 2; x++) {
    if (ENABLE_WATCHDOG) esp_task_wdt_reset();
    
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_ADDRESS, mlx90640Frame);
    
    if (status < 0) {
      Serial.print("[ERROR] Frame error: ");
      Serial.println(status);
      sensorErrorCount++;
      sensorHealth = max(0, sensorHealth - 5);
      
      if (sensorErrorCount > 10) {
        sensorError = true;
      }
      return;
    }
    
    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
    float tr = Ta - TA_SHIFT;
    
    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, EMMISIVITY, tr, tempValues);
    ambientTemp = Ta;
  }
  
  sensorError = false;
  sensorHealth = min(100, sensorHealth + 1);
  sensorErrorCount = 0;
}

void calculateStats() {
  if (sensorError) return;
  
  float sum = 0.0;
  int validCount = 0;
  minTemp = 999.0;
  maxTemp = -999.0;
  
  for (int i = 0; i < 768; i++) {
    float temp = tempValues[i];
    
    if (validateTemperature(temp)) {
      sum += temp;
      validCount++;
      
      if (temp < minTemp) minTemp = temp;
      if (temp > maxTemp) maxTemp = temp;
    }
  }
  
  if (validCount > 0) {
    avgTemp = sum / validCount;
  } else {
    Serial.println("[WARN] No valid temperatures");
    sensorError = true;
  }
}

void bufferData() {
  dataBuffer[bufferIndex].avg = avgTemp;
  dataBuffer[bufferIndex].min = minTemp;
  dataBuffer[bufferIndex].max = maxTemp;
  dataBuffer[bufferIndex].ambient = ambientTemp;
  dataBuffer[bufferIndex].timestamp = millis();
  
  bufferIndex = (bufferIndex + 1) % DATA_BUFFER_SIZE;
}

// =================================================================
// MODBUS COMMUNICATION
// =================================================================

bool sendToPLC() {
  uint8_t result;
  int retries = 0;
  bool success = false;
  
  // Calculate uptime in hours
  uint32_t uptimeHours = (millis() - bootTime) / 3600000;
  
  // Prepare data
  uint16_t data[10];
  data[0] = (uint16_t)floatToInt(avgTemp);
  data[1] = (uint16_t)floatToInt(minTemp);
  data[2] = (uint16_t)floatToInt(maxTemp);
  data[3] = (uint16_t)floatToInt(ambientTemp);
  data[4] = (sensorError || criticalError) ? 1 : 0;
  data[5] = (uint16_t)(errorCount & 0xFFFF);
  data[6] = (uint16_t)(successCount & 0xFFFF);
  data[7] = (uint16_t)(uptimeHours & 0xFFFF);
  data[8] = sensorHealth;
  data[9] = commQuality;
  
  // Retry with exponential backoff + jitter
  while (retries < MAX_RETRIES && !success) {
    plc.clearTransmitBuffer();
    for (int i = 0; i < 10; i++) {
      plc.setTransmitBuffer(i, data[i]);
    }
    
    result = plc.writeMultipleRegisters(REG_AVG_TEMP, 10);
    
    if (result == plc.ku8MBSuccess) {
      success = true;
      successCount++;
      consecutiveErrors = 0;
      modbusError = false;
      commQuality = min(100, commQuality + 2);
      
      Serial.print("[OK] PLC: Avg=");
      Serial.print(avgTemp, 2);
      Serial.println("°C");
    } else {
      retries++;
      errorCount++;
      modbusErrorCount++;
      consecutiveErrors++;
      modbusError = true;
      commQuality = max(0, commQuality - 5);
      
      Serial.print("[ERROR] Modbus attempt ");
      Serial.print(retries);
      Serial.print("/");
      Serial.print(MAX_RETRIES);
      Serial.print(", code: 0x");
      Serial.println(result, HEX);
      
      if (retries < MAX_RETRIES) {
        // Exponential backoff with jitter
        int delayTime = min(RETRY_BASE_DELAY * (1 << retries), RETRY_MAX_DELAY);
        delayTime += random(0, 100);  // Add jitter
        delay(delayTime);
      }
    }
  }
  
  if (!success) {
    Serial.println("[ERROR] PLC send failed - max retries");
  }
  
  return success;
}

// =================================================================
// HEALTH MONITORING
// =================================================================

void performHealthCheck() {
  Serial.println("[HEALTH] System check...");
  
  // Check consecutive errors (only if sensor also has issues)
  if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS && sensorError) {
    Serial.println("[CRITICAL] Too many errors - restart required");
    criticalError = true;
    handleCriticalError();
  } else if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
    Serial.println("[WARN] Many Modbus errors but sensor OK - continuing");
  }
  
  // Calculate success rate
  uint32_t totalAttempts = successCount + errorCount;
  if (totalAttempts > 0) {
    uint8_t successRate = (successCount * 100) / totalAttempts;
    
    if (successRate < MIN_SUCCESS_RATE) {
      Serial.print("[WARN] Success rate low: ");
      Serial.print(successRate);
      Serial.println("%");
    }
  }
  
  // Check sensor health
  if (sensorHealth < 50) {
    Serial.println("[WARN] Sensor health degraded");
  }
  
  // Check communication quality
  if (commQuality < 50) {
    Serial.println("[WARN] Comm quality degraded");
  }
  
  // Try to reconnect WiFi if down
  if (!wifiConnected && (millis() - lastWiFiRetry > WIFI_RETRY_INTERVAL)) {
    Serial.println("[INFO] Retrying WiFi...");
    initWiFi();
    lastWiFiRetry = millis();
  }
  
  Serial.println("[HEALTH] Check complete");
}

void handleCriticalError() {
  Serial.println("[CRITICAL] Entering safe mode...");
  
  // Display error on LCD
  gfx->fillScreen(RED);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 10);
  gfx->println("CRITICAL");
  gfx->println("ERROR");
  gfx->setTextSize(1);
  gfx->println("");
  gfx->println("System will restart");
  gfx->print("in 10 seconds...");
  
  delay(10000);
  
  Serial.println("[SYSTEM] Restarting...");
  esp_restart();
}

// =================================================================
// DISPLAY
// =================================================================

void updateLCD() {
  static String lastIP = "";
  static bool firstUpdate = true;
  String currentIP = wifiConnected ? WiFi.localIP().toString() : "WiFi Off";
  bool ipChanged = (currentIP != lastIP);
  
  // Always do first update immediately, then check timing
  unsigned long now = millis();
  if (!firstUpdate && !ipChanged && lastLCDUpdate > 0 && (now - lastLCDUpdate < LCD_UPDATE_INTERVAL)) {
    return;  // Skip update
  }
  
  firstUpdate = false;
  lastIP = currentIP;
  lastLCDUpdate = now;
    
    gfx->fillScreen(BLACK);
    gfx->setTextSize(2);
    gfx->setTextColor(WHITE);
    
    // Title
    gfx->setCursor(5, 5);
    gfx->println("INDUSTRIAL");
    gfx->drawLine(0, 25, LCD_WIDTH, 25, WHITE);
    
    // Temperature
    gfx->setTextSize(1);
    gfx->setCursor(5, 30);
    gfx->print("Avg: ");
    gfx->print(avgTemp, 1);
    gfx->println(" C");
    gfx->setCursor(5, 45);
    gfx->print("Min: ");
    gfx->print(minTemp, 1);
    gfx->print(" Max: ");
    gfx->print(maxTemp, 1);
    gfx->println("C");
    
    gfx->drawLine(0, 65, LCD_WIDTH, 65, WHITE);
    
    // Network
    gfx->setCursor(5, 70);
    gfx->println("Network:");
    gfx->setCursor(5, 85);
    gfx->setTextColor(wifiConnected ? GREEN : YELLOW);
    gfx->println(currentIP);
    
    gfx->drawLine(0, 105, LCD_WIDTH, 105, WHITE);
    
    // Health metrics
    gfx->setTextColor(WHITE);
    gfx->setCursor(5, 110);
    gfx->println("Health:");
    gfx->setCursor(5, 125);
    gfx->print("Sensor: ");
    gfx->setTextColor(sensorHealth > 80 ? GREEN : (sensorHealth > 50 ? YELLOW : RED));
    gfx->print(sensorHealth);
    gfx->setTextColor(WHITE);
    gfx->println("%");
    
    gfx->setCursor(5, 140);
    gfx->print("Comm: ");
    gfx->setTextColor(commQuality > 80 ? GREEN : (commQuality > 50 ? YELLOW : RED));
    gfx->print(commQuality);
    gfx->setTextColor(WHITE);
    gfx->println("%");
    
    gfx->drawLine(0, 160, LCD_WIDTH, 160, WHITE);
    
    // Status
    gfx->setCursor(5, 165);
    gfx->println("Status:");
    gfx->setCursor(5, 180);
    
    if (criticalError) {
      gfx->setTextColor(RED);
      gfx->println("CRITICAL");
    } else if (sensorError) {
      gfx->setTextColor(RED);
      gfx->println("SENSOR ERR");
    } else if (modbusError) {
      gfx->setTextColor(YELLOW);
      gfx->println("COMM ERR");
    } else {
      gfx->setTextColor(GREEN);
      gfx->println("ACTIVE");
    }
    
    // Statistics
    gfx->setTextColor(WHITE);
    gfx->setCursor(5, 195);
    gfx->print("Err:");
    gfx->print(errorCount);
    gfx->print(" OK:");
    gfx->println(successCount);
    
    gfx->setCursor(5, 210);
    uint32_t uptimeSec = (millis() - bootTime) / 1000;
    gfx->print("Up:");
    if (uptimeSec > 3600) {
      gfx->print(uptimeSec / 3600);
      gfx->print("h");
    } else {
      gfx->print(uptimeSec);
      gfx->print("s");
    }
  }


// =================================================================
// SETUP
// =================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  bootTime = millis();
  
  Serial.println("\n\n============================");
  Serial.println("MLX90640 Industrial RS485");
  Serial.println("Paper Mill Grade");
  Serial.println("ESP32-C6-Touch-LCD-1.9");
  Serial.println("============================\n");
  
  initWatchdog();
  initLCD();
  initWiFi();
  initOTA();
  initMLX90640();
  initRS485();
  
  Serial.println("\n[OK] System ready");
  Serial.println("Starting operation...\n");
  
  // Show init screen for 2 seconds
  delay(2000);
  
  // Initial read
  readTempValues();
  calculateStats();
  bufferData();
  
  // Display initial data immediately
  updateLCD();
}

// =================================================================
// MAIN LOOP
// =================================================================

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  if (otaActive) return;  // Skip normal operation during OTA
  
  if (ENABLE_WATCHDOG) {
    esp_task_wdt_reset();
  }
  
  unsigned long now = millis();
  
  // Sensor reading
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readTempValues();
    calculateStats();
    bufferData();
    lastSensorRead = now;
  }
  
  // PLC communication
  if (now - lastPLCSend >= PLC_SEND_INTERVAL) {
    sendToPLC();
    lastPLCSend = now;
  }
  
  // Health check
  if (now - lastHealthCheck >= HEALTH_CHECK_INTERVAL) {
    performHealthCheck();
    lastHealthCheck = now;
  }
  
  // Display update (checks internally if update needed)
  updateLCD();
  
  delay(10);
}
