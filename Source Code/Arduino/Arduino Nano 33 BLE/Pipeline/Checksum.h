#ifndef CHECKSUM_H
#define CHECKSUM_H

static inline uint8_t computeChecksum(const uint8_t* data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++) { sum += data[i]; }
  return (uint8_t)(sum & 0xFF);
}

#endif