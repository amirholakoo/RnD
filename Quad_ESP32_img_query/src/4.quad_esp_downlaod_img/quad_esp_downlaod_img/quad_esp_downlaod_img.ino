#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#include <FastLED.h>


// WiFi credentials (update as needed)
const char* ssid = "RnD";
const char* password = "wnOPxFSCxb";

// Server and image URLs
const char* serverUrl = "http://192.168.10.100:5000";


const char* imageUrl = "https://fastly.picsum.photos/id/682/800/600.jpg?hmac=nDvj6j28PV7_q1jWXRsp0xS7jtAZYzHophmak9J1ymU";
// const char* imageUrl = "https://images.unsplash.com/photo-1507525428034-b723cf961d3e?ixlib=rb-4.0.3&auto=format&fit=crop&w=800&q=80";
// const char* imageUrl = "https://images.pexels.com/photos/356844/pexels-photo-356844.jpeg?auto=compress&cs=tinysrgb&w=800";
// const char* imageUrl = "https://images.unsplash.com/photo-1543466835-00a7907e9de1?ixlib=rb-4.0.3&auto=format&fit=crop&w=800&q=80";

// Image buffer size (1MB, well within 2MB PSRAM limit)
#define IMAGE_SIZE (1024 * 1024)
uint8_t* image_buffer;
size_t image_size = 0; // Actual size of the downloaded/captured image

// Semaphores for task synchronization
SemaphoreHandle_t image_ready_sem;
SemaphoreHandle_t sending_complete_sem;


// Function to download image from internet (for testing)
bool download_image(uint8_t* buffer, size_t max_size, size_t* out_size) {
  HTTPClient http;
  http.begin(imageUrl);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    size_t len = http.getSize();
    if (len > max_size) {
      Serial.println("Image too large for buffer");
      http.end();
      return false;
    }
    http.getStream().readBytes(buffer, len);
    *out_size = len;
    Serial.println("Image downloaded successfully");
    http.end();
    return true;
  } else {
    Serial.printf("Failed to download image, HTTP code: %d\n", httpCode);
    http.end();
    return false;
  }
}

// Placeholder for camera capture (to be implemented later)
bool capture_image(uint8_t* buffer, size_t max_size, size_t* out_size) {
  Serial.println("Camera capture not implemented yet");
  return false;
}


// Function pointer for image acquisition
typedef bool (*acquire_image_func_t)(uint8_t* buffer, size_t max_size, size_t* out_size);
acquire_image_func_t acquire_image = download_image; // For testing; switch to capture_image later

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
  LED_IDLE,              // Black (off)
  LED_CONNECTING_WIFI,   // Blinking Yellow
  LED_WIFI_CONNECTED,    // Solid Green
  LED_WIFI_FAILED,       // Solid Red
  LED_ACQUIRING_IMAGE,   // Solid Blue
  LED_IMAGE_ACQUIRE_FAILED, // Blinking Red
  LED_IMAGE_SAVED,       // Solid Cyan
  LED_WAITING_SERVER_ACK, // Solid Magenta
  LED_SERVER_ACK_RECEIVED, // Solid White
  LED_SENDING_IMAGE,     // Solid Orange
  LED_IMAGE_SENT_SUCCESS, // Solid Green
  LED_IMAGE_SENT_FAILED  // Blinking Red
};
LedState currentLedState = LED_IDLE;

void setup() {
  Serial.begin(115200);
  delay(1000);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  // Create LED update task
  xTaskCreate(ledUpdateTask, "LED_UPDATE_TASK", 2048, NULL, 1, &ledUpdateTaskHandle);

  // Connect to WiFi with retries
  currentLedState = LED_CONNECTING_WIFI;

  // Define timeout and retry parameters
  const unsigned long connectionTimeout = 10000; // 10 seconds timeout per attempt
  const int maxRetries = 5; 
  bool wifi_success = false;
  WiFi.mode(WIFI_STA); // Set ESP32 as a WiFi station
    delay(500);          // Optional: give time for mode to set

    int retryCount = 0;
    while (retryCount < maxRetries) {
        Serial.print("Attempting to connect to WiFi, attempt ");
        Serial.println(retryCount + 1);

        WiFi.begin(ssid, password); // Start connection attempt

        unsigned long startTime = millis();
        while (millis() - startTime < connectionTimeout) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("Connected to WiFi successfully!");
                Serial.print("IP Address: ");
                Serial.println(WiFi.localIP());
                WiFi.setAutoReconnect(true); // Enable auto-reconnect if connection drops
                wifi_success = true;
                break;
            }
            delay(100); // Check every 100ms (non-blocking)
        }
        if(wifi_success) break;

        Serial.println("Connection attempt timed out");
        WiFi.disconnect(); // Reset connection state
        retryCount++;
        delay(500); // Short delay before retrying
    }

  if (WiFi.status() == WL_CONNECTED) {
    currentLedState = LED_WIFI_CONNECTED;
    Serial.println("Connected to WiFi");
  } else {
    currentLedState = LED_WIFI_FAILED;
    Serial.println("Failed to connect to WiFi");
    while (1); // Halt or handle failure
  }

  // Allocate image buffer in PSRAM
  image_buffer = (uint8_t*)heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_SPIRAM);
  if (image_buffer == NULL) {
    Serial.println("Failed to allocate PSRAM buffer");
    while (1);
  }

  // Create semaphores
  image_ready_sem = xSemaphoreCreateBinary();
  sending_complete_sem = xSemaphoreCreateBinary();

  // Create tasks
  xTaskCreate(get_image_task, "GET_IMAGE_TASK", 4096, NULL, 1, &getImageTaskHandle);
  xTaskCreate(image_send_task, "IMAGE_SEND_TASK", 4096, NULL, 1, &imageSendTaskHandle);
}

void loop() {
  delay(1000); // Main loop is empty; tasks handle the work
}

// LED Update Task
void ledUpdateTask(void *parameter) {
  while (true) {
    updateLed();
    vTaskDelay(500 / portTICK_PERIOD_MS); // Update every 500ms for visible blinking
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

// Task to acquire and save image to PSRAM
void get_image_task(void* pvParameters) {
  while (true) {
    currentLedState = LED_ACQUIRING_IMAGE;
    bool success = acquire_image(image_buffer, IMAGE_SIZE, &image_size);
    if (success) {
      currentLedState = LED_IMAGE_SAVED;
      Serial.println("Image acquired and saved to PSRAM");
      xSemaphoreGive(image_ready_sem);
      xSemaphoreTake(sending_complete_sem, portMAX_DELAY);
    } else {
      currentLedState = LED_IMAGE_ACQUIRE_FAILED;
      Serial.println("Failed to acquire image; retrying in 1s");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

// Task to send image to server
void image_send_task(void* pvParameters) {
  while (true) {
    xSemaphoreTake(image_ready_sem, portMAX_DELAY);
    currentLedState = LED_WAITING_SERVER_ACK;
    if (get_server_ack()) {
      currentLedState = LED_SERVER_ACK_RECEIVED;
      currentLedState = LED_SENDING_IMAGE;
      if (send_image_to_server()) {
        currentLedState = LED_IMAGE_SENT_SUCCESS;
      } else {
        currentLedState = LED_IMAGE_SENT_FAILED;
        Serial.println("Failed to send image");
      }
    } else {
      currentLedState = LED_IMAGE_SENT_FAILED;
      Serial.println("Server acknowledgment failed");
    }
    xSemaphoreGive(sending_complete_sem);
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
      Serial.println("Server acknowledged: ready");
      http.end();
      return true;
    }
  }
  Serial.printf("Server ack failed, HTTP code: %d\n", httpCode);
  http.end();
  return false;
}

// Function to send image to server
bool send_image_to_server() {
  HTTPClient http;
  http.begin(String(serverUrl) + "/send_image");
  http.addHeader("X-Device-ID", WiFi.macAddress());
  http.addHeader("Content-Type", "image/jpeg");
  int httpCode = http.POST(image_buffer, image_size);
  http.end();
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Image sent successfully");
    return true;
  } else {
    Serial.printf("Failed to send image, HTTP code: %d\n", httpCode);
    return false;
  }
}