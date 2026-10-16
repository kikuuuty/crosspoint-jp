#pragma once

// A navigation gesture belongs to the screen where it started. Keep both the
// held state and its eventual release out of the next screen, without blocking
// that screen's loop (network/background work must keep running).
class InputTransitionGuard {
  bool suppressed = false;

 public:
  void suppressUntilRelease() { suppressed = true; }

  void update(bool anyHeld, bool anyEvent) {
    if (!anyHeld && !anyEvent) {
      suppressed = false;
    }
  }

  bool isSuppressed() const { return suppressed; }
};
