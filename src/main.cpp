#include "global.h"
#include "config_coreiot.h"
#include "coreiot.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "mainserver.h"
#include "tinyml.h"

#include "task_check_info.h"
#include "task_toogle_boot.h"
#include "task_wifi.h"
#include "task_webserver.h"
#include "task_mqtt.h"

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n========================================");
  Serial.println("🚀 ESP32 BOOT OK");
  Serial.println("========================================\n");
  
  // ✅ 1. Mount LittleFS FIRST
  if (!LittleFS.begin(true)) {
      Serial.println("❌ LittleFS Mount Failed");
      return;
  }
  Serial.println("✅ LittleFS Mounted");
  
  // ✅ 2. Create Semaphore BEFORE any task
  xBinarySemaphoreInternet = xSemaphoreCreateBinary();
  if (xBinarySemaphoreInternet == NULL) {
      Serial.println("❌ Failed to create semaphore!");
      return;
  }
  Serial.println("✅ Semaphore created");
  
  // ✅ 3. Initialize WiFi FIRST (CRITICAL!)
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP_STA);  // Enable both AP and STA
  delay(500);  // Wait for WiFi stack to initialize
  Serial.println("✅ WiFi stack initialized");
  
  // ✅ 4. Load configs
  if (!LittleFS.exists("/coreiot.json")) {
      Serial.println("⚠️ Creating default coreiot.json...");
      coreiot_server = "app.coreiot.io";
      coreiot_port = 1883;
      coreiot_client_id = "ESP32_" + String((uint32_t)ESP.getEfuseMac(), HEX);
      coreiot_username = "";
      coreiot_password = "";
      saveCoreIOTConfig();
  }
  loadCoreIOTConfig();
  
  // ✅ 5. Check WiFi info file
  check_info_File(0);
  
  // ✅ 6. Create tasks with proper stack sizes
  Serial.println("\n📋 Creating tasks...");
  
  xTaskCreatePinnedToCore(
    main_server_task,    // Task function
    "MainServer",        // Name
    8192,                // Stack size
    NULL,                // Parameters
    2,                   // Priority
    NULL,                // Task handle
    1                    // Core 1 (App core)
  );
  Serial.println("   ✅ MainServer task created (Core 1)");
  
  xTaskCreatePinnedToCore(
    task_mqtt,
    "MQTT",
    4096,
    NULL,
    1,                   // Lower priority
    NULL,
    1                    // Core 1
  );
  Serial.println("   ✅ MQTT task created (Core 1)");
  
  // Uncomment if needed:
  // xTaskCreate(temp_humi_monitor, "TempHumi", 4096, NULL, 1, NULL);
  // xTaskCreate(Task_Toogle_BOOT, "BootBtn", 4096, NULL, 1, NULL);
  
  Serial.println("\n========================================");
  Serial.println("✅ All tasks created successfully!");
  Serial.println("========================================\n");
}

void loop()
{
  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  
  // Check every 10 seconds
  if (now - lastCheck > 10000) {
    lastCheck = now;
    
    if (check_info_File(1)) {
      if (!Wifi_reconnect()) {
        Webserver_stop();
      } else {
        CORE_IOT_reconnect();
      }
    }
  }
  
  Webserver_reconnect();
  
  // ✅ CRITICAL: Add delay to prevent watchdog reset
  vTaskDelay(100 / portTICK_PERIOD_MS);
}