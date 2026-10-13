#ifndef FWUPDATE_H
#define FWUPDATE_H

#include "config.h"
#include "lobby.h"
#include "crypto.h"

// Firmware update between two consoles over ESP-NOW (test mode > UPDATE).
//
// One console RECEIVES (it waits for a sender), the other SENDS: it reads its
// own running firmware out of its flash chip and passes it on. The receiving
// console shows who is sending, the version and a fingerprint of the image (the
// first 8 hex digits of its SHA-256), and only installs after UP is pressed.
// Rules: an older version than the one on the receiver is refused; the same
// version number is allowed, with a message; the whole image is checked against
// its SHA-256 before it is installed; if anything fails the old firmware stays.
//
// This is NOT signed: it protects against corruption, not against a malicious
// sender. See the security notes in the README.

#define LINK_TAG_UPDATE 251
#define FW_CHUNK 192        // bytes of firmware per packet
#define FW_WINDOW 8         // chunks the sender may have in flight (fits the radio receive queue)

class FirmwareUpdate {
public:
  void begin();
  bool update();    // true when the player leaves the update page
  void handleBulk(const uint8_t* m, uint8_t n);   // from the link, for every bulk packet
  bool draw();      // draws; returns false when this frame did not change, so the manager can skip the slow display refresh
  void stop();      // radio off; throws away a half-received image

  // 10 characters: the first 8 hex digits of a SHA-256 in two groups, "A3F9 07C2"
  static void fingerprint(const uint8_t sha[32], char out[10]);

private:
  enum Phase : uint8_t {
    PH_CHOOSE,                                            // Lobby: RECEIVE / SEND
    PH_TX_PREPARE, PH_TX_OFFER, PH_TX_SEND, PH_TX_DONE,   // sender
    PH_RX_WAIT, PH_RX_CONSENT, PH_RX_RECV, PH_RX_DONE,    // receiver
    PH_FAIL
  };
  enum Msg : uint8_t {
    M_OFFER = 0x10, M_ACCEPT = 0x11, M_DENY = 0x12, M_CHUNK = 0x13,
    M_ACK = 0x14, M_RESULT = 0x15, M_PROBE = 0x17
  };
  enum Reason : uint8_t { R_SUCCESS = 0, R_DENIED = 1, R_OLDER = 2, R_NO_SPACE = 3, R_HASH = 4, R_WRITE = 5, R_HEADER = 6, R_BUSY = 7 };

  Lobby lobby;
  Phase phase = PH_CHOOSE;
  bool leave = false;

  // what is being offered
  uint32_t imageSize = 0;
  uint16_t chunks = 0;
  uint8_t imageSha[32];
  uint16_t offerVersionNum = 0;
  char offerVersion[9];

  // this console's own firmware
  uint16_t ownVersionNum;
  char ownVersion[9];
  uint32_t ownSize;

  // failure screen
  char failTitle[24], failLine1[24], failLine2[24];

  // sender
  Sha256 hasher;
  uint32_t hashPos = 0;
  uint16_t base = 0;                  // every chunk below this one was acknowledged
  struct Slot { uint16_t index; bool acked; uint8_t sends; unsigned long sentAt; };
  Slot slots[FW_WINDOW];
  int32_t lastSentIndex = -1;
  unsigned long lastActivity = 0, offerSentAt = 0, lastProgressAt = 0;
  bool allAcked = false;

  // receiver
  uint16_t rxBase = 0;                // the next chunk to be written
  uint8_t rxBuf[FW_WINDOW][FW_CHUNK];
  uint8_t rxLen[FW_WINDOW];
  bool rxHave[FW_WINDOW];
  Sha256 rxHasher;
  uint8_t ownHeader[4];
  bool updateOpen = false;
  uint8_t rxSinceAck = 0;
  unsigned long lastAck = 0;
  unsigned long restartAt = 0;
  unsigned long lastResultAt = 0;
  uint8_t lastChunk[192];
  uint8_t lastChunkLen = 0;
  uint8_t sentResult = 255;
  uint8_t offerMessage = 0;           // 0 newer, 1 same version number

  // sender side
  void startPrepare();
  void updatePrepare();
  void sendOffer();
  void updateSending(unsigned long now);
  void handleSenderMessage(const uint8_t* m, uint8_t n);
  Slot& slotFor(uint16_t index);
  void sendChunk(uint16_t index);

  // receiver side
  void handleReceiverMessage(const uint8_t* m, uint8_t n);
  void receiveOffer(const uint8_t* m, uint8_t n);
  void acceptOffer();
  void denyOffer(uint8_t reason, const char* line1, const char* line2);
  void receiveChunk(const uint8_t* m, uint8_t n);
  void drainChunks();
  void sendAck();
  void finishReceive();
  void abortReceive();
  bool linkAlive();

  void fail(const char* title, const char* line1, const char* line2);
  void sendControl(Msg type, uint8_t value);
  void drawProgress(const char* title, const char* line, const char* hint, int percent);
};

extern FirmwareUpdate fwUpdate;

#endif
