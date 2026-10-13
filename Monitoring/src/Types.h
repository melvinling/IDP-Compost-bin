#pragma once
#include <stdint.h>
#include <math.h>
enum class SensorStatus : uint8_t { Waiting, Ok, NoReply, InvalidReply, Exception, OutOfRange, NoPulse, Unsupported };
inline const char *statusName(SensorStatus s) {
  switch (s) {
    case SensorStatus::Ok: return "Live";
    case SensorStatus::NoReply: return "No RS485 reply";
    case SensorStatus::InvalidReply: return "No matching CRC-valid reply";
    case SensorStatus::Exception: return "Modbus exception";
    case SensorStatus::OutOfRange: return "Out of range";
    case SensorStatus::NoPulse: return "No ultrasonic pulse";
    case SensorStatus::Unsupported: return "Not supported by selected soil profile";
    default: return "Waiting";
  }
}
struct Reading {
  float value = NAN;
  SensorStatus status = SensorStatus::Waiting;
  uint64_t lastSuccessMs = 0;
  uint32_t acquiredMs = 0;
  uint8_t exceptionCode = 0;
  bool valid() const { return status == SensorStatus::Ok && isfinite(value); }
};
struct SensorSnapshot {
  uint32_t acquiredMs = 0;
  uint64_t uptimeMs = 0;
  uint32_t sequence = 0;
  Reading distance, ec, ph, co2, moisture, temperature;
  bool valid() const { return sequence && distance.valid() && ec.valid() && ph.valid() && co2.valid() && moisture.valid() && temperature.valid(); }
  bool fresh(uint32_t now, uint32_t limit) const {
    return valid() && uint32_t(now-distance.acquiredMs) < limit && uint32_t(now-ec.acquiredMs) < limit &&
      uint32_t(now-ph.acquiredMs) < limit && uint32_t(now-co2.acquiredMs) < limit &&
      uint32_t(now-moisture.acquiredMs) < limit && uint32_t(now-temperature.acquiredMs) < limit;
  }
};
struct ActuatorCommand { bool motor = false, pump = false, fan = false; };
enum class Stage : uint8_t { Fermentation, Finishing };
inline const char *stageName(Stage s) { return s == Stage::Finishing ? "finishing" : "fermentation"; }
enum class Decision : uint8_t { Idle, Water, Dry, Mix };
inline const char *decisionName(Decision d) {
  switch (d) { case Decision::Water: return "water"; case Decision::Dry: return "dry"; case Decision::Mix: return "mix"; default: return "idle"; }
}
struct ControlContext { Stage stage = Stage::Fermentation; bool periodicMixDue = false, postWaterMixDue = false; uint32_t revision = 0; };
struct PredictionResult {
  ActuatorCommand command;
  bool modelReady = false;
  const char *reason = "Waiting for first prediction"; // Stable lifetime required.
  Decision action = Decision::Idle;
  float confidence = 0;
  uint32_t contextRevision = 0;
};
enum class Mode : uint8_t { Manual, Auto };
inline const char *modeName(Mode m) { return m == Mode::Auto ? "auto" : "manual"; }
struct ApplicationStatus {
  Mode mode = Mode::Manual;
  bool stopLatched = false, relayConfigured = false;
  ActuatorCommand requested, commanded;
  const char *reason = "Waiting";
  PredictionResult prediction;
  Stage stage = Stage::Fermentation;
  uint32_t stageElapsedMs = 0;
  bool periodicMixDue = false, postWaterMixDue = false;
  uint32_t cooldownMs[3] = {}; // motor, pump, fan
};
