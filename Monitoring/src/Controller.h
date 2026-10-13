#pragma once
#include "Types.h"
#include "Config.h"
#include "RelayConfiguration.h"
enum class ControlAction { ManualMode, AutoMode, MotorStart, MotorStop, PumpStart, PumpStop, StopAll, ResetStop, FanStart, FanStop, FermentationStage, FinishingStage };
bool parseControlAction(const char *text, ControlAction &action);
struct ControlReply { bool accepted; const char *message; };
class Controller {
 public:
  explicit Controller(Config::RelaySettings settings = Config::RELAYS) : configError_(relayConfigurationError(settings)) {}
  void updateSample(const SensorSnapshot &s, PredictionResult p) { sample_ = s; prediction_ = p; }
  ControlReply command(ControlAction action, uint32_t now);
  ActuatorCommand tick(uint32_t now);
  ControlContext context(uint32_t now) const;
  ApplicationStatus status() const;
 private:
  struct Channel { bool on = false, manual = false, blocked = false, cooled = false; uint32_t started = 0, stopped = 0, manualAt = 0; };
  Channel channels_[3]; // motor, pump, fan
  Mode mode_ = Mode::Manual;
  Stage stage_ = Stage::Fermentation;
  bool latched_ = false, postWaterMix_ = false;
  uint32_t now_ = 0, stageStarted_ = 0, lastMix_ = 0, revision_ = 0;
  const char *configError_, *fault_ = "", *reason_ = "Manual idle";
  SensorSnapshot sample_;
  PredictionResult prediction_;
  ActuatorCommand requested_, outputs_;
  uint32_t remaining(unsigned i, uint32_t now) const;
  void stop(unsigned i, uint32_t now, bool completed);
  void stopRequests(uint32_t now);
  void latch(const char *why, uint32_t now);
};
