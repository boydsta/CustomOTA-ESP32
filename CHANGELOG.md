# Changelog

All notable changes to the CustomOTA library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
