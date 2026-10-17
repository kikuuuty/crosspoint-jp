#pragma once

#include <cstdint>

inline uint32_t fontTestClock = 0;
inline uint32_t millis() { return ++fontTestClock; }
inline uint32_t micros() { return ++fontTestClock; }
