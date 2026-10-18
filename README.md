# CustomOTA-ESP32 ![Version](https://img.shields.io/badge/Version-1.3.0-blue.svg)

ESP32 Over-The-Air firmware update library with dual partition backup system, filesystem updates, and a web interface.

![ESP32](https://img.shields.io/badge/ESP32-Compatible-green.svg)
![License](https://img.shields.io/badge/License-MIT-yellow.svg)

## What's New in v1.3.0

- **Security fixes** - Uploads are authenticated before anything is written, and `setCredentials()` followed by `begin(&server)` no longer turns authentication off.
- **Lower heap use** - The OTA page is served straight from flash instead of being built as a ~23 KB `String` on every request.
- **Real results in the browser** - Firmware update, reboot and rollback reply first, then restart after `setRestartDelay()`. The page shows actual success or failure and reloads once the device is back.
- **Many upload fixes** - Filesystem uploads, error responses, progress reaching 100%, `/update/status` routing and more.
- **Build date updates on every build**, and the hardware ID is shown on the page.

See [CHANGELOG.md](CHANGELOG.md) for the full list.

## Installation Methods

### Option 1: PlatformIO Git Dependency (Recommended)
Add to your `platformio.ini`:
```ini
lib_deps = 
    mathieucarbou/AsyncTCP@^3.2.3
    mathieucarbou/ESPAsyncWebServer@^3.1.1
    https://github.com/boydsta/CustomOTA-ESP32.git
```

### Option 2: Manual Installation
Copy the library to your project's `lib` folder:
```
your_project/
├── lib/
│   └── CustomOTA/
│       ├── src/
│       │   ├── CustomOTA.h
│       │   └── CustomOTA.cpp
│       └── library.json
├── src/
│   └── main.cpp
└── platformio.ini
```

### CRITICAL: Partition Configuration Required

**CustomOTA requires dual OTA partitions to function.** You must configure your ESP32 with the correct partition table. See changelog for new partition validation features.

#### Option 1: Use Provided Partition Files
Copy the appropriate partition file from this library to your project:

```bash
# Copy the partition file to your project root
cp lib/CustomOTA/partitions/default.csv ./partitions.csv          # 4MB Flash
cp lib/CustomOTA/partitions/default_8MB.csv ./partitions.csv      # 8MB Flash  
cp lib/CustomOTA/partitions/default_16MB.csv ./partitions.csv     # 16MB Flash
```

Add to your `platformio.ini`:
```ini
board_build.partitions = partitions.csv
```

#### Option 2: Use Custom Partition Table
Ensure your partition table includes these **required partitions**:
```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, <your_app_size>
app1,     app,  ota_1,   <offset>, <your_app_size>
spiffs,   data, spiffs,  <offset>, <remaining_space>
```

**Without dual OTA partitions, CustomOTA will not work!**

## Quick Start

### Basic Setup (No Authentication)
```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA ota;

void setup() {
    Serial.begin(115200);
    WiFi.begin("your-ssid", "your-password");
    while (WiFi.status() != WL_CONNECTED) delay(1000);
    
    ota.setFirmwareVersion("1.0.0");
    ota.setHardwareID("MyDevice v1.0");
    ota.begin(&server);
    server.begin();
    
    Serial.println("OTA: http://" + WiFi.localIP().toString() + "/update");
}

void loop() {
    ota.handle(); // Must call every loop
}
```

### With Authentication
```cpp
// Add before ota.begin():
ota.setCredentials("admin", "password123");
```

### Advanced Configuration
```cpp
// Configure restart delay (default: 3000ms)
ota.setRestartDelay(5000); // 5 seconds

// Control SPIFFS formatting behavior (default: true)
ota.setSpiffsFormatOnFailure(false); // Never auto-format SPIFFS
```

## API Reference

### Core Methods
- `begin(AsyncWebServer* server)` - Initialize OTA system
- `begin(AsyncWebServer* server, username, password)` - Initialize with authentication
- `handle()` - **Must call every loop** for real-time compatibility
- `setCredentials(username, password)` - Enable authentication
- `setFirmwareVersion(version)` - Set version display
- `setHardwareID(id)` - Set the device name shown as the OTA page heading and browser tab title

### Advanced Configuration Methods
- `setRestartDelay(unsigned long delayMs)` - Delay between replying to the browser and restarting, after a firmware update, reboot or rollback (default: 3000ms)
- `setSpiffsFormatOnFailure(bool allowFormat)` - Control automatic SPIFFS formatting on mount failure (default: true)
- `setBuildTimestamp(const char* timestamp)` - Override the build timestamp (by default PlatformIO builds set it automatically via `build_timestamp.py`)

### Status Methods
- `isUpdateInProgress()` - Check if update is in progress
- `getUpdateStatus()` - Get current update status string
- `getUpdateProgress()` - Get update progress percentage (0-100)
- `shouldRestart()` - Check if device should restart (for deferred restart handling)

### Static Information Methods
- `CustomOTA::getLibraryVersion()` - Get library version string
- `CustomOTA::getVersionMajor()` - Get major version number
- `CustomOTA::getVersionMinor()` - Get minor version number
- `CustomOTA::getVersionPatch()` - Get patch version number
- `CustomOTA::getBuildInfo()` - Get build date and time

## Features

### Core Features
- **Dual Partition Backup** - Automatic firmware rollback capability
- **Filesystem Updates** - Update SPIFFS content via web interface
- **Real-Time Compatible** - Non-blocking operations, no `delay()` calls
- **Web Interface** - File uploads with progress tracking, served from flash
- **HTTP Authentication** - Password protection for OTA operations
- **Debug Control** - Enable with `-DCUSTOM_OTA_DEBUG=1` build flag

### Advanced Features
- **Automatic Backup Detection** - Finds existing firmware in inactive partitions
- **Heap Monitoring** - Prevents updates when memory is insufficient
- **Rollback Validation** - The backup image is verified by ESP-IDF before it is made the boot partition
- **Configurable Restart Delays** - Custom reboot timing for different applications
- **Safer SPIFFS Handling** - Prevents accidental data loss during formatting
- **Progress Tracking** - Real-time upload and update progress
- **Status Monitoring** - Comprehensive system status information
- **Clear Results** - The device replies before restarting, so the page always shows the real outcome

## Web Endpoints

| Route | Method | Purpose |
|---|---|---|
| `/update` | GET | OTA web page |
| `/update` | POST | Upload firmware or filesystem image (multipart, `type=firmware` or `filesystem`) |
| `/update/status` | GET | JSON status: progress, heap, version, build date, hardware ID, partitions, `canRollback`, `restartPending` |
| `/update/reboot` | POST | Restart the device |
| `/update/rollback` | POST | Boot the previous firmware |

## Customizing the Page

The page is the `COTA_OTA_PAGE` constant in `src/CustomOTA.cpp`, with its CSS in the `<style>` block near the top. Edit it there and rebuild. It also loads `/styles.css` from your own web server if you provide one, so you can restyle it without changing the library.

Device-specific values are not in the HTML: the page's script fills them in from `/update/status`.

## File Structure
```
CustomOTA-ESP32/
├── src/
│   ├── CustomOTA.h          # Header file
│   └── CustomOTA.cpp        # Implementation (contains the web page)
├── partitions/
│   ├── default.csv          # 4MB partition table
│   ├── default_8MB.csv      # 8MB partition table
│   └── default_16MB.csv     # 16MB partition table
├── examples/
│   ├── BasicOTA/
│   ├── AdvancedOTA/
│   └── SecureOTA/
├── build_timestamp.py       # Sets the build date on every PlatformIO build
├── library.json             # PlatformIO metadata
├── library.properties       # Arduino IDE metadata
└── README.md
```

## Troubleshooting

### Common Issues & Solutions

**"Upload failed: network connection lost" / timed out**
- The connection dropped before the device replied. The device replies before it restarts, so this is a real failure, not a reboot
- Check WiFi signal and retry; the status at `/update/status` shows what the device saw

**"Upload Failed (HTTP 400/500)"**
- Check .bin file validity and ESP32 compatibility
- Ensure sufficient heap memory (minimum 32KB required)
- Verify correct partition table configuration

**"No OTA Partitions Found"**
- **You need dual OTA partitions!** See partition configuration above
- Check your partition table has both `app0` and `app1` partitions

**"No Backup Available"**
- Normal until first OTA update completes
- ESP32 automatically preserves firmware in inactive partition

**"SPIFFS Mount Failed"**
- SPIFFS partition may be corrupted
- Use `setSpiffsFormatOnFailure(true)` to enable automatic recovery
- Or manually format SPIFFS if needed

**Authentication Issues**
- Verify credentials match exactly
- Check for special characters in password
- Disable auth temporarily for testing: `ota.begin(&server)`

**"Insufficient Memory"**
- Free up heap memory before updates
- Check for memory leaks in your application
- Consider increasing ESP32 PSRAM if available

**Interface Won't Load**
- Check ESP32 IP address and network connection
- Verify AsyncWebServer is running on port 80
- Check browser console for JavaScript errors

### Debug Output
Enable detailed logging:
```ini
build_flags = -DCUSTOM_OTA_DEBUG=1
```

This provides extensive debug information including:
- SPIFFS mount status and operations
- Heap memory monitoring
- Update progress and validation steps
- Rollback validation details
- Network and authentication status

## Migration Guide

### From ArduinoOTA
Replace:
```cpp
// Old ArduinoOTA code
#include <ArduinoOTA.h>
ArduinoOTA.begin();
ArduinoOTA.handle();
```

With:
```cpp
// New CustomOTA code
#include "CustomOTA.h"
CustomOTA otaUpdater;
otaUpdater.begin(&server);
otaUpdater.handle(); // Call in loop()
```

## Performance Considerations

### Real-Time Applications
- **No blocking operations** - All functions return quickly
- **Minimal memory footprint** - Optimized for resource-constrained devices
- **Deferred operations** - Time-sensitive tasks use millis() timing

### Memory Usage
- **Base memory**: ~8KB RAM
- **Update buffer**: ~32KB heap required during updates
- **SPIFFS updates**: Variable based on filesystem size

### Network Requirements
- **Stable WiFi connection** required during updates
- **Minimum bandwidth**: 1Mbps recommended
- **Timeout handling**: Automatic recovery from network issues

## Security Best Practices

### Authentication
- **Always use authentication** in production
- **Use strong passwords** (12+ characters recommended)
- **Change default credentials** immediately

### Network Security
- **Use WPA2/3 WiFi encryption**
- **Consider VPN** for remote access
- **Firewall rules** to restrict access

### Firmware Security
- **Validate firmware signatures** (future feature)
- **Use HTTPS** for secure updates (future feature)
- **Backup validation** prevents rollback to compromised firmware

## License
MIT License - Free for commercial and personal use.

**Note**: This library depends on AsyncTCP and ESPAsyncWebServer which are licensed under LGPL 3.0. This does not affect the MIT license of this library, but users should be aware of the LGPL requirements for those dependencies.

**Created by Boyd Benton**
