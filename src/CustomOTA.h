/*
 * CustomOTA Library Header for ESP32
 * 
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

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <esp_ota_ops.h>

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
