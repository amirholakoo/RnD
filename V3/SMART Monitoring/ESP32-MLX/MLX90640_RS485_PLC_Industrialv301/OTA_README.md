# OTA Updates - Industrial Version

## Overview

The industrial version now includes **Over-The-Air (OTA)** firmware update capability, allowing you to update the device wirelessly without physical access.

## Quick Reference

### WiFi Settings (Change These!)

```cpp
const char* WIFI_SSID = "VPN1";              // Your WiFi network name
const char* WIFI_PASS = "09126141426";       // Your WiFi password
const char* OTA_HOSTNAME = "esp32-mlx-industrial";  // Device hostname
const char* OTA_PASSWORD = "mlx90640";       // OTA password - CHANGE THIS!
```

## How to Use

### Initial Setup (USB Upload)

1. Connect ESP32-C6 via USB
2. Update WiFi credentials in the code
3. Upload sketch normally
4. Open Serial Monitor (115200 baud)
5. **Record the IP address** shown (e.g., 192.168.1.100)
6. Device will show IP on LCD Info screen

### Wireless Updates (OTA)

1. In Arduino IDE: `Tools` → `Port`
2. Select network port: `esp32-mlx-industrial at [IP]`
3. Click Upload
4. Enter password: `mlx90640` (or your custom password)
5. Watch LCD for progress bar!

## OTA Display Feedback

During OTA update, the LCD shows:
- "OTA UPDATE" message
- Progress bar (0-100%)
- Percentage complete
- "SUCCESS!" or "FAILED" status

## Security

### ⚠️ CRITICAL: Change Default Password!

```cpp
const char* OTA_PASSWORD = "your-secure-password";  // Use strong password!
```

**Why?** Anyone on your network with the default password can update your device!

### Best Practices

1. Use minimum 8 characters
2. Mix letters, numbers, symbols
3. Don't share password via email/chat
4. Change periodically
5. Different password per site/device

## Industrial Considerations

### Network Requirements

- **WiFi**: 2.4GHz network (not 5GHz)
- **Signal**: Strong signal recommended (-60 dBm or better)
- **Firewall**: Allow port 3232 for OTA
- **VLAN**: ESP32 and PC must be on same network

### Update Process Flow

```
1. PC initiates OTA → Network Port
2. ESP32 receives update request
3. Validates password
4. Pauses RS485/sensor operations
5. Receives firmware chunks
6. Displays progress on LCD
7. Flashes new firmware
8. Reboots automatically
9. Resumes normal operation
```

### Operation During Update

**Suspended:**
- Temperature reading
- RS485 communication to PLC
- Display updates (except OTA screen)
- Watchdog resets

**Active:**
- WiFi connection
- OTA handler
- Progress display
- Error logging

### Failure Recovery

If OTA update fails:

1. Device **will not brick** (ESP32 has dual partitions)
2. Previous firmware remains intact
3. Device reboots to last working version
4. LCD shows "FAILED" message
5. Can retry OTA or revert to USB upload

## Troubleshooting

### Network Port Not Visible

**Check:**
1. WiFi connected? (Check Serial Monitor)
2. IP address correct?
3. Same network as PC?
4. Firewall blocking port 3232?
5. Arduino IDE restarted?

**Solution:**
```bash
# Test connectivity (Windows)
ping 192.168.1.100

# Test OTA port (Windows)
telnet 192.168.1.100 3232
```

### Authentication Failed

**Cause:** Wrong password

**Solution:** Verify `OTA_PASSWORD` matches in code

### Update Timeout

**Causes:**
1. Weak WiFi signal
2. Network congestion
3. Large sketch size

**Solutions:**
1. Move closer to router
2. Upload during off-peak hours
3. Reduce sketch size
4. Use USB as fallback

### Device Reboots During Update

**Causes:**
1. Power supply insufficient
2. Watchdog not disabled properly

**Solutions:**
1. Use 1A+ power supply
2. Check code has watchdog pause during OTA

## Industrial Deployment Tips

### Site Survey Before Deployment

1. **Test WiFi Coverage**:
   - Measure signal at installation location
   - Use WiFi analyzer app
   - Minimum -70 dBm recommended

2. **Network Configuration**:
   - Reserve DHCP IP or use static IP
   - Document IP address
   - Configure firewall rules

3. **Access Planning**:
   - Maintain USB access for emergencies
   - Document network credentials
   - Plan update windows (low-activity times)

### Maintenance Schedule

**Before Each OTA Update:**
- [ ] Notify operators
- [ ] Schedule during maintenance window
- [ ] Backup current firmware
- [ ] Test new firmware in lab
- [ ] Verify WiFi connectivity

**After Update:**
- [ ] Verify system status on LCD
- [ ] Check Serial Monitor for errors
- [ ] Monitor RS485 communication
- [ ] Verify PLC receiving data
- [ ] Log update in maintenance records

## Network Isolation (Optional)

For added security, disable WiFi after deployment:

```cpp
#define ENABLE_WIFI false  // Add this at top

void setup() {
  // ...
  
  #if ENABLE_WIFI
    initWiFi();
    initOTA();
  #endif
  
  // ...
}
```

Re-enable only when updates needed.

## OTA vs USB Comparison

| Feature | OTA | USB |
|---------|-----|-----|
| **Speed** | 30-60s | 20-40s |
| **Convenience** | High | Low |
| **Physical Access** | Not required | Required |
| **Network Dependency** | Yes | No |
| **Safety** | Dual partition | Direct flash |
| **Best For** | Remote devices | Initial setup |

## Logging OTA Events

Serial Monitor output during OTA:

```
[OTA] Update Starting...
[OTA] Progress: 10%
[OTA] Progress: 25%
[OTA] Progress: 50%
[OTA] Progress: 75%
[OTA] Progress: 100%
[OTA] Update Complete!
```

Or if failed:
```
[OTA] Update Starting...
[OTA] Progress: 15%
[OTA] Error[2]: Connection Failed
```

## Integration with PLC

### Register D4 (Status)

During OTA update, register D4 value:
- **0**: Normal operation
- **1**: Error (sensor/comm)
- **2**: Critical error
- **3**: OTA update in progress (future enhancement)

### Recommended PLC Ladder Logic

```
// Detect device offline during OTA
LD  M0
LD= D4 K3           // If status = OTA mode
OUT M103            // Set "Update in progress" flag
```

## Future Enhancements

Possible additions:

1. **OTA Status via Modbus**: Add register for OTA state
2. **Scheduled Updates**: Automatic update at specific time
3. **Firmware Version Register**: Report version to PLC
4. **OTA Trigger**: Start OTA via PLC command
5. **Update Notification**: Email/SMS when complete

## Contact & Support

For OTA-specific issues:

1. Check Serial Monitor for detailed errors
2. Verify network connectivity (ping test)
3. Try USB upload as fallback
4. Check firewall/router settings

---

**Stay updated wirelessly! 📡✨**
