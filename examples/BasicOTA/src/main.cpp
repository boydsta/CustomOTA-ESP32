/*
  CustomOTA Basic Example
  
  This example demonstrates the basic usage of the CustomOTA library.
  
  Features demonstrated:
  - Basic OTA setup without authentication
  - Setting firmware version and hardware ID
  - Simple web server setup
  
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
  
  // Initialize CustomOTA (no authentication)
  ota.begin(&server);
  
  // Set device information
  ota.setHardwareID("CustomOTA Example Device");
  ota.setFirmwareVersion("1.0.0");
  
  // Set build timestamp (automatically uses compile time in dd/mm/yyyy format if not set)
  // You can also set a custom timestamp:
  // ota.setBuildTimestamp("30/07/2025 15:30:00");
  
  // Start server
  server.begin();
  
  Serial.println("CustomOTA server started");
  Serial.println("Open your browser and go to:");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/update");
}

void loop() {
  // Your main application code goes here
  
  // Optional: Print update status if in progress (real-time compatible)
  static unsigned long lastStatusCheck = 0;
  if (millis() - lastStatusCheck > 2000) { // Check every 2 seconds
    lastStatusCheck = millis();
    
    if (ota.isUpdateInProgress()) {
      Serial.print("Update progress: ");
      Serial.print(ota.getUpdateProgress());
      Serial.println("%");
    }
  }
  
  // Handle OTA operations (must be called every loop)
  ota.handle();
}
