# MLX90640 Industrial RS485 - Paper Mill Edition
## ESP32-C6-Touch-LCD-1.9 Industrial Grade

## Overview

This is an **industrial-hardened** version of the MLX90640 thermal camera system designed specifically for harsh paper mill environments. It features enhanced reliability, diagnostics, and fault tolerance beyond the standard version.

### Industrial Features

#### Enhanced Reliability
- **Dual Watchdog System**: Hardware (90s) + Software (30s) watchdog timers
- **Extended Retry Logic**: 5 attempts with exponential backoff + random jitter
- **Data Buffering**: Stores last 10 readings for recovery
- **Input Validation**: Temperature range checking and NaN/Inf filtering
- **Health Monitoring**: Continuous system diagnostics every 10 seconds

#### Paper Mill Specific
- **EMI Immunity**: Conservative I2C speeds (400kHz) for noise reduction
- **Low Baud Rate**: 9600 bps RS485 for maximum distance and reliability
- **Power Management**: Brownout detection and graceful shutdown
- **Extended Timeout**: 2-second Modbus timeout for noisy environments
- **Automatic Recovery**: Self-healing from transient failures

#### Advanced Diagnostics
- **Sensor Health**: 0-100% health indicator based on error rate
- **Communication Quality**: Real-time RS485 link quality monitoring
- **Success Rate Tracking**: Percentage of successful transmissions
- **Uptime Counter**: Hours of continuous operation
- **Error Classification**: Separate counters for sensor vs. communication errors

#### Safety Features
- **Critical Error Handler**: Automatic restart after 20 consecutive errors
- **Safe Mode**: Displays error and waits before restart
- **Graceful Degradation**: Continues operation with WiFi failure
- **Data Integrity**: Checksum validation (reserved for future use)

## Hardware Requirements

### Industrial Components

1. **ESP32-C6-Touch-LCD-1.9** - Main controller
2. **MLX90640 Thermal Camera** - Industrial temperature grade (-40°C to +300°C)
3. **RS485 Converter** - **Isolated** MAX485 or similar (ADM2587E recommended)
4. **Delta PLC** - Any Modbus RTU compatible PLC
5. **Power Supply** - Industrial 24VDC to 5VDC converter
6. **Protection Components**:
   - **TVS Diodes**: SMAJ5.0CA for RS485 A/B lines (±15kV ESD)
   - **Ferrite Beads**: On RS485 twisted pair
   - **Shielded Cable**: CAT5e STP or CAT6 STP
   - **Termination**: 120Ω resistors (2 pieces, 1/4W)

### Pin Connections

#### MLX90640 (I2C)
```
MLX90640    →   ESP32-C6
─────────────────────────
VDD         →   3.3V
GND         →   GND
SDA         →   GPIO 18
SCL         →   GPIO 8
```

#### RS485 (UART1) - **With Isolation**
```
Isolated RS485  →   ESP32-C6
────────────────────────────
VCC_ISO         →   5V (isolated side)
GND_ISO         →   GND (isolated side)
VCC_MCU         →   3.3V or 5V (MCU side)
GND_MCU         →   GND (MCU side)
DI/TXD          →   GPIO 2 (TX)
RO/RXD          →   GPIO 3 (RX)
DE              →   GPIO 10
RE              →   GPIO 10
```

#### RS485 Bus - **Industrial Wiring**
```
                ┌─ 680Ω to +5V (bias)
RS485 Module    │
───────────┬────┴────┬───────────
           │    A    │
          120Ω      120Ω  (termination)
           │    B    │
           └────┬────┘
                └─ 680Ω to GND (bias)
                
To PLC:
  A → SD+ (or A+ or RXD+)
  B → SD- (or B- or RXD-)
  Shield → Earth ground (one end only)
```

### Critical Wiring Notes for Paper Mills

1. **Isolation is Mandatory**: Use galvanic isolated RS485 transceiver
   - Prevents ground loops from motors/pumps
   - Protects against voltage spikes
   - Recommended: ADM2587E, ISO1176, or similar

2. **Cable Routing**:
   - Run RS485 in metal conduit separate from AC power
   - Minimum 30cm separation from high-voltage lines
   - Use fiber-optic coupling if near VFDs

3. **Grounding Strategy**:
   - Single-point ground for RS485 shield (at PLC end)
   - Earth ground all metal enclosures
   - Isolated RS485 module grounds connect to local references only

4. **EMI Protection**:
   - Install ferrite beads on A/B lines near connectors
   - Add 10Ω series resistors on A/B before TVS diodes
   - Use metal connectors (M12 IP67 rated)

## Software Configuration

### Modify These Constants (in .ino file)

#### WiFi (Optional)
```cpp
const char* WIFI_SSID = "PaperMill_Network";
const char* WIFI_PASS = "YourSecurePassword";
```

#### RS485 Settings
```cpp
#define RS485_BAUD 9600           // Don't increase unless cable < 100m
#define PLC_SLAVE_ID 1            // Match your PLC address
```

#### Industrial Thresholds
```cpp
#define MAX_CONSECUTIVE_ERRORS 20  // Reboot after this many errors
#define MIN_SUCCESS_RATE 80        // Warning if below this %
#define TEMP_RANGE_MIN -20.0       // Filter out bad readings
#define TEMP_RANGE_MAX 400.0
```

#### Timing (Adjust for Your Environment)
```cpp
#define SENSOR_READ_INTERVAL 1000  // Read every 1s
#define PLC_SEND_INTERVAL 2000     // Send every 2s (can increase for less traffic)
#define HEALTH_CHECK_INTERVAL 10000 // System check every 10s
```

## Delta PLC Configuration

### Modbus RTU Settings (WPLSoft/ISPSoft)

```
Communication Port: COM1 or COM2
Protocol: Modbus RTU
Baud Rate: 9600
Data Bits: 8
Parity: None
Stop Bits: 1
Slave Address: 1 (match PLC_SLAVE_ID)
```

### Extended Register Map (10 Registers)

| Register | Address | Type | Description | Example | Units |
|----------|---------|------|-------------|---------|-------|
| D0 | 40001 | INT16 | Average Temperature × 100 | 2245 | 0.01°C |
| D1 | 40002 | INT16 | Minimum Temperature × 100 | 1850 | 0.01°C |
| D2 | 40003 | INT16 | Maximum Temperature × 100 | 2890 | 0.01°C |
| D3 | 40004 | INT16 | Ambient Temperature × 100 | 2312 | 0.01°C |
| D4 | 40005 | UINT16 | Status (0=OK, 1=Error, 2=Critical) | 0 | - |
| D5 | 40006 | UINT16 | Error Counter | 5 | count |
| D6 | 40007 | UINT16 | Success Counter | 1250 | count |
| D7 | 40008 | UINT16 | Uptime Hours | 48 | hours |
| D8 | 40009 | UINT16 | Sensor Health | 95 | % |
| D9 | 40010 | UINT16 | Communication Quality | 88 | % |

### PLC Ladder Logic Examples

#### Calculate Actual Temperature
```
// Convert D0 (2245) to actual temp (22.45°C)
MOV D0 D100        // Copy raw value
DIV D100 K100 D101 // Divide by 100
// D101 now contains integer part (22)
// For decimal, use: MOD D0 K100 D102 (gives 45)
```

#### Alarm on High Temperature
```
LD  M0             // Normally open
LD> D0 K8000       // If avg temp > 80.00°C
OUT M100           // Activate alarm
```

#### Check System Health
```
LD  M0
LD< D8 K50         // If sensor health < 50%
OR< D9 K50         // OR comm quality < 50%
OUT M101           // Maintenance required indicator
```

#### Monitor Success Rate
```
// Calculate success rate = (D6 / (D5 + D6)) * 100
MOV D5 D200        // Errors
ADD D200 D6 D201   // Total attempts
MUL D6 K100 D202   // Success * 100
DIV D202 D201 D203 // Success rate %

LD  M0
LD< D203 K80       // If success rate < 80%
OUT M102           // Poor communication warning
```

## Installation Guide - Paper Mill Specific

### Step 1: Enclosure Selection

**Requirements**:
- **IP Rating**: Minimum IP65 (IP67 recommended for wet areas)
- **Material**: Polycarbonate or stainless steel 316
- **Size**: Allow 30% free space for heat dissipation
- **Mounting**: DIN rail compatible
- **Ventilation**: If needed, use IP-rated fan with dust filter

**Location**:
- Away from direct steam vents (>2m)
- Ambient temperature: 0°C to 50°C operational
- Avoid direct vibration sources (>10g)
- Height: 1.5m to 2.5m for easy access

### Step 2: Power Supply

**Recommended Setup**:
```
24VDC PLC Power Bus
        ↓
   DC-DC Converter (24V → 5V, 1A minimum)
        ↓
   ESP32-C6 (via USB-C or 5V pin)
```

**Critical**:
- Use **isolated** DC-DC converter
- Add 1000µF capacitor at 5V output
- Fuse 24V input with 0.5A slow-blow fuse
- Include reverse polarity protection diode

### Step 3: RS485 Installation

**Cable Specification**:
- **Type**: CAT5e STP or CAT6 STP
- **Impedance**: 120Ω characteristic
- **Use**: 1 twisted pair for A/B
- **Shield**: Connect at PLC end only
- **Max Length**: 
  - @ 9600 baud: 1200m
  - @ 19200 baud: 600m

**Termination**:
1. Install 120Ω resistor across A-B at **both ends**
2. Add bias resistors (680Ω) if bus has >10 devices:
   - 680Ω from A to +5V
   - 680Ω from B to GND

**Testing**:
1. Measure A-B voltage with multimeter:
   - Idle: ~2-3V differential
   - Active: Square wave visible on oscilloscope
2. Check for reflections (use scope):
   - Clean edges = good termination
   - Ringing = add/adjust terminators

### Step 4: MLX90640 Mounting

**Thermal Considerations**:
- Mount sensor **outside** main enclosure if possible
- Use thermal paste between sensor PCB and mount
- Ensure clear line-of-sight to target (no obstructions)
- Protect from direct moisture (use IP67 window if needed)

**I2C Cable**:
- Keep **as short as possible** (<30cm ideal)
- Use twisted pair
- Add 4.7kΩ pull-ups if cable >50cm
- Route away from power cables

### Step 5: Software Upload

1. Connect ESP32-C6 via USB-C to PC
2. Arduino IDE Settings:
   - Board: "ESP32C6 Dev Module"
   - Upload Speed: 921600
   - USB CDC On Boot: "Enabled"
   - Flash Size: "4MB"
3. Upload sketch
4. Monitor Serial at 115200 baud
5. Verify startup messages

### Step 6: Commissioning

**Initial Power-On**:
1. Watch LCD for initialization sequence
2. Check Serial Monitor output:
   ```
   [OK] MLX90640 initialized
   [OK] RS485 @ 9600 baud
   [OK] System ready
   ```
3. Verify WiFi connection (or see "WiFi Off")
4. Note IP address if WiFi connected

**Modbus Verification**:
1. Use **Modbus Poll** software:
   - Connect USB-RS485 adapter to bus
   - Settings: 9600-8-N-1, Slave ID 1
   - Read Holding Registers 0-9
2. Verify data updates every 2 seconds
3. Check values are reasonable

**PLC Integration**:
1. Configure PLC COM port (as above)
2. Add ladder logic to read D0-D9
3. Monitor for 10 minutes
4. Verify error counter (D5) stays low

## Troubleshooting - Industrial Environment

### Symptom: High Error Rate (>20%)

**Possible Causes**:
1. **EMI from nearby equipment**
   - Solution: Relocate enclosure or add metal shielding
   - Test: Turn off suspected noise source, check if errors drop

2. **Poor cable quality or termination**
   - Solution: Replace with CAT6 STP, verify 120Ω at both ends
   - Test: Measure DC resistance of A-B (should be ~60Ω with terminators)

3. **Ground loop**
   - Solution: Use isolated RS485 module
   - Test: Disconnect shield at one end

4. **Voltage drop on long cable**
   - Solution: Use thicker gauge wire for power
   - Test: Measure 3.3V at ESP32 (should be >3.2V)

### Symptom: Sensor Health < 50%

**Causes**:
1. **I2C communication issues**
   - Check for loose connections
   - Reduce I2C speed to 100kHz
   - Add pull-up resistors

2. **Sensor overheating**
   - Verify ambient temp < 50°C
   - Add ventilation to enclosure
   - Check sensor not in direct steam path

3. **Bad sensor**
   - Replace MLX90640 module

### Symptom: Communication Quality < 50%

**Causes**:
1. **PLC not responding**
   - Verify PLC in RUN mode
   - Check Modbus RTU enabled
   - Confirm slave address matches

2. **Baud rate mismatch**
   - Double-check 9600-8-N-1 on both sides
   - Try different baud rate temporarily

3. **Polarity reversed**
   - Swap A and B wires
   - Verify A goes to SD+ and B to SD-

### Symptom: System Reboots Frequently

**Causes**:
1. **Watchdog timeout**
   - Increase `WATCHDOG_TIMEOUT` to 120s
   - Check Serial for where it hangs

2. **Power supply brownout**
   - Measure 5V under load (should be >4.75V)
   - Upgrade to higher current supply (1.5A)
   - Add bulk capacitor (2200µF)

3. **Memory leak** (unlikely)
   - Monitor free heap in Serial
   - Report to developer

### Symptom: LCD Shows "CRITICAL ERROR"

**Meaning**: >20 consecutive communication failures

**Actions**:
1. System will auto-restart in 10 seconds
2. Check RS485 wiring before restart
3. Verify PLC is powered and in RUN mode
4. Review Serial log for error codes

## Performance Optimization

### For Faster Updates

If you need <1 second update rate:

```cpp
#define SENSOR_READ_INTERVAL 500   // 0.5s
#define PLC_SEND_INTERVAL 1000     // 1s
#define MLX_REFRESH_RATE 0x06      // 8Hz (was 4Hz)
```

**Trade-offs**:
- Higher CPU usage
- More I2C bus traffic
- Slightly increased error rate

### For Maximum Reliability

If errors are high in your environment:

```cpp
#define MAX_RETRIES 10             // More attempts
#define RETRY_MAX_DELAY 5000       // Longer backoff
#define MODBUS_TIMEOUT 5000        // 5s timeout
#define MLX_I2C_SPEED 100000       // Slower I2C
```

**Trade-offs**:
- Slower response to failures
- Lower update rate
- Better success rate

## Maintenance Schedule

### Weekly
- [ ] Check LCD for status
- [ ] Verify error counter not climbing rapidly
- [ ] Inspect enclosure for moisture/dust

### Monthly
- [ ] Clean MLX90640 lens with soft cloth
- [ ] Check all cable connections for tightness
- [ ] Review PLC logs for trends
- [ ] Verify success rate >90%

### Quarterly
- [ ] Inspect RS485 cable for damage
- [ ] Test termination resistors with multimeter
- [ ] Check power supply voltage under load
- [ ] Update firmware if available

### Annually
- [ ] Replace RS485 cable if any corrosion
- [ ] Calibrate MLX90640 (if procedure available)
- [ ] Full system backup and documentation review

## Advanced Features

### Remote Monitoring (if WiFi enabled)

**Future Enhancement Ideas**:
1. Add web server to view data
2. MQTT publishing for cloud integration
3. Email/SMS alerts on critical errors
4. OTA firmware updates

### Data Logging

**Current Buffering**: Last 10 readings in RAM

**Expansion Options**:
1. Add microSD card module
2. Log to SPIFFS flash (4MB available)
3. Implement circular buffer
4. CSV format for easy analysis

### Multiple Sensors

To monitor multiple points:

1. Use different I2C addresses (if supported)
2. Or use TCA9548A I2C multiplexer
3. Modify code to loop through sensors
4. Allocate more PLC registers

## Safety & Compliance

### Electrical Safety
- [ ] All metal parts grounded to earth
- [ ] Fuse protection on 24V input
- [ ] Overcurrent protection on 5V rail
- [ ] Reverse polarity protection

### EMC Compliance
- [ ] Shielded cables used throughout
- [ ] Ferrite beads installed
- [ ] Metal enclosure grounded
- [ ] Meets IEC 61000-6-2 (Industrial Immunity)

### Environmental
- [ ] IP65+ rated enclosure
- [ ] Operating temp: 0°C to 50°C
- [ ] Storage temp: -20°C to 70°C
- [ ] Humidity: 10% to 90% non-condensing

## Technical Support

### Diagnostic Information to Collect

When reporting issues, provide:

1. **Serial Monitor Log** (at 115200 baud):
   - Full boot sequence
   - Error messages with timestamps
   - Continuous 5-minute capture

2. **LCD Photo**: Shows status and metrics

3. **Environment Info**:
   - Temperature/humidity at installation
   - Nearby equipment (motors, VFDs, etc.)
   - Cable lengths and routing

4. **Configuration**:
   - Baud rate, PLC model
   - Any code modifications
   - Power supply specs

### Common Questions

**Q: Can I use this with ABB/Siemens/Mitsubishi PLC?**
A: Yes! Any PLC with Modbus RTU slave support works. Just match the baud rate and slave ID.

**Q: Maximum cable length?**
A: At 9600 baud with CAT6 STP: up to 1200m. Use repeaters for longer distances.

**Q: Can I monitor multiple MLX90640 sensors?**
A: Yes, but requires code modification to use I2C multiplexer or different addresses.

**Q: Is galvanic isolation required?**
A: Highly recommended in industrial environments to prevent ground loops and protect from surges.

**Q: Can I use Ethernet instead of RS485?**
A: Possible with Modbus TCP, but requires significant code changes. RS485 is more reliable in noisy environments.

---

**Document Version**: 2.0 Industrial  
**Last Updated**: 2024  
**Hardware**: ESP32-C6-Touch-LCD-1.9  
**Application**: Paper Mill Thermal Monitoring  
**Safety Rating**: Industrial Grade  
