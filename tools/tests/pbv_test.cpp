// Checks the console's video decoder (GameSystem/pbvcodec.cpp) against the
// converter: pbv_test clip.pbv frames.raw, where frames.raw is every frame the
// converter encoded (pbv.py --dump-raw).
#include "pbvcodec.h"
#include <cstdio>
#include <cstring>
#include <vector>

static std::vector<uint8_t> slurp(const char* path) {
  std::vector<uint8_t> v;
  if (FILE* f = fopen(path, "rb")) {
    uint8_t b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) v.insert(v.end(), b, b + n);
    fclose(f);
  }
  return v;
}
static int fails = 0;
static void check(bool ok, const char* what) { if (!ok) { printf("FAIL: %s\n", what); fails++; } }

int main(int argc, char** argv) {
  if (argc < 3) { printf("usage: pbv_test clip.pbv frames.raw\n"); return 2; }
  std::vector<uint8_t> clip = slurp(argv[1]), raw = slurp(argv[2]);
  PbvHeader h;
  check(pbvParseHeader(clip.data(), clip.size(), h), "the header is accepted");
  if (fails) return 1;
  check(raw.size() == (size_t)h.frames * h.frameBytes, "the reference has every frame");

  auto u32 = [&](size_t at) { return clip[at] | (clip[at + 1] << 8) | (clip[at + 2] << 16) | ((uint32_t)clip[at + 3] << 24); };

  // every frame, in order, as the player decodes it
  std::vector<uint8_t> cur(h.frameBytes, 0);
  size_t pos = h.dataOffset();
  int bad = 0, badKey = 0;
  for (int i = 0; i < h.frames && pos + 2 <= clip.size(); i++) {
    uint16_t len = clip[pos] | (clip[pos + 1] << 8);
    if (i % h.keyEvery == 0 && u32(PBV_HEADER_SIZE + 4 * (i / h.keyEvery)) != pos) badKey++;
    if (pos + 2 + len > clip.size() || !pbvDecodeFrame(&clip[pos + 2], len, cur.data(), h.frameBytes, i % h.keyEvery != 0)) { bad++; break; }
    if (memcmp(cur.data(), &raw[(size_t)i * h.frameBytes], h.frameBytes) != 0) bad++;
    pos += 2 + len;
  }
  check(bad == 0, "every frame decodes to what the converter encoded");
  check(badKey == 0, "the key table points at the key frames");
  check(pos == clip.size(), "the frames end exactly at the end of the file");

  // seeking: a key frame decodes on its own
  int badSeek = 0;
  for (int k = 0; k < h.keyCount; k++) {
    size_t at = u32(PBV_HEADER_SIZE + 4 * k);
    uint16_t len = clip[at] | (clip[at + 1] << 8);
    std::vector<uint8_t> fresh(h.frameBytes, 0x5A);   // garbage: a key frame must not depend on it
    if (!pbvDecodeFrame(&clip[at + 2], len, fresh.data(), h.frameBytes, false) ||
        memcmp(fresh.data(), &raw[(size_t)k * h.keyEvery * h.frameBytes], h.frameBytes) != 0) badSeek++;
  }
  check(badSeek == 0, "every key frame decodes on its own");

  // damaged data is refused, not played
  {
    uint16_t len = clip[h.dataOffset()] | (clip[h.dataOffset() + 1] << 8);
    uint8_t* p = &clip[h.dataOffset() + 2];
    std::vector<uint8_t> out(h.frameBytes);
    check(!pbvDecodeFrame(p, len - 1, out.data(), h.frameBytes, false), "a cut-off frame is refused");
    check(!pbvDecodeFrame(p, len, out.data(), h.frameBytes - 1, false), "a frame that is too long is refused");
    uint8_t twice[PBV_MAX_PAYLOAD * 2];
    memcpy(twice, p, len); memcpy(twice + len, p, len);
    check(!pbvDecodeFrame(twice, len * 2, out.data(), h.frameBytes, false), "a frame with extra data is refused");
  }
  {
    uint8_t head[PBV_HEADER_SIZE]; memcpy(head, clip.data(), sizeof head);
    PbvHeader t;
    check(pbvParseHeader(head, sizeof head, t), "an untouched header passes");
    head[0] = 'X'; check(!pbvParseHeader(head, sizeof head, t), "a wrong magic is refused");
    memcpy(head, clip.data(), sizeof head); head[6] = 4; head[4] = 64; check(!pbvParseHeader(head, sizeof head, t), "wider than the screen is refused");
    memcpy(head, clip.data(), sizeof head); head[7] = 0; check(!pbvParseHeader(head, sizeof head, t), "0 fps is refused");
    memcpy(head, clip.data(), sizeof head); head[8] = head[9] = 0; check(!pbvParseHeader(head, sizeof head, t), "no frames is refused");
    check(!pbvParseHeader(head, 5, t), "a short header is refused");
  }

  if (fails) { printf("%d CHECKS FAILED\n", fails); return 1; }
  printf("ok: %d frames of %dx%d (x%d), %d key frames, %zu bytes\n", h.frames, h.width, h.height, h.scale, h.keyCount, clip.size());
  return 0;
}
