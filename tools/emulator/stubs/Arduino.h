// Just enough of the Arduino core for the console firmware to build on a PC.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <algorithm>

#define PROGMEM
#define PI 3.14159265358979
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1
#define radians(deg) ((deg) * 0.017453292519943295)
#define pgm_read_byte(a) (*(const uint8_t*)(a))
#define pgm_read_word(a) (*(const uint16_t*)(a))
#define pgm_read_dword(a) (*(const uint32_t*)(a))

class __FlashStringHelper;
#define F(s) ((const __FlashStringHelper*)(s))

typedef bool boolean;
typedef uint8_t byte;

unsigned long millis();
void delay(unsigned long ms);
extern void (*g_yieldHook)();   // the emulated radio delivers packets here, as the real one does while the sketch yields
inline void yield() { if (g_yieldHook) g_yieldHook(); }
long random(long max);
long random(long min, long max);
void pinMode(int pin, int mode);
int digitalRead(int pin);

template <class T, class U> auto min(T a, U b) -> decltype(a + b) { return a < b ? a : b; }
template <class T, class U> auto max(T a, U b) -> decltype(a + b) { return a > b ? a : b; }
template <class T, class L, class H> T constrain(T x, L lo, H hi) { return x < lo ? (T)lo : (x > hi ? (T)hi : x); }

struct String : std::string {
  using std::string::string;
  String(const std::string& s) : std::string(s) {}
  void trim() {
    size_t a = find_first_not_of(" \t\r\n");
    size_t b = find_last_not_of(" \t\r\n");
    assign(a == npos ? "" : substr(a, b - a + 1));
  }
  bool startsWith(const char* p) const { return rfind(p, 0) == 0; }
  String substring(size_t n) const { return String(substr(n)); }
  void reserve(size_t n) { std::string::reserve(n); }
  unsigned length() const { return size(); }
  String& operator+=(char c) { push_back(c); return *this; }
  bool operator!=(const char* s) const { return compare(s) != 0; }
};

struct SerialT {
  void begin(int) {}
  template <class T> void print(T) {}
  template <class T> void println(T) {}
  void println() {}
  int printf(const char*, ...) __attribute__((format(printf, 2, 3))) { return 0; }
};
extern SerialT Serial;

#include "Print.h"
