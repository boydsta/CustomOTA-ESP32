# Custom OTA Implementation for ESP32 Projects

This custom OTA implementation provides enhanced OTA functionality for ESP32 projects, including:

## Recent Updates (v1.2.4)

### 🐞 Bug Fixes
- Serial upload boot partition inconsistency fixed
- OTA boot state correction and partition validation

### ✨ Enhanced Features
- Boot partition management and startup validation
- Error recovery for partition state mismatches
- **Fixed Upload Progress** - Progress bar now works correctly (0% → 100%)
- **Fixed Unexpected Reboot Dialogs** - Reboot confirmation removed from upload process
- **Fixed Routing Issues** - Added missing POST route for proper file uploads
- **Enhanced Error Handling** - Better handling of network disconnections during reboot

## Features (v1.2.4)

✅ **Reboot Functionality** - Manual device reboot capability
✅ **Rollback Support** - Ability to rollback to previous firmware
✅ **Progress Tracking** - Real-time upload progress
✅ **Drag & Drop Interface** - Modern file upload interface
✅ **Authentication** - Password protection for OTA operations
✅ **Status Monitoring** - Real-time system status information
✅ **Custom Branding** - Easily customizable interface for your project

## Usage

### Basic Setup

```cpp
#include "CustomOTA.h"

AsyncWebServer server(80);
CustomOTA otaUpdater;

void setup() {
    // Initialize OTA with credentials
    otaUpdater.begin(&server, "admin", "password");
    
    // Set hardware information
   otaUpdater.setHardwareID("MyDevice 3.0.2");
    otaUpdater.setFirmwareVersion("3.0.7");
    
    server.begin();
}
```

### Accessing the OTA Interface

1. Connect to your device's WiFi AP
2. Navigate to: `http://<device-ip>/update`
3. Login with credentials (default: admin/password)

### Interface Features

#### Upload Section
- **Drag & Drop**: Drag .bin files directly onto the upload area
- **File Selection**: Click to browse and select firmware files
- **Progress Bar**: Real-time upload progress display

#### Control Buttons
- **🚀 Upload Firmware**: Starts the firmware update process
- **🔄 Reboot Device**: Manually reboot the device
- **⏪ Rollback Firmware**: Restore previous firmware version (if available)

#### System Information
- Hardware ID display
- Current firmware version
- Free heap memory
- Flash memory size

### API Endpoints (v1.2.4)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/update` | GET | OTA interface page |
| `/update` | POST | **Upload firmware file** (multipart form data) |
| `/update/start` | POST | Legacy upload endpoint (still supported) |
| `/update/reboot` | POST | Reboot device |
| `/update/rollback` | POST | Rollback to previous firmware |
| `/update/status` | GET | Get current update status (JSON) |

### Security Features

- HTTP Basic Authentication
- File type validation (.bin files only)
- Update process validation
- Rollback safety checks

### Configuration Options

```cpp
// Change credentials
otaUpdater.setCredentials("myuser", "mypassword");

// Update system information
otaUpdater.setHardwareID("Custom Hardware");
otaUpdater.setFirmwareVersion("1.2.3");
```

### Error Handling

The system includes comprehensive error handling:
- Invalid file type detection
- Upload failure recovery
- Network disconnection handling
- Update verification

### Rollback System

The rollback feature provides a safety net:
1. Creates backup before each update
2. Validates backup integrity
3. Restores previous firmware on demand
4. Automatic cleanup of old backups

### Customization

The interface can be customized by modifying the HTML in `CustomOTA.cpp`:
- Change colors and styling
- Modify logos and branding
- Add additional system information
- Customize button layouts



## Troubleshooting (v1.2.4)

### Common Issues

1. **Progress Bar Stuck at 0%** *(Fixed in v1.2.2)*
   - **Issue**: Upload progress remained at 0% throughout the entire upload
   - **Cause**: Routing mismatch - JavaScript sent to `/update` but server only handled `/update/start`
   - **Solution**: Update to v1.2.2 or later - progress now correctly shows 0% → 100%

2. **Unexpected Reboot Confirmation Dialog** *(Fixed in v1.2.2)*
   - **Issue**: Reboot confirmation appeared during every firmware upload
   - **Cause**: Reboot confirmation code was misplaced in the upload function
   - **Solution**: Update to v1.2.2 or later - reboot confirmation now only appears when manually rebooting

3. **Upload fails**: Check file is valid .bin firmware
4. **Authentication fails**: Verify username/password
5. **Rollback unavailable**: No previous backup exists
6. **Interface not loading**: Check WiFi connection and IP address

### Debug Information

Enable serial debugging to see detailed OTA process information:
- Upload progress
- Error messages
- System status updates
