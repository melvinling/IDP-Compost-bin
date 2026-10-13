#include <Arduino.h>
#include "RelayDriver.h"
#include "RelayConfiguration.h"
namespace {
int level(bool on, Config::Polarity p) { return on == (p == Config::Polarity::ActiveHigh) ? HIGH : LOW; }
void initialize(int pin, Config::Polarity p) {
  digitalWrite(pin, level(false, p)); // Set output latch before enabling output.
  pinMode(pin, OUTPUT);
}
}
void RelayDriver::begin() {
  ready_ = relayConfigurationError(settings_) == nullptr;
  if (!ready_) return; // NO calls to GPIO for disabled/invalid configuration.
  initialize(settings_.motorPin, settings_.motorPolarity);
  initialize(settings_.pumpPin, settings_.pumpPolarity);
  initialize(settings_.fanPin, settings_.fanPolarity);
  previous_ = {};
}
void RelayDriver::apply(ActuatorCommand state) {
  if (!ready_) return;
  if (state.motor != previous_.motor) digitalWrite(settings_.motorPin, level(state.motor, settings_.motorPolarity));
  if (state.pump != previous_.pump) digitalWrite(settings_.pumpPin, level(state.pump, settings_.pumpPolarity));
  if (state.fan != previous_.fan) digitalWrite(settings_.fanPin, level(state.fan, settings_.fanPolarity));
  previous_ = state;
}
