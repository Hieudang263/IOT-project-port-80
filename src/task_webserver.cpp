#include "task_webserver.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config_coreiot.h"
#include "coreiot.h"

static AsyncWebServer dashboardServer(8080);  
static AsyncWebSocket ws("/ws");

bool webserver_isrunning = false;

void Webserver_sendata(String data) {
    if (ws.count() > 0) {
        ws.textAll(data);
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket #%u connected\n", client->id());
    }
    else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode == WS_TEXT) {
            String message;
            message += String((char *)data).substring(0, len);
            handleWebSocketMessage(message);
        }
    }
}

void setupCoreIOTAPI() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    // ✅ GET config (ẩn mật khẩu)
    dashboardServer.on("/api/coreiot/config", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("📥 GET /api/coreiot/config");
        
        StaticJsonDocument<512> doc;
        doc["server"] = coreiot_server;
        doc["port"] = coreiot_port;
        doc["client_id"] = coreiot_client_id;
        doc["username"] = coreiot_username;
        doc["password_set"] = (coreiot_password.length() > 0);

        String response;
        serializeJson(doc, response);
        
        Serial.println("📤 Response sent");
        request->send(200, "application/json", response);
    });

    // ✅ POST config
    dashboardServer.on("/api/coreiot/config", HTTP_POST, 
        [](AsyncWebServerRequest *request){}, 
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        Serial.println("📥 POST /api/coreiot/config");
        Serial.printf("   Body: %d bytes\n", len);

        StaticJsonDocument<1024> doc;
        DeserializationError err = deserializeJson(doc, (const char*)data, len);
        StaticJsonDocument<256> response;

        if (err) {
            Serial.println("❌ JSON error: " + String(err.c_str()));
            response["success"] = false;
            response["message"] = "JSON không hợp lệ";
            String jsonResponse;
            serializeJson(response, jsonResponse);
            request->send(400, "application/json", jsonResponse);
            return;
        }

        String server = doc["server"] | "";
        int port = doc["port"] | 0;
        String client_id = doc["client_id"] | "";
        String username = doc["username"] | "";
        String password = doc["password"] | "";

        Serial.println("📄 Data received:");
        Serial.println("   Server: " + server);
        Serial.println("   Port: " + String(port));
        Serial.println("   Client ID: " + client_id);
        Serial.println("   Username: " + username);
        Serial.println("   Password: " + String(password.length() > 0 ? "***" : "(empty)"));

        if (server.length() == 0 || port == 0 || client_id.length() == 0 || username.length() == 0) {
            response["success"] = false;
            response["message"] = "Thiếu thông tin bắt buộc";
            String jsonResponse;
            serializeJson(response, jsonResponse);
            request->send(400, "application/json", jsonResponse);
            return;
        }

        if (password == "***" || password == "") {
            password = coreiot_password;
            Serial.println("   → Keeping old password");
        }

        coreiot_server = server;
        coreiot_port = port;
        coreiot_client_id = client_id;
        coreiot_username = username;
        coreiot_password = password;

        if (saveCoreIOTConfig()) {
            response["success"] = true;
            response["message"] = "Đã lưu! MQTT sẽ reconnect...";
            Serial.println("💾 Saved! MQTT reconnecting...");
        } else {
            response["success"] = false;
            response["message"] = "Lỗi ghi file";
        }

        String jsonResponse;
        serializeJson(response, jsonResponse);
        request->send(200, "application/json", jsonResponse);
    });

    // ✅ GET status
    dashboardServer.on("/api/coreiot/status", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<256> doc;
        doc["mqtt_connected"] = isMQTTConnected();
        doc["wifi_connected"] = WiFi.isConnected();
        doc["wifi_ssid"] = WiFi.SSID();
        doc["wifi_ip"] = WiFi.localIP().toString();
        doc["server"] = coreiot_server;
        doc["port"] = coreiot_port;
        doc["client_id"] = coreiot_client_id;
        doc["username"] = coreiot_username;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    Serial.println("✅ CoreIOT API initialized");
}

void connnectWSV() {
    ws.onEvent(onEvent);
    dashboardServer.addHandler(&ws);

    // ✅ Route cho file giao diện
    dashboardServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });

    dashboardServer.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/script.js", "application/javascript");
    });

    dashboardServer.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/styles.css", "text/css");
    });

    // ✅ Route favicon tránh spam log
    dashboardServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(204);  // No Content
    });

    setupCoreIOTAPI();

    dashboardServer.onNotFound([](AsyncWebServerRequest *request){
        Serial.println("❌ 404: " + request->url());
        request->send(404, "text/plain", "Not Found");
    });

    dashboardServer.begin();
    ElegantOTA.begin(&dashboardServer);
    webserver_isrunning = true;

    Serial.println("\n✅ Web Server: http://" + WiFi.localIP().toString() + ":8080");
    Serial.println("✅ API Endpoints:");
    Serial.println("   GET/POST /api/coreiot/config");
    Serial.println("   GET /api/coreiot/status\n");
}

void Webserver_stop() {
    ws.closeAll();
    dashboardServer.end();
    webserver_isrunning = false;
}

void Webserver_reconnect() {
    if (!webserver_isrunning) {
        connnectWSV();
    }
    ElegantOTA.loop();
}
