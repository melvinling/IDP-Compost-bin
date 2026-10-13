#pragma once
#include <stdint.h>
#include <stddef.h>

namespace ModbusProtocol {
inline uint16_t crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

inline void request(uint8_t *frame, uint8_t address, uint8_t function,
                    uint16_t reg, uint16_t argument) {
  frame[0] = address; frame[1] = function;
  frame[2] = reg >> 8; frame[3] = reg & 0xFF;
  frame[4] = argument >> 8; frame[5] = argument & 0xFF;
  const uint16_t crc = crc16(frame, 6);
  frame[6] = crc & 0xFF; frame[7] = crc >> 8;
}

// 0 = success; 1 = bad frame; 2 = Modbus exception.
inline uint8_t decode(const uint8_t *frame, size_t length, uint8_t address,
                      uint8_t count, uint16_t *values) {
  if (length < 5 || frame[0] != address) return 1;
  const uint16_t crc = crc16(frame, length - 2);
  if (frame[length - 2] != (crc & 0xFF) || frame[length - 1] != (crc >> 8)) return 1;
  if (frame[1] == 0x83 && length == 5) return 2;
  if (frame[1] != 3 || frame[2] != count * 2 || length != size_t(5 + count * 2)) return 1;
  for (uint8_t i = 0; i < count; ++i)
    values[i] = uint16_t(frame[3 + i * 2]) << 8 | frame[4 + i * 2];
  return 0;
}

// Resynchronize after noise, partial frames or a converter's request echo.
// Only CRC-verified replies with the requested ID and byte count are accepted.
class ReplyStream {
 public:
  uint8_t frame[32] = {};
  size_t length = 0;
  void reset() { length = 0; }
  int feed(uint8_t byte, uint8_t address, uint8_t count, uint16_t *values) {
    if (length == sizeof(frame)) discard();
    frame[length++] = byte;
    while (length) {
      if (frame[0] != address) { discard(); continue; }
      if (length < 2) break;
      if (frame[1] != 3 && frame[1] != 0x83) { discard(); continue; }
      if (length < 3) break;
      const size_t expected = frame[1] == 0x83 ? 5 : 5 + count * 2;
      if (frame[1] == 3 && frame[2] != count * 2) { discard(); continue; }
      if (expected > sizeof(frame)) { discard(); continue; }
      if (length < expected) break;
      const uint8_t result = decode(frame, expected, address, count, values);
      if (result == 0) return 1;
      if (result == 2) return -3;
      discard();
    }
    return 0;
  }
 private:
  void discard() {
    for (size_t i = 1; i < length; ++i) frame[i - 1] = frame[i];
    if (length) --length;
  }
};
}
