/*
 * CustomOTA Library Header for ESP32
 * Version: 1.5.0
 *
 * Author: Boyd Benton
 * Created: 2025
 * Last Updated: October 2026
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
 * - Robust NVS error handling
 * - Serial upload boot partition correction
 * 
 * Version History:
 * v1.5.0 (October 2026):
 *   - Update gate: setUpdateAllowed(allowed, reason) lets the application block updates,
 *     reboot, rollback and partition changes while it is not safe to restart (e.g. a
 *     vehicle that is running). Blocked requests get 409 + reason, an upload in progress
 *     is aborted, and a pending restart is held until updates are allowed again.
 *     Default is allowed, so existing applications are unaffected.
 *
 * v1.4.0 (October 2026):
 *   - Partition editor: change the partition table over the air from the OTA page
 *     (-D CUSTOM_OTA_PARTITIONS=1, off by default - with it off the code is the same as 1.3)
 *
 * v1.3.0 (October 2026):
 *   - Security: uploads are authenticated before anything is written, and
 *     setCredentials() followed by begin(server) no longer disables authentication
 *   - OTA page is served from flash (no heap copy); device details come from /update/status
 *   - Firmware update, reboot and rollback reply first, then restart after setRestartDelay()
 *   - Fixed /update/status routing, filesystem uploads going to the firmware partition,
 *     missing error responses, progress never reaching 100%, SPIFFS remount formatting
 *     the new image, rollback target selection and the CUSTOM_OTA_DEBUG flag
 *   - Build date is set on every PlatformIO build; hardware ID is shown on the page
 *
 * v1.2.4 (August 2025):
 *   - Fixed serial upload boot partition inconsistency
 *   - Added automatic boot partition correction after serial uploads
 *   - Prevents device from booting to wrong partition after serial upload
 *   - Enhanced partition state validation on startup
 * 
 * v1.2.3 (August 2025):
 *   - Fixed NVS error handling to prevent "backup_ver NOT_FOUND" errors
 *   - Added proper key existence checking before NVS operations
 *   - Improved error handling for missing backup metadata
 *   - Enhanced robustness for first-run scenarios
 * 
 * v1.2.2 (August 2025):
 *   - Fixed critical upload progress bug (progress bar stuck at 0%)
 *   - Fixed routing mismatch for multipart file uploads
 *   - Removed unexpected reboot dialogs from upload process
 *   - Enhanced network error handling for device reboots
 * 
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
#define CUSTOMOTA_VERSION_MINOR 5
#define CUSTOMOTA_VERSION_PATCH 0
#define CUSTOMOTA_VERSION_STRING "1.5.0"
#define CUSTOMOTA_BUILD_DATE __DATE__
#define CUSTOMOTA_BUILD_TIME __TIME__

// Feature flags
#define CUSTOMOTA_SUPPORT_FILESYSTEM_UPDATE 1
#define CUSTOMOTA_SUPPORT_AUTO_BACKUP_DETECTION 1
#define CUSTOMOTA_SUPPORT_DUAL_PARTITION 1
#define CUSTOMOTA_SUPPORT_REALTIME_OPERATIONS 1

// Partition editor - off by default so projects that do not use it pay nothing for it.
// Enable with -D CUSTOM_OTA_PARTITIONS=1 in platformio.ini build_flags.
#ifndef CUSTOM_OTA_PARTITIONS
#define CUSTOM_OTA_PARTITIONS 0
#endif
#define CUSTOMOTA_SUPPORT_PARTITION_EDITOR CUSTOM_OTA_PARTITIONS

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#if CUSTOM_OTA_PARTITIONS
#include "CustomOTAPartitions.h"
#endif

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
    
    // Upload result, reported by handleUpdateComplete() once the body has been received
    bool _uploadError;
    bool _uploadComplete;
    int _lastReportedProgress;

    // Restart management for real-time systems
    bool _restartPending;
    unsigned long _restartRequestedAt; // millis() when the restart was scheduled
    unsigned long _restartDelay; // Configurable restart delay in milliseconds
    bool _allowSpiffsFormat; // Control SPIFFS formatting on mount failure

    // Update gate - set by the application from its main loop, read by the web handlers
    // (AsyncTCP task), hence volatile. The reason is a pointer to a static string.
    volatile bool _updateAllowed;
    const char* volatile _blockedReason;
    bool _uploadBlocked; // Current upload was refused or aborted by the gate
    const char* blockedReason();

    // SPIFFS mount status cache - avoid repeated mounting
    bool _spiffsMounted;
    bool _hasSpiffsPartition;
    
    // Handle update process
    void handleUpdateComplete(AsyncWebServerRequest *request);
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
    
    // SPIFFS mount status helper
    bool isSpiffsMounted();

#if CUSTOM_OTA_PARTITIONS
    // Partition editor: an uploaded table is held here until it is applied or cancelled
    CustomOTAPartitions::Table _ptPending;
    bool _ptHasPending;
    uint8_t* _ptUpload;      // partitions.bin being received
    size_t _ptUploadLen;
    String _ptUploadErr;

    void handlePartitionsInfo(AsyncWebServerRequest *request);
    void handlePartitionsUploadDone(AsyncWebServerRequest *request);
    void handlePartitionsUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void handlePartitionsApply(AsyncWebServerRequest *request);
    void handlePartitionsCancel(AsyncWebServerRequest *request);
#endif

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

    // Allow or block updates, reboot, rollback and partition changes (default: allowed).
    // While blocked: new requests get 409 + reason, an upload in progress is aborted at its
    // next chunk (the running firmware is untouched), and a restart that is already
    // scheduled is held until allowed again. Call it from the main loop with the
    // application's own "safe to restart" condition.
    // reason must be a string literal or other static string, without double quotes or
    // backslashes (it is sent as-is in the /update/status JSON). nullptr = default text.
    void setUpdateAllowed(bool allowed, const char* reason = nullptr);
    bool isUpdateAllowed() { return _updateAllowed; }

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
    static bool supportsPartitionEditor() { return CUSTOMOTA_SUPPORT_PARTITION_EDITOR; }
    
    // Call this after setFirmwareVersion to refresh backup status
    void refreshBackupStatus();
    
    // REAL-TIME CRITICAL: This function must be called in main loop
    // NO delay() calls allowed - uses non-blocking timing for all operations
    void handle();
    
    // Status functions - all non-blocking and real-time safe
    bool isUpdateInProgress() { return _updateInProgress; }
    String getUpdateStatus() { return _updateStatus; }
    int getUpdateProgress() { return _updateProgress; }
    bool isRestartPending() { return _restartPending; }
    // A held restart (updates blocked) only runs once updates are allowed again
    bool shouldRestart() { return _updateAllowed && _restartPending && (millis() - _restartRequestedAt >= _restartDelay); }
    
    // HEAP OPTIMIZATION: Memory monitoring functions
    size_t getFreeHeap() { return ESP.getFreeHeap(); }
    size_t getTotalHeap() { return ESP.getHeapSize(); }
    size_t getHeapUsagePercent() { return (100 - ((ESP.getFreeHeap() * 100) / ESP.getHeapSize())); }
    bool isHeapCritical() { return ESP.getFreeHeap() < 32768; } // Less than 32KB is critical
    bool isHeapWarning() { return ESP.getFreeHeap() < 65536; }  // Less than 64KB is warning
    
    // IMPORTANT: Non-blocking restart check - call from main loop
    // Uses millis() comparison - NO delay() calls ever!
    void checkRestart();
};

#endif
