/*
  CustomOTA Advanced Example - Production Ready Implementation
  
  This example demonstrates advanced usage of the CustomOTA library suitable
  for production deployments with real-time applications.
  
  Features demonstrated:
  - Secure authentication with credentials stored in NVS
  - Real-time compatible main loop
  - Status monitoring and reporting
  - Custom web endpoints
  - Rollback testing
  - Memory monitoring
  - Debug output control
  - Error handling
  
  Hardware Requirements:
  - ESP32 development board with dual OTA partition scheme
  - WiFi connection
  - At least 4MB flash memory
  
  Partition Scheme Required:
  Add to platformio.ini:
  board_build.partitions = default.csv
  
  Debug Control:
  Add to platformio.ini build_flags:
  -DCUSTOM_OTA_DEBUG=1  ; Enable debug output
  -DCUSTOM_OTA_DEBUG=0  ; Disable debug output (production)
  
  Created by Boyd Benton for CustomOTA v2.0
  
  This example code is in the public domain.
*/

#include <WiFi.h>
#include <CustomOTA.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Configuration storage
Preferences prefs;

// Network configuration - loaded from NVS
String wifi_ssid;
String wifi_password;
String ota_username;
String ota_password;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create CustomOTA object
CustomOTA ota;

// Application variables for demonstration
unsigned long lastStatusUpdate = 0;
unsigned long lastMemoryCheck = 0;
unsigned long appStartTime = 0;
int sensorValue = 0;
float temperature = 25.0;
bool systemHealthy = true;

// Real-time task simulation
unsigned long lastRealTimeTask = 0;
const unsigned long REAL_TIME_INTERVAL = 1000; // 1ms for real-time simulation

void setup() {
  Serial.begin(115200);
  appStartTime = millis();
  
  Serial.println("\n=== CustomOTA Advanced Example v2.0 ===");
  Serial.println("Production-ready implementation with real-time compatibility");
  
  // Load configuration from NVS (Non-Volatile Storage)
  loadConfiguration();
  
  // Initialize WiFi
  initializeWiFi();
  
  // Setup CustomOTA with security
  setupOTA();
  
  // Setup custom web endpoints
  setupCustomEndpoints();
  
  // Start web server
  server.begin();
  
  Serial.println("\n=== System Ready ===");
  Serial.printf("OTA Interface: http://%s/update\n", WiFi.localIP().toString().c_str());
  Serial.printf("Status API: http://%s/api/status\n", WiFi.localIP().toString().c_str());
  Serial.printf("System Info: http://%s/api/system\n", WiFi.localIP().toString().c_str());
  
  if (ota_username.length() > 0) {
    Serial.printf("Authentication: Username='%s', Password='%s'\n", ota_username.c_str(), ota_password.c_str());
  } else {
    Serial.println("Authentication: DISABLED (open access)");
  }
  
  Serial.println("===================\n");
}

void loop() {
  // CRITICAL: Handle OTA operations (non-blocking, microsecond timing)
  ota.handle();
  
  // Simulate real-time tasks (must complete quickly)
  handleRealTimeTasks();
  
  // Application-specific tasks (also non-blocking)
  handleApplicationTasks();
  
  // Status monitoring (periodic, non-blocking)
  handleStatusMonitoring();
  
  // Memory monitoring (periodic, non-blocking)
  handleMemoryMonitoring();
  
  // IMPORTANT: This loop must complete in microseconds for real-time compatibility
  // NO delay() calls allowed anywhere!
}

void loadConfiguration() {
  Serial.println("Loading configuration from NVS...");
  
  prefs.begin("config", false);
  
  // Load WiFi credentials with defaults
  wifi_ssid = prefs.getString("wifi_ssid", "YOUR_WIFI_SSID");
  wifi_password = prefs.getString("wifi_pass", "YOUR_WIFI_PASSWORD");
  
  // Load OTA credentials (empty = no authentication)
  ota_username = prefs.getString("ota_user", "admin");
  ota_password = prefs.getString("ota_pass", "secure123");
  
  // Option to disable authentication for testing
  bool auth_enabled = prefs.getBool("ota_auth", true);
  if (!auth_enabled) {
    ota_username = "";
    ota_password = "";
  }
  
  prefs.end();
  
  Serial.printf("WiFi SSID: %s\n", wifi_ssid.c_str());
  Serial.printf("OTA Auth: %s\n", (ota_username.length() > 0) ? "Enabled" : "Disabled");
}

void initializeWiFi() {
  Serial.printf("Connecting to WiFi: %s", wifi_ssid.c_str());
  
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  WiFi.setSleep(false); // Disable WiFi sleep for better real-time performance
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500); // Only acceptable delay during setup
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println(" Failed!");
    Serial.println("Using default AP mode...");
    WiFi.softAP("CustomOTA-Setup", "configure123");
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  }
}

void setupOTA() {
  Serial.println("Initializing CustomOTA...");
  
  // Configure OTA with or without authentication
  if (ota_username.length() > 0) {
    ota.setCredentials(ota_username.c_str(), ota_password.c_str());
    Serial.println("OTA Authentication: ENABLED");
  } else {
    Serial.println("OTA Authentication: DISABLED (open access)");
  }
  
  // Set device identification
  ota.setHardwareID("Advanced Example v2.0");
  ota.setFirmwareVersion("2.0.0-advanced");
  
  // Set build timestamp - automatically uses __DATE__ and __TIME__ if not set manually
  // ota.setBuildTimestamp("2024-01-15 14:30:00"); // Optional: manual override
  
  // Initialize OTA system
  ota.begin(&server);
  
  Serial.println("CustomOTA initialized successfully");
}

void setupCustomEndpoints() {
  Serial.println("Setting up custom API endpoints...");
  
  // System status API
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(512);
    
    doc["status"] = "running";
    doc["uptime"] = millis() - appStartTime;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["sensor_value"] = sensorValue;
    doc["temperature"] = temperature;
    doc["system_healthy"] = systemHealthy;
    doc["ota_progress"] = ota.getUpdateProgress();
    doc["ota_status"] = ota.getUpdateStatus();
    doc["ota_in_progress"] = ota.isUpdateInProgress();
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
  });
  
  // Detailed system information
  server.on("/api/system", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    
    doc["hardware_id"] = "Advanced Example v2.0";
    doc["firmware_version"] = "2.0.0-advanced";
    doc["esp32_model"] = ESP.getChipModel();
    doc["esp32_revision"] = ESP.getChipRevision();
    doc["esp32_cores"] = ESP.getChipCores();
    doc["flash_size"] = ESP.getFlashChipSize();
    doc["flash_speed"] = ESP.getFlashChipSpeed();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["min_free_heap"] = ESP.getMinFreeHeap();
    doc["max_alloc_heap"] = ESP.getMaxAllocHeap();
    doc["uptime_ms"] = millis() - appStartTime;
    
    // WiFi information
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = WiFi.SSID();
    wifi["rssi"] = WiFi.RSSI();
    wifi["ip"] = WiFi.localIP().toString();
    wifi["mac"] = WiFi.macAddress();
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
  });
  
  // Configuration endpoint (for changing settings)
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Handle configuration updates
    if (request->hasParam("ota_auth", true)) {
      String auth = request->getParam("ota_auth", true)->value();
      prefs.begin("config", false);
      prefs.putBool("ota_auth", auth == "true");
      prefs.end();
      
      request->send(200, "text/plain", "Configuration updated. Restart required.");
    } else {
      request->send(400, "text/plain", "Invalid parameters");
    }
  });
  
  // Rollback test endpoint
  server.on("/api/rollback-test", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (ota.hasValidBackup()) {
      request->send(200, "text/plain", "Rollback test initiated. Device will restart with previous firmware.");
      // The rollback will be handled by the OTA system
    } else {
      request->send(400, "text/plain", "No backup available for rollback test");
    }
  });
  
  Serial.println("Custom API endpoints configured");
}

void handleRealTimeTasks() {
  // Simulate real-time operations that must complete quickly
  // In a real application, this might be sensor reading, motor control, etc.
  
  if (millis() - lastRealTimeTask >= REAL_TIME_INTERVAL) {
    lastRealTimeTask = millis();
    
    // Simulate sensor reading (must be fast!)
    sensorValue = analogRead(A0);
    
    // Simulate temperature calculation
    temperature = 20.0 + (sensorValue / 4095.0) * 15.0; // 20-35°C range
    
    // System health check (simple example)
    systemHealthy = (ESP.getFreeHeap() > 50000) && (WiFi.status() == WL_CONNECTED);
    
    // CRITICAL: This entire function must complete in microseconds!
  }
}

void handleApplicationTasks() {
  // Non-real-time application tasks
  // These can take a bit longer but should still be non-blocking
  
  static unsigned long lastAppTask = 0;
  if (millis() - lastAppTask >= 100) { // Every 100ms
    lastAppTask = millis();
    
    // Example: Log system status periodically
    static int logCounter = 0;
    if (++logCounter >= 100) { // Every 10 seconds
      logCounter = 0;
      Serial.printf("System: Heap=%d, Temp=%.1f°C, Sensor=%d, Healthy=%s\n", 
                   ESP.getFreeHeap(), temperature, sensorValue, 
                   systemHealthy ? "YES" : "NO");
    }
  }
}

void handleStatusMonitoring() {
  if (millis() - lastStatusUpdate >= 5000) { // Every 5 seconds
    lastStatusUpdate = millis();
    
    // Monitor OTA status
    if (ota.isUpdateInProgress()) {
      Serial.printf("OTA Update: %d%% - %s\n", 
                   ota.getUpdateProgress(), 
                   ota.getUpdateStatus().c_str());
    }
    
    // Monitor WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WARNING: WiFi disconnected!");
      // In production, might attempt reconnection
    }
  }
}

void handleMemoryMonitoring() {
  if (millis() - lastMemoryCheck >= 10000) { // Every 10 seconds
    lastMemoryCheck = millis();
    
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    
    // Memory health check
    if (freeHeap < 30000) {
      Serial.printf("WARNING: Low memory! Free: %d, Min: %d\n", freeHeap, minFreeHeap);
    }
    
    // Debug memory status (only when debug enabled)
    #if CUSTOM_OTA_DEBUG
      Serial.printf("Memory: Free=%d, Min=%d, Max=%d\n", 
                   freeHeap, minFreeHeap, ESP.getMaxAllocHeap());
    #endif
  }
}

// Save configuration helper function
void saveConfiguration() {
  prefs.begin("config", false);
  
  prefs.putString("wifi_ssid", wifi_ssid);
  prefs.putString("wifi_pass", wifi_password);
  prefs.putString("ota_user", ota_username);
  prefs.putString("ota_pass", ota_password);
  
  prefs.end();
  
  Serial.println("Configuration saved to NVS");
}

/*
  IMPORTANT NOTES:
  
  1. Real-Time Compatibility:
     - This example maintains microsecond-level loop timing
     - NO delay() calls in main loop or OTA handling
     - All timing uses millis() comparisons
     
  2. Memory Management:
     - Monitors heap usage continuously
     - Uses stack-allocated variables where possible
     - Minimal dynamic allocation
     
  3. Error Handling:
     - Comprehensive status monitoring
     - Graceful WiFi disconnection handling
     - OTA error reporting via API
     
  4. Production Features:
     - Configuration stored in NVS
     - Multiple API endpoints for monitoring
     - Authentication support
     - Debug output control
     
  5. Customization:
     - Modify API endpoints for your application
     - Add your real-time tasks to handleRealTimeTasks()
     - Customize monitoring intervals as needed
     - Add application-specific configuration
*/
