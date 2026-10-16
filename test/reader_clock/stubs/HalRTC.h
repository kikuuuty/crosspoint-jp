#pragma once

#include <cassert>
#include <ctime>

#include "activities/RenderLock.h"

struct TestRTC {
  bool available = true;
  bool failWrite = false;
  int writes = 0;
  tm saved = {};
  bool isAvailable() const { return available; }
  bool writeTime(const tm& utc) {
    assert(testRenderLocked);  // Never perform I2C from the SNTP callback.
    ++writes;
    if (failWrite) return false;
    saved = utc;
    return true;
  }
};
inline TestRTC halRTC;
