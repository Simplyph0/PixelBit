// PC emulator for the console: runs the real firmware sources on this computer.
//
// Screen:  an SDL window (default when a display is available) or the terminal
//          (braille characters, --tty), or nothing (--headless, for scripts).
// Buttons: arrow keys / WASD. The window sees real key presses and releases;
//          a terminal cannot, so a key counts as held for a moment there.
// Radio:   --bridge /dev/ttyACM0  a real ESP32 bridge board (see tools/bridge)
//          --udp 0 | --udp 1      two emulators on this PC talk to each other
//
// Keys: P screenshot (shots/), G start/stop GIF (gifs/), R start/stop input
// recording (recordings/), Q quit. Replay a recording or a script with
// --replay file; see README.md in this folder.

#include "Arduino.h"
#include "Wire.h"
#include "EEPROM.h"
#include "ESP8266WiFi.h"
#include "espnow.h"
#include "Updater.h"
#include "Adafruit_SSD1306.h"
#include "config.h"
#include "display.h"
#include "input.h"
#include "gamemanager.h"
#include "savesync.h"
#include "gamelink.h"
#include "highscore.h"
#include "settings.h"
#include "capture.h"

#ifdef USE_SDL
#include <SDL2/SDL.h>
#endif

#include <algorithm>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Arduino environment, with a clock that can run faster than real time
// ---------------------------------------------------------------------------
SerialT Serial;
TwoWire Wire;
EEPROMClass EEPROM;
WiFiClass WiFi;
EspClass ESP;

static uint8_t g_mac[6] = {0x02, 0, 0, 0, 0, 0x01};
static double g_timeScale = 1.0;
static bool g_quit = false;
static timespec g_t0;

static double realMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (t.tv_sec - g_t0.tv_sec) * 1000.0 + (t.tv_nsec - g_t0.tv_nsec) / 1e6;
}
unsigned long millis() { return (unsigned long)(realMs() * g_timeScale) + 1000; }
void delay(unsigned long ms) { usleep((useconds_t)(ms * 1000 / g_timeScale)); }
long random(long max) { return max > 0 ? rand() % max : 0; }
long random(long min, long max) { return max > min ? min + rand() % (max - min) : min; }
void pinMode(int, int) {}

void WiFiClass::macAddress(uint8_t* m) { memcpy(m, g_mac, 6); }
uint32_t EspClass::random() { return ((uint32_t)rand() << 16) ^ (uint32_t)rand(); }
void EspClass::restart() { g_quit = true; }   // the emulator has no reset: closing it is the closest thing
uint32_t EspClass::getChipId() { return (uint32_t)g_mac[3] << 16 | (uint32_t)g_mac[4] << 8 | g_mac[5]; }

// ---------------------------------------------------------------------------
// Front ends
// ---------------------------------------------------------------------------
enum Frontend { FE_TTY, FE_SDL, FE_HEADLESS };
static Frontend g_frontend = FE_TTY;

// Button state from every source: bit 0 up, 1 down, 2 left, 3 right
static bool g_live[4];                 // SDL key state
static unsigned long g_keyUntil[4];     // terminal: held until this time
static unsigned g_replayMask = 0;
static const unsigned long KEY_HOLD_MS = 180;

static int pinIndex(int pin) {
  return pin == PIN_BUTTON_UP ? 0 : pin == PIN_BUTTON_DOWN ? 1 : pin == PIN_BUTTON_LEFT ? 2 : pin == PIN_BUTTON_RIGHT ? 3 : -1;
}
static unsigned currentMask() {
  unsigned long now = millis();
  unsigned m = g_replayMask;
  for (int i = 0; i < 4; i++) if (g_live[i] || now < g_keyUntil[i]) m |= 1u << i;
  return m;
}
int digitalRead(int pin) {
  int i = pinIndex(pin);
  return (i >= 0 && (currentMask() >> i & 1)) ? LOW : HIGH;
}

// ---- status shown with the screen ----
// ---- persistent EEPROM (--eeprom file) ----
static std::string g_eepromPath;
void emulatorEepromLoad(uint8_t* mem, size_t size) {
  if (g_eepromPath.empty()) return;
  if (FILE* f = fopen(g_eepromPath.c_str(), "rb")) { size_t r = fread(mem, 1, size, f); (void)r; fclose(f); }
}
void emulatorEepromSave(const uint8_t* mem, size_t size) {
  if (g_eepromPath.empty()) return;
  if (FILE* f = fopen(g_eepromPath.c_str(), "wb")) { fwrite(mem, 1, size, f); fclose(f); }
}

// ---- the pretend firmware image and the updater (test mode > UPDATE) ----
static std::vector<uint8_t> g_flashImage;      // what this console "has installed": read for sending
static std::string g_updateOutPath = "received-firmware.bin";
static size_t g_freeSketchSpace = 1019 * 1024;

static void loadFlashImage(const char* path) {
  if (path) {
    if (FILE* f = fopen(path, "rb")) {
      uint8_t buf[4096]; size_t n;
      while ((n = fread(buf, 1, sizeof buf, f)) > 0) g_flashImage.insert(g_flashImage.end(), buf, buf + n);
      fclose(f);
    }
  }
  if (g_flashImage.empty()) {
    // a made-up image: the ESP8266 header (magic 0xE9, segments, flash mode, size and speed) then noise
    g_flashImage.resize(360 * 1024);
    uint32_t x = 2463534242u ^ (uint32_t)g_mac[5] * 2654435761u;
    for (auto& b : g_flashImage) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; b = x; }
    g_flashImage[0] = 0xE9; g_flashImage[1] = 4; g_flashImage[2] = 0x02; g_flashImage[3] = 0x40;
  }
}
uint32_t EspClass::getSketchSize() { return g_flashImage.size(); }
uint32_t EspClass::getFreeSketchSpace() { return g_freeSketchSpace; }
bool EspClass::flashRead(uint32_t address, uint8_t* data, size_t size) {
  if (address + size > g_flashImage.size()) return false;
  memcpy(data, g_flashImage.data() + address, size);
  return true;
}

UpdaterClass Update;
static FILE* g_updateFile = nullptr;
bool UpdaterClass::begin(size_t size, int, int, uint8_t) {
  if (open || size == 0 || size > g_freeSketchSpace) { failed = true; return false; }
  g_updateFile = fopen((g_updateOutPath + ".partial").c_str(), "wb");
  if (!g_updateFile) { failed = true; return false; }
  total = size; written = 0; open = true; failed = false;
  return true;
}
size_t UpdaterClass::write(uint8_t* data, size_t len) {
  if (!open || written + len > total) { failed = true; return 0; }
  fwrite(data, 1, len, g_updateFile);
  written += len;
  return len;
}
bool UpdaterClass::end(bool evenIfRemaining) {
  if (!open) return false;
  fclose(g_updateFile); g_updateFile = nullptr; open = false;
  if (written != total && !evenIfRemaining) { failed = true; remove((g_updateOutPath + ".partial").c_str()); return false; }
  if (written != total) { remove((g_updateOutPath + ".partial").c_str()); return false; }
  rename((g_updateOutPath + ".partial").c_str(), g_updateOutPath.c_str());   // "installed"
  return true;
}

static std::string g_status = "";
static std::string g_message = "";      // short notice (screenshot saved, ...)
static unsigned long g_messageUntil = 0;
static unsigned long g_txCount = 0, g_rxCount = 0;
static int g_i2cMs = 28;                // the real display takes about this long to refresh
static void notify(const std::string& m) { g_message = m; g_messageUntil = millis() + 3500; }

// ---- captures ----
static Frame g_lastFrame;
static GifRecorder g_gif;
static FILE* g_recFile = nullptr;
static unsigned g_recLastMask = 0;
static unsigned g_seed = 0;
static unsigned long g_replayBase = 0;   // millis() when the firmware started: replay and recording times count from here

static std::string stamp() {
  time_t t = time(nullptr);
  char b[32];
  strftime(b, sizeof b, "%Y%m%d-%H%M%S", localtime(&t));
  return b;
}
static void ensureDir(const char* d) { mkdir(d, 0755); }

static void takeScreenshot(const std::string& name = "") {
  ensureDir("shots");
  std::string path = "shots/" + (name.empty() ? "shot-" + stamp() : name) + ".png";
  if (writePng(path.c_str(), g_lastFrame, 4)) notify("saved " + path);
}

static void toggleGif() {
  if (!g_gif.active) {
    g_gif.start();
    notify("recording GIF... press G again to save");
  } else {
    ensureDir("gifs");
    std::string path = "gifs/clip-" + stamp() + ".gif";
    notify(g_gif.stop(path.c_str(), 3) ? "saved " + path : "GIF failed");
  }
}

static void toggleRecording() {
  if (!g_recFile) {
    ensureDir("recordings");
    std::string path = "recordings/input-" + stamp() + ".txt";
    g_recFile = fopen(path.c_str(), "w");
    if (!g_recFile) return;
    fprintf(g_recFile, "# input recording (times in ms since the emulator started); replay with: emulator --replay %s --seed %u\nseed %u\n%lu 0\n", path.c_str(), g_seed, g_seed, millis() - g_replayBase);
    g_recLastMask = 0;
    notify("recording inputs to " + path);
  } else {
    fclose(g_recFile);
    g_recFile = nullptr;
    notify("input recording saved");
  }
}

static void recordMask(unsigned mask) {
  if (g_recFile && mask != g_recLastMask) {
    fprintf(g_recFile, "%lu %u\n", millis() - g_replayBase, mask);
    fflush(g_recFile);
    g_recLastMask = mask;
  }
}

// ---- replay script: "ms mask", "ms shot [name]", "ms gif", "ms quit", "seed N" ----
struct ReplayEvent { unsigned long ms; int kind; unsigned value; std::string text; };   // kind 0 mask, 1 shot, 2 quit, 3 gif
static std::vector<ReplayEvent> g_replay;
static size_t g_replayPos = 0;

static bool loadReplay(const char* path) {
  FILE* f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return false; }
  char line[200];
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#' || line[0] == '\n') continue;
    unsigned seed;
    if (sscanf(line, "seed %u", &seed) == 1) { g_seed = seed; continue; }
    unsigned long ms; char word[40] = "", arg[100] = "";
    int n = sscanf(line, "%lu %39s %99s", &ms, word, arg);
    if (n < 2) continue;
    ReplayEvent e{ms, 0, 0, ""};
    if (!strcmp(word, "shot")) { e.kind = 1; e.text = arg; }
    else if (!strcmp(word, "quit")) e.kind = 2;
    else if (!strcmp(word, "gif")) e.kind = 3;
    else e.value = (unsigned)atoi(word);
    g_replay.push_back(e);
  }
  fclose(f);
  // Events may be written out of order (a screenshot "during" a button hold)
  std::stable_sort(g_replay.begin(), g_replay.end(), [](const ReplayEvent& a, const ReplayEvent& b) { return a.ms < b.ms; });
  return true;
}

static void runReplay() {
  unsigned long t = millis() - g_replayBase;
  while (g_replayPos < g_replay.size() && g_replay[g_replayPos].ms <= t) {
    const ReplayEvent& e = g_replay[g_replayPos++];
    if (e.kind == 0) g_replayMask = e.value;
    else if (e.kind == 1) takeScreenshot(e.text);
    else if (e.kind == 2) g_quit = true;
    else toggleGif();
  }
}

// ---------------------------------------------------------------------------
// Terminal front end
// ---------------------------------------------------------------------------
static termios g_oldTerm;
static bool g_termSaved = false;

static void restoreTerminal() {
  if (!g_termSaved) return;
  tcsetattr(STDIN_FILENO, TCSANOW, &g_oldTerm);
  const char* s = "\x1b[?25h\x1b[?1049l";
  ssize_t r = write(STDOUT_FILENO, s, strlen(s));
  (void)r;
  g_termSaved = false;
}

static void onSignal(int) { g_quit = true; }

static void setupTerminal() {
  tcgetattr(STDIN_FILENO, &g_oldTerm);
  g_termSaved = true;
  termios raw = g_oldTerm;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
  atexit(restoreTerminal);
  const char* s = "\x1b[?1049h\x1b[?25l\x1b[2J";
  ssize_t r = write(STDOUT_FILENO, s, strlen(s));
  (void)r;
}

static void pollTerminalKeys() {
  unsigned char buf[64];
  ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
  unsigned long until = millis() + KEY_HOLD_MS;
  for (ssize_t i = 0; i < n; i++) {
    int k = -1;
    unsigned char c = buf[i];
    if (c == 0x1b && i + 2 < n && buf[i + 1] == '[') {
      switch (buf[i + 2]) { case 'A': k = 0; break; case 'B': k = 1; break; case 'D': k = 2; break; case 'C': k = 3; break; }
      i += 2;
    } else if (c == 'w' || c == 'W') k = 0;
    else if (c == 's' || c == 'S') k = 1;
    else if (c == 'a' || c == 'A') k = 2;
    else if (c == 'd' || c == 'D') k = 3;
    else if (c == 'p' || c == 'P') takeScreenshot();
    else if (c == 'g' || c == 'G') toggleGif();
    else if (c == 'r' || c == 'R') toggleRecording();
    else if (c == 'q' || c == 'Q' || c == 3) g_quit = true;
    if (k >= 0) g_keyUntil[k] = until;
  }
}

// 2x4 pixels per braille character: the whole screen fits in 64x16 characters
static void drawTerminal(const Frame fb) {
  static unsigned long lastDraw = 0;
  unsigned long now = millis();
  if (now - lastDraw < 30) return;
  lastDraw = now;

  static const int bit[4][2] = {{0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};
  std::string out = "\x1b[H";
  out += "  +" + std::string(64, '-') + "+\x1b[K\n";
  for (int cy = 0; cy < 16; cy++) {
    out += "  |";
    for (int cx = 0; cx < 64; cx++) {
      int code = 0;
      for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 2; dx++)
          if (fb[cy * 4 + dy][cx * 2 + dx]) code |= bit[dy][dx];
      uint32_t cp = 0x2800 + code;
      out += (char)(0xE0 | (cp >> 12));
      out += (char)(0x80 | ((cp >> 6) & 0x3F));
      out += (char)(0x80 | (cp & 0x3F));
    }
    out += "|\x1b[K\n";
  }
  out += "  +" + std::string(64, '-') + "+\x1b[K\n";
  char line[300];
  snprintf(line, sizeof line, "  %s\x1b[K\n  arrows/WASD buttons   P shot   G gif   R record inputs   Q quit    sent %lu, received %lu\x1b[K\n  %s\x1b[K\n",
           g_status.c_str(), g_txCount, g_rxCount, now < g_messageUntil ? g_message.c_str() : "");
  out += line;
  ssize_t r = write(STDOUT_FILENO, out.data(), out.size());
  (void)r;
}

// ---------------------------------------------------------------------------
// SDL front end
// ---------------------------------------------------------------------------
#ifdef USE_SDL
static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture* g_texture = nullptr;
static bool g_grid = true;
static const int SDL_SCALE = 6;

static bool setupSdl() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return false; }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  g_window = SDL_CreateWindow("PixelOS console emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              128 * SDL_SCALE, 64 * SDL_SCALE, SDL_WINDOW_RESIZABLE);
  if (!g_window) return false;
  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
  if (!g_renderer) return false;
  SDL_RenderSetLogicalSize(g_renderer, 128 * SDL_SCALE, 64 * SDL_SCALE);
  g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 128, 64);
  return g_texture != nullptr;
}

static void pollSdlKeys() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) g_quit = true;
    if (e.type != SDL_KEYDOWN && e.type != SDL_KEYUP) continue;
    bool down = e.type == SDL_KEYDOWN;
    int k = -1;
    switch (e.key.keysym.sym) {
      case SDLK_UP: case SDLK_w: k = 0; break;
      case SDLK_DOWN: case SDLK_s: k = 1; break;
      case SDLK_LEFT: case SDLK_a: k = 2; break;
      case SDLK_RIGHT: case SDLK_d: k = 3; break;
      case SDLK_q: case SDLK_ESCAPE: if (down) g_quit = true; break;
      case SDLK_p: case SDLK_F12: if (down && !e.key.repeat) takeScreenshot(); break;
      case SDLK_g: if (down && !e.key.repeat) toggleGif(); break;
      case SDLK_r: if (down && !e.key.repeat) toggleRecording(); break;
      case SDLK_h: if (down && !e.key.repeat) g_grid = !g_grid; break;
    }
    if (k >= 0) g_live[k] = down;
  }
}

static void drawSdl(const Frame fb) {
  static unsigned long lastDraw = 0, lastTitle = 0;
  unsigned long now = millis();
  if (now - lastDraw < 14) return;
  lastDraw = now;

  uint8_t pixels[64][128][3];
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++) memcpy(pixels[y][x], fb[y][x] ? PIXEL_ON : PIXEL_OFF, 3);
  SDL_UpdateTexture(g_texture, nullptr, pixels, 128 * 3);
  SDL_RenderClear(g_renderer);
  SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
  if (g_grid) {   // thin dark lines between the pixels, like a real OLED
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 110);
    for (int x = 1; x < 128; x++) SDL_RenderDrawLine(g_renderer, x * SDL_SCALE, 0, x * SDL_SCALE, 64 * SDL_SCALE);
    for (int y = 1; y < 64; y++) SDL_RenderDrawLine(g_renderer, 0, y * SDL_SCALE, 128 * SDL_SCALE, y * SDL_SCALE);
  }
  SDL_RenderPresent(g_renderer);

  if (now - lastTitle > 500) {
    lastTitle = now;
    char title[300];
    snprintf(title, sizeof title, "PixelOS emulator | %s | %s%s", g_status.c_str(), now < g_messageUntil ? g_message.c_str() : "P shot  G gif  R record  H grid  Q quit", g_gif.active ? "  [REC GIF]" : "");
    SDL_SetWindowTitle(g_window, title);
  }
}
#endif

// ---------------------------------------------------------------------------
// The display hook: called whenever the firmware refreshes the screen
// ---------------------------------------------------------------------------
void emulatorPresent(const uint8_t fbIn[64][128], bool inverted) {
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++) g_lastFrame[y][x] = fbIn[y][x] ^ (inverted ? 1 : 0);

  // EMULATOR_DUMP=file: also write the latest frame as text (for scripted tests)
  if (const char* dump = getenv("EMULATOR_DUMP")) {
    if (FILE* f = fopen(dump, "w")) {
      for (int y = 0; y < 64; y += 2) {
        for (int x = 0; x < 128; x++) fputs(g_lastFrame[y][x] ? (g_lastFrame[y + 1][x] ? "\xe2\x96\x88" : "\xe2\x96\x80") : (g_lastFrame[y + 1][x] ? "\xe2\x96\x84" : " "), f);
        fputc('\n', f);
      }
      fclose(f);
    }
  }

  g_gif.add(g_lastFrame, millis());
  if (g_frontend == FE_TTY) drawTerminal(g_lastFrame);
#ifdef USE_SDL
  else if (g_frontend == FE_SDL) drawSdl(g_lastFrame);
#endif
  if (g_i2cMs) delay(g_i2cMs);
}

static void pollInput() {
  if (g_frontend == FE_TTY) pollTerminalKeys();
#ifdef USE_SDL
  else if (g_frontend == FE_SDL) pollSdlKeys();
#endif
  if (!g_replay.empty()) runReplay();
  recordMask(currentMask());
}

// ---------------------------------------------------------------------------
// Radio: serial bridge or UDP loopback
// ---------------------------------------------------------------------------
static esp_now_recv_cb_t g_recvCb = nullptr;
static int g_serialFd = -1, g_udpSock = -1, g_udpIndex = 0, g_udpPort = 40000;
static std::string g_serialBuf;
static bool g_haveMac = false;

static void serialWriteFrame(const uint8_t* payload, size_t n) {
  uint8_t frame[260];
  frame[0] = 0xA5;
  frame[1] = n;
  memcpy(frame + 2, payload, n);
  uint8_t ck = n;
  for (size_t i = 0; i < n; i++) ck ^= payload[i];
  frame[2 + n] = ck;
  ssize_t r = write(g_serialFd, frame, n + 3);
  (void)r;
}

static void handleBridgeFrame(const uint8_t* p, size_t n) {
  if (n >= 7 && p[0] == 'M') {
    memcpy(g_mac, p + 1, 6);
    g_haveMac = true;
  } else if (n >= 7 && p[0] == 'R' && g_recvCb) {
    uint8_t src[6];
    memcpy(src, p + 1, 6);
    uint8_t data[260];
    memcpy(data, p + 7, n - 7);
    g_rxCount++;
    g_recvCb(src, data, n - 7);
  }
}

static void pumpSerial() {
  uint8_t buf[512];
  ssize_t n;
  while ((n = read(g_serialFd, buf, sizeof buf)) > 0) g_serialBuf.append((char*)buf, n);

  for (;;) {
    size_t s = g_serialBuf.find((char)0xA5);
    if (s == std::string::npos) { g_serialBuf.clear(); return; }
    g_serialBuf.erase(0, s);
    if (g_serialBuf.size() < 3) return;
    size_t len = (uint8_t)g_serialBuf[1];
    if (g_serialBuf.size() < len + 3) return;
    uint8_t ck = len;
    for (size_t i = 0; i < len; i++) ck ^= (uint8_t)g_serialBuf[2 + i];
    if (ck == (uint8_t)g_serialBuf[2 + len]) {
      handleBridgeFrame((const uint8_t*)g_serialBuf.data() + 2, len);
      g_serialBuf.erase(0, len + 3);
    } else {
      g_serialBuf.erase(0, 1);   // damaged: look for the next frame start
    }
  }
}

static bool openBridge(const char* dev) {
  g_serialFd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (g_serialFd < 0) { fprintf(stderr, "cannot open %s\n", dev); return false; }
  termios t;
  tcgetattr(g_serialFd, &t);
  cfmakeraw(&t);
  cfsetspeed(&t, B115200);
  t.c_cflag |= CLOCAL | CREAD;
  tcsetattr(g_serialFd, TCSANOW, &t);
  int bits = TIOCM_DTR | TIOCM_RTS;
  ioctl(g_serialFd, TIOCMBIC, &bits);   // opening the port can reset the board; let it settle
  fprintf(stderr, "waiting for the bridge on %s ...\n", dev);
  unsigned long start = millis();
  usleep(1500000);
  tcflush(g_serialFd, TCIOFLUSH);
  while (!g_haveMac && millis() - start < 12000) {
    uint8_t ask = 'M';
    serialWriteFrame(&ask, 1);
    usleep(300000);
    pumpSerial();
  }
  if (!g_haveMac) { fprintf(stderr, "the bridge did not answer (is the bridge sketch flashed?)\n"); return false; }
  return true;
}

static void udpEmit(int port, const uint8_t* buf, int n) {
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = inet_addr("127.0.0.1");
  a.sin_port = htons(port);
  sendto(g_udpSock, buf, n, 0, (sockaddr*)&a, sizeof a);
}

static void pumpUdp() {
  uint8_t buf[300];
  int n;
  while ((n = recv(g_udpSock, buf, sizeof buf, 0)) > 6) {
    if (g_recvCb) { g_rxCount++; g_recvCb(buf, buf + 6, n - 6); }
  }
}

static void airPump() {
  if (g_serialFd >= 0) pumpSerial();
  else if (g_udpSock >= 0) pumpUdp();
}

int esp_now_init() { return 0; }
int esp_now_deinit() { return 0; }
int esp_now_set_self_role(u8) { return 0; }
int esp_now_register_recv_cb(esp_now_recv_cb_t cb) { g_recvCb = cb; return 0; }
int esp_now_unregister_recv_cb() { g_recvCb = nullptr; return 0; }
static esp_now_send_cb_t g_sendCb = nullptr;
int esp_now_register_send_cb(esp_now_send_cb_t cb) { g_sendCb = cb; return 0; }
int esp_now_add_peer(u8*, u8, u8, u8*, u8) { return 0; }

int esp_now_send(u8* da, u8* data, int len) {
  g_txCount++;
  if (g_serialFd >= 0) {
    uint8_t payload[260];
    payload[0] = 'T';
    memcpy(payload + 1, da, 6);
    memcpy(payload + 7, data, len);
    serialWriteFrame(payload, 7 + len);
  } else if (g_udpSock >= 0) {
    uint8_t buf[300];
    memcpy(buf, g_mac, 6);
    memcpy(buf + 6, data, len);
    udpEmit(g_udpPort + (1 - g_udpIndex), buf, 6 + len);
  }
  if (g_sendCb) g_sendCb(da, 0);   // the real radio reports through this callback
  return 0;
}

// ---------------------------------------------------------------------------
// main: the same sequence as GameSystem.ino
// ---------------------------------------------------------------------------
void (*g_yieldHook)() = nullptr;

int main(int argc, char** argv) {
  g_yieldHook = airPump;
  clock_gettime(CLOCK_MONOTONIC, &g_t0);
  g_seed = (unsigned)(time(nullptr) ^ getpid());
  const char* bridge = nullptr;
  const char* replay = nullptr;
  const char* firmwarePath = nullptr;
  int udp = -1;
  bool wantSeed = false;
  unsigned seedArg = 0;
  bool forceTty = false, forceSdl = false, headless = false, skipTutorial = false;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--bridge") && i + 1 < argc) bridge = argv[++i];
    else if (!strcmp(argv[i], "--udp") && i + 1 < argc) udp = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--fast")) g_i2cMs = 0;
    else if (!strcmp(argv[i], "--speed") && i + 1 < argc) g_timeScale = atof(argv[++i]);
    else if (!strcmp(argv[i], "--seed") && i + 1 < argc) { seedArg = atoi(argv[++i]); wantSeed = true; }
    else if (!strcmp(argv[i], "--replay") && i + 1 < argc) replay = argv[++i];
    else if (!strcmp(argv[i], "--eeprom") && i + 1 < argc) g_eepromPath = argv[++i];
    else if (!strcmp(argv[i], "--firmware") && i + 1 < argc) firmwarePath = argv[++i];
    else if (!strcmp(argv[i], "--update-out") && i + 1 < argc) g_updateOutPath = argv[++i];
    else if (!strcmp(argv[i], "--free-space") && i + 1 < argc) g_freeSketchSpace = (size_t)atol(argv[++i]);
    else if (!strcmp(argv[i], "--no-tutorial")) skipTutorial = true;
    else if (!strcmp(argv[i], "--tty")) forceTty = true;
    else if (!strcmp(argv[i], "--sdl")) forceSdl = true;
    else if (!strcmp(argv[i], "--headless")) headless = true;
    else {
      fprintf(stderr, "usage: emulator [--sdl | --tty | --headless] [--bridge /dev/ttyACM0 | --udp 0|1]\n"
                      "                [--fast] [--speed N] [--seed N] [--replay file] [--eeprom file] [--firmware file] [--update-out file] [--no-tutorial]\n");
      return 1;
    }
  }

  if (replay && !loadReplay(replay)) return 1;
  if (wantSeed) g_seed = seedArg;
  srand(g_seed);

  // Pick the front end
  if (headless) g_frontend = FE_HEADLESS;
  else if (forceTty) g_frontend = FE_TTY;
#ifdef USE_SDL
  else if (forceSdl || getenv("WAYLAND_DISPLAY") || getenv("DISPLAY")) g_frontend = FE_SDL;
#endif
  else g_frontend = FE_TTY;
  (void)forceSdl;

  static char status[160];
  if (bridge) {
    if (!openBridge(bridge)) return 1;
    snprintf(status, sizeof status, "radio: bridge %s, MAC %02X:%02X:%02X:%02X:%02X:%02X", bridge, g_mac[0], g_mac[1], g_mac[2], g_mac[3], g_mac[4], g_mac[5]);
  } else if (udp >= 0) {
    g_udpIndex = udp & 1;
    g_mac[5] = 0x10 + g_udpIndex;
    g_mac[4] = rand();
    g_udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    a.sin_port = htons(g_udpPort + g_udpIndex);
    if (bind(g_udpSock, (sockaddr*)&a, sizeof a) < 0) { perror("bind"); return 1; }
    fcntl(g_udpSock, F_SETFL, O_NONBLOCK);
    snprintf(status, sizeof status, "radio: UDP loopback, emulator %d", g_udpIndex);
  } else {
    snprintf(status, sizeof status, "radio: none (--bridge or --udp for Pong versus)");
  }
  g_status = status;

  loadFlashImage(firmwarePath);   // after the radio has set the MAC, which seeds the made-up image
  if (g_frontend == FE_TTY) setupTerminal();
#ifdef USE_SDL
  if (g_frontend == FE_SDL && !setupSdl()) { fprintf(stderr, "no SDL display; use --tty or --headless\n"); return 1; }
#endif
  signal(SIGINT, onSignal);
  signal(SIGTERM, onSignal);

  initDisplay();
  initInput();
  g_replayBase = millis();
  initHighscores();
  if (!isFirstBoot()) showSplash();
  syncInit();
  initGameManager();
  if (skipTutorial) gameManager.skipTutorialQuietly();   // the emulator has no saved first-boot flag

  while (!g_quit) {
    pollInput();
    airPump();
    updateInput();
    updateGameManager();
    delay(16);
  }

  if (g_gif.active) toggleGif();
  if (g_recFile) fclose(g_recFile);
#ifdef USE_SDL
  if (g_frontend == FE_SDL) SDL_Quit();
#endif
  return 0;
}
