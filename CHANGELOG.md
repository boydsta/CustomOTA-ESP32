# Changelog

All notable changes to the CustomOTA library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
