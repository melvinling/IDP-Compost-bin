#pragma once
#include "Config.h"
inline bool allowedRelayPin(int pin) {
  if (pin == Config::RS485_RX || pin == Config::RS485_TX ||
      pin == Config::ULTRASONIC_TRIGGER || pin == Config::ULTRASONIC_OUTPUT) return false;
  // Conservative classic ESP32 whitelist: excludes flash, strapping, UART0,
  // input-only and this project's sensor pins.
  return pin == 18 || pin == 19 || pin == 21 || pin == 22 || pin == 23 ||
    pin == 25 || pin == 26 || pin == 32 || pin == 33;
}
inline const char *relayConfigurationError(const Config::RelaySettings &s) {
  if (!s.enabled) return "Physical relay control disabled";
  if (s.motorPin < 0 || s.pumpPin < 0 || s.motorPolarity == Config::Polarity::Unconfigured ||
      s.pumpPolarity == Config::Polarity::Unconfigured || s.fanPin < 0 || s.fanPolarity == Config::Polarity::Unconfigured) return "Relay channels or polarity unconfigured";
  if (s.motorPin == s.pumpPin || s.fanPin == s.motorPin || s.fanPin == s.pumpPin) return "Duplicate relay pins";
  if (!allowedRelayPin(s.motorPin) || !allowedRelayPin(s.pumpPin) || !allowedRelayPin(s.fanPin)) return "Relay pin conflicts with sensor/programming pins or is unsupported";
  if ((s.motorPolarity != Config::Polarity::ActiveHigh && s.motorPolarity != Config::Polarity::ActiveLow) ||
      (s.pumpPolarity != Config::Polarity::ActiveHigh && s.pumpPolarity != Config::Polarity::ActiveLow) ||
      (s.fanPolarity != Config::Polarity::ActiveHigh && s.fanPolarity != Config::Polarity::ActiveLow)) return "Invalid relay polarity";
  return nullptr;
}
