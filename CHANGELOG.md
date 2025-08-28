# Changelog

All notable changes to the CustomOTA library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.2] - 2025-08-29

### Fixed
- **Critical Upload Progress Bug** - Fixed progress bar stuck at 0% due to routing mismatch
- **Unexpected Reboot Dialogs** - Removed reboot confirmation from upload process (moved to separate reboot function)
- **Missing POST Route** - Added POST route for `/update` endpoint to handle multipart file uploads
- **Upload Reliability** - Enhanced error handling for network disconnections during device reboot
- **JavaScript Upload Function** - Separated upload and reboot functionality for cleaner user experience

### Changed
- **Upload Process** - Reboot confirmation now only appears when manually rebooting device
- **Progress Tracking** - Upload progress now correctly displays from 0% to 100%
- **Error Handling** - Better distinction between actual failures and expected connection drops

### Technical Details
- Fixed routing mismatch where JavaScript sent to `/update` but server only handled `/update/start`
- Moved reboot confirmation from `uploadUpdate()` function to separate `rebootDevice()` function
- Enhanced network error handling to properly detect successful uploads that result in device reboot
- Maintained real-time compatibility and non-blocking operations

## [1.2.1] - 2025-08-27

### Added
- **Heap Monitoring** - Automatic memory checks before updates to prevent insufficient memory failures
- **Enhanced Rollback Validation** - Sector-by-sector firmware integrity validation for safer rollbacks
- **Configurable Restart Delays** - Customizable reboot timing with `setRestartDelay()` method
- **Safer SPIFFS Handling** - Configurable SPIFFS formatting behavior to prevent data loss
- **Advanced Configuration API** - New methods for fine-tuning library behavior:
  - `setRestartDelay(unsigned long delayMs)` - Configure restart delay (default: 3000ms)
  - `setSpiffsFormatOnFailure(bool allowFormat)` - Control automatic SPIFFS formatting (default: true)
- **Enhanced Security Options** - Better control over SPIFFS data protection
- **Improved Error Handling** - More comprehensive error detection and recovery

### Changed
- **SPIFFS Mount Logic** - More conservative formatting approach to preserve user data
- **Rollback Process** - Enhanced validation with sector-by-sector integrity checks
- **Update Process** - Added heap monitoring before starting updates
- **Library Metadata** - Updated version information and feature descriptions
- **Debug Output** - Enhanced logging for new features and validation steps

### Fixed
- **SPIFFS Data Loss** - Fixed potential data loss during SPIFFS mount failures
- **Memory-Related Failures** - Added heap monitoring to prevent out-of-memory update failures
- **Rollback Reliability** - Enhanced validation prevents rollback to corrupted firmware
- **Timing Issues** - Configurable restart delays for different application requirements

### Security
- **SPIFFS Protection** - New option to disable automatic formatting for maximum data security
- **Enhanced Validation** - Multiple layers of firmware integrity checking
- **Memory Safety** - Heap monitoring prevents memory exhaustion attacks

### Technical Details
- Heap monitoring requires minimum 32KB free memory for updates
- Sector-by-sector validation checks first 32KB of firmware for ESP32 image integrity
- Configurable restart delays maintain real-time operation compatibility
- SPIFFS formatting behavior is now user-controllable for data protection

## [1.2.0] - 2025-08-07

### Added
- **Automatic Backup Detection** - New `autoDetectBackup()` function automatically detects firmware in inactive partitions
- **Enhanced Rollback Support** - Improved rollback functionality for serial upload → OTA workflow compatibility
- **Force Backup Detection** - Rollback now attempts auto-detection if no backup metadata exists
- **Version Information API** - Added static functions to query library version and feature support:
  - `getLibraryVersion()`, `getVersionMajor()`, `getVersionMinor()`, `getVersionPatch()`
  - `getBuildInfo()` for build timestamp information
  - Feature availability checks: `supportsFilesystemUpdate()`, `supportsAutoBackupDetection()`, etc.
- **Enhanced Debug Output** - Added comprehensive version and feature reporting during initialization
- **Improved Partition Analysis** - Better detection and validation of ESP32 dual partition system

### Changed
- **Backup Metadata Saving** - `saveUpdateInfo()` now proactively saves current firmware as backup before OTA update
- **Rollback Process** - Enhanced `restoreFromBackup()` with fallback auto-detection mechanism
- **Library Identification** - Updated library.json and library.properties with new version and enhanced descriptions
- **Debug Messaging** - More detailed logging for backup detection and rollback operations

### Fixed
- **Serial Upload Compatibility** - Fixed rollback not working after serial upload followed by OTA update
- **Backup Detection Edge Cases** - Resolved issues where valid backup firmware wasn't detected
- **Metadata Persistence** - Improved backup metadata saving and validation

### Technical Details
- Automatic detection checks ESP32 image header in inactive partition
- Enhanced preference storage for backup metadata
- Better error handling and fallback mechanisms
- Maintains real-time operation compatibility throughout

## [1.1.0] - 2024

### Added
- **Filesystem Update Support** - Added SPIFFS filesystem update capability
- **Enhanced Partition Display** - Comprehensive partition table analysis and display
- **Update Type Selection** - Web interface now supports both firmware and filesystem updates
- **SPIFFS Partition Detection** - Automatic detection of SPIFFS partition availability

### Changed
- **Web Interface** - Enhanced OTA page with update type selection
- **Update Process** - Differentiated handling for firmware vs filesystem updates
- **Error Handling** - Improved error reporting and debug output

## [1.0.0] - 2025-07-31

### Added
- **ESP32 Dual Partition System** - Automatic firmware backup using ESP32's built-in dual OTA partitions
- **Modern Web Interface** - Drag & drop upload with real-time progress tracking
- **Infinite Rollback Support** - Switch between partitions indefinitely
- **Real-Time Compatibility** - Non-blocking operations (no delay() calls)
- **Partition Tables** - Pre-configured partition tables for 4MB, 8MB, and 16MB flash
- **Debug Control System** - Configurable debug output via `CUSTOM_OTA_DEBUG` build flag
- **HTTP Authentication** - Optional username/password protection
- **Build Timestamp Display** - Shows compile date/time in web interface
- **Comprehensive Documentation** - Complete setup guide with partition requirements

### Features
- Dual OTA partition backup system
- Modern responsive web interface
- Real-time upload progress
- Automatic partition detection
- Rollback functionality
- Build timestamp tracking
- Debug logging system
- Authentication support

### Dependencies
- AsyncTCP ^3.2.3
- ESPAsyncWebServer ^3.1.1
- Arduino ESP32 Core

### Requirements
- ESP32 with dual OTA partitions configured
- Minimum 4MB flash memory
- PlatformIO or Arduino IDE
