// platform.h for Linux and macOS.
#include "platform.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

namespace plat {

unsigned processId() { return (unsigned)getpid(); }
void exitNow(int code) { _exit(code); }

// ---- terminal ----
static termios g_oldTerm;
static int g_oldFlags = 0;
static bool g_termSaved = false;

bool termSetup() {
  if (tcgetattr(STDIN_FILENO, &g_oldTerm) != 0) return false;   // not a terminal
  g_oldFlags = fcntl(STDIN_FILENO, F_GETFL);
  g_termSaved = true;
  termios raw = g_oldTerm;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  fcntl(STDIN_FILENO, F_SETFL, g_oldFlags | O_NONBLOCK);
  return true;
}

void termRestore() {
  if (!g_termSaved) return;
  tcsetattr(STDIN_FILENO, TCSANOW, &g_oldTerm);
  fcntl(STDIN_FILENO, F_SETFL, g_oldFlags);
  g_termSaved = false;
}

int termRead(uint8_t* buf, int max) {
  long n = read(STDIN_FILENO, buf, (size_t)max);
  return n > 0 ? (int)n : 0;
}

void termWrite(const char* data, size_t n) {
  long r = write(STDOUT_FILENO, data, n);
  (void)r;
}

// ---- UDP ----
static int g_sock = -1;

static sockaddr_in loopback(int port) {
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = inet_addr("127.0.0.1");
  a.sin_port = htons(port);
  return a;
}

bool udpOpen(int port) {
  g_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (g_sock < 0) return false;
  sockaddr_in a = loopback(port);
  if (bind(g_sock, (sockaddr*)&a, sizeof a) < 0) { perror("bind"); close(g_sock); g_sock = -1; return false; }
  fcntl(g_sock, F_SETFL, O_NONBLOCK);
  return true;
}

bool udpIsOpen() { return g_sock >= 0; }

void udpSend(int port, const uint8_t* data, int n) {
  sockaddr_in a = loopback(port);
  sendto(g_sock, data, (size_t)n, 0, (sockaddr*)&a, sizeof a);
}

int udpRecv(uint8_t* buf, int max) {
  return (int)recv(g_sock, buf, (size_t)max, 0);
}

void udpClose() {
  if (g_sock >= 0) close(g_sock);
  g_sock = -1;
}

// ---- serial ----
static int g_serial = -1;

bool serialOpen(const char* dev) {
  g_serial = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (g_serial < 0) return false;
  termios t;
  tcgetattr(g_serial, &t);
  cfmakeraw(&t);
  cfsetspeed(&t, B115200);
  t.c_cflag |= CLOCAL | CREAD;
  tcsetattr(g_serial, TCSANOW, &t);
  int bits = TIOCM_DTR | TIOCM_RTS;
  ioctl(g_serial, TIOCMBIC, &bits);   // opening the port can reset the board; the caller lets it settle
  return true;
}

bool serialIsOpen() { return g_serial >= 0; }

int serialRead(uint8_t* buf, int max) {
  long n = read(g_serial, buf, (size_t)max);
  return n > 0 ? (int)n : 0;
}

void serialWrite(const uint8_t* data, int n) {
  long r = write(g_serial, data, (size_t)n);
  (void)r;
}

void serialFlushInput() { tcflush(g_serial, TCIOFLUSH); }

}  // namespace plat
