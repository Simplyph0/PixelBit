// Firmware update between two consoles over the simulated radio.
// update_test <rx|tx> <index> <loss%> <seconds>   plus environment variables:
//   FW_IMAGE / FW_OUT / FREE_SPACE (see hostenv.cpp)
//   UT_DENY=1     the receiver presses DOWN on the consent screen
//   UT_CORRUPT=1  the sender announces a wrong SHA-256
//   UT_KILL=<pct> the sender stops dead at that percent of the transfer
#define private public
#include "fwupdate.h"
#include "input.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
extern unsigned long g_sentPackets, g_recvPackets; extern int g_index, g_port, g_loss; extern uint32_t g_chip; void air_pump(); extern bool g_restarted;
int main(int argc, char** argv) {
  bool rx = !strcmp(argv[1], "rx"); g_index = atoi(argv[2]); g_loss = atoi(argv[3]); int secs = atoi(argv[4]);
  g_chip = rx ? 0x1A2B3C : 0x00FF01; srand(getpid() * 5 + g_index);
  fwUpdate.begin(); buttons = {};
  unsigned long t0 = millis(); int lastPhase = -1; int frames = 0; bool decided = false;
  while (millis() - t0 < (unsigned long)secs * 1000) {
    air_pump(); buttons.upPressed = buttons.downPressed = buttons.leftPressed = buttons.rightPressed = false;
    auto& f = fwUpdate; LinkState ls = gameLink.state();
    if (f.phase == FirmwareUpdate::PH_CHOOSE) {
      Lobby& lb = f.lobby;
      if (lb.phase == 0) { if (!rx && lb.roleChoice == 0) buttons.rightPressed = true; else buttons.upPressed = true; }
      else if (rx && ls == LINK_REQUESTED) buttons.upPressed = true;
      else if (!rx && ls == LINK_SCANNING && gameLink.hostCount() > 0) buttons.upPressed = true;
    } else if (rx && f.phase == FirmwareUpdate::PH_RX_CONSENT && !decided) {
      if (getenv("UT_CORRUPT")) f.imageSha[0] ^= 0xFF;   // as if the announced checksum were wrong
      if (getenv("UT_DENY")) buttons.downPressed = true; else buttons.upPressed = true;
      decided = true;
    } else if (!rx && f.phase == FirmwareUpdate::PH_TX_OFFER && getenv("UT_CORRUPT") && f.imageSha[0] == f.imageSha[0]) {
      static bool done = false; if (!done) { f.imageSha[0] ^= 0xFF; done = true; f.sendOffer(); }
    }
    if (!rx && f.phase == FirmwareUpdate::PH_TX_SEND && getenv("UT_KILL")) {
      int pct = f.chunks ? (int)((uint32_t)f.base * 100 / f.chunks) : 0;
      if (pct >= atoi(getenv("UT_KILL"))) { printf("[tx] killed at %d%%\n", pct); fflush(stdout); _exit(0); }
    }
    static unsigned long ps = 0, pr = 0; unsigned long fs = millis();
    bool leave = f.update(); f.draw();
    if (getenv("UT_FRAMES") && (f.phase == FirmwareUpdate::PH_TX_SEND || f.phase == FirmwareUpdate::PH_RX_RECV) && frames < 400 && frames > 110) printf("[%s] frame %d start=%lu took=%lu sent+%lu recv+%lu base=%u rxBase=%u\n", argv[1], frames, fs - t0, millis() - fs, g_sentPackets - ps, g_recvPackets - pr, f.base, f.rxBase);
    ps = g_sentPackets; pr = g_recvPackets;
    if (getenv("UT_DEBUG") && frames % 25 == 0) printf("[%s] sent=%lu recv=%lu t=%lu base=%u rxBase=%u chunks=%u lastSent=%d acked0=%d sends0=%d\n", argv[1], g_sentPackets, g_recvPackets, millis() - t0, f.base, f.rxBase, f.chunks, (int)f.lastSentIndex, f.slots[f.base % FW_WINDOW].acked, f.slots[f.base % FW_WINDOW].sends);
    if ((int)f.phase != lastPhase) {
      lastPhase = f.phase;
      static const char* N[] = {"CHOOSE", "TX_PREPARE", "TX_OFFER", "TX_SEND", "TX_DONE", "RX_WAIT", "RX_CONSENT", "RX_RECV", "RX_DONE", "FAIL"};
      printf("[%s] phase %s t=%lu%s%s\n", argv[1], N[f.phase], millis() - t0, f.phase == FirmwareUpdate::PH_FAIL ? " : " : "", f.phase == FirmwareUpdate::PH_FAIL ? f.failTitle : "");
      if (f.phase == FirmwareUpdate::PH_RX_CONSENT) printf("[rx] offered %s (num %u) vs own %s (num %u): %s\n", f.offerVersion, f.offerVersionNum, f.ownVersion, f.ownVersionNum, f.offerMessage ? "same version" : "newer");
      fflush(stdout);
    }
    { static unsigned long endAt = 0; bool fin = f.phase == FirmwareUpdate::PH_FAIL || f.phase == FirmwareUpdate::PH_TX_DONE;
      if (fin && !endAt) endAt = millis() + 1500; if (endAt && millis() > endAt) break; }
    if (leave) break;
    if (g_restarted) break;
    frames++; usleep(frames % 8 ? 16000 : 40000);   // a normal frame is about 16 ms; now and then one is long, as when the display is refreshed
  }
  printf("[%s] done after %lu ms\n", argv[1], millis() - t0);
}
