# CustomOTA Examples

This folder contains examples demonstrating how to use the CustomOTA library with ESP32 projects.

## Examples

### BasicOTA
- **File**: `BasicOTA/src/main.cpp`
- **Description**: Simple OTA setup without authentication
- **Features**: Basic firmware updates, version display

### AdvancedOTA  
- **File**: `AdvancedOTA/src/main.cpp`
- **Description**: Production-ready OTA with all features
- **Features**: Authentication, debug output, comprehensive setup

### SecureOTA
- **File**: `SecureOTA/src/main.cpp` 
- **Description**: Secure OTA with HTTP authentication
- **Features**: Username/password protection, secure updates

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

## Troubleshooting

**"No OTA partitions found"** - Configure dual OTA partitions first
**"WiFi connection failed"** - Check SSID/password in source code
**"Upload failed"** - Ensure .bin file is valid firmware binary
**"Authentication required"** - Use correct username/password (SecureOTA only)
