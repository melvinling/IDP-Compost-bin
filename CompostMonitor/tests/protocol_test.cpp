#include "../ModbusProtocol.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
  uint8_t frame[8];
  const uint8_t co2Request[] = {1,3,0,0,0,1,0x84,0x0A};
  ModbusProtocol::request(frame, 1, 3, 0, 1);
  assert(std::memcmp(frame, co2Request, 8) == 0);
  const uint8_t changeId[] = {1,6,0,0x66,0,2,0xE8,0x14};
  ModbusProtocol::request(frame, 1, 6, 0x0066, 2);
  assert(std::memcmp(frame, changeId, 8) == 0);

  // Manufacturer's worked example: 536 ppm.
  uint8_t co2Reply[] = {1,3,2,2,0x18,0xB9,0x2E};
  uint16_t values[4] = {};
  assert(ModbusProtocol::decode(co2Reply, 7, 1, 1, values) == 0);
  assert(values[0] == 536);
  assert(ModbusProtocol::decode(co2Reply, 7, 2, 1, values) == 1);
  assert(ModbusProtocol::decode(co2Reply, 7, 1, 2, values) == 1);
  assert(ModbusProtocol::decode(co2Reply, 6, 1, 1, values) == 1);
  co2Reply[3] ^= 1;
  assert(ModbusProtocol::decode(co2Reply, 7, 1, 1, values) == 1);

  // Manufacturer's four-register soil example, including a negative temperature.
  const uint8_t soilReply[] = {1,3,8,2,0x92,0xFF,0x9B,3,0xE8,0,0x38,0x57,0xB6};
  assert(ModbusProtocol::decode(soilReply, sizeof soilReply, 1, 4, values) == 0);
  assert(values[0] == 658 && int16_t(values[1]) == -101);
  assert(values[2] == 1000 && values[3] == 56);

  uint8_t exception[] = {1,0x83,2,0,0};
  const uint16_t crc = ModbusProtocol::crc16(exception, 3);
  exception[3] = crc & 0xFF; exception[4] = crc >> 8;
  assert(ModbusProtocol::decode(exception, sizeof exception, 1, 1, values) == 2);

  ModbusProtocol::ReplyStream stream;
  // Captured v4 traces: inverted request echoes are not sensor measurements.
  const uint8_t invertedEcEcho[] = {0x7F,0x7E,0xFB,0xFF,0xFD,0xB5,0x6B};
  const uint8_t invertedPhEcho[] = {0x7F,0x7E,0xF9,0xFF,0xFD,0x17,0xEB,0};
  const uint8_t invertedCo2Echo[] = {0x7F,0x7E,0xFF,0xFF,0xFD,0xF7,0x3D};
  for (uint8_t byte : invertedEcEcho) assert(stream.feed(byte, 1, 1, values) == 0);
  for (uint8_t byte : invertedPhEcho) assert(stream.feed(byte, 1, 1, values) == 0);
  for (uint8_t byte : invertedCo2Echo) assert(stream.feed(byte, 1, 1, values) == 0);
  // Correcting the receive polarity may reveal a normal request echo only.
  for (uint16_t reg : {uint16_t(2), uint16_t(3), uint16_t(0)}) {
    ModbusProtocol::request(frame, 1, 3, reg, 1);
    for (uint8_t byte : frame) assert(stream.feed(byte, 1, 1, values) == 0);
  }
  stream.reset();
  // A request echo and a corrupted response must not mask the subsequent reply.
  for (uint8_t byte : co2Request) assert(stream.feed(byte, 1, 1, values) == 0);
  for (uint8_t byte : co2Reply) assert(stream.feed(byte, 1, 1, values) == 0);
  co2Reply[3] ^= 1; // Restore the manufacturer CRC-valid response.
  int result = 0;
  for (uint8_t byte : co2Reply) result = stream.feed(byte, 1, 1, values);
  assert(result == 1 && values[0] == 536);
  stream.reset();
  for (uint8_t byte : co2Reply) assert(stream.feed(byte, 2, 1, values) == 0);
  stream.reset();
  for (size_t i = 0; i < sizeof(co2Reply) - 1; ++i)
    assert(stream.feed(co2Reply[i], 1, 1, values) == 0);
  assert(stream.feed(co2Reply[6], 1, 1, values) == 1);
  stream.reset();
  for (uint8_t byte : exception) result = stream.feed(byte, 1, 1, values);
  assert(result == -3);
  // Bounded parsing after extended noise, followed by a complete valid frame.
  stream.reset();
  for (int i = 0; i < 1000; ++i) assert(stream.feed(0xFF, 1, 1, values) == 0);
  for (uint8_t byte : soilReply) result = stream.feed(byte, 1, 4, values);
  assert(result == 1 && values[2] == 1000 && values[3] == 56);
  std::cout << "Protocol tests passed: manufacturer frames, CRC errors, IDs, truncation, exceptions, echo/noise resynchronization.\n";
}
