# ESP32 DHT22 Monitoring System V2

A robust, infrastructure-grade ESP32-based temperature and humidity monitoring system with WiFi connectivity, HTTP data transmission, and comprehensive error handling. Designed for industrial and infrastructure applications with MISRA-C compliance.

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [Installation & Setup](#installation--setup)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Error Handling & Recovery](#error-handling--recovery)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## 🎯 Overview

This project implements a complete IoT monitoring solution using ESP32-S3 with DHT22 temperature and humidity sensors. The system provides:

- **Real-time sensor data collection** from DHT22 sensors
- **Robust WiFi connectivity** with automatic reconnection
- **HTTP-based data transmission** to remote servers
- **Comprehensive error handling** with automatic recovery
- **Infrastructure-grade reliability** with watchdog timers and restart mechanisms

## ✨ Features

### Core Functionality
- 🌡️ **DHT22 Temperature & Humidity Monitoring**
- 📡 **WiFi Connectivity** with auto-reconnection
- 🌐 **HTTP Data Transmission** with JSON payloads
- 🔄 **Automatic Error Recovery** and system restart
- 📊 **Real-time Status Monitoring**

### Reliability Features
- 🛡️ **MISRA-C Compliant** code for infrastructure use
- ⏰ **Watchdog Timers** for system health monitoring
- 🔁 **Automatic Restart** on critical failures
- 📈 **Connection Quality Monitoring** (RSSI tracking)
- 🚨 **Comprehensive Error Logging**

### Advanced Features
- 🔧 **Configurable Parameters** via Kconfig
- 📱 **Device Identification** via MAC address
- 🎛️ **Power Management** optimization
- 🔒 **Secure HTTP Communication**
- 📋 **Structured JSON Data Format**

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 Device                         │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   DHT22     │  │   WiFi      │  │   HTTP      │        │
│  │   Driver    │  │ Framework   │  │   Client    │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
│         │                │                │               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   Sensor    │  │   Network   │  │   Data      │        │
│  │   Data      │  │   Stack     │  │   Sender    │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
├─────────────────────────────────────────────────────────────┤
│                    Main Application                         │
│              (wifi_framework_example.c)                    │
└─────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────┐
│                    Remote Server                            │
│              (HTTP Endpoint)                               │
└─────────────────────────────────────────────────────────────┘
```

### Component Overview

1. **DHT22 Driver** (`dht.c/h`): Robust sensor communication with timing-critical operations
2. **WiFi Framework** (`wifi_framework.c/h`): Complete WiFi management with state machine
3. **HTTP Client** (`http_client.c/h`): Reliable HTTP communication with retry logic
4. **Data Sender** (`data_sender.c/h`): JSON data formatting and transmission
5. **Main Application** (`wifi_framework_example.c`): System orchestration and monitoring

## 🔧 Hardware Requirements

### ESP32 Development Board
- **ESP32-S3** (recommended) or ESP32
- **Flash Memory**: 2MB minimum
- **PSRAM**: 2MB (optional, for better performance)
- **GPIO**: Available GPIO pin for DHT22 sensor

### DHT22 Sensor
- **Temperature Range**: -40°C to +80°C
- **Humidity Range**: 0% to 100% RH
- **Accuracy**: ±0.5°C, ±2% RH
- **Pull-up Resistor**: 4.7kΩ to 3.3V

### Wiring Diagram
```
ESP32-S3          DHT22
─────────         ─────
GPIO4    ──────── Data
3.3V     ──────── VCC
GND      ──────── GND
                 │
               4.7kΩ
                 │
               3.3V
```

## 💻 Software Requirements

- **ESP-IDF**: v5.4 (required)
- **Python**: 3.11+ (for ESP-IDF tools)
- **CMake**: 3.16+
- **Git**: For version control

## 📁 Project Structure

```
V2/
├── CMakeLists.txt                 # Main CMake configuration
├── sdkconfig                      # ESP-IDF configuration
├── sdkconfig.defaults             # Default configuration
├── dependencies.lock              # Component dependencies
├── README.md                      # This file
│
├── main/                          # Main application source
│   ├── CMakeLists.txt            # Main component CMake
│   ├── idf_component.yml         # Component dependencies
│   ├── Kconfig.projbuild         # Configuration options
│   │
│   ├── wifi_framework_example.c  # Main application
│   ├── wifi_framework.c          # WiFi framework implementation
│   ├── wifi_framework.h          # WiFi framework interface
│   ├── dht.c                     # DHT22 sensor driver
│   ├── dht.h                     # DHT22 sensor interface
│   ├── http_client.c             # HTTP client implementation
│   ├── http_client.h             # HTTP client interface
│   ├── data_sender.c             # Data transmission logic
│   └── data_sender.h             # Data transmission interface
│
└── managed_components/            # ESP-IDF managed components
    └── espressif__led_strip/     # LED strip component (optional)
```

## ⚙️ Configuration

### WiFi Configuration
Edit `main/wifi_framework_example.c`:

```c
/* WiFi Configuration - CHANGE THESE FOR YOUR NETWORK */
#define WIFI_SSID     "YourWiFiNetwork"
#define WIFI_PASSWORD "YourWiFiPassword"
```

### Server Configuration
```c
/* HTTP Client Configuration - CHANGE THESE FOR YOUR SERVER */
#define SERVER_URL "http://your-server.com:port/"
#define AUTH_TOKEN "your-auth-token"  // Set to NULL if no auth needed
```

### Sensor Configuration
```c
/* DHT22 Sensor Configuration - CHANGE GPIO PIN AS NEEDED */
#define DHT22_GPIO_PIN GPIO_NUM_4  // Change this to your GPIO pin
```

### Error Handling Configuration
```c
/* Error Handling Configuration */
#define MAX_WIFI_CONNECTION_FAILURES 5U    /* WiFi failures before restart */
#define MAX_HTTP_SEND_FAILURES 10U         /* HTTP failures before restart */
#define WIFI_RECONNECTION_TIMEOUT_MS 5000U /* 5 seconds reconnection timeout */
```

## 🚀 Installation & Setup

### 1. Prerequisites
```bash
# Install ESP-IDF v5.4
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.4
./install.sh esp32s3
. ./export.sh
```

### 2. Clone Project
```bash
git clone <repository-url>
cd DHT22_Modules/V2
```

### 3. Configure Project
```bash
# Set target chip
idf.py set-target esp32s3

# Configure project (optional)
idf.py menuconfig
```

### 4. Build and Flash
```bash
# Build project
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

### 5. Monitor Output
```bash
# Monitor serial output
idf.py monitor
```

## 📖 Usage

### Basic Operation

1. **Power On**: Device initializes and connects to WiFi
2. **Sensor Reading**: DHT22 data is read every 5 seconds
3. **Data Transmission**: JSON data is sent to configured server
4. **Status Monitoring**: System health is continuously monitored

### Expected Log Output
```
I (396) WiFi_Framework: Starting WiFi Framework Example with HTTP Client
I (396) Main: Device MAC Address: B4:3A:45:3F:1A:90
I (426) WiFi_Framework: WiFi Configuration:
I (426) WiFi_Framework:   SSID: YourWiFiNetwork
I (426) WiFi_Framework:   Max Retries: 10
I (426) WiFi_Framework:   Connection Timeout: 60000 ms
I (4966) WiFi_Framework: DHT22 test reading successful: Temp=25.6°C, Humidity=58.2%
I (4966) Data_Sender: Data sender initialized for server: http://your-server.com:port/
I (15026) WiFi_Framework: DHT22 Read: Temp=25.6°C, Humidity=58.1%
I (15036) Data_Sender: DHT22 data: {"device_id":"B4:3A:45:3F:1A:90",...}
I (15036) Data_Sender: DHT22 data sent successfully, HTTP status: 200
```

### Data Format
The system sends JSON data in the following format:

```json
{
  "device_id": "B4:3A:45:3F:1A:90",
  "sensor_type": "DHT22",
  "data": {
    "temperature": 25.6,
    "humidity": 58.1,
    "timestamp": 1640995200000,
    "type": "dht22_data"
  }
}
```

## 📚 API Reference

### WiFi Framework API

```c
// Initialize WiFi framework
esp_err_t wifi_framework_init(const wifi_framework_config_t* config, 
                             wifi_framework_event_callback_t event_callback, 
                             void* user_data);

// Connect to WiFi
esp_err_t wifi_framework_connect(void);

// Get connection status
bool wifi_framework_is_connected(void);
bool wifi_framework_has_ip(void);

// Get detailed status
esp_err_t wifi_framework_get_status(wifi_framework_status_t* status);
```

### DHT22 Driver API

```c
// Initialize DHT22 sensor
esp_err_t dht_init(gpio_num_t gpio_num, dht_type_t type);

// Read sensor data
esp_err_t dht_read(float *temperature, float *humidity);

// Get last reading
esp_err_t dht_get_last_reading(float *temperature, float *humidity);
```

### HTTP Client API

```c
// Initialize HTTP client
esp_err_t http_client_init(const http_client_config_t* config);

// Send JSON data
esp_err_t http_client_send_json(const char* json_data, http_response_t* response);
```

### Data Sender API

```c
// Initialize data sender
esp_err_t data_sender_init(const char* server_url, const char* auth_token);

// Send DHT22 data
esp_err_t data_sender_send_dht22_data(const char* device_id, const char* sensor_type, 
                                     float temperature, float humidity, uint64_t timestamp);

// Send status update
esp_err_t data_sender_send_status(const char* device_id, const char* status);
```

## 🛡️ Error Handling & Recovery

### Automatic Recovery Mechanisms

1. **WiFi Connection Failures**
   - Automatic reconnection attempts
   - System restart after 5 consecutive failures
   - Timeout-based restart after 5 seconds of reconnection attempts

2. **HTTP Transmission Failures**
   - Retry logic with connection reset
   - System restart after 10 consecutive failures
   - Keep-alive connections for better reliability

3. **Sensor Communication Issues**
   - Automatic retry with proper timing
   - Error status reporting to server
   - Graceful degradation on sensor failures

### Error Codes and Responses

| Error Type | Action | Recovery |
|------------|--------|----------|
| WiFi Connection Failed | Retry connection | Auto-reconnect |
| WiFi Reconnection Timeout | Restart ESP32 | Full system restart |
| HTTP Send Failed | Retry with new connection | Connection reset |
| Sensor Read Failed | Report error status | Continue operation |
| Multiple HTTP Failures | Restart ESP32 | Full system restart |

## 🔧 Troubleshooting

### Common Issues

#### 1. WiFi Connection Problems
**Symptoms**: Device fails to connect or frequently disconnects
**Solutions**:
- Check WiFi credentials
- Verify signal strength (RSSI should be > -70 dBm)
- Ensure stable power supply
- Check router connection limits

#### 2. HTTP Transmission Failures
**Symptoms**: Data not reaching server
**Solutions**:
- Verify server URL and port
- Check network connectivity
- Verify server is accepting connections
- Check authentication token

#### 3. Sensor Reading Issues
**Symptoms**: Invalid or zero sensor readings
**Solutions**:
- Check wiring connections
- Verify 4.7kΩ pull-up resistor
- Ensure stable 3.3V power supply
- Check GPIO pin configuration

#### 4. System Restart Loops
**Symptoms**: Device continuously restarting
**Solutions**:
- Check power supply stability
- Verify WiFi signal strength
- Review error logs for specific failure causes
- Adjust timeout and retry parameters

### Debug Commands

```bash
# Monitor with verbose output
idf.py monitor --print_filter="*:DEBUG"

# Check WiFi status
idf.py monitor --print_filter="WiFi_Framework:*"

# Monitor HTTP traffic
idf.py monitor --print_filter="HTTP_Client:*"
```

### Log Analysis

Key log patterns to monitor:
- `WiFi connected event received` - Successful connection
- `DHT22 data sent successfully` - Successful data transmission
- `WiFi reconnection timeout` - Connection issues
- `Failed to send DHT22 data` - Transmission problems
- `Restarting ESP32 due to` - System recovery actions

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Guidelines

- Follow MISRA-C guidelines for infrastructure code
- Add comprehensive error handling
- Include detailed logging
- Update documentation for new features
- Test thoroughly on hardware

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 📞 Support

For support and questions:
- Create an issue in the repository
- Check the troubleshooting section
- Review the ESP-IDF documentation
- Consult the ESP32 community forums

---

**Version**: 2.0.0  
**Last Updated**: 2024  
**ESP-IDF Compatibility**: v5.4  
**Target Hardware**: ESP32-S3 (recommended), ESP32