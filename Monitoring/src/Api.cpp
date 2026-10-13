#include "Api.h"
#include <string.h>
bool Api::unsignedNumber(const char *s, uint32_t &n) {
  if (!s || !*s) return false;
  uint64_t value = 0;
  for (; *s; ++s) { if (*s < '0' || *s > '9') return false; value = value*10+(*s-'0'); if (value > UINT32_MAX) return false; }
  n = uint32_t(value); return true;
}
ControlReply Api::validateControl(const char *method, const char *type, const char *token,
  const char *expected, size_t count, const char *name, const char *value, ControlAction &action) {
  if (strcmp(method, "POST")) return {false, "Use POST /api/control"};
  if (strcmp(type, "application/x-www-form-urlencoded")) return {false, "Use application/x-www-form-urlencoded"};
  if (!*expected || strcmp(token, expected)) return {false, "Missing or invalid control token"};
  if (count != 1 || strcmp(name, "command") || !parseControlAction(value, action)) return {false, "Expected exactly one supported command"};
  return {true, "Valid command"};
}
