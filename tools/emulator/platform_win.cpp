// platform.h for Windows, built with MinGW-w64 (MSYS2). Windows headers are
// only included here: they clash with the firmware's Arduino stubs.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mmsystem.h>   // timeBeginPeriod; not part of WIN32_LEAN_AND_MEAN
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace plat {

// Windows sleeps in steps of about 15 ms unless asked for finer ones; the
// emulator and the radio tests sleep in 1 to 40 ms steps.
namespace {
struct TimerResolution {
  TimerResolution() { timeBeginPeriod(1); }
  ~TimerResolution() { timeEndPeriod(1); }
} g_timerResolution;
}  // namespace

unsigned processId() { return (unsigned)GetCurrentProcessId(); }
void exitNow(int code) { _exit(code); }

// ---- terminal ----
static HANDLE g_in = INVALID_HANDLE_VALUE, g_out = INVALID_HANDLE_VALUE;
static DWORD g_oldInMode = 0, g_oldOutMode = 0;
static UINT g_oldCodePage = 0;
static bool g_inIsConsole = false, g_outIsConsole = false, g_termSaved = false;

bool termSetup() {
  g_in = GetStdHandle(STD_INPUT_HANDLE);
  g_out = GetStdHandle(STD_OUTPUT_HANDLE);
  g_inIsConsole = GetConsoleMode(g_in, &g_oldInMode) != 0;
  g_outIsConsole = GetConsoleMode(g_out, &g_oldOutMode) != 0;
  g_termSaved = true;
  bool ok = true;
  if (g_inIsConsole) {
    // No line editing or echo; Ctrl+C arrives as the character 3 and quits the emulator
    DWORD m = g_oldInMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT);
    ok = SetConsoleMode(g_in, m) != 0 && ok;
  }
  if (g_outIsConsole) {
    g_oldCodePage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);   // the braille picture is UTF-8
    ok = SetConsoleMode(g_out, g_oldOutMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0 && ok;
  }
  return ok;
}

void termRestore() {
  if (!g_termSaved) return;
  if (g_inIsConsole) SetConsoleMode(g_in, g_oldInMode);
  if (g_outIsConsole) {
    SetConsoleMode(g_out, g_oldOutMode);
    SetConsoleOutputCP(g_oldCodePage);
  }
  g_termSaved = false;
}

int termRead(uint8_t* buf, int max) {
  if (!g_inIsConsole) return 0;
  int n = 0;
  DWORD pending = 0;
  while (n + 3 <= max && GetNumberOfConsoleInputEvents(g_in, &pending) && pending > 0) {
    INPUT_RECORD rec;
    DWORD got = 0;
    if (!ReadConsoleInputW(g_in, &rec, 1, &got) || got == 0) break;
    if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
    const KEY_EVENT_RECORD& k = rec.Event.KeyEvent;
    char arrow = 0;
    switch (k.wVirtualKeyCode) {
      case VK_UP: arrow = 'A'; break;
      case VK_DOWN: arrow = 'B'; break;
      case VK_RIGHT: arrow = 'C'; break;
      case VK_LEFT: arrow = 'D'; break;
    }
    if (arrow) {
      buf[n++] = 0x1b; buf[n++] = '['; buf[n++] = (uint8_t)arrow;
    } else if (k.uChar.UnicodeChar > 0 && k.uChar.UnicodeChar < 128) {
      buf[n++] = (uint8_t)k.uChar.UnicodeChar;
    }
  }
  return n;
}

void termWrite(const char* data, size_t n) {
  HANDLE h = g_out != INVALID_HANDLE_VALUE ? g_out : GetStdHandle(STD_OUTPUT_HANDLE);
  while (n > 0) {
    DWORD w = 0;
    if (!WriteFile(h, data, (DWORD)n, &w, nullptr) || w == 0) return;
    data += w;
    n -= w;
  }
}

// ---- UDP ----
static SOCKET g_sock = INVALID_SOCKET;

static sockaddr_in loopback(int port) {
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = inet_addr("127.0.0.1");
  a.sin_port = htons((u_short)port);
  return a;
}

bool udpOpen(int port) {
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
  g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (g_sock == INVALID_SOCKET) return false;
  // Sending to a port nobody listens on must not make the next receive fail
  BOOL off = FALSE;
  DWORD ret = 0;
  WSAIoctl(g_sock, SIO_UDP_CONNRESET, &off, sizeof off, nullptr, 0, &ret, nullptr, nullptr);
  sockaddr_in a = loopback(port);
  if (bind(g_sock, (sockaddr*)&a, sizeof a) == SOCKET_ERROR) {
    fprintf(stderr, "bind: error %d\n", WSAGetLastError());
    closesocket(g_sock);
    g_sock = INVALID_SOCKET;
    return false;
  }
  u_long nonBlocking = 1;
  ioctlsocket(g_sock, FIONBIO, &nonBlocking);
  return true;
}

bool udpIsOpen() { return g_sock != INVALID_SOCKET; }

void udpSend(int port, const uint8_t* data, int n) {
  sockaddr_in a = loopback(port);
  sendto(g_sock, (const char*)data, n, 0, (sockaddr*)&a, sizeof a);
}

int udpRecv(uint8_t* buf, int max) {
  return recv(g_sock, (char*)buf, max, 0);
}

void udpClose() {
  if (g_sock != INVALID_SOCKET) closesocket(g_sock);
  g_sock = INVALID_SOCKET;
}

// ---- serial ----
static HANDLE g_serial = INVALID_HANDLE_VALUE;

bool serialOpen(const char* dev) {
  // "COM3" works up to COM9; the \\.\ prefix is needed from COM10 on and is always accepted
  char path[64];
  if (!strncmp(dev, "\\\\.\\", 4)) snprintf(path, sizeof path, "%s", dev);
  else snprintf(path, sizeof path, "\\\\.\\%s", dev);
  g_serial = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (g_serial == INVALID_HANDLE_VALUE) return false;

  DCB dcb{};
  dcb.DCBlength = sizeof dcb;
  GetCommState(g_serial, &dcb);
  dcb.BaudRate = CBR_115200;
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  dcb.fParity = FALSE;
  dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  dcb.fRtsControl = RTS_CONTROL_DISABLE;
  dcb.fOutX = dcb.fInX = FALSE;
  SetCommState(g_serial, &dcb);

  COMMTIMEOUTS to{};
  to.ReadIntervalTimeout = MAXDWORD;   // a read returns at once with whatever has arrived
  to.WriteTotalTimeoutConstant = 200;
  SetCommTimeouts(g_serial, &to);
  EscapeCommFunction(g_serial, CLRDTR);
  EscapeCommFunction(g_serial, CLRRTS);
  return true;
}

bool serialIsOpen() { return g_serial != INVALID_HANDLE_VALUE; }

int serialRead(uint8_t* buf, int max) {
  DWORD got = 0;
  if (!ReadFile(g_serial, buf, (DWORD)max, &got, nullptr)) return 0;
  return (int)got;
}

void serialWrite(const uint8_t* data, int n) {
  DWORD w = 0;
  WriteFile(g_serial, data, (DWORD)n, &w, nullptr);
}

void serialFlushInput() { PurgeComm(g_serial, PURGE_RXCLEAR | PURGE_TXCLEAR); }

}  // namespace plat
