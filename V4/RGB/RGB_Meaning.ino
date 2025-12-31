// ---------- Meaning-based functions (recommended standard) ----------

// System OK (connected and sending)
void ledStatusOk() {
  setLedSolid(C_GREEN, 40);
}

// Working but not sending every second (optional)
void ledStatusIdleOk() {
  setLedBreath(C_GREEN, 2000, 50);
}

// Trying WiFi
void ledStatusWiFiConnecting() {
  setLedBlink(C_BLUE, 800, 800, 40); // slow blue blink
}

// Connected WiFi / doing hello / handshake
void ledStatusHandshake() {
  setLedBlink(C_CYAN, 300, 700, 45); // cyan blink
}

// ESP-NOW fallback active
void ledStatusEspNowFallback() {
  setLedBlink(C_PURPLE, 600, 600, 45);
}

// Buffering data (cannot deliver but still reading)
void ledStatusBuffering() {
  setLedBlink(C_YELLOW, 250, 250, 50); // medium warning
}

// Sensor error (bad reading, disconnected)
void ledStatusSensorError() {
  setLedBlink(C_ORANGE, 120, 120, 60); // fast orange
}

// Critical alarm (smoke/gas threshold)
void ledStatusAlarmCritical() {
  setLedBlink(C_RED, 100, 100, 80); // fast red
}

// Stopped / requires attention
void ledStatusStopped() {
  setLedSolid(C_RED, 80);
}

// Successful send "tick" pulse
void ledPulseSent() {
  setLedPulse(C_WHITE, 70, 90);
}

// OTA window open / ready
void ledStatusOtaWindow() {
  setLedDoubleBlink(C_MAGENTA, 120, 120, 1400, 70);
}
