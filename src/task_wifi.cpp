#include "task_wifi.h"

void startSTA()
{
    if (WIFI_SSID.isEmpty())
    {
        Serial.println("⚠️ WIFI_SSID trống, không thể kết nối");
        vTaskDelete(NULL);
        return;
    }

    WiFi.mode(WIFI_STA);

    if (WIFI_PASS.isEmpty())
    {
        WiFi.begin(WIFI_SSID.c_str());
    }
    else
    {
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    }

    Serial.print("Đang kết nối WiFi");
    
    // ✅ THÊM TIMEOUT để tránh vòng lặp vô tận
    int timeout = 0;
    const int MAX_TIMEOUT = 200; // 20 giây (200 x 100ms)
    
    while (WiFi.status() != WL_CONNECTED && timeout < MAX_TIMEOUT)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS); // ✅ Dùng vTaskDelay thay vì delay
        Serial.print(".");
        timeout++;
    }
    
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println(" Đã kết nối!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        
        // ✅ Give semaphore khi connect thành công
        if (xBinarySemaphoreInternet != NULL)
        {
            xSemaphoreGive(xBinarySemaphoreInternet);
            Serial.println("✅ Đã Give semaphore Internet");
        }
        else
        {
            Serial.println("❌ WARNING: Semaphore chưa được tạo!");
        }
    }
    else
    {
        Serial.println(" ❌ Timeout! WiFi không kết nối được sau 20s");
    }
}

bool Wifi_reconnect()
{
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED)
    {
        return true;
    }
    
    Serial.println("📡 Đang reconnect WiFi...");
    
    // ✅ Disconnect trước khi reconnect
    WiFi.disconnect(true);
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Chờ 1s cho WiFi reset
    
    WiFi.mode(WIFI_STA);
    
    if (WIFI_PASS.isEmpty())
    {
        WiFi.begin(WIFI_SSID.c_str());
    }
    else
    {
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    }
    
    // ✅ Đợi kết nối với timeout
    int timeout = 0;
    const int MAX_TIMEOUT = 200; // 20 giây
    
    while (WiFi.status() != WL_CONNECTED && timeout < MAX_TIMEOUT)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS); // ✅ Dùng vTaskDelay
        timeout++;
        
        // ✅ Chỉ in dấu chấm mỗi giây (mỗi 10 lần)
        if (timeout % 10 == 0) {
            Serial.print(".");
        }
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("✅ WiFi reconnect thành công!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        
        // ✅ Give semaphore khi reconnect thành công
        if (xBinarySemaphoreInternet != NULL)
        {
            xSemaphoreGive(xBinarySemaphoreInternet);
            Serial.println("✅ Đã Give semaphore Internet");
        }
        
        return true;
    }
    else
    {
        Serial.println("❌ WiFi reconnect thất bại sau 20s");
        return false;
    }
}