#ifndef CONTACTS_H
#define CONTACTS_H

#include "config.h"

// Aliases for other consoles' ids ("NVDKH" -> "DAD"), set on the Wi-Fi page and
// shown in chat instead of the id. They live in the EEPROM with a checksum.
//
// An alias is 1 to ALIAS_MAX characters: 10 characters are 60 pixels wide, so
// "FROM " plus an alias always fits on one 128-pixel line, and so does an alias
// next to a marker in a list. Letters (stored as capitals), digits, space and - _ . ! ? + *
#define MAX_CONTACTS 8
#define ALIAS_MAX 10

void initContacts();                        // reads the saved table (after the EEPROM has started)
int contactCount();
bool contactAt(int i, uint8_t id[3], const char** alias);   // the i-th saved contact
const char* contactAlias(const uint8_t id[3]);              // nullptr when the id has no alias
// Adds a contact or changes its alias. Returns nullptr, or a short message saying why not.
// The alias is cleaned up in place: trimmed, in capitals.
const char* contactSet(const uint8_t id[3], char* alias);
bool contactRemove(const uint8_t id[3]);
void contactsClear();
const char* contactCheckAlias(char* alias); // trims and capitalises; returns an error message or nullptr

#endif
