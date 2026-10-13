#include "Sensors.h"
#include "Config.h"
#include <esp_timer.h>
#include <ctype.h>
#include <string.h>
namespace {
portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool armed = false, rose = false, captured = false;
volatile uint32_t riseUs = 0, widthUs = 0;
void ARDUINO_ISR_ATTR pulseEdge() {
  portENTER_CRITICAL_ISR(&pulseMux);
  if (armed) {
    if (digitalRead(Config::ULTRASONIC_OUTPUT)) { riseUs = micros(); rose = true; }
    else if (rose) { widthUs = uint32_t(micros() - riseUs); captured = true; armed = false; }
  }
  portEXIT_CRITICAL_ISR(&pulseMux);
}
void invalidate(Reading &r) { r.value = NAN; r.status = SensorStatus::Waiting; r.exceptionCode = 0; }
void success(Reading &r, float value) {
  r.value = value; r.status = SensorStatus::Ok;
  r.acquiredMs = millis(); r.lastSuccessMs = uint64_t(esp_timer_get_time()) / 1000;
}
unsigned kind(unsigned query) { return Config::SOIL_PROFILE == Config::SoilProfile::EcPh && query == 2 ? 4 : query; }
Reading &reading(SensorSnapshot &s, unsigned k) {
  Reading *items[] = {&s.ec, &s.ph, &s.moisture, &s.temperature, &s.co2}; return *items[k];
}
constexpr uint16_t registers[] = {2, 3, 0, 1, 0};
const char *labels[] = {"EC", "pH", "Moisture", "Temperature", "CO2"};
}
void Sensors::begin() {
  pinMode(Config::ULTRASONIC_TRIGGER, OUTPUT); digitalWrite(Config::ULTRASONIC_TRIGGER, HIGH);
  pinMode(Config::ULTRASONIC_OUTPUT, INPUT); // GPIO35 has no output/pull-up capability.
  attachInterrupt(digitalPinToInterrupt(Config::ULTRASONIC_OUTPUT), pulseEdge, CHANGE);
  bus_.begin(Config::SOIL_BAUD, SERIAL_8N1, Config::RS485_RX, Config::RS485_TX);
  bus_.setRxInvert(Config::RX_INVERTED);
  rxInverted_ = desiredPolarity_ = Config::RX_INVERTED;
  baud_ = Config::SOIL_BAUD;
  nextSample_ = millis() + 1000;
  Serial.println("Diagnostics (Newline): TRACE ON, TRACE OFF, RX INVERTED, RX NORMAL");
}
void Sensors::serviceDiagnostics() {
  unsigned budget = 32;
  while (budget-- && Serial.available()) {
    const char c = char(Serial.read());
    if (c != '\r' && c != '\n') {
      if (diagnosticLength_ < sizeof(diagnosticLine_) - 1)
        diagnosticLine_[diagnosticLength_++] = char(toupper(static_cast<unsigned char>(c)));
      else diagnosticOverflow_ = true;
      continue;
    }
    if (!diagnosticLength_ && !diagnosticOverflow_) continue;
    diagnosticLine_[diagnosticLength_] = '\0';
    if (diagnosticOverflow_) Serial.println("Diagnostic command too long; ignored.");
    else if (!strcmp(diagnosticLine_, "TRACE ON")) { trace_ = true; Serial.println("Raw RS485 trace enabled."); }
    else if (!strcmp(diagnosticLine_, "TRACE OFF")) { trace_ = false; Serial.println("Raw RS485 trace disabled."); }
    else if (!strcmp(diagnosticLine_, "RX INVERTED") || !strcmp(diagnosticLine_, "RX NORMAL")) {
      desiredPolarity_ = !strcmp(diagnosticLine_, "RX INVERTED");
      polarityPending_ = true;
      Serial.println("RX polarity queued for next sample; TX and sensor settings unchanged.");
    } else Serial.println("Diagnostics: TRACE ON, TRACE OFF, RX INVERTED, RX NORMAL");
    diagnosticLength_ = 0; diagnosticOverflow_ = false;
  }
}
void Sensors::prepareRead() {
  const uint32_t desired = kind(query_) == 4 ? Config::CO2_BAUD : Config::SOIL_BAUD;
  if (baud_ != desired) { bus_.updateBaudRate(desired); baud_ = desired; }
  // Let the prior frame end and UART settle. Always establish an RTU silent interval.
  phase_ = Phase::Settling; phaseStarted_ = millis(); received_ = 0; rxTraceCount_ = 0;
}
void Sensors::finishRead(int result, uint16_t raw) {
  const unsigned k = kind(query_);
  Reading &r = reading(sample_, k);
  if (result == 1) {
    const float value = k == 3 ? int16_t(raw) / 10.0f : k == 1 || k == 2 ? raw / 10.0f : raw;
    if ((k == 1 && value > 14.0f) || (k == 2 && value > 100.0f) || (k == 3 && (value < -40 || value > 80))) r.status = SensorStatus::OutOfRange;
    else success(r, value);
  } else if (result == -3) {
    r.status = SensorStatus::Exception; r.exceptionCode = stream_.frame[2];
  } else r.status = received_ ? SensorStatus::InvalidReply : SensorStatus::NoReply;
  Serial.printf("RS485 %s | ID=%u baud=%lu | %s\n", labels[k],
    k == 4 ? Config::CO2_ADDRESS : Config::SOIL_ADDRESS, (unsigned long)baud_, statusName(r.status));
  if (trace_) {
    Serial.printf("RX (%lu bytes, first %u):", (unsigned long)received_, unsigned(rxTraceCount_));
    for (size_t i = 0; i < rxTraceCount_; ++i) Serial.printf(" %02X", rxTrace_[i]);
    Serial.println("");
    if (result == 1) Serial.printf("Validated raw register value: %u\n", raw);
  }
  ++query_;
  if (query_ < Config::READ_COUNT) prepareRead();
}
bool Sensors::complete(SensorSnapshot &out) {
  ++sample_.sequence; out = sample_; phase_ = Phase::Idle; return true;
}
bool Sensors::tick(SensorSnapshot &out) {
  serviceDiagnostics();
  const uint32_t now = millis();
  if (phase_ == Phase::Idle) {
    if (int32_t(now - nextSample_) < 0) return false;
    if (polarityPending_) {
      if (bus_.setRxInvert(desiredPolarity_)) rxInverted_ = desiredPolarity_;
      else Serial.println("RX polarity change failed; previous setting retained.");
      polarityPending_ = false;
      stream_.reset();
      Serial.printf("RX polarity: %s (diagnostic only; CRC validation enabled)\n", rxInverted_ ? "INVERTED" : "NORMAL");
    }
    nextSample_ += Config::SAMPLE_MS;
    if (int32_t(now - nextSample_) >= 0) nextSample_ = now + Config::SAMPLE_MS; // No catch-up flood.
    sample_.acquiredMs = now; sample_.uptimeMs = uint64_t(esp_timer_get_time()) / 1000;
    invalidate(sample_.distance); invalidate(sample_.ec); invalidate(sample_.ph); invalidate(sample_.co2);
    invalidate(sample_.moisture); invalidate(sample_.temperature);
    if (Config::SOIL_PROFILE == Config::SoilProfile::EcPh) {
      sample_.moisture.status = sample_.temperature.status = SensorStatus::Unsupported;
    }
    portENTER_CRITICAL(&pulseMux); armed = true; rose = captured = false; widthUs = 0; portEXIT_CRITICAL(&pulseMux);
    // A02 falling edge trigger: LOW for 20 us, then return to idle HIGH.
    digitalWrite(Config::ULTRASONIC_TRIGGER, LOW); delayMicroseconds(20); digitalWrite(Config::ULTRASONIC_TRIGGER, HIGH);
    phase_ = Phase::Ultrasonic; phaseStarted_ = now;
    return false;
  }
  if (phase_ == Phase::Ultrasonic) {
    bool done; uint32_t width;
    portENTER_CRITICAL(&pulseMux); done = captured; width = widthUs; portEXIT_CRITICAL(&pulseMux);
    if (!done && uint32_t(now-phaseStarted_) < Config::ULTRASONIC_TIMEOUT_MS) return false;
    portENTER_CRITICAL(&pulseMux); armed = false; portEXIT_CRITICAL(&pulseMux);
    const float cm = width / 57.5f;
    if (!done || !width) sample_.distance.status = SensorStatus::NoPulse;
    else if (width >= 34000 || cm < 3 || cm > 450) sample_.distance.status = SensorStatus::OutOfRange;
    else success(sample_.distance, cm);
    query_ = 0; prepareRead(); return false;
  }
  if (phase_ == Phase::Settling) {
    if (uint32_t(now-phaseStarted_) < 25) return false;
    unsigned budget = 256;
    while (budget-- && bus_.available()) bus_.read(); // Bounded even under continuous noise.
    if (bus_.available()) {
      // Continuous interference must not hold acquisition in settling forever.
      received_ = 256; finishRead(-1, 0);
      return query_ == Config::READ_COUNT ? complete(out) : false;
    }
    uint8_t request[8];
    ModbusProtocol::request(request, kind(query_) == 4 ? Config::CO2_ADDRESS : Config::SOIL_ADDRESS, 3,
      registers[kind(query_)], 1);
    stream_.reset(); received_ = 0;
    if (trace_) {
      Serial.printf("CHECK: baud=%lu ID=%u register=0x%04X count=1 | TX GPIO%d RX GPIO%d | RX %s\n",
        (unsigned long)baud_, request[0], unsigned(request[2]) * 256 + request[3],
        Config::RS485_TX, Config::RS485_RX, rxInverted_ ? "INVERTED" : "NORMAL");
      Serial.printf("TX:");
      for (const auto byte : request) Serial.printf(" %02X", byte);
      Serial.println("");
    }
    bus_.write(request, sizeof(request)); // 8-byte UART buffer enqueue; no flush()/delay().
    phase_ = Phase::Receiving; phaseStarted_ = now; return false;
  }
  const uint8_t address = kind(query_) == 4 ? Config::CO2_ADDRESS : Config::SOIL_ADDRESS;
  uint16_t value[1] = {};
  int result = 0;
  unsigned budget = 64;
  while (budget-- && bus_.available()) {
    const uint8_t byte = bus_.read();
    ++received_;
    if (rxTraceCount_ < sizeof(rxTrace_)) rxTrace_[rxTraceCount_++] = byte;
    result = stream_.feed(byte, address, 1, value);
    if (result) break;
  }
  if (!result && uint32_t(now-phaseStarted_) < Config::RESPONSE_MS) return false;
  finishRead(result ? result : -1, value[0]);
  return query_ == Config::READ_COUNT ? complete(out) : false;
}
