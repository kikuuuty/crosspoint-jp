#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

inline uint32_t testClock = 0;
inline std::vector<unsigned long> testDelays;
inline uint32_t millis() { return ++testClock; }
inline uint32_t micros() { return ++testClock; }
inline void delay(unsigned long ms) { testDelays.push_back(ms); }
inline struct TestEsp {
  uint32_t getFreeHeap() const { return 1024 * 1024; }
} ESP;
