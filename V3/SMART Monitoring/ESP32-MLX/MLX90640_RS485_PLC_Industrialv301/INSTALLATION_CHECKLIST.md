# Industrial Installation Checklist
## MLX90640 RS485 System - Paper Mill Deployment

## Pre-Installation (Office/Workshop)

### Hardware Verification
- [ ] ESP32-C6-Touch-LCD-1.9 board received and inspected
- [ ] MLX90640 thermal camera tested (reads reasonable temps)
- [ ] Isolated RS485 module confirmed (ADM2587E or equivalent)
- [ ] Delta PLC model and COM port identified
- [ ] All cables and connectors inventoried
- [ ] TVS diodes and protection components acquired
- [ ] Termination resistors (2× 120Ω, 1/4W) obtained
- [ ] IP65+ enclosure selected and dimensioned
- [ ] 24VDC to 5VDC converter specified (1A min)

### Software Preparation
- [ ] Arduino IDE installed with ESP32 board support
- [ ] Required libraries installed:
  - [ ] Arduino_GFX_Library
  - [ ] ModbusMaster
- [ ] Sketch uploaded successfully
- [ ] Serial monitor verified at 115200 baud
- [ ] LCD displays startup screen
- [ ] WiFi credentials configured (optional)
- [ ] Modbus parameters match PLC:
  - [ ] Baud rate: 9600
  - [ ] Slave ID: 1 (or site-specific)
  - [ ] Data format: 8-N-1

### Bench Testing
- [ ] Power ESP32-C6 from USB
- [ ] MLX90640 reads room temperature correctly
- [ ] LCD shows all metrics
- [ ] Connect RS485 to USB adapter
- [ ] Use Modbus Poll to read registers
- [ ] Verify data updates every 2 seconds
- [ ] Test retry logic (disconnect RS485, watch recovery)
- [ ] Run for 1 hour continuously
- [ ] Check error counter remains low (<5%)
- [ ] Document baseline performance

### Documentation
- [ ] Print this checklist
- [ ] Print wiring diagram
- [ ] Print PLC register map
- [ ] Prepare site survey form
- [ ] Bring camera for documentation
- [ ] Prepare tool list (see below)

## Tools & Materials Required

### Hand Tools
- [ ] Screwdriver set (Phillips & flat)
- [ ] Wire strippers (24-16 AWG)
- [ ] Wire crimpers
- [ ] Multimeter (DMM)
- [ ] Cable tester
- [ ] Drill with bits (for enclosure mounting)
- [ ] Label maker
- [ ] Zip ties
- [ ] Cable glands (M16 or M20)

### Test Equipment
- [ ] Laptop with Arduino IDE
- [ ] USB-C cable (for ESP32-C6)
- [ ] USB-RS485 adapter
- [ ] Modbus Poll software installed
- [ ] Oscilloscope (optional but recommended)

### Safety Equipment
- [ ] Safety glasses
- [ ] Insulated gloves
- [ ] Lockout/tagout kit
- [ ] Voltage detector
- [ ] First aid kit

### Consumables
- [ ] CAT6 STP cable (measure site, add 20%)
- [ ] RJ45 connectors (if using CAT6)
- [ ] Heat shrink tubing
- [ ] Electrical tape
- [ ] Wire ferrules (if required)
- [ ] Silicone sealant
- [ ] Thermal paste (for sensor mounting)
- [ ] Thread locker (blue, for screws)

## Site Survey

### Location Assessment
- [ ] Identify mounting location:
  - [ ] Temperature range: _____ °C to _____ °C
  - [ ] Humidity level: _____ %RH (estimate)
  - [ ] Vibration level: Low / Medium / High
  - [ ] Dust/particulate: Low / Medium / High
  - [ ] Height above floor: _____ m
  - [ ] Distance to PLC: _____ m
- [ ] Mark mounting points (4 corners)
- [ ] Check structural integrity of mount surface
- [ ] Verify clearance for enclosure door opening
- [ ] Identify nearest power source:
  - [ ] Voltage: _____ VDC or VAC
  - [ ] Distance: _____ m
  - [ ] Overcurrent protection available: Y / N

### Cable Routing
- [ ] Measure cable runs:
  - [ ] Power: _____ m
  - [ ] RS485 to PLC: _____ m
  - [ ] I2C to sensor: _____ cm
- [ ] Identify cable pathways:
  - [ ] Existing conduit available: Y / N
  - [ ] New conduit required: Y / N
  - [ ] Cable tray available: Y / N
- [ ] Check for EMI sources along route:
  - [ ] High-voltage cables: Distance _____ m
  - [ ] Motors/pumps: Distance _____ m
  - [ ] VFD drives: Distance _____ m
  - [ ] Welding equipment: Distance _____ m
- [ ] Plan to maintain 30cm minimum separation
- [ ] Identify grounding points

### PLC Interface
- [ ] PLC model: _________________
- [ ] COM port designation: COM1 / COM2 / Other: _____
- [ ] Current Modbus settings:
  - [ ] Baud rate: _____
  - [ ] Parity: _____
  - [ ] Data bits: _____
  - [ ] Stop bits: _____
  - [ ] Slave addresses in use: _____
- [ ] Available Modbus registers: D_____ to D_____
- [ ] Termination resistor already installed: Y / N
- [ ] Access to PLC programming: Y / N
- [ ] PLC programmer contact: _________________

## Installation Day 1 - Mechanical & Electrical

### Safety First
- [ ] Review site-specific safety procedures
- [ ] Lockout/tagout procedure completed
- [ ] Power confirmed OFF with voltage detector
- [ ] Obtain work permit (if required)
- [ ] Brief team on installation plan

### Enclosure Mounting
- [ ] Mark drill holes using enclosure as template
- [ ] Drill pilot holes
- [ ] Install mounting hardware:
  - [ ] Anchors inserted
  - [ ] Bolts/screws torqued per spec
- [ ] Verify enclosure level
- [ ] Test door operation (should open/close smoothly)
- [ ] Install cable glands in knockout holes:
  - [ ] Power entry gland
  - [ ] RS485 cable gland
  - [ ] Spare gland for future use

### Internal Wiring - Power
- [ ] Mount DIN rail inside enclosure
- [ ] Install 24VDC to 5VDC converter on DIN rail
- [ ] Wire 24VDC input:
  - [ ] (+) to converter input (+)
  - [ ] (-) to converter input (-)
  - [ ] Fuse installed (0.5A slow-blow)
- [ ] Wire 5VDC output:
  - [ ] Test output voltage: _____ VDC (should be 5.0±0.1V)
  - [ ] Add 1000µF capacitor across output
  - [ ] Connect to ESP32-C6 5V pin or USB-C
- [ ] Add ground wire from converter chassis to earth
- [ ] Label all wires

### Internal Wiring - RS485
- [ ] Mount isolated RS485 module on DIN rail or PCB standoffs
- [ ] Wire ESP32-C6 to RS485 module:
  - [ ] GPIO 2 (TX) → DI/TXD
  - [ ] GPIO 3 (RX) → RO/RXD
  - [ ] GPIO 10 → DE
  - [ ] GPIO 10 → RE (tie together)
  - [ ] GND → GND (MCU side)
  - [ ] 3.3V or 5V → VCC (MCU side)
- [ ] Wire RS485 module to terminal block:
  - [ ] A output → Terminal A
  - [ ] B output → Terminal B
  - [ ] GND (ISO side) → Terminal GND
- [ ] Install TVS diodes across A-B terminals:
  - [ ] Cathode to A
  - [ ] Anode to GND
  - [ ] Cathode to B
- [ ] Add 10Ω series resistors on A/B (optional but recommended)
- [ ] Install 120Ω termination resistor across A-B
- [ ] Label terminals clearly

### Internal Wiring - MLX90640
- [ ] Plan I2C cable routing (keep <30cm if possible)
- [ ] Connect MLX90640 to ESP32-C6:
  - [ ] VDD → 3.3V
  - [ ] GND → GND
  - [ ] SDA → GPIO 18
  - [ ] SCL → GPIO 8
- [ ] If cable >50cm, add pull-up resistors:
  - [ ] 4.7kΩ from SDA to 3.3V
  - [ ] 4.7kΩ from SCL to 3.3V
- [ ] Secure cable with zip ties
- [ ] Protect cable from sharp edges

### Internal Layout
- [ ] Position ESP32-C6 for easy USB access
- [ ] Route wires neatly along enclosure sides
- [ ] Avoid sharp bends in I2C cable
- [ ] Leave slack for servicing
- [ ] Ensure LCD is visible through window/door
- [ ] Take photo of internal wiring (documentation)

### External Cable - RS485 to PLC
- [ ] Measure and cut CAT6 STP cable
- [ ] Strip outer jacket (5cm on each end)
- [ ] Select one twisted pair for A/B:
  - [ ] Typically: Orange pair or Blue pair
  - [ ] Record which pair: _____
- [ ] Strip inner insulation (5mm)
- [ ] Install ferrules or tin with solder
- [ ] Label wires:
  - [ ] One wire → "A (SD+)"
  - [ ] Other wire → "B (SD-)"
- [ ] Connect to enclosure terminals:
  - [ ] A wire → A terminal
  - [ ] B wire → B terminal
- [ ] Connect shield:
  - [ ] At enclosure end: Leave disconnected initially
  - [ ] At PLC end: Connect to earth ground

### Cable Run to PLC
- [ ] Route cable through conduit/tray
- [ ] Maintain 30cm separation from power cables
- [ ] Avoid sharp bends (radius >10cm)
- [ ] Secure with cable ties every 1m
- [ ] Install ferrite beads at both ends (optional)
- [ ] Label cable every 3m

### PLC Connection
- [ ] Identify SD+ and SD- terminals on PLC
- [ ] Connect:
  - [ ] A wire → SD+ (or A+ or RXD+)
  - [ ] B wire → SD- (or B- or RXD-)
  - [ ] Shield → Earth ground (via screw terminal)
- [ ] Install 120Ω termination resistor across SD+/SD-
- [ ] Verify resistor installation with multimeter:
  - [ ] Measure A-B resistance: _____ Ω (should be ~60Ω)
- [ ] Add bias resistors if >10 devices on bus:
  - [ ] 680Ω from A to +5V
  - [ ] 680Ω from B to GND

### Power-On Checks (Before Energizing)
- [ ] Visual inspection complete
- [ ] All connections tight
- [ ] No loose wires
- [ ] Enclosure door closes properly
- [ ] Tools removed from enclosure
- [ ] Area clear of personnel

## Installation Day 2 - Commissioning

### Initial Power-Up
- [ ] Remove lockout/tagout
- [ ] Energize 24VDC supply
- [ ] Measure 5VDC at converter output: _____ V
- [ ] Watch LCD for boot sequence
- [ ] Observe Serial Monitor (if connected):
  - [ ] WiFi status: _____
  - [ ] MLX90640 init: OK / ERROR
  - [ ] RS485 init: OK / ERROR
- [ ] Record IP address (if WiFi enabled): _____
- [ ] Take photo of LCD showing status

### RS485 Verification
- [ ] Using multimeter, measure A-B idle voltage:
  - [ ] Voltage: _____ VDC (expect 2-3V)
- [ ] If oscilloscope available:
  - [ ] Connect probe to A
  - [ ] Ground to B
  - [ ] Observe waveform during transmission
  - [ ] Clean square wave? Y / N
  - [ ] Amplitude: _____ V (expect >1.5V)
- [ ] Disconnect one terminator and re-check:
  - [ ] Waveform quality degrades? Y / N
  - [ ] Reconnect terminator

### Modbus Communication Test
- [ ] Using Modbus Poll software:
  - [ ] Connect USB-RS485 to bus (temporarily)
  - [ ] Configure: 9600-8-N-1, Slave 1
  - [ ] Read Holding Registers 0-9
  - [ ] Data appears? Y / N
  - [ ] Updates every 2 seconds? Y / N
- [ ] Record sample data:
  - [ ] D0 (Avg Temp): _____ (÷100 = _____°C)
  - [ ] D4 (Status): _____ (0=OK)
  - [ ] D5 (Errors): _____
  - [ ] D6 (Success): _____
  - [ ] D8 (Sensor Health): _____ %
  - [ ] D9 (Comm Quality): _____ %
- [ ] Remove USB-RS485 adapter

### PLC Integration
- [ ] Access PLC programming software
- [ ] Configure COM port:
  - [ ] Protocol: Modbus RTU
  - [ ] Baud: 9600
  - [ ] Format: 8-N-1
  - [ ] Slave ID: 1
- [ ] Add ladder logic to read D0-D9
- [ ] Download program to PLC
- [ ] Monitor registers in PLC:
  - [ ] D0 updates? Y / N
  - [ ] Values reasonable? Y / N
- [ ] Test alarm logic (if implemented):
  - [ ] Heat sensor with hand
  - [ ] Watch for alarm trigger
  - [ ] Allow to cool, verify reset

### Functional Testing
- [ ] Observe system for 30 minutes:
  - [ ] Error count increases? Y / N (if yes, troubleshoot)
  - [ ] LCD status remains "ACTIVE"? Y / N
  - [ ] Sensor health >90%? Y / N
  - [ ] Comm quality >90%? Y / N
- [ ] Intentional disconnect test:
  - [ ] Disconnect RS485 A wire
  - [ ] Watch LCD status change to "COMM ERR"
  - [ ] Observe retry attempts in Serial Monitor
  - [ ] Reconnect A wire
  - [ ] Verify recovery within 10 seconds
  - [ ] Check error counter incremented
- [ ] Sensor fault simulation:
  - [ ] Disconnect I2C SDA wire
  - [ ] Watch LCD status change to "SENSOR ERR"
  - [ ] Reconnect SDA
  - [ ] Verify recovery
- [ ] Power cycle test:
  - [ ] Remove 24VDC power
  - [ ] Wait 10 seconds
  - [ ] Restore power
  - [ ] Verify full reboot
  - [ ] Check uptime counter reset

### Baseline Performance Recording
- [ ] Record metrics over 1 hour:
  - [ ] Starting error count: _____
  - [ ] Ending error count: _____
  - [ ] Errors per hour: _____
  - [ ] Success rate: _____ %
  - [ ] Average sensor health: _____ %
  - [ ] Average comm quality: _____ %
  - [ ] Acceptable? Y / N (target >95% success)

### EMI/Noise Check
- [ ] Start nearby motors/pumps
- [ ] Observe error rate change: Y / N
- [ ] If errors increase significantly:
  - [ ] Add more ferrite beads
  - [ ] Check shield grounding
  - [ ] Consider relocating enclosure
  - [ ] Add metal shielding to enclosure
- [ ] Run welding equipment (if applicable)
- [ ] Check for communication disruptions

## Final Steps

### Documentation
- [ ] Complete installation report:
  - [ ] Date installed: _____
  - [ ] Installed by: _____
  - [ ] Serial numbers: ESP32 _____, MLX90640 _____
  - [ ] Cable lengths: Power ___m, RS485 ___m
  - [ ] PLC details: Model _____, COM port _____
  - [ ] Baseline performance: See above
- [ ] Take photos:
  - [ ] Enclosure exterior
  - [ ] Enclosure interior (wiring)
  - [ ] PLC connection
  - [ ] Cable routing
  - [ ] LCD displaying metrics
- [ ] Create wiring diagram (as-built)
- [ ] Label all components:
  - [ ] Enclosure: "MLX90640 Thermal Monitor"
  - [ ] Cables: "RS485 to PLC COM1"
  - [ ] Terminals: "A (SD+)", "B (SD-)"
- [ ] Update P&ID (if applicable)

### Training
- [ ] Train operators on:
  - [ ] Normal LCD display appearance
  - [ ] What "ACTIVE" status means
  - [ ] When to call maintenance (CRITICAL ERROR)
  - [ ] Where to find IP address (if WiFi)
- [ ] Train maintenance on:
  - [ ] How to access enclosure
  - [ ] Reading PLC registers
  - [ ] Interpreting error codes
  - [ ] Basic troubleshooting steps
  - [ ] When to contact engineering

### Handover
- [ ] Provide to site:
  - [ ] Installation report
  - [ ] As-built wiring diagram
  - [ ] PLC register map
  - [ ] Troubleshooting guide
  - [ ] Maintenance schedule
  - [ ] Spare parts list
  - [ ] Contact information for support
- [ ] Schedule follow-up visit:
  - [ ] Date: _____ (1 week later)
  - [ ] Purpose: Verify stable operation
  - [ ] Check error logs
  - [ ] Adjust thresholds if needed

### Sign-Off

**Installer**: _____________________ Date: _____  
**Signature**: _____________________

**Site Contact**: _____________________ Date: _____  
**Signature**: _____________________

**Engineering Approval**: _____________________ Date: _____  
**Signature**: _____________________

---

## Follow-Up (1 Week Later)

- [ ] Review error logs in PLC
- [ ] Check cumulative error count on LCD
- [ ] Calculate success rate: _____ %
- [ ] Verify sensor health: _____ %
- [ ] Verify comm quality: _____ %
- [ ] Inspect enclosure for moisture/dust ingress
- [ ] Re-tighten all screw terminals
- [ ] Download 1 week of data (if logging enabled)
- [ ] Make any necessary adjustments
- [ ] Update documentation

**If success rate <90%**: Escalate to engineering for troubleshooting

---

**Checklist Version**: 2.0  
**Last Updated**: 2024  
**Applies To**: ESP32-C6 Industrial MLX90640 RS485 System  
