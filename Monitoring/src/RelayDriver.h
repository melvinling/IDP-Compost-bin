#pragma once
#include "Types.h"
#include "Config.h"
class RelayDriver {
 public:
  explicit RelayDriver(Config::RelaySettings settings = Config::RELAYS) : settings_(settings) {}
  void begin();
  void apply(ActuatorCommand state);
 private:
  Config::RelaySettings settings_;
  bool ready_ = false;
  ActuatorCommand previous_;
};
