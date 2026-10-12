#ifndef GAMELINK_H
#define GAMELINK_H

#include "config.h"

// Console-to-console link over ESP-NOW (no router needed).
//
// One console hosts, another joins. Pairing is: the host broadcasts a beacon,
// the joiner picks a host from the ones it hears and asks to join, and the host
// accepts or denies. After an accept both sides derive a fresh session key from
// LINK_SECRET, both MAC addresses and a random number from each side. Every
// packet after that is encrypted and authenticated (SHA-256 based, with a
// sequence number against replays); the pairing packets are authenticated with
// a key from LINK_SECRET.
//
// Every console has a short id (5 characters) taken from its chip id, which is
// fixed at the factory and unique per chip; it needs no storage. Connections use
// the full 48-bit MAC, so two consoles can never be mistaken for each other.
//
// Call poll() once per frame; nothing here blocks.

#define LINK_ID_LEN 5            // characters, without the terminating 0
#define LINK_MAX_PAYLOAD 24      // bytes per data packet
#define LINK_BULK_MAX 200        // bytes per bulk packet (authenticated, not encrypted)
#define LINK_MAX_HOSTS 4

enum LinkState : uint8_t {
  LINK_OFF,
  LINK_IDLE,        // radio on, nothing started
  LINK_HOSTING,     // beaconing, waiting for someone to ask
  LINK_REQUESTED,   // host: someone asked, waiting for accept() / deny()
  LINK_ACCEPTING,   // host: accepted, waiting for the joiner's first packet
  LINK_SCANNING,    // joiner: listening for hosts
  LINK_ASKING,      // joiner: asked a host, waiting for the answer
  LINK_CONNECTED,
  LINK_DENIED,      // joiner: the host said no
  LINK_BUSY,        // joiner: the host is talking to someone else
  LINK_NO_ANSWER,   // joiner: the host never answered
  LINK_PEER_LEFT,   // the other console left on purpose
  LINK_LOST         // the other console stopped answering
};

class GameLink {
public:
  // Turns the radio on. gameTag keeps hosts of different games apart.
  bool begin(uint8_t gameTag);
  // Says goodbye if connected, then turns the radio off.
  void end();
  void poll();

  LinkState state() const { return linkState; }
  const char* localId() const { return myId; }
  const char* peerId() const { return peerIdText; }

  // Host side
  void host();
  void accept();
  void deny();

  // Joiner side
  void scan();
  int hostCount() const { return numHosts; }
  const char* hostId(int i) const { return hosts[i].idText; }
  void join(int i);

  // Back to LINK_IDLE: stops beaconing / withdraws a join request.
  void cancel();

  // Data between connected consoles. receive() hands over the newest packet
  // that arrived since the last call and discards older ones.
  bool send(const uint8_t* data, uint8_t len);
  bool receive(uint8_t* out, uint8_t& len);
  unsigned long msSinceReceive() const;

  // Bulk packets carry up to LINK_BULK_MAX bytes and are meant for data that is
  // not secret, such as firmware: they are authenticated with the session key
  // but not encrypted, and they may arrive duplicated or out of order (the
  // sender keeps track). sendBulk() paces itself: wait for bulkReady() before
  // the next one, and check bulkFailed() to learn the last one was not acknowledged.
  bool sendBulk(const uint8_t* data, uint8_t len);
  bool bulkReady() const;
  bool bulkFailed();           // true once after a bulk packet the other console did not acknowledge
  // Called from poll() for every valid bulk packet, in arrival order
  void setBulkHandler(void (*handler)(const uint8_t* data, uint8_t len));

private:
  struct HostEntry {
    uint8_t mac[6];
    uint8_t id[3];
    char idText[LINK_ID_LEN + 1];
    unsigned long seen;
  };

  LinkState linkState = LINK_OFF;
  uint8_t tag = 0;
  uint8_t myMac[6];
  uint8_t myIdBytes[3];
  char myId[LINK_ID_LEN + 1];

  uint8_t peerMac[6];
  uint8_t peerIdBytes[3];
  char peerIdText[LINK_ID_LEN + 1];
  uint8_t nonceMine[8];
  uint8_t noncePeer[8];

  uint8_t lobbyKey[32];
  uint8_t sessionKey[32];
  uint32_t txSeq = 0;
  uint32_t rxSeq = 0;
  bool rxSeqValid = false;
  bool amHost = false;

  HostEntry hosts[LINK_MAX_HOSTS];
  int numHosts = 0;

  unsigned long lastBeacon = 0, lastTx = 0, lastRx = 0, stateSince = 0, lastResend = 0;
  uint8_t pending[LINK_MAX_PAYLOAD];
  uint8_t pendingLen = 0;
  bool havePending = false;

  void setState(LinkState s);
  void handlePacket(const uint8_t* mac, const uint8_t* data, uint8_t len);
  void handleBulk(const uint8_t* mac, const uint8_t* data, uint8_t len);
  void handleLobby(const uint8_t* mac, uint8_t type, const uint8_t* body, uint8_t n);
  void handleSession(const uint8_t* mac, const uint8_t* data, uint8_t len);
  void sendLobby(const uint8_t* mac, uint8_t type, const uint8_t* body, uint8_t n);
  bool sendSession(uint8_t kind, const uint8_t* data, uint8_t len);
  void deriveSessionKey(const uint8_t* hostMac, const uint8_t* joinerMac,
                        const uint8_t* nonceHost, const uint8_t* nonceJoiner);
  void sendJoinRequest(const HostEntry& h);
  void sendJoinOk();
  void sendBeacon();
  void randomBytes(uint8_t* out, uint8_t n);
};

// The id this console shows, available before the radio is on.
void linkOwnId(char out[LINK_ID_LEN + 1]);

extern GameLink gameLink;

#endif
