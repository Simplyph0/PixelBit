#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// SHA-256 and HMAC-SHA-256, written out here so the link code does not depend
// on a particular core version's hashing library.
struct Sha256 {
  uint32_t h[8];
  uint8_t buf[64];
  uint32_t total;   // bytes hashed so far (messages here are far below 4 GB)
  uint8_t fill;     // bytes waiting in buf

  void begin();
  void update(const uint8_t* data, size_t len);
  void finish(uint8_t out[32]);
};

void hmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t out[32]);

#endif
