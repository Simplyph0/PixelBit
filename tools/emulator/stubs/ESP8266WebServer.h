#pragma once
#include "Arduino.h"
#define HTTP_GET 1
#define HTTP_POST 2
// The save-transfer web page is not emulated
struct ESP8266WebServer {
  ESP8266WebServer(int) {}
  void on(const char*, int, void (*)()) {} void begin() {} void stop() {} void handleClient() {}
  String arg(const char*) { return ""; }
  void send(int, const char*, const String&) {} void send(int, const char*, const char*) {}
  void send_P(int, const char*, const char*) {} void sendHeader(const char*, const char*) {}
};
