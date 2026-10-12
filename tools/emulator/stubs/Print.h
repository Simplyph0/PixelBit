#pragma once
#include "Arduino.h"

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buf, size_t n) { size_t k = 0; while (n--) k += write(*buf++); return k; }
  size_t write(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t print(const char* s) { return write(s); }
  size_t print(const __FlashStringHelper* s) { return write((const char*)s); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(const String& s) { return write(s.c_str()); }
  size_t print(int v, int base = 10) { return printNum((long)v, base); }
  size_t print(unsigned v, int base = 10) { return printNum((long)v, base); }
  size_t print(long v, int base = 10) { return printNum(v, base); }
  size_t print(unsigned long v, int base = 10) { return printNum((long)v, base); }
  size_t print(double v, int digits = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", digits, v); return write(b); }
  size_t println() { return write((uint8_t)'\r') + write((uint8_t)'\n'); }
  template <class T> size_t println(T v) { return print(v) + println(); }
private:
  size_t printNum(long v, int base) {
    char b[40];
    if (base == 16) snprintf(b, sizeof b, "%lx", v); else snprintf(b, sizeof b, "%ld", v);
    return write(b);
  }
};
