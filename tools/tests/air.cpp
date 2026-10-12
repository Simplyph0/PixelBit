// Fake radio over UDP loopback for two test processes, plus a real clock.
#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "espnow.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
unsigned long g_sentPackets = 0, g_recvPackets = 0;
int g_index = 0, g_port = 41000, g_loss = 0; uint32_t g_chip = 0x123456;
static int sock = -1; static esp_now_recv_cb_t rcb = nullptr;
unsigned long millis() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000UL + t.tv_nsec / 1000000; }
void delay(unsigned long ms) { usleep(ms * 1000); }
void WiFiClass::macAddress(uint8_t* m) { m[0] = 2; m[1] = m[2] = m[3] = m[4] = 0; m[5] = g_index; }
uint32_t EspClass::random() { return rand(); } uint32_t EspClass::getChipId() { return g_chip; }
int esp_now_init() { sock = socket(AF_INET, SOCK_DGRAM, 0); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = inet_addr("127.0.0.1"); a.sin_port = htons(g_port + g_index); bind(sock, (sockaddr*)&a, sizeof a); fcntl(sock, F_SETFL, O_NONBLOCK); return 0; }
int esp_now_deinit() { if (sock >= 0) close(sock); sock = -1; return 0; }
static esp_now_send_cb_t g_sendCb = nullptr;
int esp_now_set_self_role(u8) { return 0; } int esp_now_register_recv_cb(esp_now_recv_cb_t c) { rcb = c; return 0; } int esp_now_unregister_recv_cb() { rcb = nullptr; return 0; } int esp_now_register_send_cb(esp_now_send_cb_t cb) { g_sendCb = cb; return 0; }
int esp_now_add_peer(u8*, u8, u8, u8*, u8) { return 0; }
int esp_now_send(u8* da, u8* data, int len) {
  g_sentPackets++;
  if (rand() % 100 < g_loss) { if (g_sendCb) g_sendCb(da, 1); return 0; }   // lost: no acknowledgement
  u8 buf[300]; WiFi.macAddress(buf); memcpy(buf + 6, data, len);
  sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = inet_addr("127.0.0.1"); a.sin_port = htons(g_port + 1 - g_index);
  sendto(sock, buf, 6 + len, 0, (sockaddr*)&a, sizeof a); if (g_sendCb) g_sendCb(da, 0); return 0; }
void air_pump();
extern void (*g_yieldHook)();
int esp_now_init_hook = (g_yieldHook = air_pump, 0);
void air_pump() { if (sock < 0 || !rcb) return; u8 buf[300]; for (;;) { int n = recv(sock, buf, sizeof buf, 0); if (n <= 6) break; g_recvPackets++; rcb(buf, buf + 6, n - 6); } }
