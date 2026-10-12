#include "highscore.h"

HighscoreManager highscoreManager;

static const char SAVE_TAG[4] = {'G', 'S', 'A', 'V'};
static const uint8_t SAVE_VERSION = 1;
static const size_t SAVE_HEADER = 8;
static const size_t SAVE_MAX_LEN = SAVE_HEADER + 4 * SAVE_SLOTS + 4;

// Safe to call more than once: the EEPROM is started the first time only
void initHighscores() {
  static bool started = false;
  if (started) return;
  started = true;
  highscoreManager.init();
}
int getGameHighscore(int gameId) { return highscoreManager.getHighscore(gameId); }
bool checkNewHighscore(int gameId, int score) { return highscoreManager.isNewHighscore(gameId, score); }
void saveGameHighscore(int gameId, int score) { highscoreManager.saveHighscore(gameId, score); }

// ---------------------------------------------------------
// Binary codec
// ---------------------------------------------------------
static uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
  }
  return ~crc;
}

static void putU32(uint8_t* p, uint32_t v) {
  p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static uint32_t getU32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static size_t buildSave(uint8_t* buf, const int* scores, uint8_t count) {
  memcpy(buf, SAVE_TAG, 4);
  buf[4] = SAVE_VERSION;
  buf[5] = count;
  buf[6] = buf[7] = 0;
  for (uint8_t i = 0; i < count; i++) putU32(buf + SAVE_HEADER + 4 * i, (uint32_t)scores[i]);
  size_t body = SAVE_HEADER + 4 * count;
  putU32(buf + body, crc32(buf, body));
  return body + 4;
}

// Returns nullptr when valid, otherwise an error message.
static const char* parseSave(const uint8_t* buf, size_t len, int* scores, uint8_t& count) {
  if (len < SAVE_HEADER + 4) return "Too short";
  if (memcmp(buf, SAVE_TAG, 4) != 0) return "Not a game save";
  if (buf[4] != SAVE_VERSION) return "Unsupported save version";
  count = buf[5];
  if (count > SAVE_SLOTS) return "Too many games";
  size_t body = SAVE_HEADER + 4 * (size_t)count;
  if (len != body + 4) return "Wrong length";
  if (getU32(buf + body) != crc32(buf, body)) return "Checksum mismatch (corrupted)";
  for (uint8_t i = 0; i < count; i++) {
    int32_t s = (int32_t)getU32(buf + SAVE_HEADER + 4 * i);
    if (s < 0 || s > SAVE_MAX_SCORE) return "Score out of range";
    scores[i] = s;
  }
  return nullptr;
}

// ---------------------------------------------------------
// EEPROM
// ---------------------------------------------------------
void HighscoreManager::init() {
  EEPROM.begin(EEPROM_SIZE);
  resetToDefaults();

  if (loadFromEEPROM()) return;
  if (migrateLegacy()) {
    saveToEEPROM();
    Serial.println(F("Highscores migrated from the old save format"));
    return;
  }
  Serial.println(F("No valid save found, starting fresh"));
  saveToEEPROM();
}

void HighscoreManager::resetToDefaults() {
  for (int i = 0; i < MAX_GAMES; i++) scores[i] = 0;
}

bool HighscoreManager::loadFromEEPROM() {
  uint8_t buf[SAVE_MAX_LEN];
  for (size_t i = 0; i < SAVE_HEADER; i++) buf[i] = EEPROM.read(HIGHSCORE_BASE_ADDR + i);
  if (memcmp(buf, SAVE_TAG, 4) != 0 || buf[5] > SAVE_SLOTS) return false;

  size_t len = SAVE_HEADER + 4 * (size_t)buf[5] + 4;
  for (size_t i = SAVE_HEADER; i < len; i++) buf[i] = EEPROM.read(HIGHSCORE_BASE_ADDR + i);

  int loaded[SAVE_SLOTS];
  uint8_t count;
  if (parseSave(buf, len, loaded, count)) return false;
  for (int i = 0; i < MAX_GAMES && i < count; i++) scores[i] = loaded[i];
  return true;
}

// The old format was a raw struct: u16 magic 0xABCD, int scores[N], u16 checksum
// (sum of magic and scores). N was 8, or 10 in an intermediate build.
bool HighscoreManager::migrateLegacy() {
  static const uint8_t layouts[] = {8, 10};
  for (uint8_t n : layouts) {
    uint8_t raw[4 + 4 * 10 + 2];
    size_t len = 4 + 4 * (size_t)n + 2;
    for (size_t i = 0; i < len; i++) raw[i] = EEPROM.read(HIGHSCORE_BASE_ADDR + i);
    if (raw[0] != 0xCD || raw[1] != 0xAB) continue;

    uint16_t sum = 0xABCD;
    int found[10];
    bool ok = true;
    for (uint8_t i = 0; i < n; i++) {
      int32_t s = (int32_t)getU32(raw + 4 + 4 * i);
      if (s < 0 || s > SAVE_MAX_SCORE) { ok = false; break; }
      found[i] = s;
      sum += (uint16_t)s;
    }
    uint16_t stored = raw[len - 2] | (raw[len - 1] << 8);
    if (!ok || stored != sum) continue;

    for (int i = 0; i < n && i < MAX_GAMES; i++) scores[i] = found[i];
    return true;
  }
  return false;
}

void HighscoreManager::saveToEEPROM() {
  uint8_t buf[SAVE_MAX_LEN];
  size_t len = buildSave(buf, scores, MAX_GAMES);
  for (size_t i = 0; i < len; i++) EEPROM.write(HIGHSCORE_BASE_ADDR + i, buf[i]);
  EEPROM.commit();
}

int HighscoreManager::getHighscore(int gameId) {
  return (gameId >= 0 && gameId < MAX_GAMES) ? scores[gameId] : 0;
}

bool HighscoreManager::isNewHighscore(int gameId, int score) {
  return gameId >= 0 && gameId < MAX_GAMES && score > scores[gameId];
}

void HighscoreManager::saveHighscore(int gameId, int score) {
  if (gameId < 0 || gameId >= MAX_GAMES || score <= scores[gameId]) return;
  scores[gameId] = min(score, SAVE_MAX_SCORE);
  saveToEEPROM();
}

void HighscoreManager::resetAllHighscores() {
  resetToDefaults();
  saveToEEPROM();
}

// ---------------------------------------------------------
// Export / import
// ---------------------------------------------------------
String HighscoreManager::exportText() {
  uint8_t buf[SAVE_MAX_LEN];
  size_t len = buildSave(buf, scores, MAX_GAMES);
  static const char* HEX_DIGITS = "0123456789ABCDEF";
  String out = "GSAV1-";
  out.reserve(6 + 2 * len);
  for (size_t i = 0; i < len; i++) {
    out += HEX_DIGITS[buf[i] >> 4];
    out += HEX_DIGITS[buf[i] & 15];
  }
  return out;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

const char* HighscoreManager::importText(const String& text, bool merge) {
  String t = text;
  t.trim();
  if (!t.startsWith("GSAV1-")) return "Not a game save";
  t = t.substring(6);

  uint8_t buf[SAVE_MAX_LEN];
  size_t len = 0;
  int hi = -1;
  for (size_t i = 0; i < t.length(); i++) {
    char c = t[i];
    if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
    int v = hexVal(c);
    if (v < 0) return "Bad characters in save";
    if (hi < 0) { hi = v; continue; }
    if (len >= sizeof(buf)) return "Save too long";
    buf[len++] = (hi << 4) | v;
    hi = -1;
  }
  if (hi >= 0) return "Incomplete save";

  int incoming[SAVE_SLOTS];
  uint8_t count;
  const char* err = parseSave(buf, len, incoming, count);
  if (err) return err;

  for (int i = 0; i < MAX_GAMES && i < count; i++) {
    scores[i] = merge ? max(scores[i], incoming[i]) : incoming[i];
  }
  saveToEEPROM();
  return nullptr;
}
