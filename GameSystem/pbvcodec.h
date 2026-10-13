#ifndef PBVCODEC_H
#define PBVCODEC_H

#include <stdint.h>
#include <stddef.h>

// The .pbv video format: black and white, no sound, made by tools/video/pbv.py.
// Little endian.
//
//   header (12 bytes)   "PBV1" | width u8 | height u8 | scale u8 | fps u8 | frames u16 | keyEvery u16
//   key table           one u32 per key frame: the file offset of that frame's record
//   frame records       u16 length | payload, one per frame
//
// width and height are stored pixels; every stored pixel is drawn scale x scale
// screen pixels (so a larger scale is easier to see and a smaller file), centred
// on the 128x64 screen. A frame is stored the way the OLED wants it: pages of 8
// rows, width bytes per page, bit 0 at the top. Every keyEvery-th frame is a key
// frame (the picture itself); the others are the XOR with the frame before. The
// payload is run-length coded (PackBits style): a byte below 128 is followed by
// that many plus one literal bytes, a byte from 128 up repeats the next byte
// (byte - 128 + 2) times.

#define PBV_HEADER_SIZE 12
#define PBV_MAX_FRAME_BYTES 1024    // 128 wide, 8 pages
#define PBV_MAX_PAYLOAD (PBV_MAX_FRAME_BYTES + PBV_MAX_FRAME_BYTES / 128 + 2)

struct PbvHeader {
  uint8_t width, height, scale, fps;
  uint16_t frames, keyEvery;
  uint8_t pages;           // rows of 8 pixels: ceil(height / 8)
  uint16_t frameBytes;     // width * pages
  uint16_t keyCount;       // key frames in the file
  uint32_t dataOffset() const { return PBV_HEADER_SIZE + 4UL * keyCount; }   // first frame record if the key table is skipped
};

// false if the bytes are not a header this player can play
bool pbvParseHeader(const uint8_t* bytes, size_t n, PbvHeader& h);

// Decodes one frame payload into out (h.frameBytes bytes). A key frame replaces
// out, a delta frame is XORed onto what is in it. false if the payload is damaged.
bool pbvDecodeFrame(const uint8_t* payload, uint16_t len, uint8_t* out, uint16_t frameBytes, bool delta);

#endif
