#include "fwupdate.h"
#include "display.h"
#include "input.h"
#include "ui.h"
#include "gamelink.h"
#include <Updater.h>
#include <ESP8266WiFi.h>

FirmwareUpdate fwUpdate;

static const unsigned long RTO_MS = 300;             // resend a chunk that was not acknowledged after this long
static const unsigned long PROBE_MS = 400;
static const unsigned long STALL_MS = 10000;         // give up when nothing moves for this long
static const unsigned long OFFER_TIMEOUT_MS = 40000; // the receiver has to press UP in time
static const unsigned long RESTART_DELAY_MS = 2500;

// Milliseconds since t, never negative (t may have been set a moment after the frame began)
static unsigned long since(unsigned long t) {
  unsigned long n = millis();
  return n >= t ? n - t : 0;
}

static uint32_t readLe32(const uint8_t* p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void writeLe32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

void FirmwareUpdate::fingerprint(const uint8_t sha[32], char out[10]) {
  snprintf(out, 10, "%02X%02X %02X%02X", sha[0], sha[1], sha[2], sha[3]);
}

// Bulk packets from the other console, whichever side this console is on
static void onBulk(const uint8_t* m, uint8_t n) { fwUpdate.handleBulk(m, n); }

void FirmwareUpdate::handleBulk(const uint8_t* m, uint8_t n) {
  if (phase == PH_TX_OFFER || phase == PH_TX_SEND || phase == PH_TX_DONE) handleSenderMessage(m, n);
  else if (phase == PH_RX_WAIT || phase == PH_RX_CONSENT || phase == PH_RX_RECV || phase == PH_RX_DONE) handleReceiverMessage(m, n);
}

void FirmwareUpdate::begin() {
  gameLink.setBulkHandler(onBulk);
  leave = false;
  phase = PH_CHOOSE;
  updateOpen = false;

  strncpy(ownVersion, OS_VERSION, sizeof(ownVersion) - 1);
  ownVersion[sizeof(ownVersion) - 1] = 0;
  ownVersionNum = OS_VERSION_NUM;
  ownSize = ESP.getSketchSize();
  ULOG("update: begin, own v%u, sketch %lu bytes, free sketch space %lu, flash %lu\n", ownVersionNum, (unsigned long)ownSize, (unsigned long)ESP.getFreeSketchSpace(), (unsigned long)ESP.getFlashChipSize());

  lobby.begin(LINK_TAG_UPDATE, "FIRMWARE UPDATE");
  lobby.setWording("RECEIVE", "SEND", "RECEIVING", "Waiting for a sender", "FIND CONSOLE", "wants to send firmware");
}

void FirmwareUpdate::stop() {
  abortReceive();
  lobby.end();
}

void FirmwareUpdate::fail(const char* title, const char* line1, const char* line2) {
  abortReceive();
  lobby.end();
  strncpy(failTitle, title, sizeof(failTitle) - 1); failTitle[sizeof(failTitle) - 1] = 0;
  strncpy(failLine1, line1, sizeof(failLine1) - 1); failLine1[sizeof(failLine1) - 1] = 0;
  strncpy(failLine2, line2, sizeof(failLine2) - 1); failLine2[sizeof(failLine2) - 1] = 0;
  phase = PH_FAIL;
}

void FirmwareUpdate::abortReceive() {
  if (updateOpen) {
    Update.end(false);   // not complete: nothing is installed
    updateOpen = false;
  }
}

bool FirmwareUpdate::linkAlive() {
  if (phase == PH_FAIL) return false;   // a handler already failed us: keep its message
  LinkState s = gameLink.state();
  if (s == LINK_CONNECTED) return true;
  if (phase == PH_RX_DONE || phase == PH_TX_DONE) return true;   // finished: the other side may leave first
  fail(s == LINK_PEER_LEFT ? "OTHER CONSOLE LEFT" : "CONNECTION LOST", "Nothing was changed.", "The old firmware stays.");
  return false;
}

void FirmwareUpdate::sendControl(Msg type, uint8_t value) {
  uint8_t m[2] = {(uint8_t)type, value};
  gameLink.sendBulk(m, 2);
}

// ---------------------------------------------------------------------------
bool FirmwareUpdate::update() {
  unsigned long now = millis();

  switch (phase) {
    case PH_CHOOSE: {
      LobbyResult r = lobby.update();
      if (r == LOBBY_BACK) return true;
      if (r == LOBBY_HOST) {              // this console receives
        phase = PH_RX_WAIT;
        rxBase = 0;
      } else if (r == LOBBY_CLIENT) {     // this console sends
        startPrepare();
      }
      break;
    }

    case PH_TX_PREPARE:
      updatePrepare();
      if (buttons.downPressed) { fail("CANCELLED", "Nothing was sent.", ""); }
      break;

    case PH_TX_OFFER:
    case PH_TX_SEND:
    case PH_TX_DONE: {
      gameLink.poll();   // bulk packets reach onBulk() from here
      if (!linkAlive()) break;
      if (phase == PH_TX_OFFER) {
        if (since(offerSentAt) > OFFER_TIMEOUT_MS) fail("NO ANSWER", "The other console did", "not accept in time.");
        else if (since(lastActivity) > 1500) { sendOffer(); lastActivity = now; }   // repeat until answered
        if (buttons.downPressed) fail("CANCELLED", "Nothing was sent.", "");
      } else if (phase == PH_TX_SEND) {
        updateSending(now);
        if (buttons.downPressed) fail("CANCELLED", "The other console keeps", "its old firmware.");
      } else if (buttons.upPressed) {
        leave = true;
      }
      break;
    }

    case PH_RX_WAIT:
    case PH_RX_CONSENT:
    case PH_RX_RECV:
    case PH_RX_DONE: {
      gameLink.poll();   // bulk packets reach onBulk() from here
      if (!linkAlive()) break;

      if (phase == PH_RX_CONSENT) {
        if (buttons.upPressed) acceptOffer();
        else if (buttons.downPressed) denyOffer(R_DENIED, "DENIED", "Nothing was installed.");
      } else if (phase == PH_RX_RECV) {
        // Spend a slice of the frame on the radio: chunks arrive in bursts
        unsigned long sliceStart = millis();
        while (phase == PH_RX_RECV && millis() - sliceStart < 12) {
          gameLink.poll();
          yield();
        }
        if (phase != PH_RX_RECV) break;
        if (rxSinceAck) sendAck();   // whatever arrived in this slice
        if (since(lastActivity) > STALL_MS) {
          char d[24];
          snprintf(d, sizeof(d), "got %u bad %u at %u", dbgChunksRx, dbgChunksBad, rxBase);
          ULOG("update: receiver stalled (%s)\n", d);
          fail("TRANSFER STALLED", "Nothing was changed.", d);
        } else if (since(lastAck) > 150 && rxSinceAck) {
          sendAck();
        }
      } else if (phase == PH_RX_DONE) {
        // the sender may have missed the result: repeat it until we restart
        if (since(lastResultAt) > 300) { sendControl(M_RESULT, R_SUCCESS); lastResultAt = millis(); }
        if (millis() >= restartAt) ESP.restart();
      } else if (buttons.downPressed) {
        leave = true;
      }
      break;
    }

    case PH_FAIL:
      if (buttons.upPressed) leave = true;
      break;
  }

  if (leave) {
    stop();
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Sender
// ---------------------------------------------------------------------------
void FirmwareUpdate::startPrepare() {
  imageSize = ownSize;
  chunks = (imageSize + FW_CHUNK - 1) / FW_CHUNK;
  hasher.begin();
  hashPos = 0;
  phase = PH_TX_PREPARE;
}

// Hashes the running firmware a little each frame, so the screen keeps going
void FirmwareUpdate::updatePrepare() {
  gameLink.poll();   // keeps the link alive while the hash is worked out
  uint8_t buf[256];
  for (int i = 0; i < 16 && hashPos < imageSize; i++) {
    uint32_t n = imageSize - hashPos < sizeof(buf) ? imageSize - hashPos : sizeof(buf);
    ESP.flashRead(hashPos, buf, n);
    hasher.update(buf, n);
    hashPos += n;
  }
  if (hashPos >= imageSize) {
    hasher.finish(imageSha);
    ULOG("update: image hashed (%lu bytes, %u chunks), offering it\n", (unsigned long)imageSize, chunks);
    base = 0;
    for (int i = 0; i < FW_WINDOW; i++) slots[i].index = 0xFFFF;
    allAcked = false;
    phase = PH_TX_OFFER;
    offerSentAt = millis();
    lastActivity = 0;
    sendOffer();
  }
}

void FirmwareUpdate::sendOffer() {
  uint8_t m[1 + 2 + 8 + 4 + 32];
  m[0] = M_OFFER;
  m[1] = ownVersionNum;
  m[2] = ownVersionNum >> 8;
  memset(m + 3, 0, 8);
  memcpy(m + 3, ownVersion, strlen(ownVersion) < 8 ? strlen(ownVersion) : 8);
  writeLe32(m + 11, imageSize);
  memcpy(m + 15, imageSha, 32);
  while (!gameLink.bulkReady()) { gameLink.poll(); yield(); }
  bool sent = gameLink.sendBulk(m, sizeof(m));
  ULOG("update: offer sent (%d)\n", sent);
}

FirmwareUpdate::Slot& FirmwareUpdate::slotFor(uint16_t index) {
  Slot& s = slots[index % FW_WINDOW];
  if (s.index != index) {
    s.index = index;
    s.acked = false;
    s.sends = 0;
    s.sentAt = 0;
  }
  return s;
}

void FirmwareUpdate::sendChunk(uint16_t index) {
  uint8_t m[3 + FW_CHUNK];
  m[0] = M_CHUNK;
  m[1] = index;
  m[2] = index >> 8;
  uint32_t offset = (uint32_t)index * FW_CHUNK;
  uint32_t n = imageSize - offset < FW_CHUNK ? imageSize - offset : FW_CHUNK;
  ESP.flashRead(offset, m + 3, n);
  bool sent = gameLink.sendBulk(m, 3 + n);
  lastSentIndex = index;
  dbgSent++;
  if (!sent || dbgSent <= 5 || dbgSent % 100 == 0) ULOG("update: chunk %u sent=%d (packets %u, acks %u, lost %u, base %u)\n", index, sent, dbgSent, dbgAcks, dbgLost, base);
}

void FirmwareUpdate::updateSending(unsigned long now) {
  // A packet the other console did not acknowledge is sent again at once
  if (gameLink.bulkFailed() && lastSentIndex >= 0) { slotFor(lastSentIndex).sentAt = 0; dbgLost++; }

  if (!allAcked) {
    // Keep the radio busy for a few milliseconds per frame, within the window
    unsigned long start = millis();
    bool sentAny = false;
    while (millis() - start < 20) {
      gameLink.poll();
      if (phase != PH_TX_SEND) return;
      if (!gameLink.bulkReady()) { yield(); continue; }

      bool sent = false;
      for (uint16_t idx = base; idx < base + FW_WINDOW && idx < chunks; idx++) {
        Slot& s = slotFor(idx);
        if (s.acked) continue;
        if (s.sends == 0 || s.sentAt == 0 || millis() - s.sentAt >= RTO_MS) {
          sendChunk(idx);
          s.sends++;
          s.sentAt = millis();
          sent = sentAny = true;
          break;
        }
      }
      if (!sent) break;
    }

    // Nothing to send but not everything acknowledged: ask where the receiver is
    if (!sentAny && since(lastActivity) > PROBE_MS) {
      uint8_t probe = M_PROBE;
      if (gameLink.bulkReady()) gameLink.sendBulk(&probe, 1);
      lastActivity = now;
    }
  }

  if (since(lastProgressAt) > STALL_MS) {
    if (allAcked) fail("NO CONFIRMATION", "Everything was sent but", "no result came back.");
    else {
      char d[24];
      snprintf(d, sizeof(d), "tx%u ack%u lost%u", dbgSent, dbgAcks, dbgLost);
      ULOG("update: stalled at chunk %u of %u (%s)\n", base, chunks, d);
      fail("TRANSFER STALLED", "The other console stopped", d);
    }
  }
}

void FirmwareUpdate::handleSenderMessage(const uint8_t* m, uint8_t n) {
  if (n < 1) return;
  unsigned long now = millis();

  switch (m[0]) {
    case M_ACCEPT:
      if (phase == PH_TX_OFFER) {
        dbgSent = dbgAcks = dbgLost = 0;
        ULOG("update: the other console accepted\n");
        phase = PH_TX_SEND;
        lastProgressAt = lastActivity = now;
        base = 0;
        for (int i = 0; i < FW_WINDOW; i++) slots[i].index = 0xFFFF;
      }
      break;

    case M_DENY:
      if (phase == PH_TX_OFFER || phase == PH_TX_SEND) {
        uint8_t reason = n > 1 ? m[1] : R_DENIED;
        if (reason == R_OLDER) fail("REFUSED", "That console already has", "a newer version.");
        else if (reason == R_NO_SPACE) fail("REFUSED", "The image does not fit", "on that console.");
        else if (reason == R_BUSY) fail("REFUSED", "That console is busy.", "");
        else fail("DENIED", "The other console said", "no. Nothing was sent.");
      }
      break;

    case M_ACK:
      if (phase == PH_TX_SEND && n >= 7) {
        dbgAcks++;
        if (dbgAcks <= 5 || dbgAcks % 50 == 0) ULOG("update: ack %u, base %u\n", dbgAcks, m[1] | (uint16_t)m[2] << 8);
        uint16_t ackBase = m[1] | (uint16_t)m[2] << 8;
        uint32_t bitmap = readLe32(m + 3);
        if (ackBase > base) { base = ackBase; lastProgressAt = now; }
        for (int k = 0; k < 15; k++) {
          uint32_t idx = (uint32_t)ackBase + 1 + k;
          if ((bitmap >> k & 1) && idx < chunks && idx < (uint32_t)base + FW_WINDOW) slotFor(idx).acked = true;
        }
        if (base >= chunks && !allAcked) { allAcked = true; lastProgressAt = now; }   // now waiting for the result
        lastActivity = now;
      }
      break;

    case M_RESULT:
      if (n >= 2 && (phase == PH_TX_SEND || phase == PH_TX_DONE)) {
        uint8_t code = m[1];
        if (code == R_SUCCESS) {
          phase = PH_TX_DONE;
        } else if (code == R_HASH) {
          fail("UPDATE FAILED", "The image did not match", "its checksum. Not installed.");
        } else if (code == R_HEADER) {
          fail("UPDATE FAILED", "That console refused", "the image header.");
        } else {
          fail("UPDATE FAILED", "The other console could", "not write the image.");
        }
      }
      break;
  }
}

// ---------------------------------------------------------------------------
// Receiver
// ---------------------------------------------------------------------------
void FirmwareUpdate::handleReceiverMessage(const uint8_t* m, uint8_t n) {
  if (n < 1) return;
  switch (m[0]) {
    case M_OFFER:
      if (phase == PH_RX_WAIT) receiveOffer(m, n);
      else if (phase == PH_RX_RECV && rxBase == 0) sendControl(M_ACCEPT, 0);   // our ACCEPT was lost: say it again
      break;
    case M_CHUNK:  if (phase == PH_RX_RECV) receiveChunk(m, n); break;
    case M_PROBE:  if (phase == PH_RX_RECV) sendAck(); break;
    default: break;
  }
}

void FirmwareUpdate::receiveOffer(const uint8_t* m, uint8_t n) {
  if (n < 47) return;
  offerVersionNum = m[1] | (uint16_t)m[2] << 8;
  memcpy(offerVersion, m + 3, 8);
  offerVersion[8] = 0;
  imageSize = readLe32(m + 11);
  memcpy(imageSha, m + 15, 32);
  chunks = (imageSize + FW_CHUNK - 1) / FW_CHUNK;
  ULOG("update: offer received, v%u, %lu bytes, %u chunks\n", offerVersionNum, (unsigned long)imageSize, chunks);

  if (offerVersionNum < ownVersionNum) {
    denyOffer(R_OLDER, "REFUSED", "That is an older version.");
    return;
  }
  if (imageSize < 1024 || imageSize > ESP.getFreeSketchSpace()) {
    denyOffer(R_NO_SPACE, "REFUSED", "The image does not fit.");
    return;
  }
  offerMessage = offerVersionNum == ownVersionNum ? 1 : 0;
  phase = PH_RX_CONSENT;
}

void FirmwareUpdate::denyOffer(uint8_t reason, const char* line1, const char* line2) {
  sendControl(M_DENY, reason);
  // keep the radio up for a moment so the message gets out, then show why
  unsigned long until = millis() + 120;
  while (millis() < until) { gameLink.poll(); yield(); }
  fail(line1, line2, "Nothing was installed.");
}

void FirmwareUpdate::acceptOffer() {
  // This console's own flash settings (mode, size, speed) go into the new image
  ESP.flashRead(0, ownHeader, 4);

  dbgChunksRx = dbgChunksBad = 0;
  bool began = Update.begin(imageSize);
  ULOG("update: Update.begin(%lu) = %d, error %u, free heap %lu\n", (unsigned long)imageSize, began, (unsigned)Update.getError(), (unsigned long)ESP.getFreeHeap());
  if (!began) {
    char d[24];
    snprintf(d, sizeof(d), "updater error %u", (unsigned)Update.getError());
    denyOffer(R_NO_SPACE, "CANNOT START", d);
    return;
  }
  updateOpen = true;
  rxBase = 0;
  rxSinceAck = 0;
  memset(rxHave, 0, sizeof(rxHave));
  rxHasher.begin();
  lastActivity = lastAck = millis();
  phase = PH_RX_RECV;
  sendControl(M_ACCEPT, 0);
}

void FirmwareUpdate::receiveChunk(const uint8_t* m, uint8_t n) {
  if (n < 4) return;
  uint16_t index = m[1] | (uint16_t)m[2] << 8;
  uint8_t len = n - 3;
  dbgChunksRx++;
  if (dbgChunksRx <= 5 || dbgChunksRx % 100 == 0) ULOG("update: chunk packet %u (index %u, %u bytes, written %u, bad %u)\n", dbgChunksRx, index, len, rxBase, dbgChunksBad);

  uint32_t expected = (uint32_t)index * FW_CHUNK < imageSize ? imageSize - (uint32_t)index * FW_CHUNK : 0;
  if (expected > FW_CHUNK) expected = FW_CHUNK;
  if (index >= chunks || len != expected) { dbgChunksBad++; if (dbgChunksBad <= 5) ULOG("update: chunk %u refused: %u bytes, expected %lu\n", index, len, (unsigned long)expected); return; }   // not what the offer described
  lastActivity = millis();

  if (index < rxBase) { sendAck(); return; }          // a repeat of something already written
  if (index >= rxBase + FW_WINDOW) return;            // too far ahead: the sender will repeat it

  uint8_t slot = index % FW_WINDOW;
  if (!rxHave[slot]) {
    memcpy(rxBuf[slot], m + 3, len);
    rxLen[slot] = len;
    rxHave[slot] = true;
  }
  rxSinceAck++;
  drainChunks();
  if (rxSinceAck >= 2 || rxBase >= chunks) sendAck();   // answer quickly: the sender is waiting for window space
}

// Writes every chunk that is now in order
void FirmwareUpdate::drainChunks() {
  while (rxBase < chunks && rxHave[rxBase % FW_WINDOW]) {
    uint8_t slot = rxBase % FW_WINDOW;
    uint8_t* data = rxBuf[slot];
    uint8_t len = rxLen[slot];

    rxHasher.update(data, len);   // the hash is of the image as sent

    if (rxBase == 0) {
      if (data[0] != 0xE9) { sendControl(M_RESULT, R_HEADER); fail("UPDATE FAILED", "That is not a firmware", "image. Nothing installed."); return; }
      data[2] = ownHeader[2];     // keep this console's own flash mode, size and speed
      data[3] = ownHeader[3];
    }
    if (rxBase == chunks - 1) {
      // Hold the last chunk back until the hash has been checked: the Updater
      // installs as soon as the last byte is written, so it must stay unfinished.
      memcpy(lastChunk, data, len);
      lastChunkLen = len;
    } else if (Update.write(data, len) != len) {
      sendControl(M_RESULT, R_WRITE);
      char d[24];
      snprintf(d, sizeof(d), "chunk %u, error %u", rxBase, (unsigned)Update.getError());
      ULOG("update: Update.write failed at chunk %u, error %u\n", rxBase, (unsigned)Update.getError());
      fail("UPDATE FAILED", "Writing the image failed.", d);
      return;
    }
    rxHave[slot] = false;
    rxBase++;
  }
  if (rxBase >= chunks && phase == PH_RX_RECV) finishReceive();
}

void FirmwareUpdate::sendAck() {
  uint8_t m[7];
  m[0] = M_ACK;
  m[1] = rxBase;
  m[2] = rxBase >> 8;
  uint32_t bitmap = 0;
  for (int k = 0; k < 15; k++) {
    uint32_t idx = (uint32_t)rxBase + 1 + k;
    if (idx < chunks && idx < (uint32_t)rxBase + FW_WINDOW && rxHave[idx % FW_WINDOW]) bitmap |= 1UL << k;
  }
  writeLe32(m + 3, bitmap);
  while (!gameLink.bulkReady()) { gameLink.poll(); yield(); }
  gameLink.sendBulk(m, sizeof(m));
  rxSinceAck = 0;
  lastAck = millis();
}

// Everything arrived: check the whole image, and only then install
void FirmwareUpdate::finishReceive() {
  ULOG("update: all %u chunks received, checking the hash\n", chunks);
  uint8_t sha[32];
  rxHasher.finish(sha);
  if (memcmp(sha, imageSha, 32) != 0) {
    sendControl(M_RESULT, R_HASH);
    fail("UPDATE FAILED", "The image did not match", "its checksum. Not installed.");
    return;
  }
  if (Update.write(lastChunk, lastChunkLen) != lastChunkLen || !Update.end(true)) {
    updateOpen = false;
    sendControl(M_RESULT, R_WRITE);
    fail("UPDATE FAILED", "Installing the image failed.", "The old firmware stays.");
    return;
  }
  updateOpen = false;
  sendAck();
  sendControl(M_RESULT, R_SUCCESS);
  unsigned long until = millis() + 200;
  while (millis() < until) { gameLink.poll(); yield(); }
  phase = PH_RX_DONE;
  lastResultAt = millis();
  restartAt = millis() + RESTART_DELAY_MS;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void FirmwareUpdate::drawProgress(const char* title, const char* line, const char* hint, int percent) {
  clearDisplay();
  drawCenteredText(title, 0, 1);
  char big[8];
  snprintf(big, sizeof(big), "%d%%", constrain(percent, 0, 100));
  drawCenteredText(big, 14, 3);
  display.drawRect(8, 42, 112, 7, SSD1306_WHITE);
  display.fillRect(10, 44, (int)(108L * constrain(percent, 0, 100) / 100), 3, SSD1306_WHITE);
  if (line && line[0]) drawCenteredText(line, 51, 1);
  drawCenteredText(hint, 57, 1);
}

bool FirmwareUpdate::draw() {
  char a[24], b[24], fp[10];

  // During the transfer the screen only needs a refresh a few times a second;
  // skipping the rest leaves the time to the radio (a display refresh takes ~30 ms)
  if (phase == PH_TX_SEND || phase == PH_RX_RECV) {
    static unsigned long lastDraw = 0;
    if (millis() - lastDraw < 250) return false;
    lastDraw = millis();
  }

  switch (phase) {
    case PH_CHOOSE:
      lobby.draw();
      break;

    case PH_TX_PREPARE:
      drawProgress("PREPARING", "Reading my firmware...", "DOWN: CANCEL", ownSize ? (int)(hashPos * 100UL / ownSize) : 0);
      break;

    case PH_TX_OFFER:
      fingerprint(imageSha, fp);
      snprintf(a, sizeof(a), "%s  %s", ownVersion, fp);
      drawMessageScreen("SENDING FIRMWARE", gameLink.peerId(), a, "Waiting for them to accept", "DOWN: CANCEL");
      break;

    case PH_TX_SEND: {
      int pct = chunks ? (int)((uint32_t)base * 100UL / chunks) : 0;
      fingerprint(imageSha, fp);
      snprintf(a, sizeof(a), "to %s  %s", gameLink.peerId(), fp);
      drawProgress("SENDING FIRMWARE", a, "DOWN: CANCEL", pct);
      break;
    }

    case PH_TX_DONE:
      snprintf(a, sizeof(a), "%s was updated", gameLink.peerId());
      drawMessageScreen("DONE", "OK", a, "It is restarting now.", "UP: OK");
      break;

    case PH_RX_WAIT:
      drawMessageScreen("RECEIVING", gameLink.peerId(), "Connected. Waiting for", "the firmware offer...", "");
      break;

    case PH_RX_CONSENT: {
      clearDisplay();
      drawCenteredText("FIRMWARE UPDATE", 0, 1);
      fingerprint(imageSha, fp);
      snprintf(a, sizeof(a), "From %s %s", gameLink.peerId(), fp);
      display.setCursor(0, 11);
      display.print(a);
      snprintf(b, sizeof(b), "%s -> %s (%s)", ownVersion, offerVersion, offerMessage ? "same" : "newer");
      display.setCursor(0, 21);
      display.print(b);
      if (offerMessage) {
        display.setCursor(0, 31);
        display.print(F("Same version number as"));
        display.setCursor(0, 41);
        display.print(F("this build. Install?"));
      } else {
        snprintf(a, sizeof(a), "%lu KB", (unsigned long)((imageSize + 1023) / 1024));
        display.setCursor(0, 31);
        display.print(a);
        display.setCursor(0, 41);
        display.print(F("Installs, then restarts"));
      }
      drawCenteredText("UP:INSTALL DOWN:DENY", 56, 1);
      break;
    }

    case PH_RX_RECV: {
      int pct = chunks ? (int)((uint32_t)rxBase * 100UL / chunks) : 0;
      drawProgress("RECEIVING", "Do not switch me off", "", pct);
      break;
    }

    case PH_RX_DONE:
      drawMessageScreen("UPDATE OK", "OK", "Restarting into the", "new firmware...", "");
      break;

    case PH_FAIL:
      drawMessageScreen(failTitle, "", failLine1, failLine2, "UP: OK");
      break;
  }
  return true;
}
