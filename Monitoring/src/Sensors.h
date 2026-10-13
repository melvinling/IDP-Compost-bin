#pragma once
#include <Arduino.h>
#include "Types.h"
#include "ModbusProtocol.h"
class Sensors {
 public:
  void begin();
  // Cooperative: bounded work per tick. Returns true exactly once per sample.
  bool tick(SensorSnapshot &completed);
 private:
  enum class Phase { Idle, Ultrasonic, Settling, Receiving };
  HardwareSerial bus_{2};
  Phase phase_ = Phase::Idle;
  SensorSnapshot sample_;
  ModbusProtocol::ReplyStream stream_;
  uint32_t nextSample_ = 0, phaseStarted_ = 0, baud_ = 0, received_ = 0;
  uint8_t query_ = 0;
  bool trace_ = false, rxInverted_ = false, polarityPending_ = false, desiredPolarity_ = false;
  uint8_t rxTrace_[64] = {};
  size_t rxTraceCount_ = 0;
  char diagnosticLine_[40] = {};
  size_t diagnosticLength_ = 0;
  bool diagnosticOverflow_ = false;
  void serviceDiagnostics();
  void prepareRead();
  void finishRead(int result, uint16_t raw);
  bool complete(SensorSnapshot &out);
};
