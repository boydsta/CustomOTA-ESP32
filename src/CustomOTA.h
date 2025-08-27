/*
 * CustomOTA Library Header for ESP32
 * Version: 1.2.1
 * 
 * Author: Boyd Benton
 * Created: 2025
 * Last Updated: August 2025
 * 
 * Features:
 * - Dual firmware/filesystem OTA updates
 * - Automatic backup detection and rollback
 * - ESP32 dual partition support
 * - Real-time operation compatibility
 * - SPIFFS filesystem updates
 * - Web-based update interface
 * - Authentication support
 * - Progress tracking and status reporting
 * - Heap monitoring and enhanced rollback validation
 * - Configurable restart delays and SPIFFS handling
 * 
 * Version History:
 * v1.2.1 (August 2025):
 *   - Added heap monitoring for update safety
 *   - Enhanced rollback validation with sector-by-sector checks
 *   - Configurable restart delays for different applications
 *   - Safer SPIFFS handling with format control
 *   - Advanced configuration options
 * 
 * v1.2.0 (August 2025):
 *   - Added automatic backup detection
 *   - Enhanced rollback functionality
 *   - Improved serial upload → OTA workflow compatibility
 *   - Better partition table analysis
 *   - Force backup detection for edge cases
 * 
 * v1.1.0:
 *   - Added filesystem (SPIFFS) update support
 *   - Enhanced partition table display
 *   - Improved error handling and debug output
 * 
 * v1.0.0:
 *   - Initial release with firmware OTA updates
 *   - Basic rollback functionality
 *   - Real-time compatibility design
 * 
 * This library performs real-time operations. Any delay() calls will
 * interfere with critical timing requirements. All functions in this
 * library are designed to be non-blocking and real-time compatible.
 * 
 * Real-time Compatibility Rules:
 * 1. NEVER call delay() - use millis() based timing instead
 * 2. NEVER use blocking operations in main thread  
 * 3. Use deferred/scheduled operations for time-sensitive tasks
 * 4. Keep all functions non-blocking and return quickly
 * 5. All timing operations use non-blocking millis() comparisons
 * 
 * Debug Output Control:
 * - Set CUSTOM_OTA_DEBUG=1 in platformio.ini build_flags to enable debug output
 * - Set CUSTOM_OTA_DEBUG=0 in platformio.ini build_flags to disable debug output
 * - Debug macros are prefixed with COTA_DEBUG_ to avoid conflicts with other libraries
 * 
 * Created by Boyd Benton
 * Maintains real-time compatibility throughout all operations
 */

#ifndef CUSTOMOTA_H
#define CUSTOMOTA_H

// Library version information
#define CUSTOMOTA_VERSION_MAJOR 1
#define CUSTOMOTA_VERSION_MINOR 2
#define CUSTOMOTA_VERSION_PATCH 1
#define CUSTOMOTA_VERSION_STRING "1.2.1"
#define CUSTOMOTA_BUILD_DATE __DATE__
#define CUSTOMOTA_BUILD_TIME __TIME__

// Feature flags
#define CUSTOMOTA_SUPPORT_FILESYSTEM_UPDATE 1
#define CUSTOMOTA_SUPPORT_AUTO_BACKUP_DETECTION 1
#define CUSTOMOTA_SUPPORT_DUAL_PARTITION 1
#define CUSTOMOTA_SUPPORT_REALTIME_OPERATIONS 1

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// Debug macros - controlled by CUSTOM_OTA_DEBUG build flag
#if CUSTOM_OTA_DEBUG
    #define COTA_DEBUG_PRINT(x) Serial.print(x)
    #define COTA_DEBUG_PRINTLN(x) Serial.println(x)
    #define COTA_DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
    #define COTA_DEBUG_PRINT(x)
    #define COTA_DEBUG_PRINTLN(x)
    #define COTA_DEBUG_PRINTF(fmt, ...)
#endif

class CustomOTA {
private:
    AsyncWebServer* _server;
    String _username;
    String _password;
    bool _authenticated;
    Preferences _prefs;
    String _firmwareVersion;
    String _hardwareID;
    String _buildTimestamp;
    
    // Rollback support
    bool _hasValidBackup;
    size_t _backupSize;
    
    // Update status
    String _updateStatus;
    int _updateProgress;
    bool _updateInProgress;
    int _updateType; // 0 = firmware, 1 = filesystem
    
    // Restart management for real-time systems
    bool _restartPending;
    unsigned long _restartTime;
    unsigned long _restartDelay; // Configurable restart delay in milliseconds
    bool _allowSpiffsFormat; // Control SPIFFS formatting on mount failure
    
    // Generate HTML page
    String generateOTAPage();
    String generateStatusPage();
    
    // Handle update process
    void handleUpdateStart(AsyncWebServerRequest *request);
    void handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void handleReboot(AsyncWebServerRequest *request);
    void handleRollback(AsyncWebServerRequest *request);
    void handleStatus(AsyncWebServerRequest *request);
    
    // Backup management
    bool createBackup();
    bool restoreFromBackup();
    bool autoDetectBackup();
    void saveUpdateInfo();
    void loadUpdateInfo();
    
    // SPIFFS partition checking
    bool checkSpiffsPartition();

public:
    CustomOTA();
    void begin(AsyncWebServer* server, const char* username, const char* password);
    void begin(AsyncWebServer* server); // No authentication version
    void setCredentials(const char* username, const char* password);
    void setFirmwareVersion(const char* version);
    void setHardwareID(const char* id);
    void setBuildTimestamp(const char* timestamp);
    void setRestartDelay(unsigned long delayMs); // Set restart delay for firmware updates
    void setSpiffsFormatOnFailure(bool allowFormat); // Control SPIFFS formatting behavior on mount failure
    
    // Version information functions
    static String getLibraryVersion() { return String(CUSTOMOTA_VERSION_STRING); }
    static int getVersionMajor() { return CUSTOMOTA_VERSION_MAJOR; }
    static int getVersionMinor() { return CUSTOMOTA_VERSION_MINOR; }
    static int getVersionPatch() { return CUSTOMOTA_VERSION_PATCH; }
    static String getBuildInfo() { return String(CUSTOMOTA_BUILD_DATE) + " " + String(CUSTOMOTA_BUILD_TIME); }
    
    // Feature availability checks
    static bool supportsFilesystemUpdate() { return CUSTOMOTA_SUPPORT_FILESYSTEM_UPDATE; }
    static bool supportsAutoBackupDetection() { return CUSTOMOTA_SUPPORT_AUTO_BACKUP_DETECTION; }
    static bool supportsDualPartition() { return CUSTOMOTA_SUPPORT_DUAL_PARTITION; }
    static bool supportsRealtimeOperations() { return CUSTOMOTA_SUPPORT_REALTIME_OPERATIONS; }
    
    // Call this after setFirmwareVersion to refresh backup status
    void refreshBackupStatus();
    
    // REAL-TIME CRITICAL: This function must be called in main loop
    // NO delay() calls allowed - uses non-blocking timing for all operations
    void handle();
    
    // Status functions - all non-blocking and real-time safe
    bool isUpdateInProgress() { return _updateInProgress; }
    String getUpdateStatus() { return _updateStatus; }
    int getUpdateProgress() { return _updateProgress; }
    bool shouldRestart() { return _restartPending && (millis() >= _restartTime); }
    
    // IMPORTANT: Non-blocking restart check - call from main loop
    // Uses millis() comparison - NO delay() calls ever!
    void checkRestart();
};

#endif
