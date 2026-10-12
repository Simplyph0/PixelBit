// ESP-NOW <-> USB serial bridge for an ESP32 board (tested target: ESP32-C3 SuperMini).
//
// It lets the PC emulator (tools/emulator) talk to real consoles: the emulator
// hands packets to this board over USB, and the board sends them on the air,
// and the other way round. It does not look inside the packets.
//
// Board settings (Arduino IDE): "ESP32C3 Dev Module", USB CDC On Boot: Enabled.
// Serial frames (both directions): 0xA5, length, payload, checksum, where the
// checksum is the XOR of the length byte and the payload bytes.
//   PC -> board:  'M'                     asks for this board's MAC
//                 'T' + dest MAC(6) + data   sends data by ESP-NOW
//   board -> PC:  'M' + MAC(6)            this board's MAC
//                 'R' + source MAC(6) + data  data received by ESP-NOW

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static const uint8_t CHANNEL = 1;   // must match LINK_CHANNEL in GameSystem/config.h
static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct RxSlot {
  uint8_t mac[6];
  uint8_t len;
  uint8_t data[250];
};
static const uint8_t SLOTS = 12;
static RxSlot queue[SLOTS];
static volatile uint8_t head = 0, tail = 0;

static void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  uint8_t next = (head + 1) % SLOTS;
  if (next == tail || len > 200) return;   // full: drop
  memcpy(queue[head].mac, info->src_addr, 6);
  memcpy(queue[head].data, data, len);
  queue[head].len = len;
  head = next;
}

static void addPeer(const uint8_t* mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;   // the current channel
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

static void writeFrame(const uint8_t* payload, size_t n) {
  uint8_t ck = n;
  for (size_t i = 0; i < n; i++) ck ^= payload[i];
  Serial.write((uint8_t)0xA5);
  Serial.write((uint8_t)n);
  Serial.write(payload, n);
  Serial.write(ck);
}

static void sendMac() {
  uint8_t payload[7] = {'M'};
  WiFi.macAddress(payload + 1);
  writeFrame(payload, 7);
}

static void handleFrame(const uint8_t* p, size_t n) {
  if (n >= 1 && p[0] == 'M') {
    sendMac();
  } else if (n >= 8 && p[0] == 'T') {
    addPeer(p + 1);
    esp_now_send(p + 1, p + 7, n - 7);
  }
}

// Reads frames from the PC one byte at a time
static void pollSerial() {
  static uint8_t buf[260];
  static size_t have = 0, want = 0;
  static enum { SYNC, LENGTH, BODY } state = SYNC;

  while (Serial.available()) {
    uint8_t c = Serial.read();
    switch (state) {
      case SYNC:
        if (c == 0xA5) state = LENGTH;
        break;
      case LENGTH:
        want = c + 1;   // payload plus checksum
        have = 0;
        state = want > 1 ? BODY : SYNC;
        buf[0] = c;     // the checksum starts from the length byte
        break;
      case BODY:
        buf[1 + have++] = c;
        if (have == want) {
          uint8_t ck = buf[0];
          for (size_t i = 0; i < want - 1; i++) ck ^= buf[1 + i];
          if (ck == buf[want]) handleFrame(buf + 1, want - 1);
          state = SYNC;
        }
        break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_now_init();
  esp_now_register_recv_cb(onReceive);
  addPeer(BROADCAST);
}

void loop() {
  pollSerial();

  while (tail != head) {
    uint8_t payload[1 + 6 + 200];
    payload[0] = 'R';
    memcpy(payload + 1, queue[tail].mac, 6);
    memcpy(payload + 7, queue[tail].data, queue[tail].len);
    writeFrame(payload, 7 + queue[tail].len);
    tail = (tail + 1) % SLOTS;
  }
}
