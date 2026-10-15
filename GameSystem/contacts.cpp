#include "contacts.h"
#include "gamelink.h"   // linkOwnIdBytes()
#include "highscore.h"   // CONTACTS_ADDR: the EEPROM map
#include <EEPROM.h>

// Record at CONTACTS_ADDR: "CONT", then MAX_CONTACTS slots of [id (3), alias (ALIAS_MAX + 1)]
// (an empty alias marks an empty slot) and a checksum byte over everything before it.
struct Slot { uint8_t id[3]; char alias[ALIAS_MAX + 1]; };
static Slot slots[MAX_CONTACTS];
static bool loaded = false;

static const size_t SLOT_BYTES = sizeof(Slot);
static const size_t RECORD_BYTES = 4 + MAX_CONTACTS * SLOT_BYTES + 1;
static_assert(CONTACTS_ADDR + 4 + MAX_CONTACTS * (3 + ALIAS_MAX + 1) + 1 <= TEST_SCRATCH_ADDR, "the contacts would run into the test area");

static uint8_t checksum(const uint8_t* b, size_t n) {
  uint8_t sum = 0x5A;
  for (size_t i = 0; i < n; i++) sum = (uint8_t)(sum * 31 + b[i]);
  return sum;
}

static void save() {
  uint8_t buf[RECORD_BYTES];
  memcpy(buf, "CONT", 4);
  memcpy(buf + 4, slots, MAX_CONTACTS * SLOT_BYTES);
  buf[RECORD_BYTES - 1] = checksum(buf, RECORD_BYTES - 1);
  for (size_t i = 0; i < RECORD_BYTES; i++) EEPROM.write(CONTACTS_ADDR + i, buf[i]);
  EEPROM.commit();
}

void initContacts() {
  loaded = true;
  memset(slots, 0, sizeof(slots));
  uint8_t buf[RECORD_BYTES];
  for (size_t i = 0; i < RECORD_BYTES; i++) buf[i] = EEPROM.read(CONTACTS_ADDR + i);
  if (memcmp(buf, "CONT", 4) != 0 || buf[RECORD_BYTES - 1] != checksum(buf, RECORD_BYTES - 1)) return;
  memcpy(slots, buf + 4, MAX_CONTACTS * SLOT_BYTES);
  for (int i = 0; i < MAX_CONTACTS; i++) slots[i].alias[ALIAS_MAX] = 0;   // never trust the end of a string
}

static void ensureLoaded() { if (!loaded) initContacts(); }

int contactCount() {
  ensureLoaded();
  int n = 0;
  for (int i = 0; i < MAX_CONTACTS; i++) if (slots[i].alias[0]) n++;
  return n;
}

bool contactAt(int i, uint8_t id[3], const char** alias) {
  ensureLoaded();
  for (int s = 0; s < MAX_CONTACTS; s++) {
    if (!slots[s].alias[0]) continue;
    if (i-- == 0) {
      memcpy(id, slots[s].id, 3);
      *alias = slots[s].alias;
      return true;
    }
  }
  return false;
}

static int find(const uint8_t id[3]) {
  for (int i = 0; i < MAX_CONTACTS; i++)
    if (slots[i].alias[0] && memcmp(slots[i].id, id, 3) == 0) return i;
  return -1;
}

const char* contactAlias(const uint8_t id[3]) {
  ensureLoaded();
  int i = find(id);
  return i < 0 ? nullptr : slots[i].alias;
}

const char* contactCheckAlias(char* alias) {
  // trim
  char* start = alias;
  while (*start == ' ') start++;
  size_t len = strlen(start);
  while (len && start[len - 1] == ' ') len--;
  memmove(alias, start, len);
  alias[len] = 0;
  if (len < 1 || len > ALIAS_MAX) return "The alias needs 1 to 10 characters";
  for (size_t i = 0; i < len; i++) {
    char c = alias[i];
    if (c >= 'a' && c <= 'z') c -= 32;
    bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr(" -_.!?+*", c);
    if (!ok || !c) return "Use letters, digits, space and - _ . ! ? + *";
    alias[i] = c;
  }
  return nullptr;
}

const char* contactSet(const uint8_t id[3], char* alias) {
  ensureLoaded();
  const char* err = contactCheckAlias(alias);
  if (err) return err;
  uint8_t own[3];
  linkOwnIdBytes(own);
  if (memcmp(own, id, 3) == 0) return "That is this console's own id";
  int i = find(id);
  if (i < 0) {
    for (int s = 0; s < MAX_CONTACTS; s++) if (!slots[s].alias[0]) { i = s; break; }
    if (i < 0) return "The contact list is full (8)";
  }
  memcpy(slots[i].id, id, 3);
  memset(slots[i].alias, 0, sizeof(slots[i].alias));
  strncpy(slots[i].alias, alias, ALIAS_MAX);
  save();
  return nullptr;
}

bool contactRemove(const uint8_t id[3]) {
  ensureLoaded();
  int i = find(id);
  if (i < 0) return false;
  memset(&slots[i], 0, sizeof(Slot));
  save();
  return true;
}

void contactsClear() {
  ensureLoaded();
  memset(slots, 0, sizeof(slots));
  save();
}
