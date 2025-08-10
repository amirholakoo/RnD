
#### ESP32 RGB LED Monitoring
- **LED States and Colors**:
  - **Idle**: Black (off)
  - **Connecting to WiFi**: Blinking Yellow
  - **WiFi Connected**: Solid Green
  - **WiFi Failed**: Solid Red
  - **Acquiring Image**: Solid Blue
  - **Image Acquire Failed**: Blinking Red
  - **Image Saved**: Solid Cyan
  - **Waiting for Server Ack**: Solid Magenta
  - **Server Ack Received**: Solid White
  - **Sending Image**: Solid Orange
  - **Image Sent Success**: Solid Green
  - **Image Sent Failed**: Blinking Red
- **LED Update Task**:
  - A dedicated FreeRTOS task (`ledUpdateTask`) updates the LED every 500ms based on the `currentLedState`, ensuring no repeated `updateLed` calls in the main code.
  - Blinking states toggle every 500ms for visibility.

#### Server Logging
- **Logged Data**:
  - Device ID (from `X-Device-ID` header)
  - Timestamp (automatically added by `logging`)
  - Request path (e.g., `/request_send`, `/send_image`)
  - Response details (e.g., "ready", "Image received")
  - Status code (e.g., 200, 400, 500)
  - Image size and saved filename (for `/send_image`)
  - Errors (if any, with warning or error level)
- **Format**:
  - Neatly structured in `server.log` with timestamps, e.g.:
    ```
    2023-10-10 12:34:56,789 - Request from AA:BB:CC:DD:EE:FF: path=/request_send, response=ready, status=200
    2023-10-10 12:34:57,123 - Image received from AA:BB:CC:DD:EE:FF: path=/send_image, size=102400 bytes, saved as images/AA_BB_CC_DD_EE_FF/image_20231010_123457_123456.jpg, response=Image received, status=200
    ```

---

### How to Use
1. **ESP32**:
   - Upload the code to your ESP32 with the FastLED library installed.
   - Connect an RGB LED (e.g., WS2812B) to GPIO48.
   - Update WiFi credentials and server URL as needed.
   - The LED will visually indicate the system’s state during operation.
2. **Server**:
   - Run the Python script on a machine accessible to the ESP32.
   - Check `server.log` for detailed records of all interactions.
