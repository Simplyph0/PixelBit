#pragma once
#include "Arduino.h"
struct TwoWire { void begin(int, int) {} void begin() {} void setClock(long) {}
  void beginTransmission(int) {} int endTransmission() { return 0; } };
extern TwoWire Wire;
