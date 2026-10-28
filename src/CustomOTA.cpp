
/*
 * CustomOTA Library for ESP32
 * 
 * CRITICAL DESIGN REQUIREMENT: NEVER USE delay() ANYWHERE!
 * 
 * This device performs real-time operations and any blocking delay() calls
 * will interfere with the critical timing requirements. All timing operations
 * must use non-blocking approaches with millis() comparisons.
 * 
 * Real-time Compatibility Rules:
 * 1. NEVER call delay() - use millis() based timing instead
 * 2. NEVER use blocking operations in main thread
 * 3. Use deferred/scheduled operations for time-sensitive tasks
 * 4. Keep all functions non-blocking and return quickly
 * 
 * Created by Boyd Benton
 * Maintains real-time compatibility throughout all operations
 */
#ifndef CUSTOM_OTA_DEBUG
#define CUSTOM_OTA_DEBUG 0
#endif
#include "CustomOTA.h"
#include <esp_partition.h>

// OTA web page, defined further down
extern const char COTA_OTA_PAGE[];

// Helper to check for otadata partition
bool hasOtaDataPartition() {
    const esp_partition_t* otadata = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    return (otadata != nullptr);
}

bool isSingleOtaPartition = false;
void detectOtaPartitionMode() {
    int count = 0;
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    if (it) { count++; esp_partition_iterator_release(it); }
    it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (it) { count++; esp_partition_iterator_release(it); }
    isSingleOtaPartition = (count < 2);
}

/**
 * @brief Find partition by label dynamically
 * @param label The partition label to find
 * @return Pointer to the partition, or nullptr if not found
 */
const esp_partition_t* find_partition_by_label(const char* label) {
  // Get all APP partitions
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);

  const esp_partition_t* found_part = nullptr;
  while (it != nullptr) {
    const esp_partition_t* part = esp_partition_get(it);
    if (part && strcmp(part->label, label) == 0) {
      found_part = part;
      break;
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);

  return found_part;
}
// Helper function to format build date from __DATE__ to dd/mm/yyyy format
String formatBuildDate() {
#ifdef COTA_BUILD_TIMESTAMP
    // Set on every PlatformIO build by build_timestamp.py, already dd/mm/yyyy hh:mm:ss
    return String(COTA_BUILD_TIMESTAMP);
#else
    // Fallback (e.g. Arduino IDE): when this file was last compiled
    // HEAP OPTIMIZATION: Use const char* and minimize String operations
    const char* date = __DATE__;  // Format: "Jul 30 2025"
    const char* time = __TIME__;  // Format: "14:30:25"
    
    // Month name to number mapping - use const array for efficiency
    static const char* months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    // Parse the date string using more efficient approach
    char month[4];
    strncpy(month, date, 3);
    month[3] = '\0';
    
    // Extract day and year using pointer arithmetic
    const char* dayPtr = date + 4;
    const char* yearPtr = date + 7;
    
    // Skip leading space in day if present
    if (*dayPtr == ' ') dayPtr++;
    
    // Find month number efficiently
    int monthNum = 1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(month, months[i]) == 0) {
            monthNum = i + 1;
            break;
        }
    }
    
    // Parse day number
    int dayNum = atoi(dayPtr);
    
    // Build result using snprintf for efficiency
    char result[32];
    snprintf(result, sizeof(result), "%02d/%02d/%s %s", dayNum, monthNum, yearPtr, time);

    return String(result);
#endif
}

CustomOTA::CustomOTA() {
    // Initialize all variables - NO DELAY CALLS EVER!
    // This constructor must complete quickly for real-time compatibility
    _authenticated = false;
    _updateInProgress = false;
    _updateProgress = 0;
    _updateStatus = "Ready";
    _hasValidBackup = false;
    _backupSize = 0;
    _updateType = 0; // Default to firmware
    _uploadError = false;
    _uploadComplete = false;
    _lastReportedProgress = -1;
    _lastUploadFailed = false;
    _lastChunkAt = 0;
    _uploadTimeout = 30000; // Default 30 seconds without data
    _server = nullptr;
    _restartPending = false;
    _restartRequestedAt = 0;
    _restartDelay = 3000; // Default 3 seconds
    _allowSpiffsFormat = true; // Default to allowing format for compatibility
    _updateAllowed = true; // Default allowed - only applications that call setUpdateAllowed() are gated
    _blockedReason = nullptr;
    _uploadBlocked = false;

    // Initialize SPIFFS cache variables
    _spiffsMounted = false;
    _hasSpiffsPartition = checkSpiffsPartition();

#if CUSTOM_OTA_PARTITIONS
    _ptHasPending = false;
    _ptUpload = nullptr;
    _ptUploadLen = 0;
#endif

    // Set default build timestamp in dd/mm/yyyy format (will be overridden if setBuildTimestamp is called)
    _buildTimestamp = formatBuildDate();
}

void CustomOTA::begin(AsyncWebServer* server, const char* username, const char* password) {
    _server = server;

    // Detect partition mode before anything else
    detectOtaPartitionMode();
    
    // CRITICAL FIX: Clear any stale OTA boot partition setting after serial upload
    // This prevents the device from booting to the wrong partition after serial upload
    if (!isSingleOtaPartition) {
        const esp_partition_t* running = esp_ota_get_running_partition();
        const esp_partition_t* boot_partition = esp_ota_get_boot_partition();

        COTA_DEBUG_PRINTLN("CustomOTA: Checking boot partition state...");
        COTA_DEBUG_PRINTF("CustomOTA: Running partition: %s\n", running ? running->label : "UNKNOWN");
        COTA_DEBUG_PRINTF("CustomOTA: Boot partition: %s\n", boot_partition ? boot_partition->label : "UNKNOWN");

        if (running && boot_partition && strcmp(running->label, boot_partition->label) != 0) {
            COTA_DEBUG_PRINTLN("CustomOTA: WARNING - Boot partition mismatch detected after serial upload");
            COTA_DEBUG_PRINTF("CustomOTA: Running: %s, Boot set to: %s\n", running->label, boot_partition->label);
            COTA_DEBUG_PRINTLN("CustomOTA: Correcting boot partition to match running partition");

            // Set boot partition to match the currently running partition
            esp_err_t err = esp_ota_set_boot_partition(running);
            if (err == ESP_OK) {
                COTA_DEBUG_PRINTLN("CustomOTA: Boot partition corrected successfully");
                COTA_DEBUG_PRINTF("CustomOTA: Boot partition now set to: %s\n", running->label);
            } else {
                COTA_DEBUG_PRINTF("CustomOTA: ERROR - Failed to correct boot partition: %s\n", esp_err_to_name(err));
            }
        } else if (running && boot_partition) {
            COTA_DEBUG_PRINTLN("CustomOTA: Boot partition state is correct");
        } else {
            COTA_DEBUG_PRINTLN("CustomOTA: WARNING - Could not determine partition state");
        }
    }
    
    COTA_DEBUG_PRINTLN("==========================================");
    COTA_DEBUG_PRINTF("CustomOTA Library v%s\n", CUSTOMOTA_VERSION_STRING);
    COTA_DEBUG_PRINTF("Build: %s %s\n", CUSTOMOTA_BUILD_DATE, CUSTOMOTA_BUILD_TIME);
    COTA_DEBUG_PRINTF("Features: Firmware=%s, Filesystem=%s, AutoBackup=%s, RealTime=%s\n",
                       CUSTOMOTA_SUPPORT_DUAL_PARTITION ? "Yes" : "No",
                       CUSTOMOTA_SUPPORT_FILESYSTEM_UPDATE ? "Yes" : "No", 
                       CUSTOMOTA_SUPPORT_AUTO_BACKUP_DETECTION ? "Yes" : "No",
                       CUSTOMOTA_SUPPORT_REALTIME_OPERATIONS ? "Yes" : "No");
    COTA_DEBUG_PRINTLN("==========================================");
    
    if (username != nullptr && password != nullptr && strlen(username) > 0 && strlen(password) > 0) {
        setCredentials(username, password);
    }
    // Keep credentials set earlier via setCredentials() - begin(server) must not wipe them
    _authenticated = _username.length() > 0 && _password.length() > 0;
    if (_authenticated) {
        COTA_DEBUG_PRINTLN("CustomOTA: Authentication enabled");
        COTA_DEBUG_PRINTF("CustomOTA: Username: %s\n", _username.c_str());
    } else {
        COTA_DEBUG_PRINTLN("CustomOTA: No authentication (open access)");
    }
    
    // Initialize SPIFFS for filesystem updates (only if not already initialized)
    COTA_DEBUG_PRINTLN("CustomOTA: Checking SPIFFS status...");
    if (!SPIFFS.begin(false)) {
        COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS mount failed");
        
        if (_allowSpiffsFormat) {
            COTA_DEBUG_PRINTLN("CustomOTA: Attempting SPIFFS format and remount...");
            // Try to format SPIFFS only if it's truly corrupted/unusable
            // This preserves user data in normal operation scenarios
            if (SPIFFS.format() && SPIFFS.begin(false)) {
                COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS formatted and mounted successfully");
                COTA_DEBUG_PRINTLN("CustomOTA: WARNING - SPIFFS was corrupted and has been formatted (user data may be lost)");
                _spiffsMounted = true;
            } else {
                COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS format failed - filesystem updates unavailable");
                _spiffsMounted = false;
            }
        } else {
            COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS format disabled - filesystem updates unavailable");
            COTA_DEBUG_PRINTLN("CustomOTA: Use setSpiffsFormatOnFailure(true) to enable automatic formatting");
            _spiffsMounted = false;
        }
    } else {
        COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS already mounted by main application");
        _spiffsMounted = true;
    }
    
    // Initialize preferences
    COTA_DEBUG_PRINTLN("CustomOTA: Initializing preferences...");
    _prefs.begin("customota", false);
    loadUpdateInfo();
    
    COTA_DEBUG_PRINTLN("CustomOTA: Setting up web routes...");
    // Setup web routes
    // NOTE: AsyncWebServer treats "/update" as matching "/update/<anything>", and handlers
    // are checked in registration order, so the specific /update/* routes must come first
    _server->on("/update/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleReboot(request);
    });
    
    _server->on("/update/rollback", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleRollback(request);
    });
    
    _server->on("/update/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleStatus(request);
    });
    
#if CUSTOM_OTA_PARTITIONS
    // apply and cancel before /update/partitions, which would also match them
    _server->on("/update/partitions/apply", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePartitionsApply(request);
    });

    _server->on("/update/partitions/cancel", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePartitionsCancel(request);
    });

    _server->on("/update/partitions", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handlePartitionsInfo(request);
    });

    _server->on("/update/partitions", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePartitionsUploadDone(request);
    }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handlePartitionsUpload(request, filename, index, data, len, final);
    });
#endif

    _server->on("/update/start", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleUpdateComplete(request);
    }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handleUpdateUpload(request, filename, index, data, len, final);
    });

    COTA_DEBUG_PRINTLN("CustomOTA: Registering /update route");
    _server->on("/update", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
            return request->requestAuthentication();
        }
        // Streamed straight from flash - no heap copy of the page
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html",
            (const uint8_t*)COTA_OTA_PAGE, strlen(COTA_OTA_PAGE));
        // The page must not be cached, so a firmware update with a new UI is always picked up
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });

    // Multipart upload route
    _server->on("/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleUpdateComplete(request);
    }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handleUpdateUpload(request, filename, index, data, len, final);
    });
}

void CustomOTA::setCredentials(const char* username, const char* password) {
    _username = String(username);
    _password = String(password);
    _authenticated = true;
}

void CustomOTA::begin(AsyncWebServer* server) {
    // No authentication version - calls main begin with empty credentials
    begin(server, "", "");
}

void CustomOTA::setFirmwareVersion(const char* version) {
    _firmwareVersion = String(version);
    // Refresh backup status now that we have the firmware version
    // Use isKey() to avoid NVS errors when key doesn't exist
    if (_prefs.isKey("backup_ver")) {
        loadUpdateInfo();
    }
}

void CustomOTA::setHardwareID(const char* id) {
    _hardwareID = String(id);
}

void CustomOTA::setBuildTimestamp(const char* timestamp) {
    _buildTimestamp = String(timestamp);
}

void CustomOTA::setRestartDelay(unsigned long delayMs) {
    _restartDelay = delayMs;
    COTA_DEBUG_PRINTF("CustomOTA: Restart delay set to %lu ms\n", delayMs);
}

void CustomOTA::setUploadTimeout(unsigned long timeoutMs) {
    _uploadTimeout = timeoutMs;
    COTA_DEBUG_PRINTF("CustomOTA: Upload timeout set to %lu ms\n", timeoutMs);
}

void CustomOTA::setSpiffsFormatOnFailure(bool allowFormat) {
    _allowSpiffsFormat = allowFormat;
    COTA_DEBUG_PRINTF("CustomOTA: SPIFFS format on failure set to: %s\n", allowFormat ? "enabled" : "disabled");
}

void CustomOTA::setUpdateAllowed(bool allowed, const char* reason) {
    // Called every loop by gated applications - only log changes
    if (allowed != _updateAllowed) {
        COTA_DEBUG_PRINTF("CustomOTA: Updates %s\n", allowed ? "allowed" : "blocked");
    }
    // Reason first, so a handler that sees the gate closed also sees its reason
    _blockedReason = reason;
    _updateAllowed = allowed;
}

const char* CustomOTA::blockedReason() {
    const char* reason = _blockedReason;
    return reason ? reason : "Updates are disabled by the application";
}

void CustomOTA::refreshBackupStatus() {
    COTA_DEBUG_PRINTLN("CustomOTA: Refreshing backup status...");
    loadUpdateInfo();
}

// HEAP OPTIMIZATION: The OTA page is a constant in flash and is streamed straight from there,
// so serving it costs no heap. Device-specific values are filled in by the page's script
// from /update/status.
const char COTA_OTA_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Over The Air Update</title>
    <script>document.write('<link rel="stylesheet" href="/styles.css?v=' + Date.now() + '">');</script>
    <style>
        /* Base styles - fallback if external CSS fails */
        body {
            font-size: 20px;
            text-align: left;
            height: 100%;
            font-family: "Times New Roman", Times, serif;
            margin: 0;
            padding: 15px;
            background-color: white;
            box-sizing: border-box;
        }
        
        .button-container {
            display: flex;
            align-items: center;
            margin-bottom: 20px;
        }
        
        .button {
            display: inline-block;
            border: 2px solid #000;
            background-color: #007BFF;
            color: white;
            padding: 3px 5px;
            font-size: 15px;
            text-decoration: none;
            width: 150px;
            font-weight: bold;
            text-align: center;
            border-radius: 8px;
            transition: background-color 0.3s;
            margin: 0 10px;
            font-family: "Times New Roman", Times, serif;
        }
        
        .button:hover {
            background-color: #0056b3;
        }
        
        .grey-button {
            width: 14ch;
            font-size: 20px;
            padding: 5px;
            margin: 10px 5px;
            box-sizing: border-box;
            text-align: center;
            text-decoration: none;
            background-color: #f0f0f0;
            color: black;
            border: 2px solid #000;
            border-radius: 5px;
            cursor: pointer;
            font-family: "Times New Roman", Times, serif;
        }
        
        .grey-button:hover {
            background-color: #a5a3a3;
        }
        
        .grey-button:disabled {
            background-color: #d0d0d0;
            color: #666;
            cursor: not-allowed;
        }
        
        .display-box {
            border: 2px solid #000;
            padding: 20px;
            margin: 20px 0;
            font-size: 18px;
            background-color: #f0f0f0;
            border-radius: 8px;
            width: calc(100% - 44px); /* Full width minus padding and border */
            box-sizing: border-box;
        }
        
        .full-width-box {
            border: 2px solid #000;
            padding: 25px;
            margin: 20px 0;
            font-size: 18px;
            background-color: #f0f0f0;
            border-radius: 8px;
            width: 100%;
            box-sizing: border-box;
        }
        
        hr {
            width: 100%;
            border: 1px solid black;
            margin: 20px 0;
        }
        
        h1 {
            font-size: 28px;
            margin-bottom: 20px;
        }
        
        h2 {
            font-size: 24px;
            margin: 20px 0 15px 0;
        }
        
        /* Additional OTA-specific styles */
        .upload-area {
            border: 2px solid #000;
            border-radius: 8px;
            padding: 30px;
            text-align: center;
            margin: 20px 0;
            background-color: #f7f7f7;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        
        .upload-area:hover {
            background-color: #e0e0e0;
        }
        
        .upload-area.dragover {
            background-color: #e0e0e0;
            border-color: #007BFF;
        }
        
        .file-input {
            display: none;
        }
        
        .bar-container {
            position: relative;
            width: 100%;
            height: 30px;
            background-color: #e0e0e0;
            border: 2px solid #000;
            border-radius: 5px;
            margin: 20px 0;
        }
        
        .bar {
            height: 100%;
            background-color: #ffd500;
            width: 0%;
            border-radius: 3px;
            transition: width 0.3s ease-in-out;
        }
        
        .bar-text {
            position: absolute;
            top: 0;
            left: 50%;
            transform: translateX(-50%);
            font-size: 16px;
            font-weight: bold;
            color: black;
            line-height: 30px;
        }
        
        .status {
            padding: 10px;
            margin: 15px 0;
            border-radius: 5px;
            border: 2px solid #000;
            font-weight: bold;
        }
        
        .status.success {
            background-color: #d4edda;
            color: #155724;
        }
        
        .status.error {
            background-color: #f8d7da;
            color: #721c24;
        }
        
        .status.info {
            background-color: #d1ecf1;
            color: #0c5460;
        }
        
        .status-good {
            color: #28a745;
            font-weight: bold;
        }
        
        .status-warning {
            color: #ffc107;
            font-weight: bold;
        }
        
        .status-error {
            color: #dc3545;
            font-weight: bold;
        }
        
        .radio-group {
            margin: 20px 0;
        }
        
        .radio-group label {
            display: block;
            margin-bottom: 15px;
            font-size: 20px;
            padding: 10px 0;
        }
        
        .radio-group input[type="radio"] {
            margin-right: 15px;
            transform: scale(1.5);
        }
    </style>
</head>
<body>
    <div class="button-container">
        <button class="button" onclick="window.location.href='/'">Home</button>
    </div>
    
    <h1 id="deviceName">CustomOTA Update</h1>
    
    <div class="full-width-box">
        <h2>Firmware Information</h2>
        <p><strong>Version:</strong> <span id="fwVersion"></span></p>
        <p><strong>Build Date:</strong> <span id="fwBuild"></span></p>
        <p><strong>Running Partition:</strong> <span id="fwRunning"></span></p>
        <p><strong>Backup Partition:</strong> <span id="fwBackup"></span></p>
    </div>
    
    <hr>
    
    <div class="full-width-box">
        <h2>Update Type Selection</h2>
        <div class="radio-group">
            <label>
                <input type="radio" name="updateType" value="firmware" checked onchange="updateTypeChanged()"> 
                <strong>Firmware Update</strong> - Update the main application code (.bin file)
            </label>
            <label id="fsOption" style="display: none;">
                <input type="radio" name="updateType" value="filesystem" onchange="updateTypeChanged()">
                <strong>Filesystem Update</strong> - Update web files, data, and configuration (filesystem.bin file)
            </label>
            <label id="fsUnavailable" style="display: none; color: #666; font-style: italic;">
                <input type="radio" disabled>
                <strong>Filesystem Update</strong> - Not available (no SPIFFS partition found)
            </label>
        </div>
    </div>
    
    <div class="upload-area" onclick="document.getElementById('updateFile').click();">
        <h2 id="uploadTitle">Select Firmware File</h2>
        <p id="uploadDescription">Click here to browse for your .bin file</p>
        <input type="file" id="updateFile" class="file-input" accept=".bin" onchange="handleFileSelect(this)">
    </div>
    
    <div class="bar-container" id="progressContainer" style="display: none;">
        <div class="bar" id="progressFill"></div>
        <div class="bar-text" id="progressText">0%</div>
    </div>
    
    <div id="statusMessage" class="status" style="display: none;"></div>

    <div id="blockedMessage" class="status error" style="display: none;"></div>

    <div class="button-container">
        <button class="grey-button" id="uploadBtn" onclick="uploadUpdate()" disabled>
            Upload
        </button>
        <button class="grey-button" id="rollbackBtn" style="display: none;" onclick="rollbackFirmware()">
            Rollback
        </button>
        <button class="grey-button" id="noBackupBtn" style="display: none;" onclick="alert('Rollback functionality available - ESP32 dual partition system automatically preserves previous firmware')" title="ESP32 dual partition system automatically preserves previous firmware - rollback available when other partition has valid firmware">
            Enable Rollback
        </button>
        <button class="grey-button" id="rebootBtn" onclick="rebootDevice()">
            Reboot
        </button>
    </div>
)rawliteral"
#if CUSTOM_OTA_PARTITIONS
R"rawliteral(
    <hr>

    <style>
        .pt-table { border-collapse: collapse; width: 100%; font-size: 16px; margin: 10px 0; }
        .pt-table th, .pt-table td { border: 1px solid #000; padding: 4px 8px; text-align: left; }
        .pt-table td.num { font-family: monospace; }
        .pt-table tr.pt-running td { font-weight: bold; }
        .pt-table tr.pt-changed td { background-color: #fff3cd; }
        .pt-steps li { margin-bottom: 10px; }
        .pt-steps code, .pt-box code { background-color: #e8e8e8; padding: 0 3px; }
        .pt-list li { margin-bottom: 6px; }
    </style>

    <div class="full-width-box" id="ptSection">
        <h2>Partition Table (advanced)</h2>

        <div id="ptFsNeeded" class="status error" style="display: none;">
            The partition table was changed and the filesystem is now empty, so this device's own web pages are missing.
            Select <strong>Filesystem Update</strong> above and upload the filesystem image built for the new layout
            (<code>.pio/build/&lt;env&gt;/spiffs.bin</code>).
        </div>

        <p>The partition table sets how the flash is split between firmware (app slots), the filesystem (spiffs)
           and saved settings (nvs). Change it only to make room - for example a bigger firmware slot.
           <strong>Read all the steps before starting.</strong></p>

        <h3>Current table</h3>
        <table class="pt-table" id="ptCurrent"></table>
        <p style="font-size: 16px;">Running firmware: <strong id="ptRunning"></strong>
           (<span id="ptSketch"></span>). Flash: <span id="ptFlash"></span>.</p>

        <details>
            <summary><strong>How to change the partition table - step by step</strong></summary>
            <ol class="pt-steps">
                <li><strong>Make the new table.</strong> Create a new .csv next to your old one (keep the old one), point
                    <code>board_build.partitions</code> at it in platformio.ini, and build. PlatformIO writes the table to
                    <code>.pio/build/&lt;env&gt;/partitions.bin</code>. Also build the filesystem image for the new layout:
                    <code>pio run -t buildfs -e &lt;env&gt;</code> gives <code>.pio/build/&lt;env&gt;/spiffs.bin</code>.</li>
                <li><strong>Check where the firmware is running.</strong> After the change the device starts the firmware at
                    the start of the new <code>ota_0</code> slot, so the running firmware must already be there. The check in
                    step 3 tells you if it is not; then upload the current firmware once more (it runs from the other slot
                    afterwards) and start again at step 3.</li>
                <li><strong>Upload partitions.bin below.</strong> Nothing is written yet. The device checks the new table and
                    shows what changes and what will be lost.</li>
                <li><strong>Apply.</strong> The device rewrites the table and restarts on the new layout. Writing takes well
                    under a second - <strong>do not remove power while it happens</strong>. Settings are kept if nvs did not
                    move.</li>
                <li><strong>Upload the filesystem image</strong> (Filesystem Update above) if the filesystem moved or changed
                    size - its files are gone until you do. This update page keeps working without it because it is
                    built into the firmware. The first start after the change can take up to about 20 seconds while
                    the new filesystem is formatted.</li>
                <li><strong>Now upload the bigger firmware.</strong> The new slot size is the limit from here on.</li>
            </ol>
            <p style="font-size: 16px;">If the device does not start after the change, reflash it over USB:
               <code>pio run -t upload</code> then <code>pio run -t uploadfs</code>.</p>
        </details>

        <div class="upload-area" onclick="document.getElementById('ptFile').click();">
            <h2>Select partitions.bin</h2>
            <p>Click here to browse for the new partition table (.pio/build/&lt;env&gt;/partitions.bin)</p>
            <input type="file" id="ptFile" class="file-input" accept=".bin" onchange="ptUpload(this)">
        </div>

        <div id="ptPreview" style="display: none;">
            <h3>New table <span style="font-size: 16px; font-weight: normal;">(changed rows highlighted)</span></h3>
            <table class="pt-table" id="ptNew"></table>
            <div id="ptErrorsBox" class="status error" style="display: none;">
                Cannot apply this table:
                <ul class="pt-list" id="ptErrors"></ul>
            </div>
            <div id="ptWarningsBox" class="status info" style="display: none;">
                Applying this table will:
                <ul class="pt-list" id="ptWarnings"></ul>
            </div>
            <div class="button-container">
                <button class="grey-button" id="ptApplyBtn" onclick="ptApply()" disabled>Apply</button>
                <button class="grey-button" onclick="ptCancel()">Cancel</button>
            </div>
        </div>

        <div id="ptStatus" class="status" style="display: none;"></div>
    </div>

    <script>
        let ptInfo = null;

        function ptHex(n) { return '0x' + n.toString(16).toUpperCase(); }
        function ptSize(n) {
            return (n >= 1048576 && n % 1048576 === 0) ? (n / 1048576) + ' MB' : Math.round(n / 1024) + ' KB';
        }
        function ptSame(a, b) {
            return a.type === b.type && a.subtype === b.subtype && a.offset === b.offset && a.size === b.size;
        }

        function ptStatus(message, type) {
            const el = document.getElementById('ptStatus');
            el.textContent = message;
            el.className = 'status ' + (type || 'info');
            el.style.display = message ? 'block' : 'none';
        }

        // Rows are built with textContent - labels come from the uploaded file
        function ptRenderTable(id, rows, compareTo, runningOffset) {
            const table = document.getElementById(id);
            table.innerHTML = '';
            const head = table.insertRow();
            ['Name', 'Type', 'Subtype', 'Offset', 'Size'].forEach(h => {
                const th = document.createElement('th');
                th.textContent = h;
                head.appendChild(th);
            });
            rows.forEach(p => {
                const tr = table.insertRow();
                if (p.offset === runningOffset && p.type === 'app') tr.className = 'pt-running';
                if (compareTo && !compareTo.some(c => ptSame(c, p))) tr.className += ' pt-changed';
                const name = p.label + (p.offset === runningOffset && p.type === 'app' ? ' (running)' : '');
                [name, p.type, p.subtype, ptHex(p.offset), ptSize(p.size)].forEach((v, i) => {
                    const td = tr.insertCell();
                    td.textContent = v;
                    if (i >= 3) td.className = 'num';
                });
            });
        }

        function ptFillList(id, items) {
            const ul = document.getElementById(id);
            ul.innerHTML = '';
            items.forEach(t => {
                const li = document.createElement('li');
                li.textContent = t;
                ul.appendChild(li);
            });
            ul.parentElement.style.display = items.length ? 'block' : 'none';
        }

        function ptShow(info) {
            ptInfo = info;
            ptRenderTable('ptCurrent', info.current, null, info.runningOffset);
            document.getElementById('ptRunning').textContent = info.running + ' at ' + ptHex(info.runningOffset);
            document.getElementById('ptSketch').textContent = ptSize(info.sketchSize) + ' firmware';
            document.getElementById('ptFlash').textContent = ptSize(info.flashSize);
            document.getElementById('ptFsNeeded').style.display = info.fsNeeded ? 'block' : 'none';

            const preview = document.getElementById('ptPreview');
            if (!info.pending) {
                preview.style.display = 'none';
                return;
            }
            preview.style.display = 'block';
            ptRenderTable('ptNew', info.pending, info.current, info.runningOffset);
            ptFillList('ptErrors', info.errors);
            ptFillList('ptWarnings', info.warnings);
            document.getElementById('ptApplyBtn').disabled = info.errors.length > 0 || info.restartPending;
            ptStatus(info.errors.length ? 'Fix the problems listed above, then upload partitions.bin again.'
                                        : 'Checks passed. Read what will be lost, then press Apply.',
                     info.errors.length ? 'error' : 'info');
        }

        function ptLoad() {
            fetch('/update/partitions', { cache: 'no-store' })
                .then(r => r.ok ? r.json() : r.text().then(t => { throw new Error(t); }))
                .then(ptShow)
                .catch(e => ptStatus('Could not read the partition table: ' + e.message, 'error'));
        }
        document.addEventListener('DOMContentLoaded', ptLoad);

        function ptUpload(input) {
            const file = input.files && input.files[0];
            input.value = '';
            if (!file) return;
            if (!file.name.toLowerCase().endsWith('.bin')) {
                ptStatus('Select partitions.bin', 'error');
                return;
            }
            const form = new FormData();
            form.append('table', file);
            ptStatus('Checking ' + file.name + '...', 'info');
            fetch('/update/partitions', { method: 'POST', body: form })
                .then(r => r.ok ? r.json() : r.text().then(t => { throw new Error(t); }))
                .then(ptShow)
                .catch(e => ptStatus(e.message, 'error'));
        }

        function ptApply() {
            if (!ptInfo || !ptInfo.pending) return;
            let message = '\n\nCHANGE THE PARTITION TABLE?\n\n' +
                '- The device rewrites its partition table and restarts\n' +
                '- DO NOT remove power until it is back\n' +
                '- If it does not come back, it must be reflashed over USB\n';
            if (ptInfo.warnings.length) {
                message += '\nThis will:\n- ' + ptInfo.warnings.join('\n- ') + '\n';
            }
            message += '\nContinue?';
            if (!confirm(message)) return;

            document.getElementById('ptApplyBtn').disabled = true;
            ptStatus('Writing the partition table - do not remove power...', 'info');
            fetch('/update/partitions/apply', { method: 'POST' })
                .then(r => r.text().then(t => {
                    if (!r.ok) throw new Error(t);
                    ptStatus(t + ' This page reloads when the device is back.', 'success');
                    reloadWhenRestarted();
                }))
                .catch(e => {
                    ptStatus('Apply failed: ' + e.message, 'error');
                    ptLoad();
                });
        }

        function ptCancel() {
            fetch('/update/partitions/cancel', { method: 'POST' })
                .then(r => r.text())
                .then(t => { ptStatus(t, 'info'); ptLoad(); })
                .catch(e => ptStatus(e.message, 'error'));
        }
    </script>
)rawliteral"
#endif
R"rawliteral(
    <p id="libVersion" style="font-size: 12px; color: #888;"></p>

    <script>
        let cotaSelectedFile = null;
        let cotaUpdateType = 'firmware';
        let cotaUpdateBlocked = false;
        let cotaUploading = false;    // This page is uploading - its own XHR reports progress
        let cotaRemoteUpload = false; // Device reports an upload this page did not start

        // Uploads this page did not start (another browser, or one cut off before a reload)
        // and the last failed upload, so it is clear whether to wait or try again.
        // The failure is shown on load and when a watched upload ends, not on every poll,
        // so it doesn't keep overwriting the page's own messages.
        function applyUploadState(s, onLoad) {
            if (cotaUploading) return;
            const container = document.getElementById('progressContainer');
            if (s.inProgress) {
                cotaRemoteUpload = true;
                container.style.display = 'block';
                updateProgress(s.progress);
                showStatus('An upload is in progress (' + s.progress + '%) - wait for it to finish', 'info');
            } else if (onLoad || cotaRemoteUpload) {
                const watched = cotaRemoteUpload;
                cotaRemoteUpload = false;
                container.style.display = 'none';
                if (s.lastUploadFailed) {
                    showStatus('Last upload failed: ' + s.status, 'error');
                } else if (watched) {
                    // Finished on the device: reload to show the new version
                    showStatus(s.status, 'success');
                    if (s.restartPending) reloadWhenRestarted();
                    else setTimeout(() => window.location.reload(), 3000);
                }
            }
        }

        // Update gate (setUpdateAllowed): show the reason and grey out the buttons while
        // blocked. The device refuses blocked requests anyway - this just says why up front.
        function applyGate(s) {
            cotaUpdateBlocked = (s.updateAllowed === false);
            const msg = document.getElementById('blockedMessage');
            msg.textContent = cotaUpdateBlocked ? 'Updates blocked: ' + (s.blockedReason || 'disabled by the application') : '';
            msg.style.display = cotaUpdateBlocked ? 'block' : 'none';
            document.getElementById('uploadBtn').disabled = cotaUpdateBlocked || cotaRemoteUpload || cotaUploading || !cotaSelectedFile;
            ['rebootBtn', 'rollbackBtn', 'ptApplyBtn'].forEach(id => {
                const b = document.getElementById(id);
                if (b && cotaUpdateBlocked) b.disabled = true;
                else if (b && id !== 'ptApplyBtn') b.disabled = false; // ptApplyBtn is managed by ptShow()
            });
        }

        // The gate and upload state can change while the page is open (e.g. key turned on,
        // or an upload from another browser), so keep checking
        function pollGate() {
            fetch('/update/status', { cache: 'no-store' })
                .then(r => r.json())
                .then(s => { applyUploadState(s, false); applyGate(s); })
                .catch(() => {})
                .finally(() => setTimeout(pollGate, 5000));
        }

        // Device details come from /update/status so the page itself can be served from flash
        function loadDeviceInfo() {
            fetch('/update/status', { cache: 'no-store' })
                .then(r => r.json())
                .then(s => {
                    if (s.hardwareID) {
                        document.getElementById('deviceName').textContent = s.hardwareID;
                        document.title = s.hardwareID + ' - OTA Update';
                    }
                    document.getElementById('libVersion').textContent = 'CustomOTA v' + s.libraryVersion;
                    document.getElementById('fwVersion').textContent = s.version || 'Unknown';
                    document.getElementById('fwBuild').textContent = s.buildTimestamp || 'Unknown';
                    document.getElementById('fwRunning').textContent = s.runningPartition || 'UNKNOWN';
                    document.getElementById('fwBackup').textContent = s.backupPartition || 'None';
                    document.getElementById(s.hasSpiffsPartition ? 'fsOption' : 'fsUnavailable').style.display = '';
                    if (s.canRollback) {
                        document.getElementById(s.hasBackup ? 'rollbackBtn' : 'noBackupBtn').style.display = '';
                    }
                    applyUploadState(s, true);
                    applyGate(s);
                })
                .catch(() => showStatus('Could not load device information', 'error'))
                .finally(() => setTimeout(pollGate, 5000));
        }
        document.addEventListener('DOMContentLoaded', loadDeviceInfo);

        // Reload once the device has restarted: the old firmware reports restartPending
        // until it goes down, the restarted device reports false
        function reloadWhenRestarted() {
            setTimeout(function poll() {
                fetch('/update/status', { cache: 'no-store' })
                    .then(r => r.json())
                    .then(s => { if (s.restartPending) setTimeout(poll, 2000); else window.location.reload(); })
                    .catch(() => setTimeout(poll, 2000));
            }, 2000);
        }

        function updateTypeChanged() {
            const selectedEl = document.querySelector('input[name="updateType"]:checked');
            cotaUpdateType = (selectedEl && selectedEl.value) ? selectedEl.value : 'firmware';
            
            const titleElement = document.getElementById('uploadTitle');
            const descElement = document.getElementById('uploadDescription');
            const buttonElement = document.getElementById('uploadBtn');
            
            if (titleElement && descElement && buttonElement) {
                if (cotaUpdateType === 'firmware') {
                    titleElement.textContent = 'Select Firmware File';
                    descElement.textContent = 'Click here to browse for your firmware .bin file';
                    buttonElement.textContent = 'Upload';
                } else {
                    titleElement.textContent = 'Select Filesystem File';
                    descElement.textContent = 'Click here to browse for your filesystem.bin file';
                    buttonElement.textContent = 'Upload';
                }
                
                // Reset file selection
                cotaSelectedFile = null;
                const fileInput = document.getElementById('updateFile');
                if (fileInput) fileInput.value = '';
                buttonElement.disabled = true;
            }
        }
        
        function handleFileSelect(input) {
            const cotaFile = (input && input.files && input.files.length > 0) ? input.files[0] : null;
            if (cotaFile && cotaFile.name && cotaFile.name.toLowerCase().endsWith('.bin')) {
                cotaSelectedFile = cotaFile;
                const uploadBtn = document.getElementById('uploadBtn');
                if (uploadBtn) uploadBtn.disabled = cotaUpdateBlocked || cotaRemoteUpload;
                showStatus('File selected: ' + cotaFile.name + ' (' + cotaUpdateType + ' update)', 'info');
            } else {
                showStatus('Please select a valid .bin file', 'error');
            }
        }
        
        function showStatus(message, type) {
            const statusDiv = document.getElementById('statusMessage');
            if (!statusDiv) return;
            statusDiv.textContent = message;
            statusDiv.className = 'status ' + (type || 'info');
            statusDiv.style.display = 'block';
        }
        
        function updateProgress(percent) {
            const fill = document.getElementById('progressFill');
            const text = document.getElementById('progressText');
            if (fill) fill.style.width = percent + '%';
            if (text) text.textContent = percent + '%';
        }
        
        function uploadUpdate() {
            if (!cotaSelectedFile) {
                showStatus('Please select a file first', 'error');
                return;
            }
            
            // Check if this is a firmware update (will cause reboot)
            const isFirmwareUpdate = cotaUpdateType === 'firmware';
            
            // Show appropriate confirmation dialog based on update type
            let confirmMessage = '';
            if (isFirmwareUpdate) {
                confirmMessage = `

WARNING: This will REBOOT the device!

- The device will restart automatically
- Real-time operations will be interrupted
- Ensure the device is not actively controlling critical systems
- The update takes about 30-60 seconds total

Continue with firmware update?`;
            } else {
                confirmMessage = `

This will NOT reboot the device!

- Web interface files will be updated instantly
- Device continues normal operation
- No interruption to real-time functions
- Just refresh your browser after update

Continue with filesystem update?`;
            }
            
            if (!confirm(confirmMessage)) {
                return; // User cancelled
            }
            
            // 'type' must precede the file: the server reads it when the first file chunk arrives
            const cotaFormData = new FormData();
            cotaFormData.append('type', cotaUpdateType);
            cotaFormData.append('update', cotaSelectedFile);
            
            const cotaXhr = new XMLHttpRequest();
            
            cotaXhr.upload.addEventListener('progress', function(e) {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    updateProgress(percent);
                }
            });
            
            // The device answers before it restarts, so the HTTP status is the real result
            cotaXhr.onload = function() {
                cotaUploading = false;
                document.getElementById('progressContainer').style.display = 'none';
                if (cotaXhr.status >= 200 && cotaXhr.status < 300) {
                    showStatus(cotaXhr.responseText, 'success');
                    if (isFirmwareUpdate) {
                        reloadWhenRestarted();
                    } else {
                        setTimeout(() => window.location.reload(), 3000);
                    }
                } else {
                    showStatus('Upload failed (HTTP ' + cotaXhr.status + '): ' + cotaXhr.responseText, 'error');
                }
            };

            cotaUploading = true;
            document.getElementById('uploadBtn').disabled = true;
            document.getElementById('progressContainer').style.display = 'block';
            updateProgress(0);
            showStatus('Uploading ' + cotaUpdateType + '...', 'info');

            cotaXhr.onerror = function() {
                cotaUploading = false;
                document.getElementById('progressContainer').style.display = 'none';
                showStatus('Upload failed: network connection lost', 'error');
            };

            cotaXhr.ontimeout = function() {
                cotaUploading = false;
                document.getElementById('progressContainer').style.display = 'none';
                showStatus('Upload failed: request timed out', 'error');
            };
            
            // Set a reasonable timeout for the request
            cotaXhr.timeout = 120000; // 2 minutes for large firmware files
            
            // Also pass the type in the URL so it is available regardless of multipart field order
            cotaXhr.open('POST', '/update?type=' + encodeURIComponent(cotaUpdateType), true);
            cotaXhr.send(cotaFormData);
        }
        
        function rebootDevice() {
            if (confirm('Are you sure you want to reboot the device?')) {
                // Show immediate feedback
                showStatus('Sending reboot command...', 'info');
                
                fetch('/update/reboot', { method: 'POST' })
                    .then(response => response.text().then(result => {
                        if (response.ok) {
                            showStatus('Device is restarting...', 'success');
                            reloadWhenRestarted();
                        } else {
                            showStatus('Reboot failed (HTTP ' + response.status + '): ' + result, 'error');
                        }
                    }))
                    .catch(err => showStatus('Reboot failed: ' + err.message, 'error'));
            }
        }
        
        function rollbackFirmware() {
            if (confirm('Are you sure you want to rollback to the previous firmware?')) {
                showStatus('Initiating rollback...', 'info');
                
                fetch('/update/rollback', { method: 'POST' })
                    .then(response => response.text().then(result => {
                        if (response.ok) {
                            showStatus(result, 'success');
                            reloadWhenRestarted();
                        } else {
                            showStatus(result, 'error');
                        }
                    }))
                    .catch(err => showStatus('Rollback failed: ' + err.message, 'error'));
            }
        }
        
    </script>

</body>
</html>
)rawliteral";

void CustomOTA::handleUpdateComplete(AsyncWebServerRequest *request) {
    // AsyncWebServer calls this AFTER handleUpdateUpload() has processed the whole body
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        return request->requestAuthentication();
    }

    if (_uploadBlocked) {
        _lastUploadFailed = true;
        request->send(409, "text/plain", _updateStatus);
    } else if (_uploadError) {
        _lastUploadFailed = true;
        request->send(500, "text/plain", _updateStatus);
    } else if (_uploadComplete) {
        request->send(200, "text/plain", _updateStatus);
    } else {
        // No file part, the body ended before the final chunk, or a stalled upload whose
        // connection came back after handle() aborted it
        if (Update.isRunning()) {
            Update.abort();
        }
        _updateInProgress = false;
        _lastUploadFailed = true;
        _updateStatus = F("Update failed: no complete file received");
        request->send(400, "text/plain", _updateStatus);
    }
    _uploadError = false;
    _uploadComplete = false;
    _uploadBlocked = false;
}

void CustomOTA::handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0) {
        _uploadError = false;
        _uploadComplete = false;
        _uploadBlocked = false;
        _updateInProgress = false;
        _lastUploadFailed = false;

        // Authenticate before writing anything - the request handler only runs after the
        // whole body (and therefore the whole update) has been processed
        if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
            COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Unauthenticated upload rejected");
            return; // _updateInProgress stays false, so remaining chunks are ignored
        }

        if (_restartPending) {
            _updateStatus = F("Device is restarting - try again once it is back");
            _uploadError = true;
            return;
        }

        if (!_updateAllowed) {
            _updateStatus = String(F("Update blocked: ")) + blockedReason();
            _uploadError = true;
            _uploadBlocked = true;
            COTA_DEBUG_PRINTLN("CustomOTA: Upload refused - updates are blocked");
            return; // _updateInProgress stays false, so remaining chunks are ignored
        }

        if (Update.isRunning()) {
            COTA_DEBUG_PRINTLN("CustomOTA: WARNING - Previous update still running, aborting it");
            Update.abort();
        }

        // Get update type from request parameters - try both POST params and URL params
        _updateType = 0; // Default to firmware

        // Check POST parameters first (multipart form data)
        if (request->hasParam("type", true)) {
            const char* typeValue = request->getParam("type", true)->value().c_str();
            if (strcmp(typeValue, "filesystem") == 0) {
                _updateType = 1;
            }
            COTA_DEBUG_PRINTF("CustomOTA: Found type in POST params: %s\n", typeValue);
        }
        // Fallback to URL parameters
        else if (request->hasParam("type", false)) {
            const char* typeValue = request->getParam("type", false)->value().c_str();
            if (strcmp(typeValue, "filesystem") == 0) {
                _updateType = 1;
            }
            COTA_DEBUG_PRINTF("CustomOTA: Found type in URL params: %s\n", typeValue);
        }
        // Additional check - look at filename for hints
        else if (filename.indexOf("filesystem") >= 0 || filename.indexOf("spiffs") >= 0) {
            _updateType = 1;
            COTA_DEBUG_PRINTLN("CustomOTA: Detected filesystem update from filename");
        }
        else {
            COTA_DEBUG_PRINTLN("CustomOTA: No type parameter found, defaulting to firmware");
        }
        
        // Initialize update state immediately. Timestamp first, so handle() never sees
        // an upload in progress with an old timestamp and aborts it as stalled.
        _lastChunkAt = millis();
        _updateInProgress = true;
        _updateProgress = 0;
        _lastReportedProgress = -1;
        
        // HEAP OPTIMIZATION: Use more efficient status string construction
        if (_updateType == 0) {
            _updateStatus = F("Starting firmware update...");
        } else {
            _updateStatus = F("Starting filesystem update...");
        }
        
        COTA_DEBUG_PRINTF("CustomOTA: Update init complete - Type: %s\n", _updateType == 0 ? "firmware" : "filesystem");
    }
    
    // Now proceed with normal upload handling
    if (!_updateInProgress) {
        COTA_DEBUG_PRINTF("CustomOTA: DEBUG - Upload received but _updateInProgress=%s, index=%d, len=%d\n", 
                     _updateInProgress ? "true" : "false", index, len);
        COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Upload received but no update in progress");
        return; // Skip this chunk
    }
    _lastChunkAt = millis(); // Still receiving - not stalled

    // Gate closed mid-upload: abort before writing more. The boot partition is only
    // switched after a complete upload, so the running firmware is untouched.
    if (!_updateAllowed) {
        if (Update.isRunning()) {
            Update.abort();
        }
        _updateInProgress = false;
        _uploadError = true;
        _uploadBlocked = true;
        _updateStatus = String(F("Update aborted: ")) + blockedReason();
        COTA_DEBUG_PRINTLN("CustomOTA: Upload aborted - updates were blocked mid-upload");
        return;
    }

    if (index == 0) {
        COTA_DEBUG_PRINTLN("==========================================");
        COTA_DEBUG_PRINTF("CustomOTA: Update Start: %s (%s)\n", filename.c_str(), _updateType == 0 ? "firmware" : "filesystem");
        
        // HEAP OPTIMIZATION: Check heap before major memory allocation
        size_t freeHeap = ESP.getFreeHeap();
        const size_t minHeapRequired = 32768; // 32KB minimum for OTA operations
        
        COTA_DEBUG_PRINTF("CustomOTA: Free heap before update: %d bytes\n", freeHeap);
        COTA_DEBUG_PRINTF("CustomOTA: Required heap: %d bytes\n", minHeapRequired);
        
        if (freeHeap < minHeapRequired) {
            _updateStatus = F("Insufficient memory for update");
            _updateInProgress = false;
            _uploadError = true;
            COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Insufficient heap for OTA update");
            COTA_DEBUG_PRINTLN("==========================================");
            return;
        }
        
        // Proceed with update if heap is sufficient
        COTA_DEBUG_PRINTLN("CustomOTA: Starting firmware update...");
        // The ESP32 OTA system automatically preserves firmware on the inactive partition
        // We'll update _hasValidBackup after the upload completes
        
        // Start update process based on type
        int cotaUpdateCommand = (_updateType == 0) ? U_FLASH : U_SPIFFS;
        COTA_DEBUG_PRINTF("CustomOTA: Starting update with command: %d\n", cotaUpdateCommand);
        
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cotaUpdateCommand)) {
            _updateStatus = F("Update begin failed");
            _updateInProgress = false;
            _uploadError = true;
            COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Update.begin() failed:");
            #if CUSTOM_OTA_DEBUG
            Update.printError(Serial);
            #endif
            COTA_DEBUG_PRINTLN("==========================================");
            return;
        }
        COTA_DEBUG_PRINTLN("CustomOTA: Update.begin() successful");
        COTA_DEBUG_PRINTLN("==========================================");
    }
    
    // Write data with error checking
    if (len > 0) {
        size_t cotaBytesWritten = Update.write(data, len);
        if (cotaBytesWritten != len) {
            _updateStatus = F("Update write failed");
            _updateInProgress = false;
            _uploadError = true;
            COTA_DEBUG_PRINTF("CustomOTA: ERROR - Write failed: attempted %d bytes, wrote %d bytes\n", len, cotaBytesWritten);
            #if CUSTOM_OTA_DEBUG
            Update.printError(Serial);
            #endif
            Update.abort();
            return;
        }

        // Update progress against the request size - with UPDATE_SIZE_UNKNOWN, Update.size()
        // is the whole partition, so progress would never reach 100%
        size_t total = request->contentLength();
        if (total > 0) {
            _updateProgress = (int)min((size_t)100, (size_t)(((uint64_t)Update.progress() * 100) / total));
        }

        // HEAP OPTIMIZATION: Only update status string when progress changes
        if (_updateProgress != _lastReportedProgress) {
            char statusBuffer[64];
            const char* updateTypeName = _updateType == 0 ? "Firmware" : "Filesystem";
            snprintf(statusBuffer, sizeof(statusBuffer), "Uploading %s... %d%%", updateTypeName, _updateProgress);
            _updateStatus = String(statusBuffer);
            if (_updateProgress / 10 != _lastReportedProgress / 10) {
                COTA_DEBUG_PRINTF("CustomOTA: Upload progress: %d%% (%d bytes)\n", _updateProgress, Update.progress());
            }
            _lastReportedProgress = _updateProgress;
        }
    }
    
    if (final) {
        COTA_DEBUG_PRINTLN("==========================================");
        COTA_DEBUG_PRINTLN("CustomOTA: Final upload chunk received");
        COTA_DEBUG_PRINTF("CustomOTA: Total bytes uploaded: %d\n", index + len);
        COTA_DEBUG_PRINTF("CustomOTA: Free heap after upload: %d bytes\n", ESP.getFreeHeap());
        
        if (Update.end(true)) {
            COTA_DEBUG_PRINTF("CustomOTA: Update Success: %uB\n", index + len);
            
            // HEAP OPTIMIZATION: Use efficient status string construction
            if (_updateType == 0) {
                _updateStatus = F("Firmware update completed successfully");
            } else {
                _updateStatus = F("Filesystem update completed successfully");
            }
            _updateInProgress = false;

            // Handle different update types differently
            if (_updateType == 0) {
                // Record the running firmware as the backup (only firmware updates create one)
                COTA_DEBUG_PRINTLN("CustomOTA: Saving update info...");
                saveUpdateInfo();

                // Firmware update - requires reboot to switch partitions
                COTA_DEBUG_PRINTLN("CustomOTA: Firmware update completed, setting boot partition...");
                const esp_partition_t* cotaNextPartition = esp_ota_get_next_update_partition(nullptr);
                if (cotaNextPartition) {
                    COTA_DEBUG_PRINTF("CustomOTA: Setting boot partition to: %s\n", cotaNextPartition->label);
                    esp_err_t cotaSetErr = esp_ota_set_boot_partition(cotaNextPartition);
                    if (cotaSetErr == ESP_OK) {
                        COTA_DEBUG_PRINTLN("CustomOTA: Boot partition set successfully");
                    } else {
                        COTA_DEBUG_PRINTF("CustomOTA: ERROR - Failed to set boot partition: %s\n", esp_err_to_name(cotaSetErr));
                    }
                } else {
                    COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Could not find next update partition");
                }
                
                // Deferred restart: handleUpdateComplete() sends the response first, and
                // handle() restarts once the delay has passed
                char msg[64];
                snprintf(msg, sizeof(msg), "Firmware update completed. Rebooting in %lu seconds...", _restartDelay / 1000);
                _updateStatus = msg;
                _uploadComplete = true;
                _restartPending = true;
                _restartRequestedAt = millis();
                COTA_DEBUG_PRINTF("CustomOTA: Firmware update - restarting in %lu ms\n", _restartDelay);
            } else {
                // SPIFFS filesystem update - just remount, no reboot needed
                COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS update completed, remounting filesystem...");
                
                // Remount SPIFFS to pick up new files. Never format here - that would
                // erase the image that was just written.
                SPIFFS.end();
                if (SPIFFS.begin(false)) {
                    COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS remounted successfully - new files available");
                    _spiffsMounted = true;
                    _updateStatus = F("Upload completed successfully. Filesystem update completed successfully. Web interface updated - refresh your browser to see changes.");
#if CUSTOM_OTA_PARTITIONS
                    // The filesystem asked for after a partition change has now been uploaded
                    if (_prefs.isKey("pt_fs_needed")) _prefs.remove("pt_fs_needed");
#endif
                } else {
                    COTA_DEBUG_PRINTLN("CustomOTA: WARNING - SPIFFS remount failed, may need manual reboot");
                    _spiffsMounted = false;
                    _updateStatus = F("Upload completed successfully. Filesystem update completed but remount failed. Please reboot device manually if web interface issues occur.");
                }
                // Response is sent by handleUpdateComplete()
                _uploadComplete = true;
                
                // No restart needed for SPIFFS updates
                COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS update complete - no reboot required");
            }
            COTA_DEBUG_PRINTLN("==========================================");
        } else {
            _updateStatus = F("Update failed");
            _updateInProgress = false;
            _uploadError = true;
            COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Update.end() failed:");
            #if CUSTOM_OTA_DEBUG
            Update.printError(Serial);
            #endif
            COTA_DEBUG_PRINTLN("==========================================");
        }
    }
}

void CustomOTA::handleReboot(AsyncWebServerRequest *request) {
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        return request->requestAuthentication();
    }

    if (!_updateAllowed) {
        request->send(409, "text/plain", String(F("Reboot blocked: ")) + blockedReason());
        return;
    }

    // Deferred restart so the response actually reaches the browser - send() only
    // queues it, and restarting here would drop it
    COTA_DEBUG_PRINTLN("CustomOTA: Manual reboot requested, scheduling restart");
    request->send(200, "text/plain", "Rebooting device...");
    _restartPending = true;
    _restartRequestedAt = millis();
}

void CustomOTA::handleRollback(AsyncWebServerRequest *request) {
    // REAL-TIME CRITICAL: NO delay() calls allowed!
    // Uses deferred restart to maintain real-time operations
    
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        return request->requestAuthentication();
    }
    
    COTA_DEBUG_PRINTLN("CustomOTA: Manual rollback requested via web interface");

    // restoreFromBackup() switches the boot partition straight away, so refuse before it
    if (!_updateAllowed) {
        request->send(409, "text/plain", String(F("Rollback blocked: ")) + blockedReason());
        return;
    }

#if CUSTOM_OTA_PARTITIONS
    // After a partition change otadata belongs to the new layout - leave it alone until the restart
    if (_restartPending) {
        request->send(409, "text/plain", "Device is restarting - try again once it is back");
        return;
    }
#endif

    if (restoreFromBackup()) {
        request->send(200, "text/plain", "Rollback successful. Device will reboot to previous firmware in " + String(_restartDelay / 1000) + " seconds...");
        COTA_DEBUG_PRINTLN("CustomOTA: Rollback successful, scheduling restart");
        // IMPORTANT: Non-blocking deferred restart - NO delay() calls!
        _restartPending = true;
        _restartRequestedAt = millis();
    } else {
        String errorMsg = "Rollback failed. ";
        if (!_hasValidBackup) {
            errorMsg += "No backup available. You need to perform at least one successful firmware update before rollback is possible.";
        } else {
            errorMsg += "Unable to set boot partition for rollback.";
        }
        request->send(400, "text/plain", errorMsg);
        COTA_DEBUG_PRINTLN("CustomOTA: Rollback failed");
    }
}

void CustomOTA::handleStatus(AsyncWebServerRequest *request) {
    // HEAP OPTIMIZATION: Use efficient JSON building with minimal String operations
    char jsonBuffer[1024]; // Room for the application's blocked reason
    size_t freeHeap = ESP.getFreeHeap();
    size_t totalHeap = ESP.getHeapSize();
    size_t heapPercent = (freeHeap * 100) / totalHeap;

    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* backup = running ? esp_ota_get_next_update_partition(running) : nullptr;
    bool canRollback = !isSingleOtaPartition && hasOtaDataPartition();

    snprintf(jsonBuffer, sizeof(jsonBuffer),
        "{"
        "\"inProgress\":%s,"
        "\"lastUploadFailed\":%s,"
        "\"progress\":%d,"
        "\"status\":\"%s\","
        "\"hasBackup\":%s,"
        "\"freeHeap\":%zu,"
        "\"totalHeap\":%zu,"
        "\"heapPercent\":%zu,"
        "\"heapCritical\":%s,"
        "\"spiffsMounted\":%s,"
        "\"hasSpiffsPartition\":%s,"
        "\"hardwareID\":\"%s\","
        "\"version\":\"%s\","
        "\"buildTimestamp\":\"%s\","
        "\"runningPartition\":\"%s\","
        "\"backupPartition\":\"%s\","
        "\"canRollback\":%s,"
        "\"restartPending\":%s,"
        "\"updateAllowed\":%s,"
        "\"blockedReason\":\"%s\","
        "\"libraryVersion\":\"" CUSTOMOTA_VERSION_STRING "\""
        "}",
        _updateInProgress ? "true" : "false",
        _lastUploadFailed ? "true" : "false",
        _updateProgress,
        _updateStatus.c_str(),
        _hasValidBackup ? "true" : "false",
        freeHeap,
        totalHeap,
        heapPercent,
        (freeHeap < 32768) ? "true" : "false", // Critical if less than 32KB
        _spiffsMounted ? "true" : "false",
        _hasSpiffsPartition ? "true" : "false",
        _hardwareID.c_str(),
        _firmwareVersion.c_str(),
        _buildTimestamp.c_str(),
        running ? running->label : "",
        (backup && backup != running) ? backup->label : "",
        canRollback ? "true" : "false",
        _restartPending ? "true" : "false",
        _updateAllowed ? "true" : "false",
        _updateAllowed ? "" : blockedReason()
    );
    
    request->send(200, "application/json", jsonBuffer);
}

bool CustomOTA::createBackup() {
    // ESP32 dual partition system automatically handles backup
    // Just update our backup status based on partition contents
    COTA_DEBUG_PRINTLN("CustomOTA: ESP32 dual partition system handles backup automatically");
    
    // Get the other partition (not currently running)
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(running);
    
    if (next && running) {
        COTA_DEBUG_PRINTF("CustomOTA: Running partition: %s, Other partition: %s\n", 
                          running->label, next->label);
        
        // Check if the other partition contains firmware
        uint32_t first_word = 0xFFFFFFFF;
        esp_err_t err = esp_partition_read(next, 0, &first_word, sizeof(first_word));
        
        if (err == ESP_OK && first_word != 0xFFFFFFFF) {
            COTA_DEBUG_PRINTLN("CustomOTA: Other partition contains firmware - backup available");
            _hasValidBackup = true;
            return true;
        } else {
            COTA_DEBUG_PRINTLN("CustomOTA: Other partition empty - no backup available yet");
            _hasValidBackup = false;
            return false;
        }
    }
    
    COTA_DEBUG_PRINTLN("CustomOTA: Could not access partition information");
    _hasValidBackup = false;
    return false;
}

bool CustomOTA::restoreFromBackup() {
    if (!_hasValidBackup) {
        COTA_DEBUG_PRINTLN("CustomOTA: ERROR - No backup available for rollback");
        COTA_DEBUG_PRINTLN("CustomOTA: Checking preferences for backup metadata...");
        
        // Try to reload backup info
        loadUpdateInfo();
        if (!_hasValidBackup) {
            COTA_DEBUG_PRINTLN("CustomOTA: No backup metadata found, attempting forced auto-detection...");
            // Force backup detection for immediate fix
            if (!autoDetectBackup()) {
                COTA_DEBUG_PRINTLN("CustomOTA: Auto-detection failed - no backup firmware found");
                return false;
            }
        }
    }
    
    COTA_DEBUG_PRINTLN("==========================================");
    COTA_DEBUG_PRINTLN("CustomOTA: ROLLBACK INITIATED");
    COTA_DEBUG_PRINTF("CustomOTA: Rolling back to firmware version: %s\n", _prefs.getString("backup_ver", "unknown").c_str());

    // Get current running partition
    const esp_partition_t* cotaRunningPartition = esp_ota_get_running_partition();
    if (!cotaRunningPartition) {
        COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Cannot get current running partition");
        return false;
    }

    // The backup is the other OTA slot
    const esp_partition_t* cotaBackupPartition = esp_ota_get_next_update_partition(cotaRunningPartition);
    if (!cotaBackupPartition || cotaBackupPartition == cotaRunningPartition) {
        COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Cannot find backup partition");
        return false;
    }

    COTA_DEBUG_PRINTF("CustomOTA: Current partition: %s, Backup partition: %s\n",
                      cotaRunningPartition->label, cotaBackupPartition->label);
    
    // Quick check for the ESP32 image magic byte before the full verification below
    uint8_t magic = 0xFF;
    esp_err_t err = esp_partition_read(cotaBackupPartition, 0, &magic, sizeof(magic));
    if (err != ESP_OK || magic != 0xE9) {
        COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Backup partition does not contain valid firmware");
        return false;
    }

    COTA_DEBUG_PRINTF("CustomOTA: Rollback partition: %s\n", cotaBackupPartition->label);

    // Set the backup partition as the boot partition. esp_ota_set_boot_partition() verifies
    // the complete image (segments and checksum/hash) and refuses an invalid one.
    esp_err_t cotaRollbackErr = esp_ota_set_boot_partition(cotaBackupPartition);
    if (cotaRollbackErr != ESP_OK) {
        COTA_DEBUG_PRINTF("CustomOTA: ERROR - Failed to set boot partition: %s\n", esp_err_to_name(cotaRollbackErr));
        return false;
    }
    
    COTA_DEBUG_PRINTLN("CustomOTA: Boot partition set to rollback partition successfully");
    COTA_DEBUG_PRINTLN("CustomOTA: Rollback will take effect after reboot");
    COTA_DEBUG_PRINTLN("==========================================");
    
    // Don't clear backup info yet - keep it until we confirm rollback worked
    // _prefs.remove("backup_version");
    // _prefs.remove("backup_partition");
    // _hasValidBackup = false;
    
    return true;
}

void CustomOTA::saveUpdateInfo() {
    // Get current running partition before the update (this becomes the backup)
    const esp_partition_t* currentPartition = esp_ota_get_running_partition();
    if (currentPartition) {
        // Save the current firmware as backup info BEFORE switching partitions
        _prefs.putString("backup_ver", _firmwareVersion.length() > 0 ? _firmwareVersion : "Previous Firmware");
        _prefs.putString("backup_part", String(currentPartition->label));
        _prefs.putString("backup_valid", "YES");
        _prefs.putUInt("backup_size", currentPartition->size);
        
        COTA_DEBUG_PRINTF("CustomOTA: Saved backup info - Version: %s, Partition: %s\n", 
                         _firmwareVersion.c_str(), currentPartition->label);
        COTA_DEBUG_PRINTLN("CustomOTA: Previous firmware preserved as backup for rollback");
    }
    
    // Also save the update timestamp
    _prefs.putString("last_update", _firmwareVersion);
    _prefs.putULong("update_time", millis());
}

void CustomOTA::loadUpdateInfo() {
    // Check if backup_ver key exists before trying to read it
    // Use isKey() to avoid NVS errors when key doesn't exist
    if (_prefs.isKey("backup_ver")) {
        size_t requiredSize = _prefs.getBytesLength("backup_ver");
        if (requiredSize > 0) {
            String backupVersion = _prefs.getString("backup_ver", "");
            String backupPartition = _prefs.getString("backup_part", "");
            String backupValid = _prefs.getString("backup_valid", "");

            COTA_DEBUG_PRINTF("CustomOTA: Loading backup info - Version: '%s', Partition: '%s', Valid: '%s'\n",
                             backupVersion.c_str(), backupPartition.c_str(), backupValid.c_str());

            if (backupVersion.length() > 0 && backupPartition.length() > 0 && backupValid == "YES") {
                // Additional validation: ensure the backup partition exists and contains firmware
                const esp_partition_t* backup_part = find_partition_by_label(backupPartition.c_str());
                if (backup_part) {
                    // Check if backup partition actually contains firmware
                    uint32_t first_word = 0xFFFFFFFF;
                    esp_err_t err = esp_partition_read(backup_part, 0, &first_word, sizeof(first_word));

                    if (err == ESP_OK && first_word != 0xFFFFFFFF) {
                        _hasValidBackup = true;
                        _backupSize = _prefs.getUInt("backup_size", 0);
                        COTA_DEBUG_PRINTLN("CustomOTA: Valid backup found and verified");
                        COTA_DEBUG_PRINTF("CustomOTA: Backup version: %s, Partition: %s, Size: %d bytes\n",
                                         backupVersion.c_str(), backupPartition.c_str(), _backupSize);
                        return; // Exit early - we found a valid backup
                    } else {
                        COTA_DEBUG_PRINTLN("CustomOTA: Backup partition exists but contains no firmware - clearing invalid metadata");
                        // Clear invalid backup metadata
                        _prefs.remove("backup_ver");
                        _prefs.remove("backup_part");
                        _prefs.remove("backup_valid");
                        _prefs.remove("backup_size");
                    }
                } else {
                    COTA_DEBUG_PRINTLN("CustomOTA: Backup partition not found - clearing invalid metadata");
                    // Clear invalid backup metadata
                    _prefs.remove("backup_ver");
                    _prefs.remove("backup_part");
                    _prefs.remove("backup_valid");
                    _prefs.remove("backup_size");
                }
            } else {
                _hasValidBackup = false;
                COTA_DEBUG_PRINTLN("CustomOTA: Incomplete backup metadata found");
            }
        } else {
            _hasValidBackup = false;
            COTA_DEBUG_PRINTLN("CustomOTA: No previous backup (first run or backup cleared)");
        }
    } else {
        _hasValidBackup = false;
        COTA_DEBUG_PRINTLN("CustomOTA: No backup metadata found in NVS");
    }

    // If no backup found in preferences, try auto-detection (only once)
    if (!_hasValidBackup) {
        COTA_DEBUG_PRINTLN("CustomOTA: No backup metadata found, attempting auto-detection...");
        autoDetectBackup();
    }
}

bool CustomOTA::autoDetectBackup() {
    // Get current running partition
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) {
        COTA_DEBUG_PRINTLN("CustomOTA: Auto-detect failed - cannot get running partition");
        return false;
    }
    
    // The backup is the other OTA slot
    const esp_partition_t* other = esp_ota_get_next_update_partition(running);
    if (!other || other == running) {
        COTA_DEBUG_PRINTLN("CustomOTA: Auto-detect failed - cannot find other partition");
        return false;
    }
    
    // Check if other partition has firmware (check first 4 bytes for ESP32 image header)
    uint32_t first_word = 0xFFFFFFFF;
    esp_err_t err = esp_partition_read(other, 0, &first_word, sizeof(first_word));
    
    if (err == ESP_OK && first_word != 0xFFFFFFFF) {
        // Other partition has firmware - we have a backup!
        COTA_DEBUG_PRINTF("CustomOTA: Auto-detected backup firmware in %s partition\n", other->label);
        COTA_DEBUG_PRINTF("CustomOTA: Current partition: %s, Backup partition: %s\n", running->label, other->label);
        
        // Save this info for future use
        _prefs.putString("backup_part", String(other->label));
        _prefs.putString("backup_valid", "YES");
        _prefs.putString("backup_ver", "Previous Firmware (auto-detected)");
        _prefs.putUInt("backup_size", other->size);
        
        _hasValidBackup = true;
        _backupSize = other->size;
        
        COTA_DEBUG_PRINTLN("CustomOTA: Auto-detection successful - backup metadata saved");
        return true;
    }
    
    COTA_DEBUG_PRINTF("CustomOTA: Auto-detect - other partition (%s) is empty or invalid\n", other->label);
    return false;
}

void CustomOTA::checkRestart() {
    // REAL-TIME CRITICAL FUNCTION: NO delay() calls allowed!
    // This function is called from main loop and must return immediately
    // Uses non-blocking millis() comparison for timing
    
    if (shouldRestart()) {
        COTA_DEBUG_PRINTLN("==========================================");
        COTA_DEBUG_PRINTLN("CustomOTA: Performing deferred restart...");
        COTA_DEBUG_PRINTLN("==========================================");
        COTA_DEBUG_PRINTLN("CustomOTA: Flushing serial before restart...");
        #if CUSTOM_OTA_DEBUG
        Serial.flush(); // Only flush if debugging is enabled
        #endif
        
        COTA_DEBUG_PRINTLN("CustomOTA: REBOOTING NOW!");
        ESP.restart(); // Final restart - unavoidably blocking but necessary
    }
    // Function returns immediately if restart not ready - maintains real-time operation
}

void CustomOTA::handle() {
    // REAL-TIME CRITICAL MAIN LOOP FUNCTION: NO delay() calls allowed!
    // This function is called every loop iteration and must be non-blocking
    // Any blocking operations will interfere with real-time device operations

    // A dropped connection never delivers the final chunk or calls handleUpdateComplete(),
    // so the upload would stay "in progress" until reboot. Abort it once no data has
    // arrived for _uploadTimeout. Clear the flag first so a late chunk is ignored. The boot
    // partition is only switched after a complete upload, so the running firmware is safe.
    // The upload handler runs on the AsyncTCP task and updates _lastChunkAt while this runs.
    // Read it before millis(), and compare signed: if millis() were read first, a chunk
    // landing in between makes _lastChunkAt newer than "now", the unsigned subtraction
    // wraps, and an active upload is aborted as stalled.
    unsigned long lastChunk = _lastChunkAt;
    if (_updateInProgress && (long)(millis() - lastChunk) >= (long)_uploadTimeout) {
        _updateInProgress = false;
        if (Update.isRunning()) {
            Update.abort();
        }
        _lastUploadFailed = true;
        char msg[128];
        if (_updateType == 0) {
            snprintf(msg, sizeof(msg), "Upload stalled at %d%% - not applied, running firmware unchanged. Safe to try again.", _updateProgress);
        } else {
            snprintf(msg, sizeof(msg), "Upload stalled at %d%% - filesystem image incomplete. Upload it again.", _updateProgress);
        }
        _updateStatus = msg;
        COTA_DEBUG_PRINTF("CustomOTA: Upload stalled - no data for %lu ms, aborted\n", _uploadTimeout);
    }

    // Check for pending restart using non-blocking timing
    checkRestart();
    
    // This function completes in microseconds - maintains real-time compatibility
    // All time-sensitive operations use deferred/scheduled approaches
}

bool CustomOTA::checkSpiffsPartition() {
    // Check if SPIFFS partition exists in the partition table
    const esp_partition_t* spiffs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
    return (spiffs_partition != nullptr);
}

/**
 * @brief Check SPIFFS mount status using cached value
 * @return true if SPIFFS is mounted, false otherwise
 */
bool CustomOTA::isSpiffsMounted() {
    return _spiffsMounted;
}

#if CUSTOM_OTA_PARTITIONS

// Current table, the pending upload (if any) with its check results, and what the page
// needs for its instructions
void CustomOTA::handlePartitionsInfo(AsyncWebServerRequest *request) {
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        return request->requestAuthentication();
    }

    CustomOTAPartitions::Table current;
    String err;
    if (!CustomOTAPartitions::load(current, err)) {
        request->send(500, "text/plain", err);
        return;
    }

    const esp_partition_t* running = esp_ota_get_running_partition();
    String json;
    json.reserve(2048);
    json += F("{\"running\":\"");
    json += running ? running->label : "";
    json += F("\",\"runningOffset\":");
    json += String(running ? running->address : 0);
    json += F(",\"sketchSize\":");
    json += String(ESP.getSketchSize());
    json += F(",\"flashSize\":");
    json += String(ESP.getFlashChipSize());
    json += F(",\"fsNeeded\":");
    json += _prefs.isKey("pt_fs_needed") ? F("true") : F("false");
    json += F(",\"restartPending\":");
    json += _restartPending ? F("true") : F("false");
    json += F(",\"current\":");
    CustomOTAPartitions::appendJson(json, current);

    if (_ptHasPending) {
        std::vector<String> errors, warnings;
        CustomOTAPartitions::check(current, _ptPending, errors, warnings);
        json += F(",\"pending\":");
        CustomOTAPartitions::appendJson(json, _ptPending);
        json += F(",\"errors\":[");
        for (size_t i = 0; i < errors.size(); i++) {
            if (i) json += ',';
            CustomOTAPartitions::appendJsonString(json, errors[i]);
        }
        json += F("],\"warnings\":[");
        for (size_t i = 0; i < warnings.size(); i++) {
            if (i) json += ',';
            CustomOTAPartitions::appendJsonString(json, warnings[i]);
        }
        json += ']';
    } else {
        json += F(",\"pending\":null,\"errors\":[],\"warnings\":[]");
    }
    json += '}';

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void CustomOTA::handlePartitionsUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0) {
        free(_ptUpload);
        _ptUpload = nullptr;
        _ptUploadLen = 0;
        _ptUploadErr = "";
        // Authenticate before keeping anything - the request handler runs after the body
        if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
            return;
        }
        _ptUpload = (uint8_t*)malloc(ESP_PARTITION_TABLE_SIZE);
        if (!_ptUpload) {
            _ptUploadErr = F("Out of memory receiving the partition table");
            return;
        }
        COTA_DEBUG_PRINTF("CustomOTA: Receiving partition table %s\n", filename.c_str());
    }
    if (!_ptUpload || _ptUploadErr.length()) return;
    if (_ptUploadLen + len > ESP_PARTITION_TABLE_SIZE) {
        _ptUploadErr = F("This file is too big to be a partition table. Select partitions.bin from .pio/build/<env>/, not firmware.bin or spiffs.bin.");
        return;
    }
    memcpy(_ptUpload + _ptUploadLen, data, len);
    _ptUploadLen += len;
}

void CustomOTA::handlePartitionsUploadDone(AsyncWebServerRequest *request) {
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        free(_ptUpload);
        _ptUpload = nullptr;
        return request->requestAuthentication();
    }

    String err = _ptUploadErr;
    CustomOTAPartitions::Table table;
    if (err.length() == 0) {
        if (!_ptUpload) {
            err = F("No file received");
        } else if (CustomOTAPartitions::parse(_ptUpload, _ptUploadLen, table, err)) {
            _ptPending = table;
            _ptHasPending = true;
        }
    }
    free(_ptUpload);
    _ptUpload = nullptr;
    _ptUploadLen = 0;

    if (err.length()) {
        request->send(400, "text/plain", err);
        return;
    }
    COTA_DEBUG_PRINTF("CustomOTA: Partition table with %u entries waiting to be applied\n", (unsigned)table.size());
    handlePartitionsInfo(request);
}

void CustomOTA::handlePartitionsApply(AsyncWebServerRequest *request) {
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        return request->requestAuthentication();
    }
    if (_restartPending || _updateInProgress || Update.isRunning()) {
        request->send(409, "text/plain", "An update or restart is in progress - try again once it is finished");
        return;
    }
    if (!_updateAllowed) {
        request->send(409, "text/plain", String(F("Partition change blocked: ")) + blockedReason());
        return;
    }
    if (!_ptHasPending) {
        request->send(400, "text/plain", "Upload partitions.bin first");
        return;
    }

    CustomOTAPartitions::Table current;
    String err;
    if (!CustomOTAPartitions::load(current, err)) {
        request->send(500, "text/plain", err);
        return;
    }
    bool fsChanged = CustomOTAPartitions::filesystemChanged(current, _ptPending);

    // Nothing may write to the old filesystem location once the table has changed
    if (fsChanged && _spiffsMounted) {
        SPIFFS.end();
        _spiffsMounted = false;
    }

    if (CustomOTAPartitions::apply(current, _ptPending, err) != ESP_OK) {
        request->send(500, "text/plain", err);
        return;
    }
    _ptHasPending = false;
    _ptPending.clear();

    // The backup slot may have moved; it is auto-detected again after the restart
    _prefs.remove("backup_ver");
    _prefs.remove("backup_part");
    _prefs.remove("backup_valid");
    _prefs.remove("backup_size");
    if (fsChanged) {
        _prefs.putBool("pt_fs_needed", true);
    }

    String msg = "Partition table written. The device restarts on the new layout in " +
                 String(_restartDelay / 1000) + " seconds.";
    if (fsChanged) {
        msg += " The filesystem moved, so upload the filesystem image for the new layout once it is back.";
    }
    request->send(200, "text/plain", msg);
    COTA_DEBUG_PRINTLN("CustomOTA: Partition table changed - restarting");
    _restartPending = true;
    _restartRequestedAt = millis();
}

void CustomOTA::handlePartitionsCancel(AsyncWebServerRequest *request) {
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        return request->requestAuthentication();
    }
    _ptHasPending = false;
    _ptPending.clear();
    request->send(200, "text/plain", "Cancelled - the partition table was not changed");
}

#endif // CUSTOM_OTA_PARTITIONS
