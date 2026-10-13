// The few things the emulator and the test programs need from the operating
// system, behind one interface so the rest of the code is the same on Linux,
// macOS and Windows. Implementations: platform_common.cpp (clock, sleep, files),
// platform_posix.cpp (Linux, macOS) and platform_win.cpp (Windows, MinGW-w64).
//
// This header must stay free of system headers: <windows.h> and the firmware's
// Arduino stubs define clashing names (INPUT, min, max, ...).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace plat {

// ---- time, files, process ----
void sleepUs(unsigned long long us);
double monotonicMs();                      // milliseconds since an arbitrary start
unsigned processId();
void makeDir(const char* path);            // no error if it already exists
bool replaceFile(const char* from, const char* to);   // rename, replacing an existing target
void exitNow(int code);                    // end the process at once, no cleanup (a simulated power cut)

// ---- terminal front end ----
// termRead() returns the keys pressed since the last call as bytes the way a
// VT100 terminal sends them: letters as themselves, arrows as ESC [ A/B/C/D.
bool termSetup();                          // raw keys, UTF-8 and ANSI escapes on; false if not possible
void termRestore();
int termRead(uint8_t* buf, int max);       // never blocks; returns 0 when there is nothing
void termWrite(const char* data, size_t n);

// ---- UDP on 127.0.0.1: the pretend radio between two emulators ----
bool udpOpen(int port);                    // bind, non-blocking
bool udpIsOpen();
void udpSend(int port, const uint8_t* data, int n);
int udpRecv(uint8_t* buf, int max);        // never blocks; <= 0 when there is nothing
void udpClose();

// ---- serial port: the ESP32 bridge board ----
// dev is "/dev/ttyACM0" or "/dev/cu.usbmodem1101", or "COM3" on Windows.
bool serialOpen(const char* dev);          // 115200 8N1, raw, non-blocking, DTR/RTS low
bool serialIsOpen();
int serialRead(uint8_t* buf, int max);     // never blocks; 0 when there is nothing
void serialWrite(const uint8_t* data, int n);
void serialFlushInput();

}  // namespace plat
