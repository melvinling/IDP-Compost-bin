/*
  Local Wi-Fi monitoring for one compost bin.
  A0221AM PWM: yellow/control GPIO27, white/output GPIO35, red 3.3V.
  SN-3001-TR-ECPH-N01: EC and pH, factory address 1 at 4800 baud.
  XM7870B: CO2, factory address 1 at 9600 baud.
  XY-485 auto-direction adapter: ESP32 TX GPIO17, RX GPIO16.
  Read START-HERE.md before connecting/programming the RS485 devices.
  No relay or motor control is included.
*/
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_timer.h>
#include <math.h>
#include "ModbusProtocol.h"
#include "Dashboard.h"

const char *WIFI_NAME = "CompostBin";
const char *WIFI_PASSWORD = "compost123"; // Change if desired: at least 8 characters.
constexpr int ULTRASONIC_TRIGGER = 27;
constexpr int ULTRASONIC_OUTPUT = 35;
constexpr int RS485_RX = 16;
constexpr int RS485_TX = 17;
constexpr uint8_t SOIL_ADDRESS = 1;
constexpr uint8_t CO2_ADDRESS = 1;
constexpr uint32_t SOIL_BAUD = 4800;
constexpr uint32_t CO2_BAUD = 9600;
constexpr uint32_t SAMPLE_MS = 2000;
constexpr uint32_t RESPONSE_MS = 600;
constexpr size_t HISTORY_SIZE = 900; // About 30 minutes in RAM.

WebServer server(80);
HardwareSerial bus(2);

struct Sample {
  uint64_t timeMs;
  uint32_t sequence;
  float distance, ec, ph, co2;
};
Sample history[HISTORY_SIZE];
Sample current;
size_t historyHead = 0, historyCount = 0;
uint32_t sequenceNumber = 0, nextSample = 0;
uint32_t bootId = 0;
bool soilReady = false, co2Ready = false, monitoring = true;
const char *distanceStatus = "Waiting for first sample";
const char *ecStatus = "Waiting for first read";
const char *phStatus = "Waiting for first read";
const char *co2Status = "Waiting for first read";
String commandLine;
uint32_t busBaud = SOIL_BAUD;
bool traceReads = false;
bool rxInverted = false; // Diagnostic only; TX and sensor settings remain normal.
uint8_t receivedTrace[64];
size_t receivedCount = 0;
ModbusProtocol::ReplyStream replyStream;

constexpr uint32_t SCAN_BAUDS[] = {9600, 4800, 2400, 19200, 38400, 115200};
// Every probe is a single holding-register read. Never write on this shared scan.
constexpr uint16_t SCAN_REGS[] = {0x07D0, 0x07D1, 0x0002, 0x0003,
                                0x0066, 0x0067, 0x0000};
struct SharedScan {
  bool active = false, resumeMonitoring = false;
  uint8_t address = 1, lastAddress = 10, baudIndex = 0, step = 0;
  uint32_t queries = 0, valid = 0, exceptions = 0, noBytes = 0, badBytes = 0;
  uint32_t soilCandidates = 0, co2Candidates = 0, started = 0;
} scan;

struct Transaction {
  bool active = false;
  uint8_t address = 0, count = 0;
  uint8_t frame[32];
  size_t length = 0, expected = 0;
  uint32_t started = 0;
} transaction;
enum class Phase { Idle, SoilEc, SoilPh, Co2 };
Phase phase = Phase::Idle;

void clearBus() { while (bus.available()) bus.read(); }

void setBusBaud(uint32_t baud) {
  // Keep the UART driver/pins and chosen RX polarity when changing speed.
  bus.flush(true);
  clearBus();
  if (busBaud != baud) bus.updateBaudRate(baud);
  busBaud = baud;
  delay(20);
  clearBus();
}

void setReceivePolarity(bool inverted) {
  // Abandon the current transaction; never combine bytes from two polarities.
  transaction.active = false;
  phase = Phase::Idle;
  bus.flush(true);
  clearBus();
  if (!bus.setRxInvert(inverted)) {
    Serial.println("RX polarity change failed; previous setting retained.");
    return;
  }
  rxInverted = inverted;
  replyStream.reset();
  receivedCount = 0;
  nextSample = millis();
  Serial.printf("RX polarity: %s | TX unchanged | diagnostic only, CRC validation stays enabled\n",
      rxInverted ? "INVERTED" : "NORMAL");
}

void startRead(uint8_t address, uint16_t reg, uint8_t count) {
  delay((40000UL + busBaud - 1) / busBaud + 1); // RTU silent interval, including 2400 baud.
  clearBus();
  uint8_t request[8];
  ModbusProtocol::request(request, address, 3, reg, count);
  receivedCount = 0;
  if (traceReads) {
    Serial.printf("CHECK: baud=%lu ID=%u register=0x%04X count=%u | TX GPIO%d RX GPIO%d\n",
        (unsigned long)busBaud, address, reg, count, RS485_TX, RS485_RX);
    Serial.print("TX:");
    for (uint8_t byte : request) Serial.printf(" %02X", byte);
    Serial.println();
  }
  transaction.active = true;
  transaction.address = address;
  transaction.count = count;
  transaction.length = transaction.expected = 0;
  replyStream.reset();
  bus.write(request, sizeof(request));
  bus.flush();
  transaction.started = millis();
}

// 0 pending, 1 success, -1 timeout, -2 invalid frame, -3 exception.
int serviceRead(uint16_t *values) {
  while (bus.available()) {
    const uint8_t byte = bus.read();
    if (receivedCount < sizeof(receivedTrace)) receivedTrace[receivedCount++] = byte;
    const int result = replyStream.feed(byte, transaction.address, transaction.count, values);
    if (result) {
      transaction.length = replyStream.length;
      memcpy(transaction.frame, replyStream.frame, transaction.length);
      transaction.active = false;
      return result;
    }
  }
  if (uint32_t(millis() - transaction.started) >= RESPONSE_MS) {
    transaction.active = false;
    return receivedCount ? -2 : -1;
  }
  return 0;
}

void pauseMonitoring() {
  monitoring = false;
  transaction.active = false;
  phase = Phase::Idle;
  clearBus();
  soilReady = co2Ready = false;
  ecStatus = phStatus = co2Status = "Monitoring paused";
}

uint32_t scanBaudFromCode(bool soil, uint16_t code) {
  const uint32_t soilRates[] = {2400, 4800, 9600};
  const uint32_t co2Rates[] = {2400, 4800, 9600, 19200, 38400, 115200};
  if (soil) return code < 3 ? soilRates[code] : 0;
  return code >= 1 && code <= 6 ? co2Rates[code - 1] : 0;
}

void finishSharedScan(bool cancelled) {
  scan.active = false;
  transaction.active = false;
  setBusBaud(9600);
  Serial.printf("SCAN %s | queries=%lu valid=%lu exceptions=%lu no-RX=%lu invalid-RX=%lu\n",
      cancelled ? "STOPPED" : "FINISHED", (unsigned long)scan.queries,
      (unsigned long)scan.valid, (unsigned long)scan.exceptions,
      (unsigned long)scan.noBytes, (unsigned long)scan.badBytes);
  Serial.printf("CANDIDATES | soil=%lu CO2=%lu | elapsed=%lu seconds\n",
      (unsigned long)scan.soilCandidates, (unsigned long)scan.co2Candidates,
      (unsigned long)((millis() - scan.started) / 1000));
  monitoring = scan.resumeMonitoring;
  phase = Phase::Idle;
  nextSample = millis();
  Serial.println(monitoring ? "No sensor settings written. Direct monitoring resumes automatically." :
      "No sensor settings written. Monitoring stays paused; send START MONITORING to resume.");
  if (!scan.valid && !scan.exceptions)
    Serial.println("No valid replies. Check adapter signal direction/levels, GND, A/B and sensor power. Unknown settings or colliding replies remain possible.");
  Serial.println("Copy CANDIDATE, READ and SCAN FINISHED lines. Same-ID/same-baud devices cannot be identified reliably by this scan.");
}

void startSharedScan(bool full) {
  const bool resume = monitoring;
  pauseMonitoring();
  traceReads = false;
  scan = SharedScan{};
  scan.active = true;
  scan.resumeMonitoring = resume;
  scan.lastAddress = full ? 247 : 10;
  scan.started = millis();
  setBusBaud(SCAN_BAUDS[0]);
  Serial.printf("SHARED BUS SCAN v3 | both sensors stay connected | IDs 1-%u | 8N1\n", scan.lastAddress);
  Serial.println("Read-only: 9600, 4800, 2400, 19200, 38400, 115200 baud. Send STOP SCAN to cancel.");
  Serial.println(full ? "Full scan may take about 30 minutes with no replies." : "Common-ID scan takes about 80 seconds with no replies.");
  Serial.printf("SCAN PROGRESS | baud=%lu ID=%u\n", (unsigned long)busBaud, scan.address);
}

void serviceSharedScan() {
  if (!scan.active) return;
  if (!transaction.active) {
    startRead(scan.address, SCAN_REGS[scan.step], 1);
    ++scan.queries;
    return;
  }
  uint16_t value[1];
  const int result = serviceRead(value);
  if (!result) return;
  if (result == 1) ++scan.valid;
  else if (result == -3) ++scan.exceptions;
  else if (receivedCount) ++scan.badBytes;
  else ++scan.noBytes;
  if (receivedCount) {
    Serial.printf("SCAN REPLY | baud=%lu ID=%u reg=0x%04X | ",
        (unsigned long)busBaud, scan.address, SCAN_REGS[scan.step]);
    if (result == 1) Serial.printf("valid value=%u", value[0]);
    else if (result == -3) Serial.printf("exception=%u", transaction.frame[2]);
    else Serial.print("bytes received; no matching CRC-valid reply");
    Serial.print(" | RX:");
    for (size_t i = 0; i < receivedCount; ++i) Serial.printf(" %02X", receivedTrace[i]);
    Serial.println();
  }
  // Gate measurement reads on both documented configuration registers.
  // These are candidate signatures, not proof of unique sensor identity.
  switch (scan.step) {
    case 0: scan.step = result == 1 && value[0] == scan.address ? 1 : 4; break;
    case 1:
      if (result == 1 && scanBaudFromCode(true, value[0]) == busBaud) {
        ++scan.soilCandidates;
        Serial.printf("CANDIDATE SOIL | ID=%u baud=%lu | configuration matches soil manual\n", scan.address, (unsigned long)busBaud);
        scan.step = 2;
      } else scan.step = 4;
      break;
    case 2:
      if (result == 1) Serial.printf("READ SOIL | ID=%u baud=%lu | EC=%u uS/cm\n", scan.address, (unsigned long)busBaud, value[0]);
      scan.step = 3; break;
    case 3:
      if (result == 1) {
        if (value[0] <= 140) Serial.printf("READ SOIL | ID=%u baud=%lu | pH=%.1f\n", scan.address, (unsigned long)busBaud, value[0] / 10.0f);
        else Serial.println("READ SOIL | pH outside expected range; not accepted");
      }
      scan.step = 4; break;
    case 4: scan.step = result == 1 && value[0] == scan.address ? 5 : 7; break;
    case 5:
      if (result == 1 && scanBaudFromCode(false, value[0]) == busBaud) {
        ++scan.co2Candidates;
        Serial.printf("CANDIDATE CO2 | ID=%u baud=%lu | configuration matches XM7870B manual\n", scan.address, (unsigned long)busBaud);
        scan.step = 6;
      } else scan.step = 7;
      break;
    case 6:
      if (result == 1) Serial.printf("READ CO2 | ID=%u baud=%lu | CO2=%u ppm\n", scan.address, (unsigned long)busBaud, value[0]);
      scan.step = 7; break;
  }
  if (scan.step < 7) return;
  scan.step = 0;
  if (scan.address < scan.lastAddress) ++scan.address;
  else {
    scan.address = 1;
    if (++scan.baudIndex >= sizeof(SCAN_BAUDS) / sizeof(SCAN_BAUDS[0])) {
      finishSharedScan(false);
      return;
    }
    setBusBaud(SCAN_BAUDS[scan.baudIndex]);
  }
  if (scan.address == 1 || scan.address % 5 == 0)
    Serial.printf("SCAN PROGRESS | baud=%lu ID=%u\n", (unsigned long)busBaud, scan.address);
}

void processSerial() {
  while (Serial.available()) {
    const char character = Serial.read();
    if (character == '\r' || character == '\n') {
      if (!commandLine.length()) continue;
      commandLine.trim();
      commandLine.toUpperCase();
      if (commandLine == "STOP SCAN") {
        if (scan.active) finishSharedScan(true);
        else Serial.println("No scan is running.");
      }
      else if (scan.active) Serial.println("Scan in progress. Send STOP SCAN before another command.");
      else if (commandLine == "SCAN BUS") startSharedScan(false);
      else if (commandLine == "SCAN BUS FULL") startSharedScan(true);
      else if (commandLine == "STOP MONITORING") pauseMonitoring();
      else if (commandLine == "RX INVERTED") setReceivePolarity(true);
      else if (commandLine == "RX NORMAL") setReceivePolarity(false);
      else if (commandLine == "START MONITORING") {
        transaction.active = false;
        phase = Phase::Idle;
        monitoring = true;
        nextSample = millis();
        Serial.printf("Direct monitoring started: soil ID%u/%lu, CO2 ID%u/%lu. No setup command needed.\n",
            SOIL_ADDRESS, (unsigned long)SOIL_BAUD, CO2_ADDRESS, (unsigned long)CO2_BAUD);
      } else if (commandLine == "TRACE ON") {
        traceReads = true;
        Serial.println("Raw RS485 trace enabled.");
      } else if (commandLine == "TRACE OFF") {
        traceReads = false;
        Serial.println("Raw RS485 trace disabled.");
      } else Serial.println("Commands: START MONITORING, STOP MONITORING, TRACE ON, TRACE OFF, RX INVERTED, RX NORMAL, SCAN BUS, SCAN BUS FULL, STOP SCAN");
      commandLine = "";
    } else if (commandLine.length() < 80) commandLine += character;
  }
}

void commitSample() {
  current.sequence = ++sequenceNumber;
  history[historyHead] = current;
  historyHead = (historyHead + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) ++historyCount;
  phase = Phase::Idle;
  Serial.printf("Sample %lu | distance ", (unsigned long)current.sequence);
  if (isfinite(current.distance)) Serial.print(current.distance, 1); else Serial.print("unavailable");
  Serial.print(" | EC ");
  if (isfinite(current.ec)) Serial.print(current.ec, 0); else Serial.print("unavailable");
  Serial.print(" | pH ");
  if (isfinite(current.ph)) Serial.print(current.ph, 1); else Serial.print("unavailable");
  Serial.print(" | CO2 ");
  if (isfinite(current.co2)) Serial.print(current.co2, 0); else Serial.print("unavailable");
  Serial.println();
}

const char *transactionStatus(int result) {
  if (result == -1) return "No reply: check wiring, address and power";
  if (result == -3) return "Sensor rejected register request";
  return "Invalid reply: check bus wiring and addresses";
}

void reportMeasurementRead(const char *name, int result, uint16_t value) {
  Serial.printf("RS485 %s | ID=%u baud=%lu | ", name, transaction.address, (unsigned long)busBaud);
  if (result == 1) Serial.printf("OK raw=%u", value);
  else if (result == -1) Serial.print("no received bytes");
  else if (result == -3) Serial.printf("exception=%u", transaction.frame[2]);
  else Serial.print("received bytes, no matching CRC-valid reply");
  if (traceReads && receivedCount) {
    Serial.print(" | RX:");
    for (size_t i = 0; i < receivedCount; ++i) Serial.printf(" %02X", receivedTrace[i]);
  }
  Serial.println();
}

void sampleSensors() {
  const uint32_t now = millis();
  if (phase == Phase::Idle && int32_t(now - nextSample) >= 0) {
    nextSample = now + SAMPLE_MS;
    current.timeMs = uint64_t(esp_timer_get_time()) / 1000;
    current.distance = current.ec = current.ph = current.co2 = NAN;
    soilReady = co2Ready = false;
    digitalWrite(ULTRASONIC_TRIGGER, LOW);
    delayMicroseconds(20);
    digitalWrite(ULTRASONIC_TRIGGER, HIGH);
    const unsigned long pulse = pulseIn(ULTRASONIC_OUTPUT, HIGH, 60000UL);
    if (traceReads) Serial.printf("ULTRASONIC | GPIO%d pulse_us=%lu\n", ULTRASONIC_OUTPUT, pulse);
    const float distance = pulse / 57.5f;
    if (!pulse) distanceStatus = "No signal: check wiring and power";
    else if (pulse >= 34000UL || distance > 450.0f || distance < 3.0f)
      distanceStatus = "No target within 3-450 cm";
    else { current.distance = distance; distanceStatus = "Live"; }
    if (!monitoring) {
      ecStatus = phStatus = co2Status = scan.active ? "Shared-bus scan in progress" : "Monitoring paused";
      commitSample();
    } else {
      // Same shared UART: change the ESP32 baud only, never sensor settings.
      setBusBaud(SOIL_BAUD);
      phase = Phase::SoilEc;
      startRead(SOIL_ADDRESS, 0x0002, 1);
      return;
    }
  }
  if (phase == Phase::Idle) return;
  uint16_t values[1] = {};
  const int result = serviceRead(values);
  if (!result) return;
  if (phase == Phase::SoilEc) {
    reportMeasurementRead("soil EC", result, values[0]);
    if (result == 1) { current.ec = values[0]; ecStatus = "Live"; }
    else ecStatus = transactionStatus(result);
    phase = Phase::SoilPh;
    startRead(SOIL_ADDRESS, 0x0003, 1);
  } else if (phase == Phase::SoilPh) {
    reportMeasurementRead("soil pH", result, values[0]);
    if (result == 1 && values[0] <= 140) { current.ph = values[0] / 10.0f; phStatus = "Live"; }
    else phStatus = result == 1 ? "pH outside expected range" : transactionStatus(result);
    soilReady = isfinite(current.ec) && isfinite(current.ph);
    setBusBaud(CO2_BAUD);
    phase = Phase::Co2;
    startRead(CO2_ADDRESS, 0x0000, 1);
  } else {
    reportMeasurementRead("CO2", result, values[0]);
    if (result == 1) { current.co2 = values[0]; co2Status = "Live"; co2Ready = true; }
    else co2Status = transactionStatus(result);
    commitSample();
  }
}

void appendNumber(String &json, float value, uint8_t decimals = 1) {
  if (isfinite(value)) json += String(value, static_cast<unsigned int>(decimals)); else json += "null";
}

void serveData() {
  uint32_t after = strtoul(server.arg("after").c_str(), nullptr, 10);
  const String boot = String(bootId, HEX);
  if (server.arg("boot") != boot || after > sequenceNumber) after = 0;
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  String chunk;
  chunk.reserve(1400);
  chunk = "{\"boot\":\"" + boot + "\",\"sequence\":" + String(sequenceNumber);
  chunk += ",\"uptime\":" + String(uint64_t(esp_timer_get_time()) / 1000);
  chunk += ",\"monitoring\":";
  chunk += monitoring ? "true" : "false";
  chunk += ",\"soilReady\":"; chunk += soilReady ? "true" : "false";
  chunk += ",\"co2Ready\":"; chunk += co2Ready ? "true" : "false";
  chunk += ",\"status\":[\"" + String(distanceStatus) + "\",\"" + String(ecStatus);
  chunk += "\",\"" + String(phStatus) + "\",\"" + String(co2Status) + "\"],\"samples\":[";
  bool first = true;
  const size_t oldest = (historyHead + HISTORY_SIZE - historyCount) % HISTORY_SIZE;
  for (size_t i = 0; i < historyCount; ++i) {
    const Sample &sample = history[(oldest + i) % HISTORY_SIZE];
    if (sample.sequence <= after) continue;
    if (!first) chunk += ',';
    first = false;
    chunk += '['; chunk += String(sample.sequence); chunk += ',';
    chunk += String(sample.timeMs); chunk += ',';
    appendNumber(chunk, sample.distance); chunk += ',';
    appendNumber(chunk, sample.ec, 0); chunk += ',';
    appendNumber(chunk, sample.ph); chunk += ',';
    appendNumber(chunk, sample.co2, 0); chunk += ']';
    if (chunk.length() > 1100) { server.sendContent(chunk); chunk = ""; }
  }
  chunk += "]}";
  server.sendContent(chunk);
  server.sendContent("");
}

void setup() {
  Serial.begin(115200);
  digitalWrite(ULTRASONIC_TRIGGER, HIGH);
  pinMode(ULTRASONIC_TRIGGER, OUTPUT);
  pinMode(ULTRASONIC_OUTPUT, INPUT);
  bus.setRxBufferSize(256);
  bus.begin(SOIL_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);
  bus.setRxInvert(false);
  // Always start direct reads at the selected defaults. Ignore old setup flags.
  monitoring = true;
  bootId = esp_random();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  if (!WiFi.softAP(WIFI_NAME, WIFI_PASSWORD)) {
    Serial.println("Wi-Fi startup failed; check password length and supply.");
    return;
  }
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
  });
  server.on("/api/data", HTTP_GET, serveData);
  server.onNotFound([]() { server.send(404, "text/plain", "Open http://192.168.4.1/"); });
  server.begin();
  nextSample = millis() + 1000;
  Serial.println("\nCOMPOST MONITOR READY - RX diagnostic v5");
  Serial.printf("Wi-Fi: %s | Password: %s\n", WIFI_NAME, WIFI_PASSWORD);
  Serial.print("Dashboard: http://"); Serial.println(WiFi.softAPIP());
  Serial.printf("Automatic reads: soil ID=%u baud=%lu; CO2 ID=%u baud=%lu; 8N1.\n",
      SOIL_ADDRESS, (unsigned long)SOIL_BAUD, CO2_ADDRESS, (unsigned long)CO2_BAUD);
  Serial.println("No scan or setup command needed. Both sensors stay connected. Serial Monitor: 115200, Newline.");
}

void loop() {
  processSerial();
  serviceSharedScan();
  sampleSensors();
  server.handleClient();
  delay(1);
}
