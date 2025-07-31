# CustomOTA Library

ESP32 Over-The-Air firmware update library with dual partition backup system and modern web interface.

![ESP32](https://img.shields.io/badge/ESP32-Compatible-green.svg)
![Version](https://img.shields.io/badge/Version-2.2.0-blue.svg)
![License](https://img.shields.io/badge/License-MIT-yellow.svg)

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

### ⚠️ CRITICAL: Partition Configuration Required

**CustomOTA requires dual OTA partitions to function.** You must configure your ESP32 with the correct partition table.

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

## API Reference

### Core Methods
- `begin(AsyncWebServer* server)` - Initialize OTA system
- `handle()` - **Must call every loop** for real-time compatibility
- `setCredentials(username, password)` - Enable authentication
- `setFirmwareVersion(version)` - Set version display
- `setHardwareID(id)` - Set hardware identifier

### Features
- **Dual Partition Backup** - Automatic firmware rollback capability
- **Real-Time Compatible** - Non-blocking operations, no `delay()` calls
- **Modern Web Interface** - Drag & drop uploads with progress tracking
- **Debug Control** - Enable with `-DCUSTOM_OTA_DEBUG=1` build flag

## CSS Customization

The web interface styling is embedded in `src/CustomOTA.cpp`. To customize:

1. **Open** `src/CustomOTA.cpp`
2. **Find** the CSS section in the `generateOTAPage()` function (around line 200)
3. **Modify** colors, fonts, and layout:

```cpp
// Example customizations in the CSS section:
.header {
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); // Your colors
}

.card {
    border-radius: 15px;     // More rounded corners
    box-shadow: 0 8px 32px rgba(0,0,0,0.1); // Different shadow
}

.upload-area {
    border: 3px dashed #your-color; // Change upload area border
}

// Add your custom CSS here
.my-custom-style {
    // Your custom styling
}
```

4. **Rebuild and upload** your firmware to see changes

### Common Customizations

**Change Color Scheme:**
```cpp
:root {
    --primary-color: #your-primary-color;
    --secondary-color: #your-secondary-color;
    --accent-color: #your-accent-color;
}
```

**Add Logo:**
```cpp
.logo {
    background-image: url('data:image/svg+xml;base64,YOUR_BASE64_LOGO');
    background-size: contain;
    background-repeat: no-repeat;
    height: 40px;
    width: 120px;
}
```

**Custom Buttons:**
```cpp
.btn-primary {
    background: linear-gradient(45deg, #your-color1, #your-color2);
    border: none;
    border-radius: 25px;
}
```

## File Structure
```
CustomOTA-ESP32/
├── src/
│   ├── CustomOTA.h          # Header file
│   └── CustomOTA.cpp        # Implementation (contains CSS)
├── partitions/
│   ├── default.csv          # 4MB partition table
│   ├── default_8MB.csv      # 8MB partition table
│   └── default_16MB.csv     # 16MB partition table
├── examples/
│   ├── BasicOTA/
│   ├── AdvancedOTA/
│   └── SecureOTA/
├── library.json             # PlatformIO metadata
├── library.properties       # Arduino IDE metadata
└── README.md
```

## Troubleshooting

**"Upload Failed"** - Check .bin file validity and WiFi stability  
**"No OTA Partitions Found"** - ⚠️ **You need dual OTA partitions!** See partition configuration above  
**"No Backup Available"** - Normal until first OTA update  
**Authentication Issues** - Verify credentials or disable auth for testing  
**Interface Won't Load** - Check ESP32 IP address and network connection

## Debug Output
Enable detailed logging:
```ini
build_flags = -DCUSTOM_OTA_DEBUG=1
```

## License
MIT License - Free for commercial and personal use.

**Note**: This library depends on AsyncTCP and ESPAsyncWebServer which are licensed under LGPL 3.0. This does not affect the MIT license of this library, but users should be aware of the LGPL requirements for those dependencies.

**Created by Boyd Benton**
