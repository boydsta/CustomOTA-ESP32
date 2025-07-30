# CustomOTA Library

<div align="center">
  <h3>🚀 Advanced ESP32 OTA Update Library</h3>
  <p>Professional Over-The-Air firmware updates with dual partition backup system</p>
  
  ![ESP32](https://img.shields.io/badge/ESP32-Compatible-green.svg)
  ![Version](https://img.shields.io/badge/Version-2.1.0-blue.svg)
  ![License](https://img.shields.io/badge/License-MIT-yellow.svg)
</div>

---

## 🌟 Key Features

- **🔄 ESP32 Dual Partition System** - Automatic firmware backup and rollback
- **📱 Modern Web Interface** - Drag & drop file uploads with real-time progress
- **🛡️ Smart Error Recovery** - Emergency mode for failed updates
- **🔒 Optional Authentication** - HTTP Basic Auth protection
- **📊 System Monitoring** - Real-time memory, partition, and system info
- **⚡ Real-Time Compatible** - Non-blocking operations for time-critical applications
- **🎯 Debug Control** - Configurable debug output via build flags

---

## 📦 Installation

### 1. Library Structure
Place the CustomOTA library in your project's `lib` folder:
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

### 2. Dependencies
Add these dependencies to your `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32@6.10.0
board = esp32dev
framework = arduino

lib_deps = 
    mathieucarbou/AsyncTCP@^3.2.3
    mathieucarbou/ESPAsyncWebServer@^3.1.1

# Optional: Enable debug output
build_flags = 
    -DCUSTOM_OTA_DEBUG=1
```

### 3. Partition Scheme
**Required**: Configure dual OTA partitions in your `platformio.ini`:

```ini
board_build.partitions = default_8MB.csv
# OR for 4MB flash:
board_build.partitions = default.csv
```

The library requires the standard ESP32 dual OTA partition layout:
- **app0** - Primary firmware partition (1MB)
- **app1** - Secondary firmware partition (1MB) 
- **spiffs** - File system partition (remaining space)

---

## 🚀 Quick Start

### Basic Implementation (No Authentication)

```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA otaUpdater;

void setup() {
    Serial.begin(115200);
    
    // Connect to WiFi
    WiFi.begin("your-ssid", "your-password");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    
    // Initialize CustomOTA
    otaUpdater.setHardwareID("MyDevice v1.0");
    otaUpdater.setFirmwareVersion("1.0.0");
    // otaUpdater.setBuildTimestamp("2024-01-15 14:30:00"); // Optional: uses __DATE__ __TIME__ if not set
    otaUpdater.begin(&server);
    
    server.begin();
    Serial.println("OTA Server started at: http://" + WiFi.localIP().toString());
}

void loop() {
    // IMPORTANT: Must call handle() every loop iteration
    otaUpdater.handle();
    
    // Your application code here...
}
```

### Secure Implementation (With Authentication)

```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA otaUpdater;

void setup() {
    Serial.begin(115200);
    
    // Connect to WiFi
    WiFi.begin("your-ssid", "your-password");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    
    // Configure CustomOTA with authentication
    otaUpdater.setCredentials("admin", "your-secure-password");
    otaUpdater.setHardwareID("MyDevice v1.0");
    otaUpdater.setFirmwareVersion("1.0.0");
    // otaUpdater.setBuildTimestamp("2024-01-15 14:30:00"); // Optional: uses __DATE__ __TIME__ if not set
    otaUpdater.begin(&server);
    
    server.begin();
    Serial.println("Secure OTA Server started at: http://" + WiFi.localIP().toString());
}

void loop() {
    otaUpdater.handle();
    // Your application code...
}
```

---

## 🔧 API Reference

### Core Methods

#### `void begin(AsyncWebServer* server)`
Initialize the OTA system and register web routes.
- **Parameter**: Pointer to your AsyncWebServer instance
- **Must be called**: After WiFi connection and server setup

#### `void handle()`
**CRITICAL**: Must be called every loop iteration for proper operation.
- Handles deferred restarts (non-blocking)
- Processes internal housekeeping
- **Real-time safe**: Completes in microseconds

#### `void setCredentials(const char* username, const char* password)`
Enable HTTP Basic Authentication (optional).
- **Parameters**: Username and password for web interface
- **Call before**: `begin()` method

#### `void setHardwareID(const char* id)`
Set the hardware identifier displayed in the web interface.

#### `void setFirmwareVersion(const char* version)`
Set the current firmware version displayed in the web interface.

#### `void setBuildTimestamp(const char* timestamp)`
Set the build timestamp displayed in the web interface.
- **Parameter**: `timestamp` - Build date/time string (e.g., "2024-01-15 14:30:00")
- **Note**: If not called, automatically uses `__DATE__` and `__TIME__` compiler macros
- **Call before**: `begin()` method

### Advanced Methods

#### `bool hasValidBackup()`
Check if a firmware backup is available for rollback.
- **Returns**: `true` if backup firmware exists on inactive partition

#### `String getRunningPartition()`
Get information about the currently running partition.
- **Returns**: Partition label and details

---

## 🖥️ Web Interface

### Accessing the Interface
Navigate to `http://your-esp32-ip/update` in your web browser.

### Interface Features

1. **System Information Panel**
   - Hardware ID and firmware version
   - Running partition information
   - Automatic backup status (based on ESP32 dual partition system)
   - Memory usage and flash size
   - SPIFFS status

2. **Update Type Selection**
   - **Firmware Update**: Upload compiled .bin files
   - **Filesystem Update**: Upload SPIFFS filesystem.bin files

3. **File Upload Area**
   - Drag & drop interface
   - Automatic .bin file validation
   - Real-time upload progress

4. **Control Buttons**
   - **Upload**: Start the update process
   - **Rollback**: Switch to previous firmware (if available)
   - **Reboot**: Restart the device

---

## 🔄 Backup & Rollback System

### How It Works
The CustomOTA library leverages ESP32's built-in dual partition OTA system:

1. **Initial State**: Single firmware on app0 partition, no backup available
2. **First Update**: New firmware uploaded to app1 partition, device switches to app1
3. **Backup Available**: app0 now contains previous firmware as automatic backup
4. **Subsequent Updates**: Alternates between app0 and app1 partitions
5. **Rollback**: Simply switches boot partition back to previous firmware

### Key Benefits
- **Automatic**: No manual backup management required
- **Reliable**: Uses ESP32's proven partition switching system
- **Space Efficient**: No duplicate firmware storage needed
- **Fast Recovery**: Instant rollback via partition switching

### Rollback Process
1. Click "Rollback" button in web interface
2. System sets boot partition to inactive partition
3. Device restarts automatically  
4. Previous firmware loads and runs
5. **Infinite Rollback**: Can switch back and forth between partitions indefinitely

**Note**: After each rollback, the button remains available to switch back to the other partition, allowing unlimited switching between the two firmware versions.

---

## 🐛 Debug System

### Enabling Debug Output
Add build flag to `platformio.ini`:

```ini
[env:esp32dev]
build_flags = 
    -DCUSTOM_OTA_DEBUG=1    # Enable debug output
    # -DCUSTOM_OTA_DEBUG=0  # Disable debug output (production)
```

### Debug Features
- **Conditional Compilation**: Debug code completely removed in production builds
- **Detailed Logging**: Upload progress, partition information, error details
- **No Performance Impact**: Zero overhead when disabled
- **Macro-based**: Uses `COTA_DEBUG_PRINT`, `COTA_DEBUG_PRINTLN`, `COTA_DEBUG_PRINTF`

### Example Debug Output
```
CustomOTA: System initialization
CustomOTA: Running partition: app0, Size: 1048576 bytes
CustomOTA: ESP32 dual partition system handles backup automatically  
CustomOTA: Other partition contains firmware - backup available
CustomOTA: Starting firmware update...
CustomOTA: Upload progress: 45% (461824/1024000 bytes)
CustomOTA: Update completed successfully, restarting...
```

---

## ⚡ Real-Time Compatibility

### Design Principles
The CustomOTA library is designed for real-time applications:

- **No `delay()` calls**: All timing uses `millis()` comparisons
- **Non-blocking operations**: Functions return quickly (microseconds)
- **Deferred actions**: Restarts scheduled for future execution
- **Async web server**: Non-blocking HTTP handling

### Critical Guidelines

#### ✅ **DO - Real-Time Safe**
```cpp
// Deferred restart scheduling
_restartPending = true;
_restartTime = millis() + 2000;

// Later check (non-blocking)
if (_restartPending && millis() >= _restartTime) {
    ESP.restart();
}
```

#### ❌ **DON'T - Blocks Real-Time Operations**
```cpp
// NEVER do this - blocks for 2 seconds!
delay(2000);
ESP.restart();
```

### Integration with Time-Critical Code
```cpp
void loop() {
    // Handle OTA (microseconds)
    otaUpdater.handle();
    
    // Your real-time operations continue uninterrupted
    readSensors();        // Critical timing maintained
    controlOutputs();     // Microsecond precision preserved
    communicateWithPLC(); // Real-time protocols work normally
}
```

---

## 🛠️ Customization

### Styling Customization
The web interface uses embedded CSS that can be customized by modifying the `CustomOTA.cpp` file:

1. Locate the CSS section in the `htmlContent` string
2. Modify colors, fonts, and layout as needed
3. Rebuild and upload firmware

### Adding Custom Routes
You can add additional routes to the same AsyncWebServer instance:

```cpp
// After otaUpdater.begin(&server)
server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{\"status\":\"ok\"}");
});
```

---

## 🚨 Error Handling

### Automatic Error Recovery
- **Upload failures**: Automatic cleanup and retry capability
- **Corruption detection**: Built-in validation and error reporting
- **Emergency mode**: Recovery options for failed updates
- **Watchdog integration**: Compatible with ESP32 watchdog systems

### Error Scenarios
1. **Network interruption**: Upload resumes or reports clear error
2. **Invalid firmware**: File validation prevents corrupt uploads
3. **Insufficient space**: Pre-upload space verification
4. **Power loss during update**: ESP32 boot loader handles recovery

---

## 🔍 Troubleshooting

### Common Issues

#### "Upload Failed" Error
- **Check file**: Ensure .bin file is valid ESP32 firmware
- **Check space**: Verify sufficient flash memory available
- **Check network**: Ensure stable WiFi connection during upload

#### "No Backup Available"
- **Expected behavior**: No backup available until first OTA update
- **After first update**: Backup becomes available automatically
- **Check partitions**: Verify dual OTA partition scheme configured

#### Authentication Issues  
- **Clear browser cache**: Remove stored credentials
- **Check credentials**: Verify username/password match setCredentials()
- **Try incognito mode**: Test without cached credentials

### Debug Steps
1. **Enable debug output**: Add `CUSTOM_OTA_DEBUG=1` build flag
2. **Monitor Serial output**: Watch for detailed error messages
3. **Check partition table**: Verify dual OTA partitions configured
4. **Test with simple firmware**: Upload minimal test firmware first

---

## 📝 Example Projects

### Minimal Example
```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA ota;

void setup() {
    Serial.begin(115200);
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(1000);
    
    ota.setFirmwareVersion("1.0.0");
    ota.begin(&server);
    server.begin();
}

void loop() {
    ota.handle();
}
```

### Production Example with Authentication
```cpp
#include <WiFi.h>
#include <CustomOTA.h>
#include <Preferences.h>

AsyncWebServer server(80);
CustomOTA ota;
Preferences prefs;

void setup() {
    Serial.begin(115200);
    
    // Load WiFi credentials from preferences
    prefs.begin("config");
    String ssid = prefs.getString("ssid", "default-ssid");
    String password = prefs.getString("password", "default-password");
    prefs.end();
    
    // Connect to WiFi
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting...");
    }
    
    // Configure secure OTA
    ota.setCredentials("admin", "secure-password-123");
    ota.setHardwareID("ProductionDevice v2.1");
    ota.setFirmwareVersion("2.1.0");
    ota.begin(&server);
    
    // Add custom API endpoints
    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"device\":\"ProductionDevice\",\"uptime\":";
        json += millis();
        json += "}";
        request->send(200, "application/json", json);
    });
    
    server.begin();
    Serial.println("Production OTA server ready: http://" + WiFi.localIP().toString());
}

void loop() {
    ota.handle();
    
    // Your production application code
    runProductionTasks();
}
```

---

## 📄 License

MIT License - Feel free to use in commercial and personal projects.

## 🤝 Contributing

Issues and pull requests welcome! Please maintain real-time compatibility in all contributions.

## 📞 Support

For support and questions:
- Create an issue on the project repository
- Check the troubleshooting section above
- Review debug output for detailed error information

---

**Created by Boyd Benton**  
*Real-time compatibility maintained throughout all operations*

## Library Features

### Professional Styling
- Complete UI integration
- Consistent navigation and branding
- Professional appearance matching main interface

### Real-Time Compatible Operations
- **Non-blocking OTA updates** with progress tracking
- **Deferred restart mechanism** - no blocking delays
- **Background file uploads** without interrupting real-time operations
- **Async web server integration** for responsive UI

### Dual Update Support
- **Firmware updates** (.bin files)
- **Filesystem updates** (filesystem.bin files)
- **Partition switching** for firmware updates
- **SPIFFS support** for web file updates

## Usage Example

```cpp
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA otaUpdater;

void setup() {
    // Initialize CustomOTA - NO delay() calls anywhere!
    otaUpdater.begin(&server, "", ""); // No authentication
    otaUpdater.setHardwareID("MyDevice v1.0");
    otaUpdater.setFirmwareVersion("3.0.7");
    
    server.begin();
}

void loop() {
    // CRITICAL: Call this every loop iteration
    // This function is non-blocking and completes in microseconds
    otaUpdater.handle();
    
    // Your real-time operations continue here...
    // Loop timing: microseconds, NOT milliseconds!
}
```

## Key Functions

### `void handle()`
- **MUST be called every loop iteration**
- **Non-blocking** - completes in microseconds
- Handles deferred restarts using `millis()` comparison
- **NEVER uses `delay()`**

### `void handleReboot()`
- Uses **deferred restart mechanism**
- Schedules restart 2 seconds in future
- **NO blocking `delay()` calls**
- Real-time operations continue until restart

### `void checkRestart()`
- **Non-blocking restart check**
- Uses `millis()` comparison for timing
- Called from `handle()` function
- **Real-time safe** - returns immediately if not ready

## Timing Architecture

```cpp
// ✅ CORRECT - Non-blocking timing
_restartPending = true;
_restartTime = millis() + 2000; // Schedule for 2 seconds later

// Later in main loop:
if (_restartPending && millis() >= _restartTime) {
    ESP.restart(); // Only when ready
}

// ❌ WRONG - NEVER DO THIS!
delay(2000); // Blocks real-time operations for 2 seconds!
ESP.restart();
```

## Development Guidelines

### For All Future Development:

1. **Before writing any timing code** - ask: "Does this use `delay()`?"
2. **If timing is needed** - use `millis()` comparison approach
3. **Test all functions** to ensure they complete quickly
4. **Document any time-sensitive operations** clearly
5. **Always maintain real-time compatibility**

### Code Review Checklist:

- [ ] No `delay()` calls anywhere
- [ ] All timing uses `millis()` comparison
- [ ] Functions return quickly (microseconds)
- [ ] No blocking operations in main thread
- [ ] Real-time operations can continue during updates

## Integration Notes

- Works with **AsyncWebServer** for non-blocking web operations
- Compatible with **real-time control loops**
- Maintains **microsecond-level timing precision**
- **Web interface remains responsive** during updates
- **Real-time power management** continues during OTA operations

## Created By

Boyd Benton  
**Real-time compatibility maintained throughout all operations**

---

### Remember: Real-time ESP32 applications require precision timing. Every microsecond matters!

- **System Information Panel** - Hardware ID, firmware version, memory status, SPIFFS usage
- **Update Type Selection** - Choose between firmware or filesystem updates
- **Drag & Drop Upload Area** - Simply drag .bin files to upload
- **Progress Tracking** - Real-time upload progress with visual feedback
- **Control Buttons** - Upload, Reboot, and Rollback functionality
- **Status Messages** - Clear feedback for all operations

---

## 🚀 Quick Start

### Installation

1. **Copy the library** to your project's `lib` folder
2. **Add dependencies** to your `platformio.ini`:

```ini
lib_deps = 
    mathieucarbou/AsyncTCP@^3.2.3
    mathieucarbou/ESPAsyncWebServer@^3.1.1
```

### Basic Usage (No Authentication)

```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA otaUpdater;

void setup() {
    Serial.begin(115200);
    
    // Setup WiFi (your existing WiFi code)
    WiFi.begin("your-ssid", "your-password");
    
    // Initialize CustomOTA without authentication (open access)
    otaUpdater.begin(&server);
    otaUpdater.setHardwareID("MyDevice v1.0");
    otaUpdater.setFirmwareVersion("1.0.0");
    
    // Start server
    server.begin();
    Serial.println("OTA Server started at http://your-ip/update");
}

void loop() {
    // Your main code here
}
```

### Basic Usage (With Authentication)

```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA otaUpdater;

void setup() {
    Serial.begin(115200);
    
    // Setup WiFi (your existing WiFi code)  
    WiFi.begin("your-ssid", "your-password");
    
    // Initialize CustomOTA with authentication
    otaUpdater.begin(&server, "admin", "password");
    otaUpdater.setHardwareID("MyDevice v1.0");
    otaUpdater.setFirmwareVersion("1.0.0");
    
    // Start server
    server.begin();
    Serial.println("Secure OTA Server started at http://your-ip/update");
}

void loop() {
    // Your main code here
}
```

---

## 📚 API Reference

### Class: `CustomOTA`

#### Constructor
```cpp
CustomOTA()
```
Creates a new CustomOTA instance.

#### Methods

##### `begin(AsyncWebServer* server)`
Initialize the OTA service without authentication (open access).

**Parameters:**
- `server` - Pointer to AsyncWebServer instance

```cpp
otaUpdater.begin(&server); // No authentication required
```

##### `begin(AsyncWebServer* server, const char* username, const char* password)`
Initialize the OTA service with authentication.

**Parameters:**
- `server` - Pointer to AsyncWebServer instance
- `username` - Authentication username
- `password` - Authentication password

```cpp
otaUpdater.begin(&server, "admin", "secretpass");
```

##### `setCredentials(const char* username, const char* password)`
Update authentication credentials after initialization.

```cpp
otaUpdater.setCredentials("newuser", "newpass");
```

##### `setFirmwareVersion(const char* version)`
Set the current firmware version string.

```cpp
otaUpdater.setFirmwareVersion("2.1.0");
```

##### `setHardwareID(const char* id)`
Set the hardware identifier string.

```cpp
otaUpdater.setHardwareID("MyDevice v1.0");
```

##### `isUpdateInProgress()`
Check if an update is currently running.

```cpp
if (otaUpdater.isUpdateInProgress()) {
    Serial.println("Update in progress...");
}
```

##### `getUpdateStatus()`
Get current update status as string.

```cpp
String status = otaUpdater.getUpdateStatus();
```

##### `getUpdateProgress()`
Get current update progress (0-100).

```cpp
int progress = otaUpdater.getUpdateProgress();
```

---

## 🌐 Web Interface

### Accessing the Interface

1. Connect to your ESP32's IP address
2. Navigate to: `http://your-esp32-ip/update`
3. Login with your configured credentials

### Interface Elements

#### System Information Card
- **Hardware ID** - Device identifier
- **Firmware Version** - Current firmware version
- **Free Heap** - Available RAM memory
- **Flash Size** - Total flash memory

#### Upload Area
- **Update Type Selection** - Choose between Firmware or Filesystem updates
- **Drag & Drop** - Drag .bin files directly onto the area
- **Click to Browse** - Traditional file selection dialog
- **File Validation** - Automatic .bin file type checking

#### Control Buttons
- **🚀 Upload Firmware** - Start firmware update process
- **💾 Upload Filesystem** - Start SPIFFS filesystem update process
- **🔄 Reboot Device** - Manually restart the device
- **⏪ Rollback Firmware** - Restore previous firmware (if available)

---

## 🔌 API Endpoints

| Endpoint | Method | Description | Response |
|----------|--------|-------------|----------|
| `/update` | GET | Main OTA interface | HTML page |
| `/update/start` | POST | Upload firmware or filesystem file | Status message |
| `/update/reboot` | POST | Reboot device | Confirmation |
| `/update/rollback` | POST | Rollback firmware | Status message |
| `/update/status` | GET | Get update status | JSON object |

### Status API Response
```json
{
    "inProgress": false,
    "progress": 0,
    "status": "Ready",
    "hasBackup": true,
    "freeHeap": 234567,
    "version": "1.0.0"
}
```

---

## 🛡️ Security Features

### Authentication (Optional)
- HTTP Basic Authentication (when enabled)
- Configurable username/password or open access
- Session-based access control

**With Authentication:**
```cpp
ota.begin(&server, "admin", "password123");
```

**Without Authentication (Open Access):**
```cpp
ota.begin(&server); // Anyone can access the OTA interface
```

### File Validation
- Automatic .bin file type checking
- File size validation
- Upload integrity verification

### Error Handling
- Comprehensive error messages
- Automatic recovery from failed uploads
- Safe rollback mechanism

---

## 🎨 Customization

### Interface Styling
The web interface can be customized by modifying the CSS in `CustomOTA.cpp`:

```cpp
// Find the CSS section in generateOTAPage()
// Modify colors, fonts, layout, etc.
.header {
    background: linear-gradient(135deg, #4CAF50 0%, #45a049 100%);
    // Change to your brand colors
}
```

### Logo and Branding
```cpp
// Modify the logo section
.logo {
    // Add your custom logo or styling
}
```

### Additional Information
Add custom system information by modifying the info card section.

---

## 📋 Examples

### Example 1: Basic Setup (No Authentication)
```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA ota;

void setup() {
    Serial.begin(115200);
    WiFi.begin("MyWiFi", "password");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting...");
    }
    
    ota.begin(&server); // No authentication - open access
    ota.setFirmwareVersion("1.0.0");
    server.begin();
    
    Serial.println("OTA ready at: http://" + WiFi.localIP().toString() + "/update");
    Serial.println("No authentication required - open access");
}

void loop() {
    delay(1000);
}
```

### Example 2: With Authentication
```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA ota;

void setup() {
    Serial.begin(115200);
    WiFi.begin("MyWiFi", "password");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }
    
    // Setup with authentication
    ota.begin(&server, "admin", "supersecret");
    ota.setHardwareID("MyProject v2.0");
    ota.setFirmwareVersion("2.0.1");
    
    server.begin();
    Serial.println("Secure OTA ready!");
}

void loop() {
    if (ota.isUpdateInProgress()) {
        Serial.println("Update: " + String(ota.getUpdateProgress()) + "%");
    }
    delay(1000);
}
```

### Example 3: Custom Status Monitoring
```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA ota;

void setup() {
    // ... WiFi setup ...
    
    ota.begin(&server, "user", "pass");
    ota.setFirmwareVersion("1.2.3");
    
    // Add custom status endpoint
    server.on("/custom-status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"uptime\":" + String(millis()) + ",";
        json += "\"temperature\":" + String(25.6) + ",";
        json += "\"updateProgress\":" + String(ota.getUpdateProgress());
        json += "}";
        request->send(200, "application/json", json);
    });
    
    server.begin();
}

void loop() {
    // Your application code
}
```

---

## � Filesystem Updates

The CustomOTA library supports both firmware and filesystem (SPIFFS) updates. This allows you to update web files, configuration data, and other filesystem content separately from your main application code.

### Building Filesystem .bin Files

#### Using PlatformIO

1. **Create your data folder** with web files, configurations, etc:
```
project/
├── data/
│   ├── index.html
│   ├── style.css
│   ├── config.json
│   └── images/
└── src/
    └── main.cpp
```

2. **Build filesystem image**:
```bash
pio run --target buildfs
```

3. **Generate filesystem .bin file**:
```bash
# The .bin file will be created in .pio/build/[env]/spiffs.bin
# Copy it to a location for OTA upload
cp .pio/build/esp32dev/spiffs.bin filesystem_update.bin
```

#### Using Arduino IDE

1. **Install ESP32 Sketch Data Upload plugin**
2. **Place files in data/ folder**
3. **Use "ESP32 Sketch Data Upload" from Tools menu** to generate SPIFFS image
4. **Find the generated .bin file** in your build output

### Upload Process

1. **Select "Filesystem"** as update type in the web interface
2. **Upload your filesystem .bin file**
3. **Device will update SPIFFS** and reboot automatically
4. **New files will be available** after reboot

### Important Notes

- Filesystem updates require SPIFFS to be initialized in your code
- The library automatically detects the update type and uses appropriate ESP32 Update command
- Filesystem updates are separate from firmware - you can update either independently
- Always backup important data before filesystem updates

---

## �🔧 Troubleshooting

### Common Issues

#### "Upload failed" Error
- **Cause**: Invalid firmware file or corrupted upload
- **Solution**: Ensure you're uploading a valid .bin file compiled for ESP32

#### "Authentication failed" Error
- **Cause**: Incorrect username/password when authentication is enabled
- **Solution**: Check credentials in your code and browser, or disable authentication for testing

#### Interface doesn't load / Access denied
- **Cause**: Authentication required but not provided, or network connectivity issues
- **Solution**: Either provide correct credentials or initialize without authentication using `ota.begin(&server)`

#### "Rollback unavailable" Message
- **Cause**: No previous firmware backup exists
- **Solution**: Only available after at least one successful update

#### Interface doesn't load
- **Cause**: Network connectivity or server issues
- **Solution**: Check WiFi connection and ESP32 IP address

### Debug Information

Enable serial debugging to see detailed information:

```cpp
void setup() {
    Serial.begin(115200);
    // ... rest of setup ...
    
    Serial.setDebugOutput(true); // Enable ESP32 debug output
}
```

---

## 🔄 Migration Guide

### From ElegantOTA
```cpp
// Old ElegantOTA code
#include <ElegantOTA.h>
ElegantOTA.begin(&server);

// New CustomOTA code
#include <CustomOTA.h>
CustomOTA ota;
ota.begin(&server);
```

### From PrettyOTA
```cpp
// Old PrettyOTA code
#include <PrettyOTA.h>
PrettyOTA updates;
updates.Begin(&server);
updates.SetAppVersion("1.0.0");

// New CustomOTA code
#include <CustomOTA.h>
CustomOTA ota;
ota.begin(&server);
ota.setFirmwareVersion("1.0.0");
```

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🤝 Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/boydsta/CustomOTA-ESP32/issues)
- **Documentation**: This README and inline code comments
- **Examples**: See the `examples/` folder

---

## 🙏 Acknowledgments

- Built for ESP32 projects requiring real-time compatibility
- Inspired by ElegantOTA and PrettyOTA libraries
- Uses AsyncWebServer for optimal performance

---

<div align="center">
  <p>Made with ❤️ by <a href="https://github.com/boydsta">Boyd Benton</a></p>
</div>
