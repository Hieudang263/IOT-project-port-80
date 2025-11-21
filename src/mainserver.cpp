#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#include "global.h"
#include "task_check_info.h"
#include "mainserver.h"

// ==================== LED CONFIG ====================
#define LED1_PIN 48
#define LED2_PIN 41
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

struct LEDState {
  bool isOn;
  int brightness;
  int pwmValue;
};

LEDState led1 = {false, 50, 127};
LEDState led2 = {false, 50, 127};

// ==================== EXTERN VARIABLES ====================
extern String ssid;
extern String password;
extern String wifi_ssid;
extern String wifi_password;
extern bool isWifiConnected;
extern SemaphoreHandle_t xBinarySemaphoreInternet;
extern String WIFI_SSID;
extern String WIFI_PASS;
extern void Save_info_File(String, String, String, String, String);
extern float glob_temperature;
extern float glob_humidity;

// ==================== GLOBAL VARIABLES ====================
WebServer server(80);
bool isAPMode = false;
bool connecting = false;
unsigned long connect_start_ms = 0;
String ap_ssid = "ESP32-Setup-Wifi";
String ap_password = "123456789";

// ==================== PWM FUNCTIONS ====================
void setupPWM() {
  Serial.println("🔧 Setting up PWM...");
  
  // ✅ Use new ESP32 Arduino Core 3.x API
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    if (!ledcAttach(LED1_PIN, PWM_FREQ, PWM_RESOLUTION)) {
      Serial.println("❌ Failed to attach LED1");
    }
    if (!ledcAttach(LED2_PIN, PWM_FREQ, PWM_RESOLUTION)) {
      Serial.println("❌ Failed to attach LED2");
    }
  #else
    // Old API for Arduino Core 2.x
    ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(1, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(LED1_PIN, 0);
    ledcAttachPin(LED2_PIN, 1);
  #endif
  
  // Start OFF
  ledcWrite(LED1_PIN, 0);
  ledcWrite(LED2_PIN, 0);
  
  Serial.println("✅ PWM initialized (LED1:GPIO48, LED2:GPIO41)");
}

void setLED(int num, bool state, int brightness) {
  LEDState* led = (num == 1) ? &led1 : &led2;
  uint8_t pin = (num == 1) ? LED1_PIN : LED2_PIN;
  
  led->isOn = state;
  led->brightness = constrain(brightness, 0, 100);
  
  if (state && brightness > 0) {
    led->pwmValue = map(led->brightness, 0, 100, 0, 255);
    ledcWrite(pin, led->pwmValue);
    Serial.printf("💡 LED%d: ON @ %d%% (PWM:%d)\n", num, led->brightness, led->pwmValue);
  } else {
    led->pwmValue = 0;
    ledcWrite(pin, 0);
    Serial.printf("💡 LED%d: OFF\n", num);
  }
}

// ==================== HTML PAGE ====================
String mainPage() {
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    if (f) {
      String html = f.readString();
      f.close();
      return html;
    }
  }
  return "<!DOCTYPE html><html><body><h1>Upload index.html</h1></body></html>";
}

// ==================== HTTP HANDLERS ====================
void handleRoot() {
  Serial.println("📥 GET /");
  server.send(200, "text/html", mainPage());
}

void handleControl() {
  int device = server.arg("device").toInt();
  String state = server.arg("state");
  int brightness = server.arg("brightness").toInt();
  
  Serial.println("\n======== LED CONTROL ========");
  Serial.printf("Device:%d State:%s Bright:%d%%\n", device, state.c_str(), brightness);
  
  if (device < 1 || device > 2) {
    server.send(400, "text/plain", "Invalid device");
    return;
  }
  
  bool isOn = (state == "ON" || state == "on");
  setLED(device, isOn, brightness);
  
  String json = "{\"ok\":true,\"led\":" + String(device) + 
                ",\"state\":\"" + (isOn?"ON":"OFF") + 
                "\",\"brightness\":" + String(brightness) + "}";
  server.send(200, "application/json", json);
  Serial.println("=============================\n");
}

void handleScan() {
  Serial.println("📥 GET /scan");
  int n = WiFi.scanNetworks();
  
  String json = "{\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"encryption\":\"";
    switch (WiFi.encryptionType(i)) {
      case WIFI_AUTH_OPEN: json += "Open"; break;
      case WIFI_AUTH_WPA2_PSK: json += "WPA2"; break;
      default: json += "Protected";
    }
    json += "\"}";
  }
  json += "]}";
  
  server.send(200, "application/json", json);
  Serial.printf("✅ Found %d networks\n", n);
}

void handleConnect() {
  Serial.println("\n======== WIFI CONNECT ========");
  
  wifi_ssid = server.arg("ssid");
  wifi_password = server.arg("pass");
  
  Serial.println("SSID: " + wifi_ssid);
  
  if (wifi_ssid.isEmpty()) {
    server.send(400, "text/plain", "SSID required");
    return;
  }
  
  WIFI_SSID = wifi_ssid;
  WIFI_PASS = wifi_password;
  
  // ✅ Send response BEFORE saving (which causes restart)
  server.send(200, "text/plain", "Connecting to: " + wifi_ssid);
  delay(100);  // Let response send
  
  // Save and restart
  Save_info_File(WIFI_SSID, WIFI_PASS, "", "", "");
  Serial.println("==============================\n");
}

void handleAPConfig() {
  String newSSID = server.arg("ssid");
  String newPass = server.arg("pass");
  
  if (newSSID.isEmpty()) {
    server.send(400, "text/plain", "SSID required");
    return;
  }
  if (!newPass.isEmpty() && newPass.length() < 8) {
    server.send(400, "text/plain", "Password min 8 chars");
    return;
  }
  
  File f = LittleFS.open("/ap_config.txt", "w");
  if (f) { f.println(newSSID); f.println(newPass); f.close(); }
  
  server.send(200, "text/plain", "Saved! Restarting...");
  delay(1000);
  ESP.restart();
}

void handleSensor() {
  String json;
  if (isnan(glob_temperature) || glob_temperature == -1) {
    json = "{\"error\":true,\"temperature\":0,\"humidity\":0}";
  } else {
    json = "{\"error\":false,\"temperature\":" + String(glob_temperature, 1) + 
           ",\"humidity\":" + String(glob_humidity, 1) + "}";
  }
  server.send(200, "application/json", json);
}

void handleStatic(String path, String type) {
  if (LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    if (f) { server.streamFile(f, type); f.close(); return; }
  }
  server.send(404, "text/plain", "Not found");
}

// ==================== AP FUNCTIONS ====================
void startAP() {
  Serial.println("\n======== START AP ========");
  
  // Load saved config
  if (LittleFS.exists("/ap_config.txt")) {
    File f = LittleFS.open("/ap_config.txt", "r");
    if (f) {
      ap_ssid = f.readStringUntil('\n'); ap_ssid.trim();
      ap_password = f.readStringUntil('\n'); ap_password.trim();
      f.close();
    }
  }
  
  Serial.println("AP SSID: " + ap_ssid);
  
  // ✅ DON'T change WiFi mode here - it's already set in setup()
  // Just configure the AP
  if (ap_password.length() >= 8) {
    WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
  } else {
    WiFi.softAP(ap_ssid.c_str());
  }
  
  delay(100);  // Wait for AP to start
  
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("==========================\n");
  
  isAPMode = true;
}

// ==================== MAIN SERVER TASK ====================
void main_server_task(void *pvParameters) {
  Serial.println("\n📡 Main Server Task Starting...");
  
  // ✅ Wait a bit for WiFi stack to be fully ready
  vTaskDelay(500 / portTICK_PERIOD_MS);
  
  // Setup PWM
  setupPWM();
  
  // Start AP
  startAP();
  
  // ✅ Register routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/connect", HTTP_GET, handleConnect);
  server.on("/apconfig", HTTP_GET, handleAPConfig);
  server.on("/sensor", HTTP_GET, handleSensor);
  
  server.on("/script.js", HTTP_GET, []() { handleStatic("/script.js", "application/javascript"); });
  server.on("/styles.css", HTTP_GET, []() { handleStatic("/styles.css", "text/css"); });
  
  server.onNotFound([]() {
    Serial.println("404: " + server.uri());
    server.send(404, "text/plain", "Not Found");
  });
  
  server.begin();
  Serial.println("✅ HTTP Server started on port 80");
  Serial.println("🌐 Access: http://" + WiFi.softAPIP().toString());
  
  // ==================== MAIN LOOP ====================
  for (;;) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}