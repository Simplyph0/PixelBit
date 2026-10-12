// Saved settings: round trip, the first-boot flag, the old version-1 layout, damage
#include "settings.h"
#include <cstdio>
#include <cstring>
#include <EEPROM.h>
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)
static uint16_t fletcher16(const uint8_t* d, size_t n) { uint16_t a = 0, b = 0; for (size_t i = 0; i < n; i++) { a = (a + d[i]) % 255; b = (b + a) % 255; } return (b << 8) | a; }
int main() {
  EEPROM.begin(512);
  memset(EEPROM.mem, 0, sizeof(EEPROM.mem));
  uint8_t v[MAX_GAMES][MAX_GAME_OPTIONS] = {}, out[MAX_GAMES][MAX_GAME_OPTIONS] = {}; uint16_t seen = 0, seenOut = 0; uint8_t flags = 0, flagsOut = 0;
  loadGameOptions(out, &seenOut, &flagsOut);
  CHECK(flagsOut == 0 && seenOut == 0, "blank EEPROM: no flags, nothing seen");

  v[3][0] = 2; v[8][1] = 4; seen = 0x0108; flags = SETTING_TUTORIAL_DONE;
  saveGameOptions(v, seen, flags);
  memset(out, 0, sizeof(out)); seenOut = 0; flagsOut = 0;
  loadGameOptions(out, &seenOut, &flagsOut);
  CHECK(memcmp(v, out, sizeof(v)) == 0 && seenOut == seen && flagsOut == SETTING_TUTORIAL_DONE, "round trip keeps options, seen mask and the tour flag");

  // damage one byte: everything falls back
  EEPROM.mem[128 + 20] ^= 0x55; memset(out, 7, sizeof(out)); seenOut = 9; flagsOut = 9;
  loadGameOptions(out, &seenOut, &flagsOut);
  CHECK(out[0][0] == 7 && seenOut == 9 && flagsOut == 9, "a damaged block is ignored");

  // a version-1 block (older firmware): options load, the tour flag is clear
  memset(EEPROM.mem, 0, sizeof(EEPROM.mem));
  uint8_t old[8 + 13 * MAX_GAME_OPTIONS] = {}; memcpy(old, "GOPT", 4); old[4] = 1; old[5] = 13; old[6] = MAX_GAME_OPTIONS; old[8 + 2 * MAX_GAME_OPTIONS] = 3;
  size_t len = sizeof(old); uint16_t sum = fletcher16(old, len);
  for (size_t i = 0; i < len; i++) EEPROM.mem[128 + i] = old[i];
  EEPROM.mem[128 + len] = sum & 0xFF; EEPROM.mem[128 + len + 1] = sum >> 8;
  memset(out, 0, sizeof(out)); seenOut = 5; flagsOut = 5;
  loadGameOptions(out, &seenOut, &flagsOut);
  CHECK(out[2][0] == 3 && seenOut == 0 && flagsOut == 0, "version 1 still loads, with no tour flag");

  printf("%s\n", fails ? "settings: SOME CHECKS FAILED" : "settings: all checks passed");
  return fails != 0; }
