#pragma once
// The ESP8266's LittleFS, on a folder of this computer: the emulator's
// "flash file system". The folder is PB_FS (environment variable), default
// "pixelbit-fs" in the current directory. Put a .pbv in <folder>/clips/ and the
// video player sees it. Only what the firmware uses is here.
#include "Arduino.h"
#include <filesystem>
#include <memory>
#include <stdio.h>
#include <string>
#include <vector>

namespace fs {

enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };

struct FSInfo {
  size_t totalBytes, usedBytes, blockSize, pageSize, maxOpenFiles, maxPathLength;
};

class File {
public:
  File() {}
  File(FILE* f, const std::string& n) : fp(f, [](FILE* p) { if (p) fclose(p); }), nm(n) {}
  explicit operator bool() const { return (bool)fp; }
  size_t read(uint8_t* buf, size_t n) { return fp ? fread(buf, 1, n, fp.get()) : 0; }
  size_t write(const uint8_t* buf, size_t n) { return fp ? fwrite(buf, 1, n, fp.get()) : 0; }
  bool seek(uint32_t pos, SeekMode mode = SeekSet) { return fp && fseek(fp.get(), (long)pos, mode == SeekSet ? SEEK_SET : mode == SeekCur ? SEEK_CUR : SEEK_END) == 0; }
  size_t position() const { return fp ? (size_t)ftell(fp.get()) : 0; }
  size_t size() {
    if (!fp) return 0;
    long cur = ftell(fp.get());
    fseek(fp.get(), 0, SEEK_END);
    long end = ftell(fp.get());
    fseek(fp.get(), cur, SEEK_SET);
    return (size_t)end;
  }
  int available() { return fp ? (int)(size() - position()) : 0; }
  void close() { fp.reset(); }
  const char* name() const { return nm.c_str(); }
private:
  std::shared_ptr<FILE> fp;
  std::string nm;
};

class Dir {
public:
  struct Entry { std::string name; size_t size; };
  std::vector<Entry> entries;
  int at = -1;
  bool next() { return ++at < (int)entries.size(); }
  String fileName() { return String(entries[at].name); }
  size_t fileSize() { return entries[at].size; }
};

class FS {
public:
  bool begin() { std::error_code ec; std::filesystem::create_directories(root(), ec); return !ec; }
  bool format() { return true; }
  bool exists(const char* path) { std::error_code ec; return std::filesystem::exists(real(path), ec); }
  File open(const char* path, const char* mode) {
    bool write = mode[0] == 'w' || mode[0] == 'a';
    FILE* f = fopen(real(path).c_str(), write ? (mode[0] == 'a' ? "ab" : "wb") : "rb");
    return f ? File(f, std::filesystem::path(path).filename().string()) : File();
  }
  bool remove(const char* path) { std::error_code ec; return std::filesystem::remove(real(path), ec); }
  bool rename(const char* from, const char* to) { std::error_code ec; std::filesystem::rename(real(from), real(to), ec); return !ec; }
  bool mkdir(const char* path) { std::error_code ec; std::filesystem::create_directories(real(path), ec); return !ec; }
  Dir openDir(const char* path) {
    Dir d;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(real(path), ec))
      if (e.is_regular_file(ec)) d.entries.push_back({e.path().filename().string(), (size_t)e.file_size(ec)});
    return d;
  }
  bool info(FSInfo& i) {
    i = {1024 * 1024, 0, 8192, 256, 5, 31};
    std::error_code ec;
    for (auto& e : std::filesystem::recursive_directory_iterator(root(), ec))
      if (e.is_regular_file(ec)) i.usedBytes += (size_t)e.file_size(ec);
    return true;
  }
private:
  static std::string root() { const char* r = getenv("PB_FS"); return r && *r ? r : "pixelbit-fs"; }
  static std::string real(const char* path) { return root() + (path[0] == '/' ? "" : "/") + path; }
};

}  // namespace fs

using fs::Dir;
using fs::File;
using fs::FSInfo;
using fs::SeekCur;
using fs::SeekEnd;
using fs::SeekMode;
using fs::SeekSet;

inline fs::FS LittleFS;
