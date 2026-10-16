#pragma once

#include <cassert>

inline bool testRendererBusy = false;
inline bool testRenderLocked = false;

class RenderLock {
 public:
  RenderLock() {
    assert(!testRenderLocked);
    testRenderLocked = true;
  }
  ~RenderLock() { testRenderLocked = false; }
  static bool peek() { return testRendererBusy; }
};
