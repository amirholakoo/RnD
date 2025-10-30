#include <Arduino.h>
#include <ESP32QRCodeReader.h>


#define CAMERA_MODEL_CUSTOM \
  {                             \
    .PWDN_GPIO_NUM = -1,        \
    .RESET_GPIO_NUM = -1,       \
    .XCLK_GPIO_NUM = 15,         \
    .SIOD_GPIO_NUM = 4,        \
    .SIOC_GPIO_NUM = 6,        \
    .Y9_GPIO_NUM = 12,          \
    .Y8_GPIO_NUM = 16,          \
    .Y7_GPIO_NUM = 11,          \
    .Y6_GPIO_NUM = 17,          \
    .Y5_GPIO_NUM = 10,          \
    .Y4_GPIO_NUM = 18,          \
    .Y3_GPIO_NUM = 9,          \
    .Y2_GPIO_NUM = 8,           \
    .VSYNC_GPIO_NUM = 5,       \
    .HREF_GPIO_NUM = 7,        \
    .PCLK_GPIO_NUM = 13,        \
  }  
ESP32QRCodeReader reader(CAMERA_MODEL_CUSTOM);


void onQrCodeTask(void *pvParameters) {
  struct QRCodeData qrCodeData;

  while (true) {
    if (reader.receiveQrCode(&qrCodeData, 100)) {
      Serial.println("Scanned new QRCode");
      if (qrCodeData.valid) {
        Serial.print("Valid payload: ");
        Serial.println((const char *)qrCodeData.payload);
      }
      else {
        Serial.print("Invalid payload: ");
        Serial.println((const char *)qrCodeData.payload);
      }
    }

    Serial.println("no query");
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  reader.setup();
  Serial.println("Setup QRCode Reader");

  reader.beginOnCore(1);
  Serial.println("Begin on Core 1");

  xTaskCreate(onQrCodeTask, "onQrCode", 4 * 1024, NULL, 4, NULL);
}

void loop() {
  delay(100);
}