#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <esp_heap_caps.h>
#include <FastLED.h>
#include "esp_camera.h"

// Camera pin definitions for CAMERA_MODEL_ESP32S3_EYE
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

// WiFi credentials (update as needed)
const char* ssid = "RnD";
const char* password = "wnOPxFSCxb";

// Server and image URLs
const char* serverUrl = "http://192.168.10.100:5000";
const char* imageUrl = "https://fastly.picsum.photos/id/682/800/600.jpg?hmac=nDvj6j28PV7_q1jWXRsp0xS7jtAZYzHophmak9J1ymU";

// Image buffer size (1MB per buffer, 2MB total within 8MB PSRAM)
#define IMAGE_SIZE (512 * 1024)
uint8_t* buffers[2];    // Two buffers in PSRAM
size_t image_sizes[2];  // Actual size of each captured image
int next_capture = 0;   // Next buffer index for capture task
int next_send = 0;      // Next buffer index for send task

// Semaphores for double buffer synchronization
SemaphoreHandle_t empty_sem;  // Counting semaphore for empty buffers
SemaphoreHandle_t full_sem;   // Counting semaphore for full buffers

// Queue for log messages
QueueHandle_t log_queue;

// Function to send log to server
void log_to_server(const String& message) {
  HTTPClient http;
  http.begin(String(serverUrl) + "/log");
  http.addHeader("X-Device-ID", WiFi.macAddress());
  http.addHeader("Content-Type", "text/plain");
  int httpCode = http.POST(message);
  if (httpCode != HTTP_CODE_OK) {
    // Optionally, handle failure
  }
  http.end();
}

// Task to send logs
void log_send_task(void* pvParameters) {
  String* msg;
  while (true) {
    if (xQueueReceive(log_queue, &msg, portMAX_DELAY) == pdTRUE) {
      if (WiFi.status() == WL_CONNECTED) {
        log_to_server(*msg);
      }
      delete msg;
    }
  }
}

// Function to log messages
void log_message(const String& message) {
  Serial.println(message);  // For debugging, remove if not needed
  if (log_queue != NULL) {
    String* msg = new String(message);
    if (xQueueSend(log_queue, &msg, 0) != pdTRUE) {
      delete msg;  // Queue full, discard log
    }
  }
}

// Function to capture image from OV5640 camera
bool capture_image(uint8_t* buffer, size_t max_size, size_t* out_size) {
  camera_fb_t* fb = esp_camera_fb_get();  // Get a frame from the camera
  if (!fb) {
    log_message("Camera capture failed");
    return false;
  }
  if (fb->format != PIXFORMAT_JPEG) {
    log_message("Non-JPEG data not supported");
    esp_camera_fb_return(fb);
    return false;
  }
  if (fb->len > max_size) {
    log_message("Image too large for buffer: " + String(fb->len) + " > " + String(max_size));
    esp_camera_fb_return(fb);
    return false;
  }
  memcpy(buffer, fb->buf, fb->len);  // Copy frame to buffer
  *out_size = fb->len;
  esp_camera_fb_return(fb);  // Return the frame buffer
  log_message("Image captured successfully, size: " + String(*out_size));
  return true;
}

// Function pointer for image acquisition
typedef bool (*acquire_image_func_t)(uint8_t* buffer, size_t max_size, size_t* out_size);
acquire_image_func_t acquire_image = capture_image;

void IRAM_ATTR get_image_task(void* pvParameters);
void IRAM_ATTR image_send_task(void* pvParameters);

// Task handles
TaskHandle_t getImageTaskHandle = NULL;
TaskHandle_t imageSendTaskHandle = NULL;
TaskHandle_t ledUpdateTaskHandle = NULL;

// LED Definitions
#define NUM_LEDS 1
#define DATA_PIN 48
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define BRIGHTNESS 50
CRGB leds[NUM_LEDS];

// LED States
enum LedState {
  LED_IDLE,                  // Black (off)
  LED_CONNECTING_WIFI,       // Blinking Yellow
  LED_WIFI_CONNECTED,        // Solid Green
  LED_WIFI_FAILED,           // Solid Red
  LED_ACQUIRING_IMAGE,       // Solid Blue
  LED_IMAGE_ACQUIRE_FAILED,  // Blinking Red
  LED_IMAGE_SAVED,           // Solid Cyan
  LED_WAITING_SERVER_ACK,    // Solid Magenta
  LED_SERVER_ACK_RECEIVED,   // Solid White
  LED_SENDING_IMAGE,         // Solid Orange
  LED_IMAGE_SENT_SUCCESS,    // Solid Green
  LED_IMAGE_SENT_FAILED      // Blinking Red
};
LedState currentLedState = LED_IDLE;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Create log queue and task
  log_queue = xQueueCreate(10, sizeof(String*));
  if (log_queue == NULL) {
    Serial.println("Failed to create log queue");
    while (1)
      ;
  }
  xTaskCreate(log_send_task, "LOG_SEND_TASK", 4096, NULL, 1, NULL);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  // Create LED update task
  xTaskCreate(ledUpdateTask, "LED_UPDATE_TASK", 2048, NULL, 1, &ledUpdateTaskHandle);

  // Connect to WiFi with retries
  currentLedState = LED_CONNECTING_WIFI;

  // Define timeout and retry parameters
  const unsigned long connectionTimeout = 10000;  // 10 seconds timeout per attempt
  const int maxRetries = 5;
  bool wifi_success = false;
  WiFi.mode(WIFI_STA);  // Set ESP32 as a WiFi station
  WiFi.disconnect();
  delay(500);           // Optional: give time for mode to set


  Serial.println("Scanning networks...");
  int n = WiFi.scanNetworks();  // Scan for WiFi networks
  if (n == 0) {
    Serial.println("No networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found:");
    for (int i = 0; i < n; ++i) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));  // Print SSID
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));  // Print signal strength
      Serial.println(" dBm)");
    }
  }


  WiFi.begin(ssid, password);  // Start connection attempt
  WiFi.setAutoReconnect(true);

  delay(1000);

  Serial.println("\r\n Connecting.");
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print("WIFI LOOP: ");
    Serial.println(WiFi.status());
    // if(Wifi.status() == WL_CONNECTED) break;
  }

/*
  int retryCount = 0;
  while (retryCount < maxRetries) {
    log_message("Attempting to connect to WiFi, attempt " + String(retryCount + 1));
    WiFi.begin(ssid);  // Start connection attempt
    WiFi.setAutoReconnect(true);

    delay(2000);

    unsigned long startTime = millis();
    while (millis() - startTime < connectionTimeout) {
      if (WiFi.status() == WL_CONNECTED) {
        log_message("Connected to WiFi successfully! RSSI: " + String(WiFi.RSSI()));
        log_message("IP Address: " + WiFi.localIP().toString());
        // WiFi.setAutoReconnect(true);  // Enable auto-reconnect if connection drops
        wifi_success = true;
        break;
      }
      delay(100);  // Check every 100ms (non-blocking)
    }
    if (wifi_success) break;

    log_message("Connection attempt timed out");
    
    WiFi.disconnect();  // Reset connection state
    retryCount++;
    delay(500);  // Short delay before retrying
  }
  */

  if (WiFi.status() == WL_CONNECTED) {
    currentLedState = LED_WIFI_CONNECTED;
    log_message("Connected to WiFi");
  } else {
    currentLedState = LED_WIFI_FAILED;
    log_message("Failed to connect to WiFi");
    while (1)
      ;  // Halt or handle failure
  }

  // Allocate image buffers in PSRAM
  buffers[0] = (uint8_t*)heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_SPIRAM);
  buffers[1] = (uint8_t*)heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_SPIRAM);
  if (buffers[0] == NULL || buffers[1] == NULL) {
    log_message("Failed to allocate PSRAM buffers");
    while (1)
      ;
  }

  // Initialize camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_HD;
  config.jpeg_quality = 5;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    log_message("Camera init failed with error " + String(err));
    while (1)
      ;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != NULL) {
    sensor->set_vflip(sensor, 1);
    log_message("Vertical flip enabled");
  } else {
    log_message("Failed to get camera sensor");
  }

  log_message("Camera initialized successfully");

  // Create semaphores for double buffering
  empty_sem = xSemaphoreCreateCounting(2, 2);  // Both buffers start empty
  full_sem = xSemaphoreCreateCounting(2, 0);   // No buffers start full
  if (empty_sem == NULL || full_sem == NULL) {
    log_message("Failed to create semaphores");
    while (1)
      ;
  }

  // Create tasks
  xTaskCreate(get_image_task, "GET_IMAGE_TASK", 4096, NULL, 1, &getImageTaskHandle);
  xTaskCreate(image_send_task, "IMAGE_SEND_TASK", 4096, NULL, 1, &imageSendTaskHandle);
}

void loop() {
  delay(1000);  // Main loop is empty; tasks handle the work
  vTaskDelete(NULL);
}

// LED Update Task
void ledUpdateTask(void* parameter) {
  while (true) {
    updateLed();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// Update LED based on current state
void updateLed() {
  static bool blinkState = false;
  blinkState = !blinkState;
  switch (currentLedState) {
    case LED_IDLE:
      leds[0] = CRGB::Black;
      break;
    case LED_CONNECTING_WIFI:
      leds[0] = blinkState ? CRGB::Yellow : CRGB::Black;
      break;
    case LED_WIFI_CONNECTED:
      leds[0] = CRGB::Green;
      break;
    case LED_WIFI_FAILED:
      leds[0] = CRGB::Red;
      break;
    case LED_ACQUIRING_IMAGE:
      leds[0] = CRGB::Blue;
      break;
    case LED_IMAGE_ACQUIRE_FAILED:
      leds[0] = blinkState ? CRGB::Red : CRGB::Black;
      break;
    case LED_IMAGE_SAVED:
      leds[0] = CRGB::Cyan;
      break;
    case LED_WAITING_SERVER_ACK:
      leds[0] = CRGB::Magenta;
      break;
    case LED_SERVER_ACK_RECEIVED:
      leds[0] = CRGB::White;
      break;
    case LED_SENDING_IMAGE:
      leds[0] = CRGB::Orange;
      break;
    case LED_IMAGE_SENT_SUCCESS:
      leds[0] = CRGB::Green;
      break;
    case LED_IMAGE_SENT_FAILED:
      leds[0] = blinkState ? CRGB::Red : CRGB::Black;
      break;
  }
  FastLED.show();
}

// Task to acquire and save image to PSRAM using double buffering
void IRAM_ATTR get_image_task(void* pvParameters) {
  while (true) {
    // Wait for an empty buffer
    xSemaphoreTake(empty_sem, portMAX_DELAY);

    int buffer_to_capture = next_capture;
    next_capture = (next_capture + 1) % 2;

    currentLedState = LED_ACQUIRING_IMAGE;
    log_message("Starting to acquire image, RSSI: " + String(WiFi.RSSI()));

    bool success = capture_image(buffers[buffer_to_capture], IMAGE_SIZE, &image_sizes[buffer_to_capture]);
    if (success) {
      currentLedState = LED_IMAGE_SAVED;
      log_message("Image acquired and saved to PSRAM, buffer: " + String(buffer_to_capture));
    } else {
      currentLedState = LED_IMAGE_ACQUIRE_FAILED;
      log_message("Failed to acquire image; retrying in 1s");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      xSemaphoreGive(empty_sem);  // Reclaim the empty semaphore on failure
      continue;
    }

    // Signal that a buffer is full
    xSemaphoreGive(full_sem);
  }
}

// Task to send image to server using double buffering
void IRAM_ATTR image_send_task(void* pvParameters) {
  while (true) {
    // Wait for a full buffer
    xSemaphoreTake(full_sem, portMAX_DELAY);

    int buffer_to_send = next_send;
    next_send = (next_send + 1) % 2;

    currentLedState = LED_WAITING_SERVER_ACK;
    log_message("Requesting server acknowledgment, RSSI: " + String(WiFi.RSSI()));

    if (get_server_ack()) 
    {
      currentLedState = LED_SERVER_ACK_RECEIVED;
      currentLedState = LED_SENDING_IMAGE;
      log_message("Sending image from buffer: " + String(buffer_to_send) + ", size: " + String(image_sizes[buffer_to_send]) + ", RSSI: " + String(WiFi.RSSI()));
      if (send_image_to_server(buffers[buffer_to_send], image_sizes[buffer_to_send])) 
      {
        currentLedState = LED_IMAGE_SENT_SUCCESS;
      } 
      else 
      {
        currentLedState = LED_IMAGE_SENT_FAILED;
        log_message("Failed to send image");
      }
    } 
    else 
    {
      currentLedState = LED_IMAGE_SENT_FAILED;
      log_message("Server acknowledgment failed");
    }

    // Signal that the buffer is empty
    xSemaphoreGive(empty_sem);
  }
}

// Function to get acknowledgment from server
bool get_server_ack() {
  HTTPClient http;
  http.begin(String(serverUrl) + "/request_send");
  http.addHeader("X-Device-ID", WiFi.macAddress());
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    if (response == "ready") {
      log_message("Server acknowledged: ready");
      http.end();
      return true;
    }
  }
  log_message("Server ack failed, HTTP code: " + String(httpCode));
  http.end();
  return false;
}

// Function to send image to server
bool send_image_to_server(uint8_t* buffer, size_t size) {
  HTTPClient http;
  http.begin(String(serverUrl) + "/send_image");
  http.addHeader("X-Device-ID", WiFi.macAddress());
  http.addHeader("Content-Type", "image/jpeg");
  int httpCode = http.POST(buffer, size);
  http.end();
  if (httpCode == HTTP_CODE_OK) {
    log_message("Image sent successfully");
    return true;
  } else {
    log_message("Failed to send image, HTTP code: " + String(httpCode));
    return false;
  }
}