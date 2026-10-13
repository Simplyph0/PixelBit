#pragma once
#include "Arduino.h"
#define HTTP_GET 1
#define HTTP_POST 2
// The web pages (save transfer, video upload) are not emulated; this is just
// enough for the firmware that serves them to compile.
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload {
  HTTPUploadStatus status;
  String filename, name, type;
  size_t totalSize, currentSize, contentLength;
  uint8_t buf[1436];
};
struct ESP8266WebServer {
  HTTPUpload up{};
  ESP8266WebServer(int) {}
  void on(const char*, int, void (*)()) {}
  void on(const char*, int, void (*)(), void (*)()) {}
  void begin() {} void stop() {} void handleClient() {}
  HTTPUpload& upload() { return up; }
  String arg(const char*) { return ""; }
  bool hasArg(const char*) { return false; }
  void send(int, const char*, const String&) {} void send(int, const char*, const char*) {}
  void send_P(int, const char*, const char*) {} void sendHeader(const char*, const char*) {}
};
