#pragma once

#include <cstdint>

class HalGPIO {
 public:
  static constexpr uint8_t BTN_BACK = 0;
  static constexpr uint8_t BTN_CONFIRM = 1;
  static constexpr uint8_t BTN_LEFT = 2;
  static constexpr uint8_t BTN_RIGHT = 3;
  static constexpr uint8_t BTN_UP = 4;
  static constexpr uint8_t BTN_DOWN = 5;
  static constexpr uint8_t BTN_POWER = 6;

  uint8_t held = 0;
  uint8_t pressed = 0;
  uint8_t released = 0;
  unsigned long heldTime = 0;
  bool x3 = false;

  void update() {}
  bool deviceIsX3() const { return x3; }
  bool isPressed(uint8_t button) const { return held & (1 << button); }
  bool wasPressed(uint8_t button) const { return pressed & (1 << button); }
  bool wasReleased(uint8_t button) const { return released & (1 << button); }
  bool wasAnyPressed() const { return pressed != 0; }
  bool wasAnyReleased() const { return released != 0; }
  unsigned long getHeldTime() const { return heldTime; }
};
