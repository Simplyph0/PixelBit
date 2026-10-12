#pragma once
#include "Arduino.h"
struct TwoWire { void begin(int, int) {} void begin() {} void setClock(long) {} };
extern TwoWire Wire;
