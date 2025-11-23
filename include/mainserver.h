#ifndef MAINSERVER_H
#define MAINSERVER_H

#include <Arduino.h>
#include "global.h"

extern bool isAPMode;

String mainPage();
String settingsPage();

// ✅ Khai báo hàm startAP để các file khác có thể dùng
void startAP();

void setupServer();
void connectToWiFi();

// Shared LED control helper so both port 80 and port 8080 can reuse it
String processLedControl(int device, const String &state, int brightness, int &httpCode);
void setLED(int num, bool state, int brightness);

void main_server_task(void *pvParameters);

#endif
