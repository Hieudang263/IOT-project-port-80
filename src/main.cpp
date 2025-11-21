#include "global.h"
#include "config_coreiot.h"  // ✅ THÊM
#include "coreiot.h"          // ✅ THÊM

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "mainserver.h"
#include "tinyml.h"

// include task
#include "task_check_info.h"
#include "task_toogle_boot.h"
#include "task_wifi.h"
#include "task_webserver.h"
#include "task_mqtt.h"  // ✅ File task MQTT CoreIOT

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("BOOT OK");
  
  // ✅ Mount LittleFS
  if (!LittleFS.begin(true)) {
      Serial.println("❌ LittleFS Mount Failed");
  } else {
      Serial.println("✅ LittleFS Mounted");
  }
  
  // ✅ TẠO SEMAPHORE (QUAN TRỌNG - CHỚ THÊM DÒNG NÀY!)
  xBinarySemaphoreInternet = xSemaphoreCreateBinary();
  
  // ✅ THÊM: Tạo file coreiot.json mặc định nếu chưa có
  if (!LittleFS.exists("/coreiot.json")) {
      Serial.println("⚠️ Chưa có coreiot.json, tạo file mặc định...");
      
      // Tạo config mặc định
      coreiot_server = "app.coreiot.io";
      coreiot_port = 1883;
      coreiot_client_id = "ESP32_" + String((uint32_t)ESP.getEfuseMac(), HEX);
      coreiot_username = "";
      coreiot_password = "";
      
      if (saveCoreIOTConfig()) {
          Serial.println("✅ Đã tạo coreiot.json mặc định");
      }
  }
  
  // ✅ Load CoreIOT config
  loadCoreIOTConfig();
  
  check_info_File(0);

  //xTaskCreate(led_blinky, "Task LED Blink", 2048, NULL, 2, NULL);
  //xTaskCreate(neo_blinky, "Task NEO Blink", 2048, NULL, 2, NULL);
  //xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor", 4096, NULL, 2, NULL);
  xTaskCreate(main_server_task, "Task Main Server", 8192, NULL, 2, NULL);
  // xTaskCreate(tiny_ml_task, "Tiny ML Task", 2048, NULL, 2, NULL);
  xTaskCreate(task_mqtt, "MQTT Task", 4096, NULL, 1, NULL);
  //xTaskCreate(Task_Toogle_BOOT, "Task_Toogle_BOOT", 4096, NULL, 2, NULL);

  Serial.println("✅ Tất cả task đã được tạo");
}

void loop()
{
  static unsigned long lastWifiCheck = 0;
  unsigned long now = millis();
  
  if (check_info_File(1))
  {
    // ✅ Chỉ check WiFi mỗi 10 giây
    if (now - lastWifiCheck > 10000)
    {
      lastWifiCheck = now;
      
      if (!Wifi_reconnect())
      {
        Webserver_stop();
      }
      else
      {
        CORE_IOT_reconnect();
      }
    }
  }
  
  Webserver_reconnect();
  
  // ✅ THÊM DELAY để tránh watchdog (QUAN TRỌNG!)
  vTaskDelay(100 / portTICK_PERIOD_MS);
}