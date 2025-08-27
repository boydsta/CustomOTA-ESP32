# CustomOTA Library - AI Agent Instructions

This guide helps AI agents understand the key patterns and workflows in the CustomOTA library codebase. CustomOTA is a standalone, reusable ESP32 library for OTA updates, rollback, and filesystem management.

## 🎯 Library Overview

CustomOTA is an independent ESP32 library that provides:
- Firmware and SPIFFS updates via web interface
- Rollback support with automatic backup detection
- Real-time compatibility (no blocking operations)
- Flexible authentication and configuration
- Comprehensive error handling and validation

## 🔑 Key Architecture Components

### 1. Core Update System
- Non-blocking OTA updates with progress tracking
- Dual-partition firmware management
- SPIFFS filesystem updates
- Automatic backup detection and rollback

### 2. Web Interface
- Dynamic HTML generation for OTA page
- Authentication-protected endpoints
- Real-time status updates
- Partition table visualization

### 3. Real-Time Compatibility
- No `delay()` calls anywhere in the codebase
- Deferred restarts using millis-based timing
- Non-blocking operations throughout
- Minimal resource usage

## 💻 Development Workflow

### Build System
- Uses PlatformIO with ESP32 Arduino framework
- Key build flags in `platformio.ini` control debug output:
  ```ini
  build_flags = 
    -D CUSTOM_OTA_DEBUG=0
    # ... other debug flags



    lib_deps = 
    mathieucarbou/AsyncTCP@^3.2.3
    mathieucarbou/ESPAsyncWebServer@^3.1.1


    Monitor serial output: pio device monitor
Enable debug flags in platformio.ini
Check web interface at device's IP address
3. Integration
Include CustomOTA.h in your project
Initialize with CustomOTA otaUpdater;
Call otaUpdater.handle() in main loop
⚠️ Important Considerations
Real-Time Critical

NEVER use delay() calls
Use non-blocking patterns for timing
All functions must return quickly
Error Handling

Comprehensive validation of partitions
Graceful degradation for missing components
Clear error messages and recovery paths
Partition Management

Robust detection of otadata, app0, app1, SPIFFS
Validation before operations
Fallback mechanisms for failures
Memory Management

Minimal heap usage
Efficient string handling
Resource cleanup
🔧 Library-Specific Guidelines
Core Principles
Library Independence: CustomOTA operates as a self-contained module. It should not modify, assume, or depend on host application logic.
Maturity and Robustness: Handle all edge cases, including partition mismatches, corrupt data, network failures, and hardware variations.
Simplicity: Keep code clean, modular, and well-documented. Avoid over-engineering.
Non-Interference: Maintain real-time compatibility by using non-blocking operations. Ensure the library integrates seamlessly without disrupting host application timing or resources.
Development Best Practices
Error Handling: Every operation must include error checking and graceful degradation.
Partition Management: Robustly detect and validate partitions.
Update Process: Support firmware and filesystem updates with progress tracking.
Rollback Logic: Implement bidirectional rollback with validation.
Web Interface: Generate dynamic, cache-busting HTML/JS for the OTA page.
Real-Time Compatibility: All functions must return quickly. Use millis-based timing for deferred actions.
Testing and Validation: Test on various ESP32 boards, partition tables, and scenarios.
Documentation: Maintain clear comments, function descriptions, and usage examples.
Minimal Dependencies: Rely only on core ESP32/Arduino libraries.
Versioning: Use semantic versioning and maintain backward compatibility where possible.
Integration Guidelines
Host Application Integration: Provide simple APIs that require minimal setup.
Configuration: Use flexible options via constructor parameters or setters.
Eventualities: Handle scenarios like missing partitions, network interruptions, insufficient memory.
Fallbacks: If an operation fails, provide clear recovery paths.
By adhering to these guidelines, CustomOTA remains a reliable, versatile library suitable for diverse ESP32 applications.

