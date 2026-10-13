#pragma once
#include <stdint.h>
#include <stddef.h>
namespace Config {
constexpr char AP_NAME[] = "CompostControl";
constexpr char AP_PASSWORD[] = "compost123"; // Change before commissioning (8+ chars).
constexpr int RS485_RX = 16, RS485_TX = 17;
constexpr int ULTRASONIC_TRIGGER = 27, ULTRASONIC_OUTPUT = 35;
constexpr uint8_t SOIL_ADDRESS = 1, CO2_ADDRESS = 1;
constexpr uint32_t SOIL_BAUD = 4800, CO2_BAUD = 9600;
constexpr bool RX_INVERTED = false; // Diagnostic only; not a verified hardware fix.
enum class SoilProfile { EcPh, EcTemperatureMoisturePh };
constexpr SoilProfile SOIL_PROFILE = SoilProfile::EcTemperatureMoisturePh;
constexpr unsigned READ_COUNT = SOIL_PROFILE == SoilProfile::EcPh ? 3 : 5;
enum class Polarity : int8_t { Unconfigured = -1, ActiveLow = 0, ActiveHigh = 1 };
struct RelaySettings {
  bool enabled = false;
  int motorPin = -1, pumpPin = -1;
  Polarity motorPolarity = Polarity::Unconfigured, pumpPolarity = Polarity::Unconfigured;
  int fanPin = -1;
  Polarity fanPolarity = Polarity::Unconfigured;
};
constexpr bool ACTUATORS_ENABLED = false;
constexpr int MOTOR_RELAY = -1, PUMP_RELAY = -1;
constexpr Polarity MOTOR_POLARITY = Polarity::Unconfigured, PUMP_POLARITY = Polarity::Unconfigured;
// Relay 1 = fan, Relay 2 = pump, Relay 3 = motor. Confirm GPIOs and polarity.
constexpr int FAN_RELAY = -1;
constexpr Polarity FAN_POLARITY = Polarity::Unconfigured;
constexpr RelaySettings RELAYS{ACTUATORS_ENABLED, MOTOR_RELAY, PUMP_RELAY, MOTOR_POLARITY, PUMP_POLARITY, FAN_RELAY, FAN_POLARITY};
constexpr uint32_t SAMPLE_MS = 4000, RESPONSE_MS = 600;
constexpr uint32_t ULTRASONIC_TIMEOUT_MS = 65, FRESH_MS = 12000;
constexpr uint32_t MAX_RUNTIME_MS = 10000, MANUAL_RUN_MS = 5000;
constexpr uint32_t MOTOR_RUN_MS = 5000, PUMP_RUN_MS = 2000, FAN_RUN_MS = 8000;
constexpr uint32_t MOTOR_COOLDOWN_MS = 60000, PUMP_COOLDOWN_MS = 60000, FAN_COOLDOWN_MS = 30000;
constexpr uint32_t MIX_INTERVAL_MS = 1800000;
constexpr uint32_t SAFETY_TICK_MS = 20;
constexpr size_t HISTORY_SIZE = 900, API_PAGE_SIZE = 60;
static_assert(MANUAL_RUN_MS <= MAX_RUNTIME_MS, "Manual duration exceeds runtime limit");
static_assert(SAMPLE_MS > ULTRASONIC_TIMEOUT_MS + READ_COUNT * (RESPONSE_MS + 30), "Sample period too short");
static_assert(MOTOR_RUN_MS < MAX_RUNTIME_MS && PUMP_RUN_MS < MAX_RUNTIME_MS && FAN_RUN_MS < MAX_RUNTIME_MS, "Automatic pulse exceeds runtime limit");
}
