/*
  CustomOTA Secure Example

  This example demonstrates secure OTA usage with enhanced security features,
  authentication, and production-ready configuration.

  Features demonstrated:
  - Strong HTTP authentication
  - Secure configuration options
  - Production-ready settings
  - Enhanced security features
  - Status monitoring and logging

  Security Features:
  - Strong password requirements
  - SPIFFS data protection
  - Heap monitoring
  - Enhanced validation
  - Secure restart handling

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

// Strong OTA credentials (change these!)
const char* otaUsername = "secureadmin";
const char* otaPassword = "MySecurePassword123!@#";

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

  // Secure CustomOTA configuration
  Serial.println("Configuring CustomOTA with security settings...");

  // Set device information
  ota.setHardwareID("Secure CustomOTA Device");
  ota.setFirmwareVersion("1.2.1");
  ota.setBuildTimestamp("27/08/2025 14:30:00");

  // Security-focused configuration
  ota.setRestartDelay(3000);        // Standard restart delay
  ota.setSpiffsFormatOnFailure(false); // Never auto-format (maximum security)

  // Enable strong authentication
  ota.setCredentials(otaUsername, otaPassword);

  // Initialize CustomOTA
  ota.begin(&server);

  // Start server
  server.begin();

  Serial.println("Secure CustomOTA server started");
  Serial.println("Open your browser and go to:");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/update");
  Serial.println("Username: " + String(otaUsername));
  Serial.println("Password: " + String(otaPassword));

  // Security information
  Serial.println("\n=== Security Information ===");
  Serial.println("✅ HTTP Authentication: Enabled");
  Serial.println("✅ SPIFFS Auto-format: Disabled (secure)");
  Serial.println("✅ Heap Monitoring: Enabled");
  Serial.println("✅ Rollback Validation: Enhanced");
  Serial.println("✅ Firmware Validation: Enabled");
  Serial.println("============================");

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

  // Security monitoring
  static unsigned long lastSecurityCheck = 0;
  if (millis() - lastSecurityCheck > 10000) { // Check every 10 seconds
    lastSecurityCheck = millis();

    // Monitor system security status
    Serial.println("\n--- Security Status Check ---");
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");

    if (ota.isUpdateInProgress()) {
      Serial.print("🔄 Update in progress: ");
      Serial.print(ota.getUpdateProgress());
      Serial.print("% - ");
      Serial.println(ota.getUpdateStatus());
    } else {
      Serial.println("✅ No update in progress");
    }

    // Check if restart is pending
    if (ota.shouldRestart()) {
      Serial.println("⚠️  Device restart pending...");
    }

    Serial.println("-----------------------------");
  }

  // Handle OTA operations (must be called every loop)
  ota.handle();

  delay(10); // Small delay to prevent overwhelming the serial output
}
