/*
  CustomOTA Secure Example
  
  This example demonstrates the CustomOTA library with HTTP authentication
  and advanced features including both firmware and filesystem updates.
  
  Features demonstrated:
  - OTA setup with username/password authentication
  - Firmware update (.bin files)
  - Filesystem update (SPIFFS .bin files)
  - Update progress monitoring
  - System status monitoring
  - Custom endpoints
  
  Circuit:
  - ESP32 development board
  - WiFi connection
  
  Created by Boyd Benton
  
  This example code is in the public domain.
*/

#include <WiFi.h>
#include <SPIFFS.h>
#include <CustomOTA.h>

// Replace with your network credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// OTA credentials
const char* ota_username = "admin";
const char* ota_password = "secure123";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create CustomOTA object
CustomOTA ota;

void setup() {
  Serial.begin(115200);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Initialize CustomOTA with authentication
  ota.begin(&server, ota_username, ota_password);
  
  // Set device information
  ota.setHardwareID("SecureOTA Example v1.2");
  ota.setFirmwareVersion("1.2.3");
  
  // Add custom endpoints
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Simple device info endpoint
    String html = "<h1>Device Information</h1>";
    html += "<p>Uptime: " + String(millis() / 1000) + " seconds</p>";
    html += "<p>Free Heap: " + String(ESP.getFreeHeap()) + " bytes</p>";
    html += "<p>Flash Size: " + String(ESP.getFlashChipSize()) + " bytes</p>";
    html += "<p>CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz</p>";
    html += "<p>SPIFFS Total: " + String(SPIFFS.totalBytes()) + " bytes</p>";
    html += "<p>SPIFFS Used: " + String(SPIFFS.usedBytes()) + " bytes</p>";
    html += "<br><a href='/update'>Go to OTA Update</a>";
    request->send(200, "text/html", html);
  });
  
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Custom JSON status endpoint
    String json = "{";
    json += "\"device\":\"SecureOTA Example\",";
    json += "\"version\":\"1.2.3\",";
    json += "\"uptime\":" + String(millis()) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"spiffs_total\":" + String(SPIFFS.totalBytes()) + ",";
    json += "\"spiffs_used\":" + String(SPIFFS.usedBytes()) + ",";
    json += "\"update_in_progress\":" + String(ota.isUpdateInProgress() ? "true" : "false") + ",";
    json += "\"update_progress\":" + String(ota.getUpdateProgress());
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // Start server
  server.begin();
  
  Serial.println("Secure CustomOTA server started");
  Serial.println("Endpoints available:");
  Serial.print("OTA Update: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/update");
  Serial.print("Device Info: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/info");
  Serial.print("Status API: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/api/status");
  Serial.println();
  Serial.print("Login credentials - Username: ");
  Serial.print(ota_username);
  Serial.print(", Password: ");
  Serial.println(ota_password);
}

void loop() {
  static unsigned long lastPrint = 0;
  unsigned long currentTime = millis();
  
  // Print status every 10 seconds
  if (currentTime - lastPrint > 10000) {
    lastPrint = currentTime;
    
    Serial.println("--- System Status ---");
    Serial.print("Uptime: ");
    Serial.print(currentTime / 1000);
    Serial.println(" seconds");
    
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    Serial.print("WiFi RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    Serial.print("SPIFFS: ");
    Serial.print(SPIFFS.usedBytes());
    Serial.print(" / ");
    Serial.print(SPIFFS.totalBytes());
    Serial.println(" bytes used");
    
    if (ota.isUpdateInProgress()) {
      Serial.print("OTA Update in progress: ");
      Serial.print(ota.getUpdateProgress());
      Serial.println("%");
      Serial.print("Status: ");
      Serial.println(ota.getUpdateStatus());
    } else {
      Serial.println("OTA Status: Ready");
    }
    
    Serial.println("--------------------");
  }
  
  // Your main application code goes here
  delay(100);
}
