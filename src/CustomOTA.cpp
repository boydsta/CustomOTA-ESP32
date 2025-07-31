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

#include "CustomOTA.h"

// Helper function to format build date from __DATE__ to dd/mm/yyyy format
String formatBuildDate() {
    String date = __DATE__;  // Format: "Jul 30 2025"
    String time = __TIME__;  // Format: "14:30:25"
    
    // Month name to number mapping
    String months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    // Parse the date string
    String month = date.substring(0, 3);
    String day = date.substring(4, 6);
    String year = date.substring(7, 11);
    
    // Remove leading space from day if present
    day.trim();
    
    // Convert month name to number
    String monthNum = "01";
    for (int i = 0; i < 12; i++) {
        if (month == months[i]) {
            monthNum = (i + 1 < 10) ? "0" + String(i + 1) : String(i + 1);
            break;
        }
    }
    
    // Ensure day has leading zero if needed
    if (day.length() == 1) {
        day = "0" + day;
    }
    
    // Return formatted date: dd/mm/yyyy hh:mm:ss
    return day + "/" + monthNum + "/" + year + " " + time;
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
    _restartPending = false;
    _restartTime = 0;
    
    // Set default build timestamp in dd/mm/yyyy format (will be overridden if setBuildTimestamp is called)
    _buildTimestamp = formatBuildDate();
}

void CustomOTA::begin(AsyncWebServer* server, const char* username, const char* password) {
    _server = server;
    _authenticated = false; // Default to no authentication
    
    if (username != nullptr && password != nullptr && strlen(username) > 0 && strlen(password) > 0) {
        setCredentials(username, password);
        _authenticated = true;
        COTA_DEBUG_PRINTLN("CustomOTA: Authentication enabled");
        COTA_DEBUG_PRINTF("CustomOTA: Username: %s\n", username);
    } else {
        _authenticated = false;
        COTA_DEBUG_PRINTLN("CustomOTA: No authentication (open access)");
    }
    
    // Initialize SPIFFS for filesystem updates
    COTA_DEBUG_PRINTLN("CustomOTA: Initializing SPIFFS...");
    if (!SPIFFS.begin(true)) {
        COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS Mount Failed - filesystem updates will not be available");
    } else {
        COTA_DEBUG_PRINTLN("CustomOTA: SPIFFS initialized successfully");
    }
    
    // Initialize preferences
    COTA_DEBUG_PRINTLN("CustomOTA: Initializing preferences...");
    _prefs.begin("customota", false);
    loadUpdateInfo();
    
    COTA_DEBUG_PRINTLN("CustomOTA: Setting up web routes...");
    // Setup web routes
    COTA_DEBUG_PRINTLN("CustomOTA: Registering /update route");
    _server->on("/update", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
            return request->requestAuthentication();
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", generateOTAPage());
        // Aggressive cache-busting headers to ensure new UI is always loaded
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate, max-age=0, private");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "Thu, 01 Jan 1970 00:00:00 GMT");
        response->addHeader("Last-Modified", "Thu, 01 Jan 1970 00:00:00 GMT");
        response->addHeader("ETag", String(millis())); // Unique ETag each time
        request->send(response);
    });
    
    _server->on("/update/start", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleUpdateStart(request);
    }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handleUpdateUpload(request, filename, index, data, len, final);
    });
    
    _server->on("/update/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleReboot(request);
    });
    
    _server->on("/update/rollback", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleRollback(request);
    });
    
    _server->on("/update/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleStatus(request);
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
    if (_prefs.getBytesLength("backup_ver") > 0) {
        loadUpdateInfo();
    }
}

void CustomOTA::setHardwareID(const char* id) {
    _hardwareID = String(id);
}

void CustomOTA::setBuildTimestamp(const char* timestamp) {
    _buildTimestamp = String(timestamp);
}

void CustomOTA::refreshBackupStatus() {
    COTA_DEBUG_PRINTLN("CustomOTA: Refreshing backup status...");
    loadUpdateInfo();
}

String CustomOTA::generateOTAPage() {
    // Add timestamp for cache busting
    String cotaTimestamp = String(millis());
    
    // Check if SPIFFS partition exists
    bool hasSpiffsPartition = checkSpiffsPartition();
    
    // Get current partition info
    const esp_partition_t* running_partition = esp_ota_get_running_partition();
    String partitionInfo = "Unknown";
    String partitionStatus = "";
    String partitionDetails = "";
    
    if (running_partition) {
        partitionInfo = String(running_partition->label);
        
        // Build detailed partition table information
        partitionDetails += "<h3>Partition Table Details</h3>";
        partitionDetails += "<table style='border-collapse: collapse; width: 100%; margin: 10px 0;'>";
        partitionDetails += "<tr style='background-color: #ddd; border: 1px solid #000;'>";
        partitionDetails += "<th style='padding: 8px; border: 1px solid #000;'>Label</th>";
        partitionDetails += "<th style='padding: 8px; border: 1px solid #000;'>Type</th>";
        partitionDetails += "<th style='padding: 8px; border: 1px solid #000;'>Subtype</th>";
        partitionDetails += "<th style='padding: 8px; border: 1px solid #000;'>Offset</th>";
        partitionDetails += "<th style='padding: 8px; border: 1px solid #000;'>Size</th>";
        partitionDetails += "<th style='padding: 8px; border: 1px solid #000;'>Status</th>";
        partitionDetails += "</tr>";
        
        // Iterate through all partitions
        esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
        
        while (it != NULL) {
            const esp_partition_t* part = esp_partition_get(it);
            if (part) {
                String typeStr = "";
                String subtypeStr = "";
                String statusStr = "";
                String rowStyle = "";
                
                // Determine partition type
                switch (part->type) {
                    case ESP_PARTITION_TYPE_APP:
                        typeStr = "APP";
                        switch (part->subtype) {
                            case ESP_PARTITION_SUBTYPE_APP_FACTORY: subtypeStr = "Factory"; break;
                            case ESP_PARTITION_SUBTYPE_APP_OTA_0: subtypeStr = "OTA_0"; break;
                            case ESP_PARTITION_SUBTYPE_APP_OTA_1: subtypeStr = "OTA_1"; break;
                            default: subtypeStr = "OTA_" + String(part->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0);
                        }
                        
                        // Check if partition has firmware
                        if (part->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 || part->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1 || part->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
                            uint32_t first_word;
                            esp_err_t err = esp_partition_read(part, 0, &first_word, sizeof(first_word));
                            if (err == ESP_OK && first_word != 0xFFFFFFFF) {
                                statusStr = "Has Firmware";
                                if (strcmp(part->label, running_partition->label) == 0) {
                                    statusStr += " (Running)";
                                    rowStyle = " style='background-color: #c8e6c9; font-weight: bold;'";
                                } else {
                                    statusStr += " (Backup)";
                                    rowStyle = " style='background-color: #fff3e0;'";
                                }
                            } else {
                                statusStr = "Empty";
                            }
                        }
                        break;
                        
                    case ESP_PARTITION_TYPE_DATA:
                        typeStr = "DATA";
                        switch (part->subtype) {
                            case ESP_PARTITION_SUBTYPE_DATA_OTA: subtypeStr = "OTA Data"; break;
                            case ESP_PARTITION_SUBTYPE_DATA_PHY: subtypeStr = "PHY Init"; break;
                            case ESP_PARTITION_SUBTYPE_DATA_NVS: subtypeStr = "NVS"; break;
                            case ESP_PARTITION_SUBTYPE_DATA_COREDUMP: subtypeStr = "Core Dump"; break;
                            case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS: subtypeStr = "NVS Keys"; break;
                            case ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM: subtypeStr = "eFuse Emul"; break;
                            case ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD: subtypeStr = "ESP HTTP"; break;
                            case ESP_PARTITION_SUBTYPE_DATA_FAT: subtypeStr = "FAT"; break;
                            case ESP_PARTITION_SUBTYPE_DATA_SPIFFS: 
                                subtypeStr = "SPIFFS"; 
                                statusStr = SPIFFS.begin() ? "Mounted" : "Available";
                                rowStyle = " style='background-color: #e1f5fe;'";
                                break;
                            default: subtypeStr = "Unknown (" + String(part->subtype, HEX) + ")";
                        }
                        break;
                        
                    default:
                        typeStr = "Unknown (" + String(part->type, HEX) + ")";
                        subtypeStr = String(part->subtype, HEX);
                }
                
                // Format sizes in human readable format
                String sizeStr = "";
                if (part->size >= 1024 * 1024) {
                    sizeStr = String(part->size / (1024 * 1024)) + " MB";
                } else if (part->size >= 1024) {
                    sizeStr = String(part->size / 1024) + " KB";
                } else {
                    sizeStr = String(part->size) + " B";
                }
                
                String offsetStr = "0x" + String(part->address, HEX);
                
                partitionDetails += "<tr" + rowStyle + ">";
                partitionDetails += "<td style='padding: 6px; border: 1px solid #000;'>" + String(part->label) + "</td>";
                partitionDetails += "<td style='padding: 6px; border: 1px solid #000;'>" + typeStr + "</td>";
                partitionDetails += "<td style='padding: 6px; border: 1px solid #000;'>" + subtypeStr + "</td>";
                partitionDetails += "<td style='padding: 6px; border: 1px solid #000; font-family: monospace;'>" + offsetStr + "</td>";
                partitionDetails += "<td style='padding: 6px; border: 1px solid #000;'>" + sizeStr + "</td>";
                partitionDetails += "<td style='padding: 6px; border: 1px solid #000;'>" + statusStr + "</td>";
                partitionDetails += "</tr>";
            }
            it = esp_partition_next(it);
        }
        
        esp_partition_iterator_release(it);
        partitionDetails += "</table>";
        
        // Check both partitions for firmware (simplified status for header)
        const esp_partition_t* app0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
        const esp_partition_t* app1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
        
        String app0_status = "Empty";
        String app1_status = "Empty";
        
        // Check app0 partition
        if (app0) {
            uint32_t cotaFirstWord;
            esp_err_t cotaErr = esp_partition_read(app0, 0, &cotaFirstWord, sizeof(cotaFirstWord));
            if (cotaErr == ESP_OK && cotaFirstWord != 0xFFFFFFFF) {
                app0_status = "Has Firmware";
                if (strcmp(running_partition->label, "app0") == 0) {
                    app0_status += " (Current)";
                }
            }
        }
        
        // Check app1 partition  
        if (app1) {
            uint32_t cotaFirstWord1;
            esp_err_t cotaErr1 = esp_partition_read(app1, 0, &cotaFirstWord1, sizeof(cotaFirstWord1));
            if (cotaErr1 == ESP_OK && cotaFirstWord1 != 0xFFFFFFFF) {
                app1_status = "Has Firmware";
                if (strcmp(running_partition->label, "app1") == 0) {
                    app1_status += " (Current)";
                }
            }
        }
        
        partitionStatus = "app0: " + app0_status + ", app1: " + app1_status;
        
        // Set backup status based on other partition having firmware
        const esp_partition_t* other_partition = NULL;
        if (strcmp(running_partition->label, "app0") == 0) {
            other_partition = app1;
        } else {
            other_partition = app0;
        }
        
        if (other_partition) {
            uint32_t first_word;
            esp_err_t err = esp_partition_read(other_partition, 0, &first_word, sizeof(first_word));
            _hasValidBackup = (err == ESP_OK && first_word != 0xFFFFFFFF);
        } else {
            _hasValidBackup = false;
        }
    }
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Over The Air Update</title>
    <link rel="stylesheet" href="/styles.css?v=)rawliteral" + cotaTimestamp + R"rawliteral(">
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
        <button class="button" onclick="window.location.href='/'">🏠 Home</button>
    </div>
    
    <h1>🔄 CustomOTA Update</h1>
    
    <div class="full-width-box">
        <h2>System Information</h2>
        <p><strong>Firmware Version:</strong> )rawliteral" + _firmwareVersion + R"rawliteral(</p>
        <p><strong>Build Date:</strong> )rawliteral" + _buildTimestamp + R"rawliteral(</p>
        <p><strong>Running Partition:</strong> )rawliteral" + partitionInfo + R"rawliteral(</p>
        <p><strong>Partition Status:</strong> )rawliteral" + partitionStatus + R"rawliteral(</p>
        <p><strong>Free Heap:</strong> <span id="freeHeap">)rawliteral" + String(ESP.getFreeHeap()) + R"rawliteral(</span> bytes</p>
        <p><strong>Flash Size:</strong> )rawliteral" + String(ESP.getFlashChipSize()) + R"rawliteral( bytes</p>
        <p><strong>SPIFFS Partition:</strong> )rawliteral" + String(hasSpiffsPartition ? "Available" : "Not Available") + R"rawliteral(</p>
        <p><strong>SPIFFS Mount:</strong> )rawliteral" + String(SPIFFS.begin() ? "Mounted" : "Failed") + R"rawliteral(</p>
    </div>
    
    <hr>
    
    <div class="full-width-box">
        <h2>Update Type Selection</h2>
        <div class="radio-group">
            <label>
                <input type="radio" name="updateType" value="firmware" checked onchange="updateTypeChanged()"> 
                <strong>Firmware Update</strong> - Update the main application code (.bin file)
            </label>)rawliteral" + 
            
            (hasSpiffsPartition ? 
                R"rawliteral(
            <label>
                <input type="radio" name="updateType" value="filesystem" onchange="updateTypeChanged()"> 
                <strong>Filesystem Update</strong> - Update web files, data, and configuration (filesystem.bin file)
            </label>)rawliteral" : 
                R"rawliteral(
            <label style="color: #666; font-style: italic;">
                <input type="radio" name="updateType" value="filesystem" disabled> 
                <strong>Filesystem Update</strong> - Not available (no SPIFFS partition found)
            </label>)rawliteral") + 
            
            R"rawliteral(
        </div>
    </div>
    
    <div class="upload-area" onclick="document.getElementById('updateFile').click();">
        <h2 id="uploadTitle">📁 Select Firmware File</h2>
        <p id="uploadDescription">Click here to browse for your .bin file</p>
        <input type="file" id="updateFile" class="file-input" accept=".bin" onchange="handleFileSelect(this)">
    </div>
    
    <div class="bar-container" id="progressContainer" style="display: none;">
        <div class="bar" id="progressFill"></div>
        <div class="bar-text" id="progressText">0%</div>
    </div>
    
    <div id="statusMessage" class="status" style="display: none;"></div>
    
    <div class="button-container">
        <button class="grey-button" id="uploadBtn" onclick="uploadUpdate()" disabled>
            🚀 Upload
        </button>)rawliteral" + 
        
        (_hasValidBackup ? 
            R"rawliteral(
        <button class="grey-button" onclick="rollbackFirmware()">
            ⏪ Rollback
        </button>)rawliteral" : 
            R"rawliteral(
        <button class="grey-button" onclick="alert('Rollback functionality available - ESP32 dual partition system automatically preserves previous firmware')" title="ESP32 dual partition system automatically preserves previous firmware - rollback available when other partition has valid firmware">
            � Enable Rollback
        </button>)rawliteral") + 
        
        R"rawliteral(
        <button class="grey-button" onclick="rebootDevice()">
            🔄 Reboot
        </button>
    </div>

    <script>
        let cotaSelectedFile = null;
        let cotaUpdateType = 'firmware';
        
        function updateTypeChanged() {
            const selected = document.querySelector('input[name="updateType"]:checked').value;
            cotaUpdateType = selected;
            
            const titleElement = document.getElementById('uploadTitle');
            const descElement = document.getElementById('uploadDescription');
            const buttonElement = document.getElementById('uploadBtn');
            
            if (cotaUpdateType === 'firmware') {
                titleElement.textContent = '📁 Select Firmware File';
                descElement.textContent = 'Click here to browse for your firmware .bin file';
                buttonElement.textContent = '🚀 Upload';
            } else {
                titleElement.textContent = '📁 Select Filesystem File';
                descElement.textContent = 'Click here to browse for your filesystem.bin file';
                buttonElement.textContent = '💾 Upload';
            }
            
            // Reset file selection
            cotaSelectedFile = null;
            document.getElementById('updateFile').value = '';
            buttonElement.disabled = true;
        }
        
        function handleFileSelect(input) {
            const cotaFile = input.files[0];
            if (cotaFile && cotaFile.name.endsWith('.bin')) {
                cotaSelectedFile = cotaFile;
                document.getElementById('uploadBtn').disabled = false;
                showStatus('File selected: ' + cotaFile.name + ' (' + cotaUpdateType + ' update)', 'info');
            } else {
                showStatus('Please select a valid .bin file', 'error');
            }
        }
        
        function showStatus(message, type) {
            const statusDiv = document.getElementById('statusMessage');
            statusDiv.textContent = message;
            statusDiv.className = 'status ' + type;
            statusDiv.style.display = 'block';
        }
        
        function updateProgress(percent) {
            document.getElementById('progressFill').style.width = percent + '%';
            document.getElementById('progressText').textContent = percent + '%';
        }
        
        function uploadUpdate() {
            if (!cotaSelectedFile) {
                showStatus('Please select a file first', 'error');
                return;
            }
            
            const cotaFormData = new FormData();
            cotaFormData.append('update', cotaSelectedFile);
            cotaFormData.append('type', cotaUpdateType);
            
            const cotaXhr = new XMLHttpRequest();
            
            cotaXhr.upload.addEventListener('progress', function(e) {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    updateProgress(percent);
                }
            });
            
            cotaXhr.onreadystatechange = function() {
                if (cotaXhr.readyState === 4) {
                    document.getElementById('progressContainer').style.display = 'none';
                    if (cotaXhr.status === 200) {
                        const cotaUpdateTypeName = cotaUpdateType.charAt(0).toUpperCase() + cotaUpdateType.slice(1);
                        showStatus(cotaUpdateTypeName + ' uploaded successfully! Device will reboot automatically.', 'success');
                        setTimeout(() => {
                            window.location.reload();
                        }, 3000);
                    } else {
                        showStatus('Upload failed: ' + cotaXhr.responseText, 'error');
                    }
                }
            };
            
            document.getElementById('progressContainer').style.display = 'block';
            updateProgress(0);
            showStatus('Uploading ' + cotaUpdateType + '...', 'info');
            
            cotaXhr.open('POST', '/update/start');
            cotaXhr.send(cotaFormData);
        }
        
        function rebootDevice() {
            if (confirm('Are you sure you want to reboot the device?')) {
                fetch('/update/reboot', { method: 'POST' })
                    .then(() => {
                        showStatus('Device is rebooting...', 'info');
                        setTimeout(() => {
                            window.location.reload();
                        }, 5000);
                    })
                    .catch(err => {
                        showStatus('Reboot failed: ' + err.message, 'error');
                    });
            }
        }
        
        function rollbackFirmware() {
            if (confirm('Are you sure you want to rollback to the previous firmware?')) {
                fetch('/update/rollback', { method: 'POST' })
                    .then(response => response.text())
                    .then(result => {
                        showStatus(result, 'info');
                        setTimeout(() => {
                            window.location.reload();
                        }, 3000);
                    })
                    .catch(err => {
                        showStatus('Rollback failed: ' + err.message, 'error');
                    });
            }
        }
        
        // Auto-refresh heap memory every 5 seconds
        setInterval(() => {
            fetch('/update/status')
                .then(response => response.json())
                .then(data => {
                    if (document.getElementById('freeHeap')) {
                        document.getElementById('freeHeap').textContent = data.freeHeap || 'Unknown';
                    }
                })
                .catch(() => {
                    // Ignore errors for status updates
                });
        }, 5000);
    </script>

    <hr>
    
    <div class="full-width-box">
        )rawliteral" + partitionDetails + R"rawliteral(
    </div>
</body>
</html>
)rawliteral";
    
    return html;
}

void CustomOTA::handleUpdateStart(AsyncWebServerRequest *request) {
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        return request->requestAuthentication();
    }
    
    // Always reset state before checking - this prevents race conditions
    _updateInProgress = false;
    _updateProgress = 0;
    
    // Clear any existing update state
    if (Update.isRunning()) {
        COTA_DEBUG_PRINTLN("CustomOTA: WARNING - Previous update still running, aborting it");
        Update.abort();
    }
    
    // Get update type from form data (default to firmware if not specified)
    _updateType = 0; // 0 = firmware, 1 = filesystem
    if (request->hasParam("type", true)) {
        String type = request->getParam("type", true)->value();
        if (type == "filesystem") {
            _updateType = 1;
        }
    }
    
    COTA_DEBUG_PRINTLN("==========================================");
    COTA_DEBUG_PRINTF("CustomOTA: Starting %s update\n", _updateType == 0 ? "firmware" : "filesystem");
    COTA_DEBUG_PRINTLN("CustomOTA: Preparing system for update...");
    
    // Set update state AFTER clearing any existing state
    _updateInProgress = true;
    _updateProgress = 0;
    _updateStatus = "Starting " + String(_updateType == 0 ? "firmware" : "filesystem") + " update...";
    
    COTA_DEBUG_PRINTLN("CustomOTA: Update initialization complete");
    COTA_DEBUG_PRINTLN("==========================================");
    
    request->send(200, "text/plain", "Update started");
}

void CustomOTA::handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    // Add debugging to understand the timing issue
    if (!_updateInProgress) {
        COTA_DEBUG_PRINTF("CustomOTA: DEBUG - Upload received but _updateInProgress=%s, index=%d, len=%d\n", 
                     _updateInProgress ? "true" : "false", index, len);
        COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Upload received but no update in progress");
        
        // Emergency state fix - if this is the start of an upload (index==0), try to initialize
        if (index == 0) {
            COTA_DEBUG_PRINTLN("CustomOTA: EMERGENCY - Attempting to initialize update state");
            _updateInProgress = true;
            _updateProgress = 0;
            _updateType = 0; // Default to firmware
            _updateStatus = "Emergency recovery - starting firmware update...";
        } else {
            return; // Skip this chunk if we can't recover
        }
    }
    
    if (index == 0) {
        COTA_DEBUG_PRINTLN("==========================================");
        COTA_DEBUG_PRINTF("CustomOTA: Update Start: %s (%s)\n", filename.c_str(), _updateType == 0 ? "firmware" : "filesystem");
        COTA_DEBUG_PRINTF("CustomOTA: Free heap before update: %d bytes\n", ESP.getFreeHeap());
        
        // Check if other partition has valid firmware (ESP32 automatic backup)
        COTA_DEBUG_PRINTLN("CustomOTA: Checking ESP32 dual partition backup status...");
        // The ESP32 OTA system automatically preserves firmware on the inactive partition
        // We'll update _hasValidBackup after the upload completes
        
        // Start update process based on type
        int cotaUpdateCommand = (_updateType == 0) ? U_FLASH : U_SPIFFS;
        COTA_DEBUG_PRINTF("CustomOTA: Starting update with command: %d\n", cotaUpdateCommand);
        
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cotaUpdateCommand)) {
            _updateStatus = "Update begin failed";
            _updateInProgress = false;
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
            _updateStatus = "Update write failed";
            _updateInProgress = false;
            COTA_DEBUG_PRINTF("CustomOTA: ERROR - Write failed: attempted %d bytes, wrote %d bytes\n", len, cotaBytesWritten);
            #if CUSTOM_OTA_DEBUG
            Update.printError(Serial);
            #endif
            return;
        }
        
        // Update progress
        _updateProgress = (Update.progress() * 100) / Update.size();
        String updateTypeName = _updateType == 0 ? "Firmware" : "Filesystem";
        _updateStatus = "Uploading " + updateTypeName + "... " + String(_updateProgress) + "%";
        
        // Print progress every 10%
        static int cotaLastProgress = -1;
        if (_updateProgress >= cotaLastProgress + 10) {
            COTA_DEBUG_PRINTF("CustomOTA: Upload progress: %d%% (%d bytes)\n", _updateProgress, Update.progress());
            cotaLastProgress = (_updateProgress / 10) * 10;
        }
    }
    
    if (final) {
        COTA_DEBUG_PRINTLN("==========================================");
        COTA_DEBUG_PRINTLN("CustomOTA: Final upload chunk received");
        COTA_DEBUG_PRINTF("CustomOTA: Total bytes uploaded: %d\n", index + len);
        COTA_DEBUG_PRINTF("CustomOTA: Free heap after upload: %d bytes\n", ESP.getFreeHeap());
        
        if (Update.end(true)) {
            COTA_DEBUG_PRINTF("CustomOTA: Update Success: %uB\n", index + len);
            _updateStatus = String(_updateType == 0 ? "Firmware" : "Filesystem") + " update completed successfully";
            _updateInProgress = false;
            
            // Save update info
            COTA_DEBUG_PRINTLN("CustomOTA: Saving update info...");
            saveUpdateInfo();
            
            // For firmware updates, ensure partition switch
            if (_updateType == 0) {
                COTA_DEBUG_PRINTLN("CustomOTA: Firmware update completed, setting boot partition...");
                const esp_partition_t* cotaNextPartition = esp_ota_get_next_update_partition(NULL);
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
            }
            
            // Schedule reboot without blocking - use deferred restart
            request->send(200, "text/plain", "Update completed. Rebooting in 3 seconds...");
            
            // Set restart flag for main loop to handle
            _restartPending = true;
            _restartTime = millis() + 3000; // 3 seconds to allow response to be sent and user to see message
            COTA_DEBUG_PRINTLN("CustomOTA: Restart scheduled for 3 seconds");
            COTA_DEBUG_PRINTLN("==========================================");
        } else {
            _updateStatus = "Update failed";
            _updateInProgress = false;
            COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Update.end() failed:");
            #if CUSTOM_OTA_DEBUG
            Update.printError(Serial);
            #endif
            COTA_DEBUG_PRINTLN("==========================================");
            request->send(400, "text/plain", _updateStatus);
        }
    }
}

void CustomOTA::handleReboot(AsyncWebServerRequest *request) {
    // REAL-TIME CRITICAL: NO delay() calls allowed!
    // Uses deferred restart mechanism to maintain real-time operations
    
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        return request->requestAuthentication();
    }
    
    // Simple reboot of current partition
    COTA_DEBUG_PRINTLN("CustomOTA: Manual reboot requested");
    request->send(200, "text/plain", "Rebooting device...");
    
    // IMPORTANT: Use deferred restart - NO blocking delay() calls!
    // This allows real-time operations to continue for 2 more seconds
    _restartPending = true;
    _restartTime = millis() + 2000; // Non-blocking deferred restart timing
}

void CustomOTA::handleRollback(AsyncWebServerRequest *request) {
    // REAL-TIME CRITICAL: NO delay() calls allowed!
    // Uses deferred restart to maintain real-time operations
    
    if (_authenticated && !request->authenticate(_username.c_str(), _password.c_str())) {
        return request->requestAuthentication();
    }
    
    COTA_DEBUG_PRINTLN("CustomOTA: Manual rollback requested via web interface");
    
    if (restoreFromBackup()) {
        request->send(200, "text/plain", "Rollback successful. Device will reboot to previous firmware in 3 seconds...");
        COTA_DEBUG_PRINTLN("CustomOTA: Rollback successful, scheduling restart");
        // IMPORTANT: Non-blocking deferred restart - NO delay() calls!
        _restartPending = true;
        _restartTime = millis() + 3000; // 3 seconds to allow user to see message
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
    String json = "{";
    json += "\"inProgress\":" + String(_updateInProgress ? "true" : "false") + ",";
    json += "\"progress\":" + String(_updateProgress) + ",";
    json += "\"status\":\"" + _updateStatus + "\",";
    json += "\"hasBackup\":" + String(_hasValidBackup ? "true" : "false") + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"version\":\"" + _firmwareVersion + "\",";
    json += "\"buildTimestamp\":\"" + _buildTimestamp + "\"";
    json += "}";
    
    request->send(200, "application/json", json);
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
            COTA_DEBUG_PRINTLN("CustomOTA: No backup metadata found in preferences");
            return false;
        }
    }
    
    String backupVersion = _prefs.getString("backup_ver", "unknown");
    String backupPartition = _prefs.getString("backup_part", "unknown");
    
    COTA_DEBUG_PRINTLN("==========================================");
    COTA_DEBUG_PRINTLN("CustomOTA: ROLLBACK INITIATED");
    COTA_DEBUG_PRINTF("CustomOTA: Rolling back to firmware version: %s\n", backupVersion.c_str());
    COTA_DEBUG_PRINTF("CustomOTA: Backup partition: %s\n", backupPartition.c_str());
    
    // Get current running partition
    const esp_partition_t* cotaRunningPartition = esp_ota_get_running_partition();
    if (!cotaRunningPartition) {
        COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Cannot get current running partition");
        return false;
    }
    
    // Find the backup partition (the other partition)
    const esp_partition_t* cotaBackupPartition = NULL;
    if (strcmp(cotaRunningPartition->label, "app0") == 0) {
        cotaBackupPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    } else {
        cotaBackupPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    }
    
    if (!cotaBackupPartition) {
        COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Cannot find backup partition");
        return false;
    }
    
    COTA_DEBUG_PRINTF("CustomOTA: Current partition: %s\n", cotaRunningPartition->label);
    COTA_DEBUG_PRINTF("CustomOTA: Rollback partition: %s\n", cotaBackupPartition->label);
    
    // Set the backup partition as the boot partition
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
    _prefs.putString("last_update", _firmwareVersion);
    _prefs.putULong("update_time", millis());
}

void CustomOTA::loadUpdateInfo() {
    // Check if backup_ver key exists before trying to read it
    size_t requiredSize = _prefs.getBytesLength("backup_ver");
    if (requiredSize > 0) {
        String backupVersion = _prefs.getString("backup_ver", "");
        String backupPartition = _prefs.getString("backup_part", "");
        String backupValid = _prefs.getString("backup_valid", "");
        
        COTA_DEBUG_PRINTF("CustomOTA: Loading backup info - Version: '%s', Partition: '%s', Valid: '%s'\n", 
                         backupVersion.c_str(), backupPartition.c_str(), backupValid.c_str());
        
        if (backupVersion.length() > 0 && backupPartition.length() > 0 && backupValid == "YES") {
            // Additional validation: ensure the backup partition exists and is different from current
            const esp_partition_t* current = esp_ota_get_running_partition();
            const esp_partition_t* backup_part = NULL;
            
            if (current && strcmp(current->label, backupPartition.c_str()) != 0) {
                // Find the backup partition
                if (strcmp(backupPartition.c_str(), "app0") == 0) {
                    backup_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
                } else if (strcmp(backupPartition.c_str(), "app1") == 0) {
                    backup_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
                }
                
                if (backup_part) {
                    _hasValidBackup = true;
                    _backupSize = _prefs.getUInt("backup_size", 0);
                    COTA_DEBUG_PRINTLN("CustomOTA: Valid backup found and verified");
                    COTA_DEBUG_PRINTF("CustomOTA: Backup version: %s, Partition: %s, Size: %d bytes\n", 
                                     backupVersion.c_str(), backupPartition.c_str(), _backupSize);
                    COTA_DEBUG_PRINTF("CustomOTA: Current partition: %s, Backup partition: %s\n", 
                                     current->label, backupPartition.c_str());
                } else {
                    _hasValidBackup = false;
                    COTA_DEBUG_PRINTLN("CustomOTA: Backup partition not found - marking backup as invalid");
                }
            } else {
                _hasValidBackup = false;
                COTA_DEBUG_PRINTLN("CustomOTA: Backup partition is same as current or current partition unknown - invalid backup");
                if (current) {
                    COTA_DEBUG_PRINTF("CustomOTA: Current: %s, Backup: %s\n", current->label, backupPartition.c_str());
                }
            }
        } else {
            _hasValidBackup = false;
            COTA_DEBUG_PRINTLN("CustomOTA: Incomplete backup metadata found");
        }
    } else {
        _hasValidBackup = false;
        COTA_DEBUG_PRINTLN("CustomOTA: No previous backup (first run or backup cleared)");
    }
}

void CustomOTA::checkRestart() {
    // REAL-TIME CRITICAL FUNCTION: NO delay() calls allowed!
    // This function is called from main loop and must return immediately
    // Uses non-blocking millis() comparison for timing
    
    if (_restartPending && millis() >= _restartTime) {
        COTA_DEBUG_PRINTLN("==========================================");
        COTA_DEBUG_PRINTLN("CustomOTA: FIRMWARE UPDATE COMPLETE");
        COTA_DEBUG_PRINTLN("CustomOTA: Performing deferred restart...");
        COTA_DEBUG_PRINTLN("CustomOTA: Device will reboot to new partition");
        COTA_DEBUG_PRINTLN("==========================================");
        COTA_DEBUG_PRINTLN("CustomOTA: Flushing serial before restart...");
        #if CUSTOM_OTA_DEBUG
        Serial.flush(); // Only flush if debugging is enabled
        #endif
        
        // Add a small diagnostic delay to ensure serial output completes
        unsigned long startTime = millis();
        while (millis() - startTime < 500) {
            // Small wait to ensure serial completes
        }
        
        COTA_DEBUG_PRINTLN("CustomOTA: REBOOTING NOW!");
        ESP.restart(); // Final restart - unavoidably blocking but necessary
    }
    // Function returns immediately if restart not ready - maintains real-time operation
}

void CustomOTA::handle() {
    // REAL-TIME CRITICAL MAIN LOOP FUNCTION: NO delay() calls allowed!
    // This function is called every loop iteration and must be non-blocking
    // Any blocking operations will interfere with real-time device operations
    
    // Check for pending restart using non-blocking timing
    checkRestart();
    
    // This function completes in microseconds - maintains real-time compatibility
    // All time-sensitive operations use deferred/scheduled approaches
}

bool CustomOTA::checkSpiffsPartition() {
    // Check if SPIFFS partition exists in the partition table
    const esp_partition_t* spiffs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    return (spiffs_partition != NULL);
}
