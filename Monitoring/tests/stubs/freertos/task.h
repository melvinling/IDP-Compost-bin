#pragma once
#include "FreeRTOS.h"
#include "../Arduino.h"
namespace Fake {inline void (*taskFunction)(void*)=nullptr;}
inline TickType_t xTaskGetTickCount(){return millis();}
inline int xTaskCreatePinnedToCore(void(*fn)(void*),const char*,int,void*,int,void*,int){Fake::taskFunction=fn;return pdPASS;}
inline void vTaskDelayUntil(TickType_t*,TickType_t){}
