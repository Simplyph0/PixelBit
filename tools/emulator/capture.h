// Screenshot (PNG) and animated GIF writers for the emulator, with no
// dependencies. Frames are 128x64 arrays of 0/1.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>

typedef uint8_t Frame[64][128];

static const uint8_t PIXEL_ON[3] = {0xB8, 0xF4, 0xFF};   // the lit pixels: a cool white like the OLED
static const uint8_t PIXEL_OFF[3] = {0x00, 0x00, 0x00};

// ---- PNG: 8-bit grayscale scaled up, deflate "stored" blocks (no compression) ----
inline uint32_t capCrc(const uint8_t* d, size_t n, uint32_t crc = 0xFFFFFFFF) {
  static uint32_t table[256];
  static bool ready = false;
  if (!ready) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320 ^ (c >> 1) : c >> 1;
      table[i] = c;
    }
    ready = true;
  }
  for (size_t i = 0; i < n; i++) crc = table[(crc ^ d[i]) & 0xFF] ^ (crc >> 8);
  return crc;
}

inline void capPutBE(std::string& s, uint32_t v) {
  s += (char)(v >> 24); s += (char)(v >> 16); s += (char)(v >> 8); s += (char)v;
}

inline void capChunk(FILE* f, const char* type, const std::string& data) {
  std::string head;
  capPutBE(head, data.size());
  fwrite(head.data(), 1, 4, f);
  std::string body = std::string(type, 4) + data;
  fwrite(body.data(), 1, body.size(), f);
  std::string crc;
  capPutBE(crc, ~capCrc((const uint8_t*)body.data(), body.size()));
  fwrite(crc.data(), 1, 4, f);
}

inline bool writePng(const char* path, const Frame fb, int scale) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  int w = 128 * scale, h = 64 * scale;
  static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  fwrite(sig, 1, 8, f);

  std::string ihdr;
  capPutBE(ihdr, w); capPutBE(ihdr, h);
  ihdr += (char)8; ihdr += (char)0; ihdr += (char)0; ihdr += (char)0; ihdr += (char)0;   // 8-bit grayscale
  capChunk(f, "IHDR", ihdr);

  std::string raw;
  raw.reserve((size_t)(w + 1) * h);
  for (int y = 0; y < h; y++) {
    raw += (char)0;   // filter: none
    for (int x = 0; x < w; x++) raw += (char)(fb[y / scale][x / scale] ? 0xF0 : 0x00);
  }

  std::string z;
  z += (char)0x78; z += (char)0x01;
  size_t pos = 0;
  while (pos < raw.size()) {
    size_t n = raw.size() - pos;
    if (n > 65535) n = 65535;
    bool last = pos + n == raw.size();
    z += (char)(last ? 1 : 0);
    z += (char)(n & 0xFF); z += (char)(n >> 8); z += (char)(~n & 0xFF); z += (char)((~n >> 8) & 0xFF);
    z.append(raw, pos, n);
    pos += n;
  }
  uint32_t a = 1, b = 0;
  for (unsigned char c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  capPutBE(z, (b << 16) | a);
  capChunk(f, "IDAT", z);
  capChunk(f, "IEND", "");
  fclose(f);
  return true;
}

// ---- GIF: 2 colours, LZW, frames kept in memory until the recording stops ----
struct GifRecorder {
  std::vector<std::vector<uint8_t>> frames;   // 128*64 bytes each
  std::vector<unsigned> times;                // ms between a frame and the next
  unsigned long lastAt = 0;
  bool active = false;

  void start() { frames.clear(); times.clear(); active = true; lastAt = 0; }
  void add(const Frame fb, unsigned long nowMs) {
    if (!active) return;
    if (!frames.empty()) times.back() = nowMs - lastAt;
    lastAt = nowMs;
    frames.emplace_back(128 * 64);
    for (int y = 0; y < 64; y++) memcpy(&frames.back()[y * 128], fb[y], 128);
    times.push_back(40);
  }

  static void lzw(const std::vector<uint8_t>& px, std::string& out) {
    const int minCode = 2;
    out += (char)minCode;
    std::string bits;   // sub-block payload being built
    uint32_t acc = 0; int nacc = 0;
    int codeSize = minCode + 1;
    const int clear = 1 << minCode, eoi = clear + 1;
    int next = eoi + 1;
    std::unordered_map<uint32_t, int> dict;
    std::string data;

    auto emit = [&](int code) {
      acc |= (uint32_t)code << nacc; nacc += codeSize;
      while (nacc >= 8) { data += (char)(acc & 0xFF); acc >>= 8; nacc -= 8; }
    };

    emit(clear);
    int prefix = px[0];
    for (size_t i = 1; i < px.size(); i++) {
      uint32_t key = ((uint32_t)prefix << 8) | px[i];
      auto it = dict.find(key);
      if (it != dict.end()) { prefix = it->second; continue; }
      emit(prefix);
      if (next < 4096) {
        dict[key] = next++;
        if (next > (1 << codeSize) && codeSize < 12) codeSize++;
      } else {
        emit(clear);
        dict.clear();
        next = eoi + 1;
        codeSize = minCode + 1;
      }
      prefix = px[i];
    }
    emit(prefix);
    emit(eoi);
    if (nacc > 0) data += (char)(acc & 0xFF);

    for (size_t i = 0; i < data.size(); i += 255) {
      size_t n = data.size() - i < 255 ? data.size() - i : 255;
      out += (char)n;
      out.append(data, i, n);
    }
    out += (char)0;
  }

  bool stop(const char* path, int scale) {
    active = false;
    if (frames.empty()) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    int w = 128 * scale, h = 64 * scale;
    std::string out = "GIF89a";
    out += (char)(w & 0xFF); out += (char)(w >> 8); out += (char)(h & 0xFF); out += (char)(h >> 8);
    out += (char)0x80; out += (char)0; out += (char)0;
    out.append((const char*)PIXEL_OFF, 3);
    out.append((const char*)PIXEL_ON, 3);
    out += "\x21\xFF\x0BNETSCAPE2.0\x03\x01"; out += (char)0; out += (char)0; out += (char)0;   // loop forever

    for (size_t k = 0; k < frames.size(); k++) {
      unsigned delay = (times[k] + 5) / 10;
      if (delay < 2) delay = 2;
      out += "\x21\xF9\x04"; out += (char)0x00; out += (char)(delay & 0xFF); out += (char)(delay >> 8); out += (char)0; out += (char)0;
      out += (char)0x2C; out += (char)0; out += (char)0; out += (char)0; out += (char)0;
      out += (char)(w & 0xFF); out += (char)(w >> 8); out += (char)(h & 0xFF); out += (char)(h >> 8); out += (char)0;
      std::vector<uint8_t> px((size_t)w * h);
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) px[(size_t)y * w + x] = frames[k][(y / scale) * 128 + x / scale];
      lzw(px, out);
    }
    out += (char)0x3B;
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    frames.clear();
    return true;
  }
};
