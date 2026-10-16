#include <cstdio>
#include <initializer_list>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"

CrossPointSettings testSettings;

namespace {
int checks = 0;
int failures = 0;

void check(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    std::printf("FAIL: %s\n", message);
    ++failures;
  }
}

void frame(HalGPIO& gpio, MappedInputManager& input, uint8_t held, uint8_t pressed, uint8_t released) {
  gpio.held = held;
  gpio.pressed = pressed;
  gpio.released = released;
  input.update();
}

void checkSuppressed(const MappedInputManager& input) {
  for (const auto button :
       {MappedInputManager::Button::Back, MappedInputManager::Button::Confirm, MappedInputManager::Button::Left,
        MappedInputManager::Button::Right, MappedInputManager::Button::Up, MappedInputManager::Button::Down,
        MappedInputManager::Button::PageBack, MappedInputManager::Button::PageForward,
        MappedInputManager::Button::ValueIncrease, MappedInputManager::Button::ValueDecrease,
        MappedInputManager::Button::Power}) {
    check(!input.isPressed(button), "held state must not cross a screen transition");
    check(!input.wasPressed(button), "press event must not cross a screen transition");
    check(!input.wasReleased(button), "release event must not cross a screen transition");
  }
  check(!input.wasAnyPressed(), "aggregate press is suppressed");
  check(!input.wasAnyReleased(), "aggregate release is suppressed");
  check(input.getHeldTime() == 0, "long-press duration is suppressed");
  check(input.getPressedFrontButton() == -1, "button remapping does not consume the opening gesture");
}

void testPressTransition(uint8_t hardwareButton, bool isX3, MappedInputManager::Orientation orientation) {
  HalGPIO gpio;
  gpio.x3 = isX3;
  MappedInputManager input(gpio);
  input.setEffectiveOrientation(orientation);
  const uint8_t bit = 1 << hardwareButton;
  frame(gpio, input, bit, bit, 0);
  check(input.wasAnyPressed(), "normal press is visible before navigation");

  input.suppressUntilRelease();  // Push, Pop and Replace use the same barrier.
  checkSuppressed(input);
  gpio.heldTime = 5000;
  for (int i = 0; i < 5; ++i) {
    frame(gpio, input, bit, 0, 0);
    checkSuppressed(input);
    check(gpio.isPressed(hardwareButton), "raw HAL state remains available to power management");
  }
  frame(gpio, input, 0, 0, bit);
  checkSuppressed(input);
  frame(gpio, input, 0, 0, 0);
  frame(gpio, input, bit, bit, 0);
  check(input.wasAnyPressed(), "a fresh press after release is accepted");
  frame(gpio, input, 0, 0, bit);
  check(input.wasAnyReleased(), "a fresh release after navigation is accepted");
}

void testReleaseTransition() {
  HalGPIO gpio;
  MappedInputManager input(gpio);
  const uint8_t confirm = 1 << HalGPIO::BTN_CONFIRM;
  frame(gpio, input, 0, 0, confirm);
  input.suppressUntilRelease();
  checkSuppressed(input);
  frame(gpio, input, 0, 0, 0);
  frame(gpio, input, confirm, confirm, 0);
  check(input.wasPressed(MappedInputManager::Button::Confirm), "release-triggered navigation accepts next gesture");
}

void testRemapAndRotation() {
  HalGPIO gpio;
  MappedInputManager input(gpio);
  frame(gpio, input, 1 << HalGPIO::BTN_BACK, 1 << HalGPIO::BTN_BACK, 0);
  input.suppressUntilRelease();

  // Change both orientation and the physical mapping while the old key is down.
  testSettings.frontButtonBack = 2;
  testSettings.frontButtonConfirm = 3;
  testSettings.frontButtonLeft = 0;
  testSettings.frontButtonRight = 1;
  input.setEffectiveOrientation(MappedInputManager::Orientation::PortraitInverted);
  frame(gpio, input, 1 << HalGPIO::BTN_BACK, 0, 0);
  checkSuppressed(input);
  frame(gpio, input, 0, 0, 1 << HalGPIO::BTN_BACK);
  checkSuppressed(input);
  frame(gpio, input, 0, 0, 0);
  frame(gpio, input, 1 << HalGPIO::BTN_CONFIRM, 1 << HalGPIO::BTN_CONFIRM, 0);
  check(input.wasPressed(MappedInputManager::Button::Back), "new Back mapping works after rotation and release");
  testSettings = {};
}

void testChainedTransitions() {
  HalGPIO gpio;
  MappedInputManager input(gpio);
  const uint8_t back = 1 << HalGPIO::BTN_BACK;
  frame(gpio, input, back, back, 0);
  input.suppressUntilRelease();
  input.suppressUntilRelease();  // e.g. Wi-Fi cancellation pops two activities.
  frame(gpio, input, 0, 0, back);
  checkSuppressed(input);

  // A second key held during the transition must also finish its gesture.
  const uint8_t right = 1 << HalGPIO::BTN_RIGHT;
  frame(gpio, input, right, right, 0);
  checkSuppressed(input);
  frame(gpio, input, 0, 0, right);
  checkSuppressed(input);
  frame(gpio, input, 0, 0, 0);
  frame(gpio, input, back, back, 0);
  check(input.wasPressed(MappedInputManager::Button::Back), "input recovers after chained transitions");
}
}  // namespace

int main() {
  for (bool isX3 : {false, true}) {
    for (const auto orientation :
         {MappedInputManager::Orientation::Portrait, MappedInputManager::Orientation::PortraitInverted,
          MappedInputManager::Orientation::LandscapeClockwise,
          MappedInputManager::Orientation::LandscapeCounterClockwise}) {
      for (uint8_t button = 0; button <= HalGPIO::BTN_POWER; ++button) {
        testPressTransition(button, isX3, orientation);
      }
    }
  }
  testReleaseTransition();
  testRemapAndRotation();
  testChainedTransitions();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
