// The parts of platform.h that the standard library does portably.
#include "platform.h"
#include <chrono>
#include <filesystem>
#include <thread>

namespace plat {

void sleepUs(unsigned long long us) {
  std::this_thread::sleep_for(std::chrono::microseconds(us));
}

double monotonicMs() {
  using namespace std::chrono;
  static const steady_clock::time_point start = steady_clock::now();
  return duration<double, std::milli>(steady_clock::now() - start).count();
}

void makeDir(const char* path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
}

bool replaceFile(const char* from, const char* to) {
  std::error_code ec;
  std::filesystem::rename(from, to, ec);
  if (!ec) return true;
  // Some systems refuse to rename over an existing file
  std::filesystem::remove(to, ec);
  std::filesystem::rename(from, to, ec);
  return !ec;
}

}  // namespace plat
