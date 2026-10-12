#include "gamelink.h"
#include "crypto.h"
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <user_interface.h>

GameLink gameLink;

static const uint8_t MAGIC = 0xA7;
enum PacketType : uint8_t {
  T_BEACON = 1, T_JOIN_REQ = 2, T_JOIN_OK = 3, T_JOIN_DENY = 4, T_CANCEL = 5, T_SESSION = 8, T_BULK = 9
};
enum SessionKind : uint8_t { K_HELLO = 0, K_DATA = 1, K_BYE = 2 };

static const uint8_t TAG_LEN = 8;
static const unsigned long BEACON_MS = 400;
static const unsigned long REQUEST_RESEND_MS = 500;
static const unsigned long ASK_TIMEOUT_MS = 15000;
static const unsigned long REQUEST_TIMEOUT_MS = 20000;   // host: ignored request is denied
static const unsigned long ACCEPT_RESEND_MS = 300;
static const unsigned long ACCEPT_TIMEOUT_MS = 6000;
static const unsigned long KEEPALIVE_MS = 250;
static const unsigned long LOST_MS = 2500;
static const unsigned long HOST_EXPIRE_MS = 2500;

static uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ---------------------------------------------------------------------------
// Receive queue: the radio callback only copies the packet; poll() handles it.
// ---------------------------------------------------------------------------
struct RxSlot {
  uint8_t mac[6];
  uint8_t len;
  uint8_t data[216];   // big enough for a bulk packet (6 + 200 + 8 bytes)
};
static const uint8_t RX_SLOTS = 12;   // a burst of bulk packets can arrive between two calls to poll()
static RxSlot rxQueue[RX_SLOTS];
static volatile uint8_t rxHead = 0, rxTail = 0;

static void onReceive(uint8_t* mac, uint8_t* data, uint8_t len) {
  uint8_t next = (rxHead + 1) % RX_SLOTS;
  if (next == rxTail || len > sizeof(rxQueue[0].data)) return;   // full: drop
  memcpy(rxQueue[rxHead].mac, mac, 6);
  memcpy(rxQueue[rxHead].data, data, len);
  rxQueue[rxHead].len = len;
  rxHead = next;
}

// Bulk packets are sent one at a time: the radio tells us when one has left (and
// whether the other console acknowledged it) through this callback
static volatile bool bulkInFlight = false;
static volatile bool bulkFailedFlag = false;
static unsigned long bulkSentAt = 0;

static void onSent(uint8_t*, uint8_t status) {
  if (bulkInFlight) {
    bulkInFlight = false;
    if (status != 0) bulkFailedFlag = true;
  }
}

static void (*bulkHandler)(const uint8_t*, uint8_t) = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static const char ID_ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   // no 0/O, 1/I

// 24 bits fit in 5 base-32 characters, so the text is lossless
static void idToText(const uint8_t id[3], char* out) {
  uint32_t v = (uint32_t)id[0] << 16 | (uint32_t)id[1] << 8 | id[2];
  for (int i = 0; i < LINK_ID_LEN; i++) out[i] = ID_ALPHABET[(v >> (5 * (LINK_ID_LEN - 1 - i))) & 31];
  out[LINK_ID_LEN] = 0;
}

void linkOwnId(char out[LINK_ID_LEN + 1]) {
  uint32_t chip = ESP.getChipId();
  uint8_t id[3] = {(uint8_t)(chip >> 16), (uint8_t)(chip >> 8), (uint8_t)chip};
  idToText(id, out);
}

static bool sameBytes(const uint8_t* a, const uint8_t* b, uint8_t n) {
  uint8_t diff = 0;
  for (uint8_t i = 0; i < n; i++) diff |= a[i] ^ b[i];
  return diff == 0;
}

static void ensurePeer(const uint8_t* mac) {
  // Fails harmlessly if the peer is already known
  esp_now_add_peer((uint8_t*)mac, ESP_NOW_ROLE_COMBO, LINK_CHANNEL, NULL, 0);
}

static void rawSend(const uint8_t* mac, const uint8_t* data, uint8_t len) {
  esp_now_send((uint8_t*)mac, (uint8_t*)data, len);
}

void GameLink::randomBytes(uint8_t* out, uint8_t n) {
  for (uint8_t i = 0; i < n; i += 4) {
    uint32_t r = ESP.random();
    for (uint8_t j = 0; j < 4 && i + j < n; j++) out[i + j] = r >> (8 * j);
  }
}

void GameLink::setState(LinkState s) {
  linkState = s;
  stateSince = millis();
}

// ---------------------------------------------------------------------------
// Start / stop
// ---------------------------------------------------------------------------
bool GameLink::begin(uint8_t gameTag) {
  tag = gameTag;
  rxHead = rxTail = 0;
  bulkInFlight = false;
  bulkFailedFlag = false;

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(LINK_CHANNEL);

  if (esp_now_init() != 0) {
    WiFi.mode(WIFI_OFF);
    return false;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onReceive);
  esp_now_register_send_cb(onSent);
  ensurePeer(BROADCAST);

  WiFi.macAddress(myMac);
  uint32_t chip = ESP.getChipId();
  myIdBytes[0] = chip >> 16;
  myIdBytes[1] = chip >> 8;
  myIdBytes[2] = chip;
  idToText(myIdBytes, myId);

  Sha256 s;
  s.begin();
  s.update((const uint8_t*)"link-lobby", 10);
  s.update((const uint8_t*)LINK_SECRET, strlen(LINK_SECRET));
  s.finish(lobbyKey);

  numHosts = 0;
  havePending = false;
  peerIdText[0] = 0;
  setState(LINK_IDLE);
  return true;
}

void GameLink::end() {
  if (linkState == LINK_OFF) return;
  if (linkState == LINK_CONNECTED) {
    for (int i = 0; i < 3; i++) sendSession(K_BYE, nullptr, 0);
  } else if (linkState == LINK_ASKING) {
    cancel();
  }
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
  memset(sessionKey, 0, sizeof(sessionKey));
  linkState = LINK_OFF;
}

void GameLink::cancel() {
  if (linkState == LINK_ASKING) sendLobby(peerMac, T_CANCEL, nonceMine, 8);
  if (linkState != LINK_OFF) setState(LINK_IDLE);
  numHosts = 0;
  havePending = false;
}

// ---------------------------------------------------------------------------
// Sending
// ---------------------------------------------------------------------------
void GameLink::sendLobby(const uint8_t* mac, uint8_t type, const uint8_t* body, uint8_t n) {
  uint8_t pkt[2 + 24 + TAG_LEN];
  pkt[0] = MAGIC;
  pkt[1] = type;
  memcpy(pkt + 2, body, n);
  uint8_t mac32[32];
  hmacSha256(lobbyKey, 32, pkt, 2 + n, mac32);
  memcpy(pkt + 2 + n, mac32, TAG_LEN);
  if (mac != BROADCAST) ensurePeer(mac);
  rawSend(mac, pkt, 2 + n + TAG_LEN);
}

// Session packet: magic, type, seq (4), ciphertext of [kind, data...], tag.
// Encrypt-then-MAC; the keystream is SHA-256(key, direction, seq).
bool GameLink::sendSession(uint8_t kind, const uint8_t* data, uint8_t len) {
  if (len > LINK_MAX_PAYLOAD) return false;
  uint8_t pkt[6 + 1 + LINK_MAX_PAYLOAD + TAG_LEN];
  uint8_t plain = 1 + len;

  pkt[0] = MAGIC;
  pkt[1] = T_SESSION;
  pkt[2] = txSeq;
  pkt[3] = txSeq >> 8;
  pkt[4] = txSeq >> 16;
  pkt[5] = txSeq >> 24;

  uint8_t ks[32];
  Sha256 s;
  uint8_t dir = amHost ? 0 : 1;
  s.begin();
  s.update(sessionKey, 32);
  s.update(&dir, 1);
  s.update(pkt + 2, 4);
  s.finish(ks);

  pkt[6] = kind ^ ks[0];
  for (uint8_t i = 0; i < len; i++) pkt[7 + i] = data[i] ^ ks[1 + i];

  uint8_t mac32[32];
  hmacSha256(sessionKey, 32, pkt, 6 + plain, mac32);
  memcpy(pkt + 6 + plain, mac32, TAG_LEN);

  txSeq++;
  lastTx = millis();
  rawSend(peerMac, pkt, 6 + plain + TAG_LEN);
  return true;
}

bool GameLink::send(const uint8_t* data, uint8_t len) {
  return linkState == LINK_CONNECTED && sendSession(K_DATA, data, len);
}

bool GameLink::receive(uint8_t* out, uint8_t& len) {
  if (!havePending) return false;
  memcpy(out, pending, pendingLen);
  len = pendingLen;
  havePending = false;
  return true;
}

unsigned long GameLink::msSinceReceive() const { return millis() - lastRx; }

// ---------------------------------------------------------------------------
// Bulk channel: [magic, T_BULK, seq (4), payload..., tag]. The tag uses the
// session key and also covers the sender's direction, so only the paired console
// can make them, and a packet cannot be replayed or bounced back to its sender.
// ---------------------------------------------------------------------------
static void bulkTag(const uint8_t* key, uint8_t dir, const uint8_t* pkt, uint8_t len, uint8_t out[32]) {
  uint8_t msg[1 + 6 + LINK_BULK_MAX];
  msg[0] = dir;
  memcpy(msg + 1, pkt, len);
  hmacSha256(key, 32, msg, 1 + len, out);
}

bool GameLink::sendBulk(const uint8_t* data, uint8_t len) {
  if (linkState != LINK_CONNECTED || len > LINK_BULK_MAX) return false;
  uint8_t pkt[6 + LINK_BULK_MAX + TAG_LEN];
  pkt[0] = MAGIC;
  pkt[1] = T_BULK;
  pkt[2] = txBulkSeq;
  pkt[3] = txBulkSeq >> 8;
  pkt[4] = txBulkSeq >> 16;
  pkt[5] = txBulkSeq >> 24;
  memcpy(pkt + 6, data, len);
  uint8_t mac32[32];
  bulkTag(sessionKey, amHost ? 0 : 1, pkt, 6 + len, mac32);
  memcpy(pkt + 6 + len, mac32, TAG_LEN);

  txBulkSeq++;
  bulkInFlight = true;
  bulkSentAt = millis();
  lastTx = bulkSentAt;
  rawSend(peerMac, pkt, 6 + len + TAG_LEN);
  return true;
}

// A packet counts as gone once the radio reported it, or after a short time if
// the report never came
bool GameLink::bulkReady() const {
  return !bulkInFlight || millis() - bulkSentAt > 40;
}

bool GameLink::bulkFailed() {
  if (!bulkFailedFlag) return false;
  bulkFailedFlag = false;
  return true;
}

void GameLink::handleBulk(const uint8_t* mac, const uint8_t* data, uint8_t len) {
  if (linkState != LINK_CONNECTED && linkState != LINK_ACCEPTING) return;
  if (!sameBytes(mac, peerMac, 6)) return;
  if (len < 6 + 1 + TAG_LEN || len > 6 + LINK_BULK_MAX + TAG_LEN) return;

  uint8_t payload = len - 6 - TAG_LEN;
  uint8_t mac32[32];
  bulkTag(sessionKey, amHost ? 1 : 0, data, 6 + payload, mac32);   // the sender's direction
  if (!sameBytes(mac32, data + 6 + payload, TAG_LEN)) return;

  uint32_t seq = (uint32_t)data[2] | (uint32_t)data[3] << 8 | (uint32_t)data[4] << 16 | (uint32_t)data[5] << 24;
  if (rxBulkSeqValid && (int32_t)(seq - rxBulkSeq) <= 0) return;   // replayed or reordered
  rxBulkSeq = seq;
  rxBulkSeqValid = true;

  lastRx = millis();
  if (linkState == LINK_ACCEPTING) setState(LINK_CONNECTED);

  if (bulkHandler) bulkHandler(data + 6, payload);
}

void GameLink::setBulkHandler(void (*handler)(const uint8_t*, uint8_t)) { bulkHandler = handler; }

void GameLink::sendBeacon() {
  uint8_t body[4] = {tag, myIdBytes[0], myIdBytes[1], myIdBytes[2]};
  sendLobby(BROADCAST, T_BEACON, body, 4);
  lastBeacon = millis();
}

void GameLink::sendJoinRequest(const HostEntry& h) {
  uint8_t body[18];
  body[0] = tag;
  memcpy(body + 1, myIdBytes, 3);
  memcpy(body + 4, h.mac, 6);
  memcpy(body + 10, nonceMine, 8);
  sendLobby(h.mac, T_JOIN_REQ, body, 18);
}

void GameLink::sendJoinOk() {
  uint8_t body[16];
  memcpy(body, nonceMine, 8);      // host nonce
  memcpy(body + 8, noncePeer, 8);  // echo of the joiner's nonce
  sendLobby(peerMac, T_JOIN_OK, body, 16);
}

void GameLink::deriveSessionKey(const uint8_t* hostMac, const uint8_t* joinerMac,
                                const uint8_t* nonceHost, const uint8_t* nonceJoiner) {
  Sha256 s;
  s.begin();
  s.update((const uint8_t*)"link-session", 12);
  s.update((const uint8_t*)LINK_SECRET, strlen(LINK_SECRET));
  s.update(hostMac, 6);
  s.update(joinerMac, 6);
  s.update(nonceHost, 8);
  s.update(nonceJoiner, 8);
  s.finish(sessionKey);
  txSeq = 0;
  rxSeq = 0;
  rxSeqValid = false;
  txBulkSeq = 0;
  rxBulkSeq = 0;
  rxBulkSeqValid = false;
  havePending = false;
}

// ---------------------------------------------------------------------------
// Roles
// ---------------------------------------------------------------------------
void GameLink::host() {
  if (linkState == LINK_OFF) return;
  amHost = true;
  peerIdText[0] = 0;
  setState(LINK_HOSTING);
  lastBeacon = 0;
  sendBeacon();
}

void GameLink::accept() {
  if (linkState != LINK_REQUESTED) return;
  randomBytes(nonceMine, 8);
  deriveSessionKey(myMac, peerMac, nonceMine, noncePeer);
  setState(LINK_ACCEPTING);
  sendJoinOk();
  lastResend = millis();
}

void GameLink::deny() {
  if (linkState != LINK_REQUESTED) return;
  uint8_t body[9];
  memcpy(body, noncePeer, 8);
  body[8] = 0;   // denied
  sendLobby(peerMac, T_JOIN_DENY, body, 9);
  setState(LINK_HOSTING);
  peerIdText[0] = 0;
}

void GameLink::scan() {
  if (linkState == LINK_OFF) return;
  amHost = false;
  numHosts = 0;
  setState(LINK_SCANNING);
}

void GameLink::join(int i) {
  if (linkState != LINK_SCANNING || i < 0 || i >= numHosts) return;
  const HostEntry& h = hosts[i];
  memcpy(peerMac, h.mac, 6);
  memcpy(peerIdBytes, h.id, 3);
  strcpy(peerIdText, h.idText);
  randomBytes(nonceMine, 8);
  setState(LINK_ASKING);
  sendJoinRequest(h);
  lastResend = millis();
}

// ---------------------------------------------------------------------------
// Receiving
// ---------------------------------------------------------------------------
void GameLink::handlePacket(const uint8_t* mac, const uint8_t* data, uint8_t len) {
  if (len < 2 + TAG_LEN || data[0] != MAGIC) return;
  if (data[1] == T_SESSION) handleSession(mac, data, len);
  else if (data[1] == T_BULK) handleBulk(mac, data, len);
  else handleLobby(mac, data[1], data + 2, len - 2 - TAG_LEN);
}

void GameLink::handleLobby(const uint8_t* mac, uint8_t type, const uint8_t* body, uint8_t n) {
  // body points into the packet: [magic, type] sit 2 bytes before it, the tag follows it
  const uint8_t* pkt = body - 2;
  uint8_t mac32[32];
  hmacSha256(lobbyKey, 32, pkt, 2 + n, mac32);
  if (!sameBytes(mac32, body + n, TAG_LEN)) return;

  unsigned long now = millis();

  switch (type) {
    case T_BEACON:
      if (linkState != LINK_SCANNING || n != 4 || body[0] != tag) return;
      {
        int slot = -1;
        for (int i = 0; i < numHosts; i++) if (sameBytes(hosts[i].mac, mac, 6)) slot = i;
        if (slot < 0) {
          if (numHosts >= LINK_MAX_HOSTS) return;
          slot = numHosts++;
          memcpy(hosts[slot].mac, mac, 6);
          memcpy(hosts[slot].id, body + 1, 3);
          idToText(hosts[slot].id, hosts[slot].idText);
        }
        hosts[slot].seen = now;
      }
      break;

    case T_JOIN_REQ:
      if (n != 18 || body[0] != tag || !sameBytes(body + 4, myMac, 6)) return;
      if (linkState == LINK_HOSTING) {
        memcpy(peerMac, mac, 6);
        memcpy(peerIdBytes, body + 1, 3);
        idToText(peerIdBytes, peerIdText);
        memcpy(noncePeer, body + 10, 8);
        setState(LINK_REQUESTED);
      } else if ((linkState == LINK_REQUESTED || linkState == LINK_ACCEPTING || linkState == LINK_CONNECTED) &&
                 sameBytes(mac, peerMac, 6) && sameBytes(body + 10, noncePeer, 8)) {
        // the same request again (our answer may have been lost): nothing to do
      } else {
        uint8_t reply[9];
        memcpy(reply, body + 10, 8);
        reply[8] = 1;   // busy
        sendLobby(mac, T_JOIN_DENY, reply, 9);
      }
      break;

    case T_CANCEL:
      if (linkState == LINK_REQUESTED && n == 8 && sameBytes(mac, peerMac, 6) && sameBytes(body, noncePeer, 8)) {
        peerIdText[0] = 0;
        setState(LINK_HOSTING);
      }
      break;

    case T_JOIN_OK:
      if (linkState != LINK_ASKING || n != 16 || !sameBytes(mac, peerMac, 6) || !sameBytes(body + 8, nonceMine, 8)) return;
      memcpy(noncePeer, body, 8);   // the host's nonce
      deriveSessionKey(peerMac, myMac, noncePeer, nonceMine);
      lastRx = now;
      setState(LINK_CONNECTED);
      sendSession(K_HELLO, nullptr, 0);
      break;

    case T_JOIN_DENY:
      if (linkState != LINK_ASKING || n != 9 || !sameBytes(mac, peerMac, 6) || !sameBytes(body, nonceMine, 8)) return;
      setState(body[8] == 1 ? LINK_BUSY : LINK_DENIED);
      break;
  }
}

void GameLink::handleSession(const uint8_t* mac, const uint8_t* data, uint8_t len) {
  if (linkState != LINK_CONNECTED && linkState != LINK_ACCEPTING) return;
  if (!sameBytes(mac, peerMac, 6)) return;
  if (len < 6 + 1 + TAG_LEN || len > 6 + 1 + LINK_MAX_PAYLOAD + TAG_LEN) return;

  uint8_t plain = len - 6 - TAG_LEN;
  uint8_t mac32[32];
  hmacSha256(sessionKey, 32, data, 6 + plain, mac32);
  if (!sameBytes(mac32, data + 6 + plain, TAG_LEN)) return;

  uint32_t seq = (uint32_t)data[2] | (uint32_t)data[3] << 8 | (uint32_t)data[4] << 16 | (uint32_t)data[5] << 24;
  if (rxSeqValid && (int32_t)(seq - rxSeq) <= 0) return;   // replayed or reordered
  rxSeq = seq;
  rxSeqValid = true;

  uint8_t ks[32];
  uint8_t dir = amHost ? 1 : 0;   // the sender's direction
  Sha256 s;
  s.begin();
  s.update(sessionKey, 32);
  s.update(&dir, 1);
  s.update(data + 2, 4);
  s.finish(ks);

  uint8_t kind = data[6] ^ ks[0];
  lastRx = millis();
  if (linkState == LINK_ACCEPTING) setState(LINK_CONNECTED);

  if (kind == K_BYE) {
    setState(LINK_PEER_LEFT);
  } else if (kind == K_DATA) {
    pendingLen = plain - 1;
    for (uint8_t i = 0; i < pendingLen; i++) pending[i] = data[7 + i] ^ ks[1 + i];
    havePending = true;
  }
}

// ---------------------------------------------------------------------------
// Per-frame work
// ---------------------------------------------------------------------------
void GameLink::poll() {
  while (rxTail != rxHead) {
    RxSlot slot;
    memcpy(&slot, &rxQueue[rxTail], sizeof(slot));
    rxTail = (rxTail + 1) % RX_SLOTS;
    handlePacket(slot.mac, slot.data, slot.len);
  }

  unsigned long now = millis();
  switch (linkState) {
    case LINK_HOSTING:
      if (now - lastBeacon >= BEACON_MS) sendBeacon();
      break;

    case LINK_REQUESTED:
      if (now - lastBeacon >= BEACON_MS) sendBeacon();
      if (now - stateSince > REQUEST_TIMEOUT_MS) deny();
      break;

    case LINK_ACCEPTING:
      if (now - lastResend >= ACCEPT_RESEND_MS) {
        sendJoinOk();
        lastResend = now;
      }
      if (now - stateSince > ACCEPT_TIMEOUT_MS) {
        peerIdText[0] = 0;
        host();   // the joiner never showed up: wait for the next one
      }
      break;

    case LINK_SCANNING:
      for (int i = numHosts - 1; i >= 0; i--) {
        if (now - hosts[i].seen > HOST_EXPIRE_MS) {
          for (int j = i; j < numHosts - 1; j++) hosts[j] = hosts[j + 1];
          numHosts--;
        }
      }
      break;

    case LINK_ASKING:
      if (now - lastResend >= REQUEST_RESEND_MS) {
        HostEntry h;
        memcpy(h.mac, peerMac, 6);
        sendJoinRequest(h);
        lastResend = now;
      }
      if (now - stateSince > ASK_TIMEOUT_MS) setState(LINK_NO_ANSWER);
      break;

    case LINK_CONNECTED:
      if (now - lastTx >= KEEPALIVE_MS) sendSession(K_HELLO, nullptr, 0);
      if (now - lastRx > LOST_MS) setState(LINK_LOST);
      break;

    default:
      break;
  }
}
