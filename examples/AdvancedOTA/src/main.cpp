/*
  CustomOTA Advanced Example

  This example demonstrates advanced usage of the CustomOTA library
  with all features enabled including authentication, debug output,
  custom configuration, and comprehensive error handling.

  Features demonstrated:
  - HTTP authentication
  - Debug output enabled
  - Custom restart delay
  - SPIFFS formatting control
  - Heap monitoring
  - Status monitoring
  - Comprehensive setup

  Circuit:
  - ESP32 development board
  - WiFi connection

  Created by Boyd Benton

  This example code is in the public domain.
*/

#include <WiFi.h>
#include <CustomOTA.h>

// Replace with your network credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// OTA credentials
const char* otaUsername = "admin";
const char* otaPassword = "securePassword123";

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

  // Configure CustomOTA with advanced settings
  Serial.println("Configuring CustomOTA...");

  // Set device information
  ota.setHardwareID("CustomOTA Advanced Example Device");
  ota.setFirmwareVersion("1.2.1");
  ota.setBuildTimestamp("27/08/2025 14:30:00");

  // Advanced configuration options
  ota.setRestartDelay(5000);        // 5 second restart delay
  ota.setSpiffsFormatOnFailure(false); // Don't auto-format SPIFFS (safer)

  // Enable authentication
  ota.setCredentials(otaUsername, otaPassword);

  // Initialize CustomOTA
  ota.begin(&server);

  // Start server
  server.begin();

  Serial.println("CustomOTA Advanced server started");
  Serial.println("Open your browser and go to:");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/update");
  Serial.println("Username: " + String(otaUsername));
  Serial.println("Password: " + String(otaPassword));

  // Display system information
  Serial.println("\n=== System Information ===");
  Serial.print("Library Version: ");
  Serial.println(CustomOTA::getLibraryVersion());
  Serial.print("Build Info: ");
  Serial.println(CustomOTA::getBuildInfo());
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.print("Flash Size: ");
  Serial.print(ESP.getFlashChipSize());
  Serial.println(" bytes");
  Serial.println("========================");
}

void loop() {
  // Your main application code goes here

  // Monitor OTA status
  static unsigned long lastStatusCheck = 0;
  if (millis() - lastStatusCheck > 5000) { // Check every 5 seconds
    lastStatusCheck = millis();

    if (ota.isUpdateInProgress()) {
      Serial.print("Update in progress: ");
      Serial.print(ota.getUpdateProgress());
      Serial.print("% - ");
      Serial.println(ota.getUpdateStatus());
    }

    // Check if restart is pending
    if (ota.shouldRestart()) {
      Serial.println("Device restart pending...");
    }
  }

  // Handle OTA operations (must be called every loop)
  ota.handle();

  delay(10); // Small delay to prevent overwhelming the serial output
}
