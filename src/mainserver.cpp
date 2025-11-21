#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#include "global.h"
#include "task_check_info.h"
#include "mainserver.h"

// ✅ LED PWM Config
#define LED1_PIN 48
#define LED2_PIN 41
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8 // 0-255
#define PWM_CHANNEL_1 0
#define PWM_CHANNEL_2 1

// ✅ LED State
struct LEDState {
  bool isOn;
  int brightness; // 0-100
  int pwmValue;   // 0-255
};

LEDState led1 = {false, 50, 127};
LEDState led2 = {false, 50, 127};

// Extern variables
extern String ssid;
extern String password;
extern String wifi_ssid;
extern String wifi_password;
extern bool isWifiConnected;
extern SemaphoreHandle_t xBinarySemaphoreInternet;
extern String WIFI_SSID;
extern String WIFI_PASS;
extern void Save_info_File(String wifi_ssid, String wifi_pass,
                           String token, String server, String port);

WebServer server(80);

bool isAPMode = false;
bool connecting = false;
unsigned long connect_start_ms = 0;

// ==================== PWM FUNCTIONS ====================
void setupPWM() {
  ledcSetup(PWM_CHANNEL_1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_2, PWM_FREQ, PWM_RESOLUTION);
  
  ledcAttachPin(LED1_PIN, PWM_CHANNEL_1);
  ledcAttachPin(LED2_PIN, PWM_CHANNEL_2);
  
  // Start with LEDs OFF
  ledcWrite(PWM_CHANNEL_1, 0);
  ledcWrite(PWM_CHANNEL_2, 0);
  
  Serial.println("✅ PWM initialized (LED1: GPIO48, LED2: GPIO41)");
}

void setLED(int ledNum, bool state, int brightness) {
  LEDState* led = (ledNum == 1) ? &led1 : &led2;
  int channel = (ledNum == 1) ? PWM_CHANNEL_1 : PWM_CHANNEL_2;
  
  led->isOn = state;
  led->brightness = constrain(brightness, 0, 100);
  
  if (state) {
    // Map 0-100 to 0-255
    led->pwmValue = map(led->brightness, 0, 100, 0, 255);
    ledcWrite(channel, led->pwmValue);
    Serial.printf("💡 LED%d ON - Brightness: %d%% (PWM: %d)\n", 
                  ledNum, led->brightness, led->pwmValue);
  } else {
    led->pwmValue = 0;
    ledcWrite(channel, 0);
    Serial.printf("💡 LED%d OFF\n", ledNum);
  }
}

// ==================== HTML PAGES ====================
String mainPage() {
  // ✅ SERVE FROM LITTLEFS (PRIORITY)
  if (LittleFS.exists("/index.html")) {
    File file = LittleFS.open("/index.html", "r");
    if (file) {
      String html = file.readString();
      file.close();
      Serial.println("📄 Serving /index.html from LittleFS");
      return html;
    }
  }
  
  // ❌ FALLBACK: Simple HTML nếu không có file
  Serial.println("⚠️ /index.html not found in LittleFS, using fallback");
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 - Upload Required</title>
  <style>
    body { font-family: Arial; display: flex; justify-content: center; align-items: center; 
           min-height: 100vh; background: linear-gradient(135deg, #667eea, #764ba2); margin: 0; }
    .box { background: white; padding: 40px; border-radius: 20px; text-align: center; 
           box-shadow: 0 20px 60px rgba(0,0,0,0.3); max-width: 400px; }
    h1 { color: #667eea; margin-bottom: 20px; }
    p { color: #666; line-height: 1.6; }
    a { display: inline-block; margin-top: 20px; padding: 12px 24px; background: #667eea; 
        color: white; text-decoration: none; border-radius: 8px; }
  </style>
</head>
<body>
  <div class="box">
    <h1>⚠️ Files Missing</h1>
    <p>Please upload <strong>index.html</strong>, <strong>script.js</strong>, and <strong>styles.css</strong> to LittleFS.</p>
    <p>Use PlatformIO Upload Filesystem Image or Arduino IDE Data Upload.</p>
    <a href="/settings">Go to Wi-Fi Settings</a>
  </div>
</body>
</html>
)rawliteral";
}

String settingsPage() {
  // ✅ MINIMAL SETTINGS PAGE (không cần file riêng)
  return R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Cấu hình Wi-Fi</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #1e90ff, #00e6b8);
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      margin: 0;
    }
    .card {
      background: white;
      padding: 40px;
      border-radius: 20px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.15);
      width: 100%;
      max-width: 360px;
      text-align: center;
    }
    h2 {
      color: #1e90ff;
      margin-bottom: 10px;
    }
    p {
      color: #666;
      margin-bottom: 25px;
      font-size: 14px;
    }
    input {
      width: 100%;
      padding: 12px 16px;
      margin: 10px 0;
      border-radius: 12px;
      border: 2px solid #e0e0e0;
      font-size: 15px;
      box-sizing: border-box;
      transition: all 0.3s;
    }
    input:focus {
      outline: none;
      border-color: #1e90ff;
      box-shadow: 0 0 0 3px rgba(30, 144, 255, 0.1);
    }
    .btn-row {
      margin-top: 20px;
      display: flex;
      gap: 12px;
    }
    button {
      flex: 1;
      border: none;
      border-radius: 12px;
      padding: 12px 20px;
      font-size: 15px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
    }
    .btn-primary {
      background: linear-gradient(90deg, #1e90ff, #00e6b8);
      color: white;
    }
    .btn-primary:hover {
      transform: translateY(-2px);
      box-shadow: 0 6px 20px rgba(30, 144, 255, 0.3);
    }
    .btn-secondary {
      background: #f1f5ff;
      color: #345;
    }
    .btn-secondary:hover {
      background: #e0e8ff;
    }
    #msg {
      margin-top: 15px;
      padding: 12px;
      border-radius: 10px;
      font-size: 14px;
      display: none;
    }
    #msg.show {
      display: block;
    }
    #msg.success {
      background: #d4edda;
      color: #155724;
    }
    #msg.error {
      background: #f8d7da;
      color: #721c24;
    }
  </style>
</head>
<body>
  <div class="card">
    <h2>⚙️ Cấu hình Wi-Fi</h2>
    <p>Nhập thông tin Wi-Fi để kết nối ESP32 vào mạng của bạn</p>
    
    <input id="ssid" placeholder="Tên Wi-Fi (SSID)" autocomplete="off" />
    <input id="pass" type="password" placeholder="Mật khẩu" autocomplete="off" />
    
    <div class="btn-row">
      <button class="btn-primary" onclick="sendConfig()">Kết nối</button>
      <button class="btn-secondary" onclick="history.back()">Quay lại</button>
    </div>
    
    <div id="msg"></div>
  </div>

  <script>
    function sendConfig() {
      const ssid = document.getElementById('ssid').value.trim();
      const pass = document.getElementById('pass').value.trim();
      const msg = document.getElementById('msg');

      if (!ssid) {
        showMessage('Vui lòng nhập tên Wi-Fi!', 'error');
        return;
      }

      showMessage('Đang gửi cấu hình...', 'success');

      fetch('/connect?ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass))
        .then(res => res.text())
        .then(txt => {
          showMessage('✅ ' + txt, 'success');
        })
        .catch(err => {
          showMessage('❌ Lỗi: ' + err, 'error');
        });
    }

    function showMessage(text, type) {
      const msg = document.getElementById('msg');
      msg.textContent = text;
      msg.className = 'show ' + type;
    }
  </script>
</body>
</html>
)rawliteral";
}

// ==================== HTTP HANDLERS ====================
void handleRoot() {
  server.send(200, "text/html", mainPage());
}

void handleSettings() {
  server.send(200, "text/html", settingsPage());
}

void handleConnect() {
  Serial.println("\n===== /connect called =====");
  
  wifi_ssid = server.arg("ssid");
  wifi_password = server.arg("pass");
  
  Serial.println("SSID from web: " + wifi_ssid);
  Serial.println("PASS length: " + String(wifi_password.length()));
  
  // Update global variables
  WIFI_SSID = wifi_ssid;
  WIFI_PASS = wifi_password;
  
  // ✅ SAVE TO FILE
  Save_info_File(WIFI_SSID, WIFI_PASS, CORE_IOT_TOKEN, CORE_IOT_SERVER, CORE_IOT_PORT);
  Serial.println("💾 Saved WiFi to /info.dat");
  
  // Response
  server.send(200, "text/plain", "Đang kết nối... Xem Serial Monitor");
  
  // Start STA connection
  connecting = true;
  connect_start_ms = millis();
  connectToWiFi();
}

void handleControl() {
  int device = server.arg("device").toInt();
  String state = server.arg("state");
  int brightness = server.arg("brightness").toInt();
  
  Serial.printf("📥 Control: LED%d = %s, Brightness = %d%%\n", 
                device, state.c_str(), brightness);
  
  bool isOn = (state == "ON");
  setLED(device, isOn, brightness);
  
  server.send(200, "text/plain", "OK");
}

// ✅ SERVE STATIC FILES FROM LITTLEFS
void handleStaticFile(String path, String contentType) {
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    if (file) {
      server.streamFile(file, contentType);
      file.close();
      Serial.println("📄 Served: " + path);
      return;
    }
  }
  
  server.send(404, "text/plain", "File not found: " + path);
  Serial.println("❌ 404: " + path);
}

// ==================== WIFI FUNCTIONS ====================
void startAP() {
  Serial.println("\n=== Starting AP Mode ===");
  Serial.println("SSID: " + ssid);
  Serial.println("PASS: " + password);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str(), password.c_str());
  
  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(ip);
  
  isAPMode = true;
}

void connectToWiFi() {
  if (wifi_ssid.isEmpty()) {
    Serial.println("❌ SSID empty, cannot connect!");
    return;
  }
  
  Serial.println("\n=== Connecting to WiFi ===");
  Serial.println("SSID: " + wifi_ssid);
  Serial.println("PASS length: " + String(wifi_password.length()));
  
  WiFi.mode(WIFI_STA);
  
  if (wifi_password.isEmpty()) {
    WiFi.begin(wifi_ssid.c_str());
  } else {
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  }
}

// ==================== MAIN TASK ====================
void main_server_task(void *pvParameters) {
  Serial.println("\n=== Main Server Task Started ===");
  
  // ✅ Initialize PWM
  setupPWM();
  
  // Start AP if not already in AP/STA mode
  if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
    startAP();
  }
  
  // ✅ Register HTTP routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/connect", HTTP_GET, handleConnect);
  server.on("/control", HTTP_GET, handleControl);
  
  // ✅ Serve static files from LittleFS
  server.on("/script.js", HTTP_GET, []() {
    handleStaticFile("/script.js", "application/javascript");
  });
  server.on("/styles.css", HTTP_GET, []() {
    handleStaticFile("/styles.css", "text/css");
  });
  
  // ✅ 404 handler
  server.onNotFound([]() {
    server.send(404, "text/plain", "404 Not Found");
    Serial.println("❌ 404: " + server.uri());
  });
  
  server.begin();
  Serial.println("✅ HTTP server started on port 80");
  Serial.println("📡 Access Points:");
  Serial.println("   AP Mode: http://192.168.4.1");
  if (WiFi.getMode() == WIFI_STA && WiFi.isConnected()) {
    Serial.println("   STA Mode: http://" + WiFi.localIP().toString());
  }
  
  // Main loop
  for (;;) {
    server.handleClient();
    
    if (connecting) {
      wl_status_t st = WiFi.status();
      
      if (st == WL_CONNECTED) {
        Serial.println("\n✅ WiFi STA connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        
        isWifiConnected = true;
        connecting = false;
        isAPMode = false;
        
        if (xBinarySemaphoreInternet != NULL) {
          xSemaphoreGive(xBinarySemaphoreInternet);
          Serial.println("✅ Semaphore given");
        }
      }
      else if (millis() - connect_start_ms > 15000) {
        Serial.println("\n❌ WiFi timeout! Back to AP mode");
        
        connecting = false;
        isWifiConnected = false;
        WiFi.disconnect(true);
        startAP();
      }
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}