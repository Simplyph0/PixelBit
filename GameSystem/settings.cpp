#include "settings.h"
#include <EEPROM.h>
#include "highscore.h"   // SETTINGS_ADDR: the EEPROM map

// Layout at SETTINGS_ADDR: "GOPT", version, games, options per game, a flags
// byte (version 2); version 2 then has the 16-bit "seen" mask (bit n = the settings screens
// of game n have been shown once); then the values (row by row) and a 16-bit
// Fletcher checksum of everything before it. The high scores live at address 0
// and use less than 100 bytes. Version 1 (no mask) is still read.
//
// More than 16 games: the block has no spare bytes (the device settings sit right behind it),
// so the "seen" bits of games 16 to 20 ride in the spare bits 3 to 7 of the flags byte. The
// block keeps its size and its place, an older block (those bits clear) reads as "not seen yet",
// and older firmware, which only looks at the flag bits it knows, still reads a newer block.
static const uint8_t SETTINGS_VERSION = 2;
static const uint8_t FLAG_BITS = 0x07;         // the real flags (SETTING_*); bits 3-7 carry "seen" for games 16-20
static const int FLAG_SEEN_SHIFT = 3;          // flags bit 3 = game 16

static uint16_t fletcher16(const uint8_t* data, size_t len) {
  uint16_t a = 0, b = 0;
  for (size_t i = 0; i < len; i++) {
    a = (a + data[i]) % 255;
    b = (b + a) % 255;
  }
  return (b << 8) | a;
}

void saveGameOptions(const uint8_t values[MAX_GAMES][MAX_GAME_OPTIONS], uint32_t seen, uint8_t flags) {
  const size_t valueBytes = MAX_GAMES * MAX_GAME_OPTIONS;
  uint8_t buf[10 + valueBytes];
  memcpy(buf, "GOPT", 4);
  buf[4] = SETTINGS_VERSION;
  buf[5] = MAX_GAMES;
  buf[6] = MAX_GAME_OPTIONS;
  buf[7] = (flags & FLAG_BITS) | (uint8_t)(((seen >> 16) & 0x1F) << FLAG_SEEN_SHIFT);
  buf[8] = seen & 0xFF;
  buf[9] = (seen >> 8) & 0xFF;
  memcpy(buf + 10, values, valueBytes);
  size_t len = 10 + valueBytes;
  uint16_t sum = fletcher16(buf, len);
  for (size_t i = 0; i < len; i++) EEPROM.write(SETTINGS_ADDR + i, buf[i]);
  EEPROM.write(SETTINGS_ADDR + len, sum & 0xFF);
  EEPROM.write(SETTINGS_ADDR + len + 1, sum >> 8);
  EEPROM.commit();
}

void loadGameOptions(uint8_t values[MAX_GAMES][MAX_GAME_OPTIONS], uint32_t* seen, uint8_t* flags) {
  uint8_t head[8];
  for (int i = 0; i < 8; i++) head[i] = EEPROM.read(SETTINGS_ADDR + i);
  uint8_t version = head[4];
  if (memcmp(head, "GOPT", 4) != 0 || (version != 1 && version != 2) ||
      head[6] != MAX_GAME_OPTIONS || head[5] > MAX_GAMES) return;

  int games = head[5];
  size_t prefix = version == 2 ? 10 : 8;   // bytes before the values
  size_t len = prefix + (size_t)games * MAX_GAME_OPTIONS;
  uint8_t buf[10 + MAX_GAMES * MAX_GAME_OPTIONS];
  for (size_t i = 0; i < len; i++) buf[i] = EEPROM.read(SETTINGS_ADDR + i);
  uint16_t stored = EEPROM.read(SETTINGS_ADDR + len) | (EEPROM.read(SETTINGS_ADDR + len + 1) << 8);
  if (stored != fletcher16(buf, len)) return;

  memcpy(values, buf + prefix, (size_t)games * MAX_GAME_OPTIONS);   // games added later keep their defaults
  *seen = version == 2 ? ((uint32_t)buf[8] | ((uint32_t)buf[9] << 8) | ((uint32_t)(buf[7] >> FLAG_SEEN_SHIFT) << 16)) : 0;
  *flags = version == 2 ? (buf[7] & FLAG_BITS) : 0;
}

bool isFirstBoot() {
  uint8_t values[MAX_GAMES][MAX_GAME_OPTIONS];
  uint32_t seen = 0;
  uint8_t flags = 0;
  loadGameOptions(values, &seen, &flags);
  return !(flags & SETTING_TUTORIAL_DONE);
}
