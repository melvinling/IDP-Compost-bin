#pragma once
#include <stdint.h>
inline uint32_t esp_random(){static uint32_t x=1234;return x=x*1664525+1013904223;}
