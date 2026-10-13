// Fake radio over UDP loopback for two test processes, plus a real clock.
#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "espnow.h"
#include "platform.h"
unsigned long g_sentPackets = 0, g_recvPackets = 0;
int g_index = 0, g_port = 41000, g_loss = 0; uint32_t g_chip = 0x123456;
static esp_now_recv_cb_t rcb = nullptr;
// PB_SPEED=N runs the simulated clock N times faster than real time (the radio is a loopback, so it keeps up)
static double speed() { static double s = getenv("PB_SPEED") ? atof(getenv("PB_SPEED")) : 1.0; return s > 0 ? s : 1.0; }
unsigned long millis() { return (unsigned long)(plat::monotonicMs() * speed()); }
void air_sleep_us(unsigned long long us) { plat::sleepUs((unsigned long long)(us / speed())); }
void delay(unsigned long ms) { air_sleep_us(ms * 1000ULL); }
void WiFiClass::macAddress(uint8_t* m) { m[0] = 2; m[1] = m[2] = m[3] = m[4] = 0; m[5] = g_index; }
uint32_t EspClass::random() { return rand(); } uint32_t EspClass::getChipId() { return g_chip; }
int esp_now_init() { if (const char* p = getenv("PB_AIR_PORT")) g_port = atoi(p); plat::udpOpen(g_port + g_index); return 0; }   // PB_AIR_PORT: several pairs can play at once
int esp_now_deinit() { plat::udpClose(); return 0; }
static esp_now_send_cb_t g_sendCb = nullptr;
int esp_now_set_self_role(u8) { return 0; } int esp_now_register_recv_cb(esp_now_recv_cb_t c) { rcb = c; return 0; } int esp_now_unregister_recv_cb() { rcb = nullptr; return 0; } int esp_now_register_send_cb(esp_now_send_cb_t cb) { g_sendCb = cb; return 0; }
int esp_now_add_peer(u8*, u8, u8, u8*, u8) { return 0; }
int esp_now_send(u8* da, u8* data, int len) {
  g_sentPackets++;
  if (rand() % 100 < g_loss) { if (g_sendCb) g_sendCb(da, 1); return 0; }   // lost: no acknowledgement
  u8 buf[300]; WiFi.macAddress(buf); memcpy(buf + 6, data, len);
  plat::udpSend(g_port + 1 - g_index, buf, 6 + len); if (g_sendCb) g_sendCb(da, 0); return 0; }
void air_pump();
extern void (*g_yieldHook)();
int esp_now_init_hook = (g_yieldHook = air_pump, 0);
void air_pump() { if (!plat::udpIsOpen() || !rcb) return; u8 buf[300]; for (;;) { int n = plat::udpRecv(buf, sizeof buf); if (n <= 6) break; g_recvPackets++; rcb(buf, buf + 6, n - 6); } }
