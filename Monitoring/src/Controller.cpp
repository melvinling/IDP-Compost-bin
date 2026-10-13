#include "Controller.h"
#include <string.h>
namespace {
constexpr uint32_t duration[] = {Config::MOTOR_RUN_MS, Config::PUMP_RUN_MS, Config::FAN_RUN_MS};
constexpr uint32_t cooldown[] = {Config::MOTOR_COOLDOWN_MS, Config::PUMP_COOLDOWN_MS, Config::FAN_COOLDOWN_MS};
ActuatorCommand pack(const bool *v) { return {v[0], v[1], v[2]}; }
}
bool parseControlAction(const char *text, ControlAction &action) {
  const char *names[] = {"mode_manual", "mode_auto", "motor_start", "motor_stop", "pump_start", "pump_stop", "stop_all", "reset_stop", "fan_start", "fan_stop", "stage_fermentation", "stage_finishing"};
  for (unsigned i = 0; i < 12; ++i) if (!strcmp(text, names[i])) { action = static_cast<ControlAction>(i); return true; }
  return false;
}
uint32_t Controller::remaining(unsigned i, uint32_t now) const {
  const auto &c = channels_[i]; const uint32_t elapsed = uint32_t(now - c.stopped);
  return c.cooled && elapsed < cooldown[i] ? cooldown[i] - elapsed : 0;
}
ControlContext Controller::context(uint32_t now) const {
  return {stage_, stage_ == Stage::Finishing && uint32_t(now - lastMix_) >= Config::MIX_INTERVAL_MS, postWaterMix_, revision_};
}
ApplicationStatus Controller::status() const {
  ApplicationStatus s{mode_, latched_, !configError_, requested_, outputs_, reason_, prediction_};
  const auto c = context(now_);
  s.stage = stage_; s.stageElapsedMs = uint32_t(now_ - stageStarted_);
  s.periodicMixDue = c.periodicMixDue; s.postWaterMixDue = c.postWaterMixDue;
  for (unsigned i = 0; i < 3; ++i) s.cooldownMs[i] = remaining(i, now_);
  return s;
}
void Controller::stop(unsigned i, uint32_t now, bool completed) {
  auto &c = channels_[i]; if (!c.on) return;
  c.on = false; c.manual = false; c.cooled = true; c.stopped = now;
  if (i == 1 && completed && stage_ == Stage::Finishing) { postWaterMix_ = true; ++revision_; }
  if (i == 0 && completed) { postWaterMix_ = false; lastMix_ = now; ++revision_; }
}
void Controller::stopRequests(uint32_t now) {
  for (unsigned i = 0; i < 3; ++i) { stop(i, now, false); channels_[i].manual = false; channels_[i].blocked = false; }
  requested_ = outputs_ = {}; ++revision_;
}
void Controller::latch(const char *why, uint32_t now) { latched_ = true; fault_ = why; stopRequests(now); postWaterMix_ = false; }
ControlReply Controller::command(ControlAction a, uint32_t now) {
  tick(now);
  if (a == ControlAction::StopAll) { latch("Software Stop All latched", now); mode_ = Mode::Manual; tick(now); return {true, reason_}; }
  if (a == ControlAction::ResetStop) { stopRequests(now); postWaterMix_ = false; mode_ = Mode::Manual; latched_ = false; fault_ = ""; tick(now); return {true, "Stop reset; manual mode, outputs off"}; }
  if (a == ControlAction::ManualMode || a == ControlAction::AutoMode) {
    stopRequests(now); mode_ = a == ControlAction::AutoMode ? Mode::Auto : Mode::Manual;
    reason_ = latched_ ? fault_ : "Mode changed; outputs off"; return {true, reason_};
  }
  if (a == ControlAction::FermentationStage || a == ControlAction::FinishingStage) {
    stopRequests(now); stage_ = a == ControlAction::FinishingStage ? Stage::Finishing : Stage::Fermentation;
    stageStarted_ = lastMix_ = now; postWaterMix_ = false;
    reason_ = latched_ ? fault_ : "Stage selected; outputs off"; return {true, reason_};
  }
  const bool isStop = a == ControlAction::MotorStop || a == ControlAction::PumpStop || a == ControlAction::FanStop;
  const unsigned i = a == ControlAction::MotorStart || a == ControlAction::MotorStop ? 0 : a == ControlAction::PumpStart || a == ControlAction::PumpStop ? 1 : 2;
  if (isStop) { stop(i, now, false); channels_[i].manual = false; channels_[i].blocked = true; tick(now); return {true, "Stop accepted; automatic channel inhibited until mode reselected"}; }
  if (latched_) return {false, fault_};
  if (configError_) return {false, configError_};
  if (mode_ != Mode::Manual) return {false, "Select manual mode before Start"};
  if (stage_ != Stage::Finishing) return {false, "Select finishing before operating actuators"};
  if (remaining(i, now)) return {false, "Actuator cooldown active"};
  if ((i == 1 && channels_[2].on) || (i == 2 && channels_[1].on)) return {false, "Fan and pump are mutually exclusive"};
  channels_[i].manual = true; channels_[i].manualAt = now; channels_[i].blocked = false;
  tick(now); return {true, "Five-second manual request accepted"};
}
ActuatorCommand Controller::tick(uint32_t now) {
  now_ = now;
  for (unsigned i = 0; i < 3; ++i) {
    auto &c = channels_[i];
    if (c.on && uint32_t(now - c.started) >= Config::MAX_RUNTIME_MS) latch("Maximum continuous runtime latched", now);
    if (c.on && uint32_t(now - (mode_ == Mode::Manual ? c.manualAt : c.started)) >= (mode_ == Mode::Manual ? Config::MANUAL_RUN_MS : duration[i])) stop(i, now, true);
    if (c.manual && uint32_t(now - c.manualAt) >= Config::MANUAL_RUN_MS) c.manual = false;
  }
  bool wanted[] = {channels_[0].manual, channels_[1].manual, channels_[2].manual};
  if (mode_ == Mode::Auto) { wanted[0] = prediction_.command.motor; wanted[1] = prediction_.command.pump; wanted[2] = prediction_.command.fan; }
  for (unsigned i = 0; i < 3; ++i) if (channels_[i].blocked) wanted[i] = false;
  requested_ = pack(wanted);
  const char *inhibit = nullptr;
  if (latched_) inhibit = fault_;
  else if (configError_) inhibit = configError_;
  else if (stage_ != Stage::Finishing) inhibit = "Sealed fermentation: actuators off";
  else if (mode_ == Mode::Auto && !sample_.fresh(now, Config::FRESH_MS)) inhibit = "Automatic inputs invalid or stale";
  else if (mode_ == Mode::Auto && (!prediction_.modelReady || prediction_.contextRevision != revision_)) inhibit = "Waiting for prediction for current process context";
  else if (wanted[1] && wanted[2]) inhibit = "Conflicting fan/pump requests rejected";
  reason_ = inhibit ? inhibit : "Idle or waiting for actuator cooldown";
  bool on[3] = {};
  for (unsigned i = 0; i < 3; ++i) {
    auto &c = channels_[i];
    if (inhibit || !wanted[i]) stop(i, now, false);
    else if (!c.on && !remaining(i, now)) { c.on = true; c.started = now; }
    on[i] = c.on;
  }
  outputs_ = pack(on);
  if (latched_) requested_ = {};
  if (outputs_.motor || outputs_.pump || outputs_.fan) reason_ = "Relay ON command issued (no actuator feedback)";
  return outputs_;
}
