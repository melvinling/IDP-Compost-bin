#pragma once
#include <stdint.h>
#include <stddef.h>
#include "Controller.h"
namespace Api {
bool unsignedNumber(const char *text, uint32_t &number);
ControlReply validateControl(const char *method, const char *contentType, const char *token,
  const char *expectedToken, size_t argumentCount, const char *argumentName,
  const char *argumentValue, ControlAction &action);
}
