#include "WebServerModule.h"
#include "Api.h"
#include "WebAssets.h"
#include <WiFi.h>
#include <esp_timer.h>
#include <esp_system.h>
namespace {
String quoted(const char *s) {
  String out = "\"";
  for (; *s; ++s) {
    if (*s == '"' || *s == '\\') out += '\\';
    if ((unsigned char)*s < 32) { char b[7]; snprintf(b, sizeof b, "\\u%04x", (unsigned char)*s); out += b; }
    else out += *s;
  }
  return out + "\"";
}
void number(String &out, float n) { out += isfinite(n) ? String(n, 2) : String("null"); }
const char *jsonBool(bool b) { return b ? "true" : "false"; }
String actuator(ActuatorCommand c) { return String("{\"motor\":") + jsonBool(c.motor) + ",\"pump\":" + jsonBool(c.pump) + ",\"fan\":" + jsonBool(c.fan) + "}"; }
String statusJson(const ApplicationStatus &s) {
  return String("{\"mode\":") + quoted(modeName(s.mode)) + ",\"stopLatched\":" + jsonBool(s.stopLatched) +
    ",\"relayConfigured\":" + jsonBool(s.relayConfigured) + ",\"requested\":" + actuator(s.requested) +
    ",\"commanded\":" + actuator(s.commanded) + ",\"reason\":" + quoted(s.reason) +
    ",\"prediction\":{\"modelReady\":" + jsonBool(s.prediction.modelReady) +
    ",\"command\":" + actuator(s.prediction.command) + ",\"reason\":" + quoted(s.prediction.reason) +
    ",\"action\":" + quoted(decisionName(s.prediction.action)) + ",\"confidence\":" + String(s.prediction.confidence, 6) +
    ",\"provenance\":\"synthetic_demo\",\"model\":\"Random Forest · 32 trees / depth 6\"},\"stage\":" + quoted(stageName(s.stage)) +
    ",\"stageElapsedMs\":" + String(s.stageElapsedMs) + ",\"periodicMixDue\":" + jsonBool(s.periodicMixDue) +
    ",\"postWaterMixDue\":" + jsonBool(s.postWaterMixDue) + ",\"cooldownMs\":{\"motor\":" + String(s.cooldownMs[0]) +
    ",\"pump\":" + String(s.cooldownMs[1]) + ",\"fan\":" + String(s.cooldownMs[2]) + "}}";
}
}
void WebServerModule::begin(const History &history, const SensorSnapshot &snapshot, StatusProvider status, CommandHandler command) {
  history_ = &history; snapshot_ = &snapshot; status_ = status; command_ = command;
  char boot[17], token[33];
  snprintf(boot, sizeof boot, "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
  snprintf(token, sizeof token, "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random(),
    (unsigned long)esp_random(), (unsigned long)esp_random());
  boot_ = boot; token_ = token;
  const char *headers[] = {"Content-Type", "X-Control-Token"};
  server_.collectHeaders(headers, 2);
  server_.on("/", HTTP_GET, [this]() { server_.send_P(200, "text/html; charset=utf-8", WEB_INDEX); });
  server_.on("/style.css", HTTP_GET, [this]() { server_.send_P(200, "text/css; charset=utf-8", WEB_STYLE); });
  server_.on("/app.js", HTTP_GET, [this]() { server_.send_P(200, "text/javascript; charset=utf-8", WEB_APP); });
  server_.on("/api/data", HTTP_GET, [this]() { data(); });
  server_.on("/api/control", HTTP_POST, [this]() { control(); });
  server_.on("/api/control", HTTP_ANY, [this]() { server_.sendHeader("Allow", "POST"); reply(405, false, "Use POST /api/control"); });
  server_.onNotFound([this]() { server_.send(404, "application/json", "{\"error\":\"Not found\"}"); });
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(Config::AP_NAME, Config::AP_PASSWORD)) {
    Serial.println("AP start failed; monitoring/safety continue, dashboard unavailable.");
    return;
  }
  server_.begin();
  Serial.printf("Dashboard: http://%s/ | AP: %s\n", WiFi.softAPIP().toString().c_str(), Config::AP_NAME);
}
void WebServerModule::reply(int code, bool accepted, const char *message) {
  server_.sendHeader("Cache-Control", "no-store");
  String body = String("{\"accepted\":") + jsonBool(accepted) + ",\"message\":" + quoted(message) +
    ",\"status\":" + statusJson(status_()) + "}";
  server_.send(code, "application/json", body);
}
void WebServerModule::control() {
  ControlAction action{};
  String type = server_.header("Content-Type"); int semicolon = type.indexOf(';');
  if (semicolon >= 0) type = type.substring(0, semicolon);
  const auto checked = Api::validateControl("POST", type.c_str(), server_.header("X-Control-Token").c_str(),
    token_.c_str(), server_.args(), server_.args() ? server_.argName(0).c_str() : "",
    server_.args() ? server_.arg(0).c_str() : "", action);
  if (!checked.accepted) { reply(type != "application/x-www-form-urlencoded" ? 415 : 400, false, checked.message); return; }
  const auto result = command_(action);
  reply(result.accepted ? 200 : 409, result.accepted, result.message);
}
void WebServerModule::data() {
  uint32_t after = 0;
  if (server_.hasArg("after") && !Api::unsignedNumber(server_.arg("after").c_str(), after)) { reply(400, false, "Invalid sequence"); return; }
  const bool reset = server_.arg("boot") != boot_ || after > snapshot_->sequence;
  if (reset) after = 0;
  const uint64_t uptime = uint64_t(esp_timer_get_time()) / 1000;
  String out; out.reserve(6500);
  out = String("{\"schemaVersion\":2,\"sampleMs\":") + String(Config::SAMPLE_MS) + ",\"freshMs\":" + String(Config::FRESH_MS) +
    ",\"boot\":" + quoted(boot_.c_str()) + ",\"controlToken\":" + quoted(token_.c_str()) +
    ",\"uptime\":" + String((unsigned long long)uptime) + ",\"reset\":" + jsonBool(reset) +
    ",\"sequence\":" + String(snapshot_->sequence) + ",\"status\":" + statusJson(status_()) +
    ",\"latest\":{\"sequence\":" + String(snapshot_->sequence) + ",\"uptime\":" + String((unsigned long long)snapshot_->uptimeMs) + ",\"readings\":[";
  const Reading *readings[] = {&snapshot_->distance, &snapshot_->ec, &snapshot_->ph, &snapshot_->co2, &snapshot_->moisture, &snapshot_->temperature};
  for (unsigned i=0; i<6; ++i) {
    if (i) out += ',';
    const Reading &r = *readings[i];
    const uint32_t age = r.valid() ? uint32_t(millis()-r.acquiredMs) : 0;
    const bool fresh = r.valid() && age < Config::FRESH_MS;
    out += "{\"value\":"; number(out, fresh ? r.value : NAN);
    out += String(",\"valid\":") + jsonBool(fresh) + ",\"status\":" + quoted(r.valid() && !fresh ? "Stale reading" : statusName(r.status)) +
      ",\"lastSuccessMs\":" + String((unsigned long long)r.lastSuccessMs) +
      ",\"ageMs\":" + String(age) +
      ",\"exceptionCode\":" + String(r.exceptionCode) + "}";
  }
  out += "]},\"samples\":[";
  size_t sent = 0; uint32_t next = after; bool more = false;
  for (size_t i=0; i<history_->count(); ++i) {
    const auto &row = history_->at(i);
    if (row.sequence <= after) continue;
    if (sent == Config::API_PAGE_SIZE) { more = true; break; }
    if (sent++) out += ',';
    out += "[" + String(row.sequence) + "," + String((unsigned long long)row.uptimeMs);
    for (float v : row.values) { out += ','; number(out, v); }
    out += ']'; next = row.sequence;
  }
  out += String("],\"next\":") + String(next) + ",\"more\":" + jsonBool(more) + "}";
  server_.sendHeader("Cache-Control", "no-store"); server_.send(200, "application/json", out);
}
