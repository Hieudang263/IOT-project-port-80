#include "task_wifi.h"

void startSTA()
{
    if (WIFI_SSID.isEmpty()) {
        Serial.println("⚠️ WIFI_SSID empty, cannot connect");
        return;
    }

    Serial.println("\n======== CONNECTING WIFI ========");
    Serial.println("SSID: " + WIFI_SSID);
    
    // ✅ DON'T change mode - keep AP+STA
    // WiFi.mode(WIFI_STA);  // REMOVED
    
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 200) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        if (timeout % 10 == 0) Serial.print(".");
        timeout++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ WiFi Connected!");
        Serial.println("IP: " + WiFi.localIP().toString());
        
        if (xBinarySemaphoreInternet != NULL) {
            xSemaphoreGive(xBinarySemaphoreInternet);
        }
    } else {
        Serial.println("❌ WiFi connection failed (20s timeout)");
    }
    Serial.println("=================================\n");
}

bool Wifi_reconnect()
{
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    
    if (WIFI_SSID.isEmpty()) {
        return false;
    }
    
    Serial.println("📡 WiFi reconnecting...");
    
    // ✅ Just disconnect STA, don't touch AP
    WiFi.disconnect(false);  // false = don't turn off WiFi
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 150) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        timeout++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ WiFi reconnected: " + WiFi.localIP().toString());
        
        if (xBinarySemaphoreInternet != NULL) {
            xSemaphoreGive(xBinarySemaphoreInternet);
        }
        return true;
    }
    
    Serial.println("❌ WiFi reconnect failed");
    return false;
}