#include "pbvcodec.h"

bool pbvParseHeader(const uint8_t* b, size_t n, PbvHeader& h) {
  if (n < PBV_HEADER_SIZE) return false;
  if (b[0] != 'P' || b[1] != 'B' || b[2] != 'V' || b[3] != '1') return false;
  h.width = b[4];
  h.height = b[5];
  h.scale = b[6];
  h.fps = b[7];
  h.frames = b[8] | (b[9] << 8);
  h.keyEvery = b[10] | (b[11] << 8);

  if (h.width == 0 || h.height == 0 || h.scale == 0) return false;
  if (h.width * h.scale > 128 || h.height * h.scale > 64) return false;
  if (h.fps == 0 || h.fps > 30) return false;
  if (h.frames == 0 || h.keyEvery == 0) return false;

  h.pages = (h.height + 7) / 8;
  h.frameBytes = (uint16_t)h.width * h.pages;
  h.keyCount = (h.frames + h.keyEvery - 1) / h.keyEvery;
  return h.frameBytes <= PBV_MAX_FRAME_BYTES;
}

bool pbvDecodeFrame(const uint8_t* p, uint16_t len, uint8_t* out, uint16_t frameBytes, bool delta) {
  uint16_t i = 0, o = 0;
  while (i < len) {
    uint8_t c = p[i++];
    if (c < 128) {                       // literal bytes
      uint16_t n = c + 1;
      if (i + n > len || o + n > frameBytes) return false;
      if (delta) for (uint16_t k = 0; k < n; k++) out[o + k] ^= p[i + k];
      else       for (uint16_t k = 0; k < n; k++) out[o + k] = p[i + k];
      i += n;
      o += n;
    } else {                             // one byte, repeated
      uint16_t n = c - 128 + 2;
      if (i >= len || o + n > frameBytes) return false;
      uint8_t v = p[i++];
      if (delta) for (uint16_t k = 0; k < n; k++) out[o + k] ^= v;
      else       for (uint16_t k = 0; k < n; k++) out[o + k] = v;
      o += n;
    }
  }
  return o == frameBytes;
}
