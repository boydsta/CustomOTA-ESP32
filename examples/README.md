# CustomOTA Examples

This folder contains examples demonstrating how to use the CustomOTA library with ESP32 projects.

## Examples

### BasicOTA
- **File**: `BasicOTA/src/main.cpp`
- **Description**: Simple OTA setup without authentication
- **Features**: Basic firmware updates, version display, drag & drop interface
- **Use Case**: Getting started, development environments

### AdvancedOTA
- **File**: `AdvancedOTA/src/main.cpp`
- **Description**: Production-ready OTA with all features enabled
- **Features**: Authentication, debug output, custom configuration, heap monitoring, status monitoring
- **Use Case**: Production applications, comprehensive OTA functionality

### SecureOTA
- **File**: `SecureOTA/src/main.cpp`
- **Description**: Security-focused OTA with enhanced protection
- **Features**: Strong authentication, secure configuration, SPIFFS protection, security monitoring
- **Use Case**: High-security applications, production environments with sensitive data

## How to Use

1. **Choose an example** that matches your needs
2. **Copy the example folder** to your workspace
3. **Update WiFi credentials** in the source code
4. **Configure partition table** (copy `../../partitions/default.csv`)
5. **Build and upload** using PlatformIO

## Required Setup

All examples require:
- ESP32 with dual OTA partitions
- WiFi connection
- PlatformIO or Arduino IDE with ESP32 support

## Partition Configuration

⚠️ **CRITICAL**: All examples require dual OTA partitions!

Copy the appropriate partition file:
```bash
cp ../partitions/default.csv ./partitions.csv
```

Update your `platformio.ini`:
```ini
board_build.partitions = partitions.csv
```

## Dependencies

All examples use:
- mathieucarbou/AsyncTCP@^3.2.3
- mathieucarbou/ESPAsyncWebServer@^3.1.1
- CustomOTA library

## Building Examples

```bash
cd BasicOTA
pio run                    # Build
pio run --target upload    # Upload to ESP32
pio device monitor         # View serial output
```

## Accessing OTA Interface

After uploading:
1. Connect ESP32 to WiFi
2. Check serial monitor for IP address
3. Open browser to `http://ESP32_IP/update`
4. Upload new firmware (.bin files)

## Example Feature Comparison

| Feature | BasicOTA | AdvancedOTA | SecureOTA |
|---------|----------|-------------|-----------|
| Authentication | ❌ | ✅ | ✅ |
| Debug Output | ❌ | ✅ | ✅ |
| Custom Restart Delay | ❌ | ✅ | ✅ |
| SPIFFS Protection | Default | ✅ | ✅ |
| Heap Monitoring | ✅ | ✅ | ✅ |
| Status Monitoring | ❌ | ✅ | ✅ |
| Security Monitoring | ❌ | ❌ | ✅ |
| Production Ready | ❌ | ✅ | ✅ |

## Configuration Examples

### Basic Configuration
```cpp
#include <CustomOTA.h>
CustomOTA ota;

void setup() {
    ota.setFirmwareVersion("1.0.0");
    ota.setHardwareID("MyDevice");
    ota.begin(&server);
}
```

### Advanced Configuration
```cpp
#include <CustomOTA.h>
CustomOTA ota;

void setup() {
    ota.setFirmwareVersion("1.2.1");
    ota.setHardwareID("Advanced Device");
    ota.setBuildTimestamp("27/08/2025 14:30:00");
    ota.setRestartDelay(5000);        // 5 second delay
    ota.setSpiffsFormatOnFailure(false); // Protect SPIFFS
    ota.setCredentials("admin", "password");
    ota.begin(&server);
}
```

### Secure Configuration
```cpp
#include <CustomOTA.h>
CustomOTA ota;

void setup() {
    ota.setFirmwareVersion("1.2.1");
    ota.setHardwareID("Secure Device");
    ota.setBuildTimestamp("27/08/2025 14:30:00");
    ota.setRestartDelay(3000);
    ota.setSpiffsFormatOnFailure(false); // Maximum security
    ota.setCredentials("secureadmin", "MySecurePassword123!@#");
    ota.begin(&server);
}
```

## Troubleshooting

**"No OTA partitions found"** - Configure dual OTA partitions first
**"WiFi connection failed"** - Check SSID/password in source code
**"Upload failed"** - Ensure .bin file is valid firmware binary
**"Authentication required"** - Use correct username/password (AdvancedOTA/SecureOTA only)
**"SPIFFS mount failed"** - Check SPIFFS configuration or enable formatting

## Debug Output

Enable detailed logging by adding to your `platformio.ini`:
```ini
build_flags = -DCUSTOM_OTA_DEBUG=1
```

This provides extensive debug information including:
- SPIFFS mount status and operations
- Heap memory monitoring
- Update progress and validation steps
- Rollback validation details
- Network and authentication status

## Production Deployment

For production use:

1. **Use SecureOTA example** as a starting point
2. **Change default credentials** immediately
3. **Set strong passwords** (12+ characters recommended)
4. **Configure SPIFFS protection** based on your data requirements
5. **Enable debug output** during development, disable for production
6. **Test thoroughly** with your specific hardware and network conditions

## Migration from Other OTA Libraries

### From ArduinoOTA
Replace:
```cpp
#include <ArduinoOTA.h>
ArduinoOTA.begin();
ArduinoOTA.handle();
```

With:
```cpp
#include <CustomOTA.h>
CustomOTA ota;
ota.begin(&server);
ota.handle();
```
