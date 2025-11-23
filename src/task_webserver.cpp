#include "task_webserver.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config_coreiot.h"
#include "coreiot.h"
#include "mainserver.h"

static AsyncWebServer dashboardServer(8080);
static AsyncWebSocket ws("/ws");

bool webserver_isrunning = false;

void Webserver_sendata(String data) {
    if (ws.count() > 0) {
        ws.textAll(data);
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WS #%u connected\n", client->id());
    }
    else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WS #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode == WS_TEXT) {
            String msg = String((char*)data).substring(0, len);
            handleWebSocketMessage(msg);
        }
    }
}

void setupCoreIOTAPI() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    // GET config
    dashboardServer.on("/api/coreiot/config", HTTP_GET, [](AsyncWebServerRequest *req){
        StaticJsonDocument<512> doc;
        doc["server"] = coreiot_server;
        doc["port"] = coreiot_port;
        doc["client_id"] = coreiot_client_id;
        doc["username"] = coreiot_username;
        doc["password_set"] = (coreiot_password.length() > 0);
        
        String res;
        serializeJson(doc, res);
        req->send(200, "application/json", res);
    });

    // POST config
    dashboardServer.on("/api/coreiot/config", HTTP_POST,
        [](AsyncWebServerRequest *req){},
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
            StaticJsonDocument<512> doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"success\":false}");
                return;
            }
            
            coreiot_server = doc["server"] | "";
            coreiot_port = doc["port"] | 1883;
            coreiot_client_id = doc["client_id"] | "";
            coreiot_username = doc["username"] | "";
            String pwd = doc["password"] | "";
            if (pwd != "***" && pwd != "") coreiot_password = pwd;
            
            saveCoreIOTConfig();
            req->send(200, "application/json", "{\"success\":true}");
        });

    // GET status
    dashboardServer.on("/api/coreiot/status", HTTP_GET, [](AsyncWebServerRequest *req){
        StaticJsonDocument<256> doc;
        doc["mqtt_connected"] = isMQTTConnected();
        doc["wifi_connected"] = WiFi.isConnected();
        doc["wifi_ip"] = WiFi.localIP().toString();
        
        String res;
        serializeJson(doc, res);
        req->send(200, "application/json", res);
    });
}

void connnectWSV() {
    // ✅ CHECK: Only start if WiFi STA is connected
    if (!WiFi.isConnected()) {
        Serial.println("⚠️ Dashboard server waiting for WiFi STA...");
        return;
    }
    
    Serial.println("🚀 Starting Dashboard Server (port 8080)...");
    
    ws.onEvent(onEvent);
    dashboardServer.addHandler(&ws);

    dashboardServer.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(LittleFS, "/index.html", "text/html");
    });
    dashboardServer.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(LittleFS, "/script.js", "application/javascript");
    });
    dashboardServer.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(LittleFS, "/styles.css", "text/css");
    });

    // LED control via HTTP (same API as port 80)
    dashboardServer.on("/control", HTTP_GET, [](AsyncWebServerRequest *req){
        int device = req->hasParam("device") ? req->getParam("device")->value().toInt() : 0;
        String state = req->hasParam("state") ? req->getParam("state")->value() : "";
        int brightness = req->hasParam("brightness") ? req->getParam("brightness")->value().toInt() : 0;

        int httpCode = 200;
        String body = processLedControl(device, state, brightness, httpCode);
        req->send(httpCode, httpCode == 200 ? "application/json" : "text/plain", body);
    });
    dashboardServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(204);
    });

    setupCoreIOTAPI();

    dashboardServer.onNotFound([](AsyncWebServerRequest *req){
        req->send(404, "text/plain", "Not Found");
    });

    dashboardServer.begin();
    ElegantOTA.begin(&dashboardServer);
    webserver_isrunning = true;

    Serial.println("✅ Dashboard: http://" + WiFi.localIP().toString() + ":8080");
}

void Webserver_stop() {
    if (webserver_isrunning) {
        ws.closeAll();
        dashboardServer.end();
        webserver_isrunning = false;
        Serial.println("🛑 Dashboard server stopped");
    }
}

void Webserver_reconnect() {
    // ✅ Only start dashboard when WiFi STA is connected
    if (!webserver_isrunning && WiFi.isConnected()) {
        connnectWSV();
    }
    
    if (webserver_isrunning) {
        ElegantOTA.loop();
    }
}
