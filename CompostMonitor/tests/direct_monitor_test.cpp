// The runner includes functions extracted from the current sketch, not copies.
#include <cmath>
#include <cstring>
#include <iostream>
#include "../../RS485AutoScan/tests/Arduino.h"
using std::isfinite;
constexpr int LOW = 0, HIGH = 1;
void digitalWrite(int, int) {}
void delayMicroseconds(unsigned int) {}
unsigned long pulseIn(int, int, unsigned long) { return 5750; }
int64_t esp_timer_get_time() { return int64_t(testClock) * 1000; }
HardwareSerial bus(2);
struct { bool active = false; } scan;
#include "direct_monitor_extracted.h"

void runSample(bool replies, bool missingPh = false) {
  bus.requests.clear();
  nextSample = millis();
  const uint32_t target = sequenceNumber + 1;
  unsigned int attempts = 0;
  while (sequenceNumber != target && attempts < 2500) {
    // Disable the PH response as the EC phase starts the following request.
    testSensors = replies && !(missingPh && phase == Phase::SoilEc);
    sampleSensors();
    delay(1);
    ++attempts;
  }
  assert(sequenceNumber == target);
  if (monitoring) {
    assert(bus.requests.size() == 3);
    const uint16_t expectedRegs[] = {2, 3, 0};
    const uint32_t expectedBauds[] = {4800, 4800, 9600};
    for (size_t i = 0; i < 3; ++i) {
      const auto &request = bus.requests[i];
      assert(request.baud == expectedBauds[i] && request.frame[0] == 1);
      assert(request.frame[1] == 3);
      assert((uint16_t(request.frame[2]) << 8 | request.frame[3]) == expectedRegs[i]);
    }
  } else assert(bus.requests.empty());
  assert(current.distance == 100);
}

int main() {
  assert(monitoring); // No readiness/setup gate at startup.
  runSample(true);
  assert(current.ec == 1000 && std::fabs(current.ph - 5.6f) < 0.001f && current.co2 == 536);
  assert(soilReady && co2Ready);
  runSample(false);
  assert(std::isnan(current.ec) && std::isnan(current.ph) && std::isnan(current.co2));
  assert(!soilReady && !co2Ready); // Previous successful readings are cleared.
  assert(std::strstr(ecStatus, "No reply") && std::strstr(phStatus, "No reply"));
  runSample(true, true);
  assert(current.ec == 1000 && std::isnan(current.ph) && current.co2 == 536);
  assert(!soilReady && co2Ready);
  assert(std::strcmp(ecStatus, "Live") == 0 && std::strstr(phStatus, "No reply"));
  testSoilPh = 200;
  runSample(true);
  assert(std::isnan(current.ph) && std::strcmp(phStatus, "pH outside expected range") == 0);
  assert(current.co2 == 536);
  monitoring = false;
  runSample(true);
  assert(std::isnan(current.ec) && std::isnan(current.co2));
  assert(std::strcmp(co2Status, "Monitoring paused") == 0);
  std::cout << "Direct monitor tests passed with simulated hardware: automatic reads, correct ID/register/baud order, echo/noise replies, bounded timeouts, clearing stale values, partial soil replies, pH range check, pause without bus requests.\n";
}
