# Changelog

All notable changes to the CustomOTA library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.1.0] - 2025-01-15

### Added
- **Build Timestamp Display** - Shows compile date/time in web interface
- **setBuildTimestamp() method** - Optional manual build timestamp override
- **Automatic Build Info** - Uses `__DATE__` and `__TIME__` compiler macros by default
- **Enhanced Status API** - JSON status endpoint now includes build timestamp

### Changed
- Updated examples to demonstrate build timestamp usage
- Enhanced web interface with build information display

## [2.0.0] - 2025-07-30

### Added
- **ESP32 Dual Partition System Integration** - Automatic firmware backup using ESP32's built-in dual OTA partitions
- **Debug Control System** - Configurable debug output via `CUSTOM_OTA_DEBUG` build flag
- **Infinite Rollback Support** - Ability to switch between partitions indefinitely
- **Real-Time Compatibility** - Non-blocking operations designed for time-critical applications
- **Smart Partition Detection** - Automatic detection of firmware on inactive partitions
- **Emergency Recovery Mode** - Enhanced error handling and recovery options
- **Modern Web Interface** - Completely redesigned UI with drag & drop functionality
- **Simplified Interface** - Removed hardware ID, backup status details for cleaner UI
- **Partition Status Display** - Shows which partitions contain valid firmware
- **Comprehensive Documentation** - Detailed README with implementation examples

### Changed
- **BREAKING**: Rollback system now uses ESP32 dual partition system instead of manual backups
- **BREAKING**: Removed manual backup creation - now automatic via partition system
- **BREAKING**: Debug output now controlled by build flags instead of runtime switches
- **BREAKING**: Simplified web interface - removed hardware ID, backup status details
- Improved error handling and user feedback
- Enhanced partition status reporting with firmware detection for both partitions
- Updated web interface styling and responsiveness
- Updated authorship

### Removed
- Manual backup creation functionality (replaced by automatic partition system)
- Runtime debug control (replaced by build-time flags)
- Legacy backup metadata system

### Fixed
- Memory leaks in upload handling
- Race conditions in restart management
- Macro conflicts with other libraries (using COTA_DEBUG_ prefix)
- First-time upload failures with proper error recovery

### Security
- Enhanced file validation for uploaded firmware
- Improved authentication handling
- Better error message sanitization

## [1.0.0] - 2025-01-15

### Added
- Initial release of CustomOTA library
- Basic OTA firmware update functionality
- Web interface for file uploads
- Manual backup and rollback system
- HTTP Basic Authentication support
- Real-time progress tracking
- SPIFFS filesystem update support
- Custom styling integration

### Features
- Modern web interface with drag & drop
- Firmware and filesystem update support
- Manual reboot functionality
- Basic rollback system using preferences
- Upload progress tracking
- System information display
- Error handling and recovery

### Dependencies
- AsyncTCP ^3.2.3
- ESPAsyncWebServer ^3.1.1
- Arduino ESP32 Core

---

## Migration Guide

### From v1.x to v2.x

**Key Changes:**
1. **Backup System**: No longer need to manually create backups - ESP32 handles this automatically
2. **Debug Control**: Use build flags instead of runtime settings
3. **Infinite Rollback**: Can now rollback between partitions indefinitely

**Required Changes:**
```cpp
// OLD v1.x code
ota.setDebugOutput(true);  // Remove this line

// NEW v2.x code - use platformio.ini instead
// build_flags = -DCUSTOM_OTA_DEBUG=1
```

**New Features Available:**
- Infinite rollback between partitions
- Automatic backup status detection
- Enhanced debug system with zero performance impact when disabled
- Improved real-time compatibility

**No Breaking Changes in API:**
- All existing `begin()`, `setCredentials()`, etc. methods work the same
- Web interface remains compatible
- Upload process unchanged
