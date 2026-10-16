// Execute the production Activity/ActivityManager and input mapper without a
// render task. Probe activities exercise lifecycle, stack and input boundaries.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "CrossPointSettings.h"
#include "activities/Activity.h"

CrossPointSettings testSettings;
GfxRenderer renderer;
HalGPIO gpio;
MappedInputManager input(gpio);
ActivityManager activityManager(renderer, input);

namespace {
int checks = 0;
int failures = 0;
int exits = 0;
int results = 0;
int confirms = 0;
std::string lastLoop;

void check(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    std::printf("FAIL: %s\n", message);
    ++failures;
  }
}

class ProbeActivity : public Activity {
  bool closeOnPress;

 public:
  ProbeActivity(const char* label, bool closeOnPress = false)
      : Activity(label, ::renderer, input), closeOnPress(closeOnPress) {}

  void loop() override {
    lastLoop = name;
    if (input.wasPressed(MappedInputManager::Button::Confirm) ||
        input.wasReleased(MappedInputManager::Button::Confirm)) {
      ++confirms;
    }
    if (closeOnPress ? input.wasPressed(MappedInputManager::Button::Back)
                     : input.wasReleased(MappedInputManager::Button::Back)) {
      ActivityResult result;
      result.isCancelled = true;
      setResult(std::move(result));
      finish();
    }
  }

  void onExit() override { ++exits; }
};

void frame(uint8_t held = 0, uint8_t pressed = 0, uint8_t released = 0) {
  gpio.held = held;
  gpio.pressed = pressed;
  gpio.released = released;
  input.update();
  activityManager.loop();
}

ProbeActivity* openRoot() {
  auto root = std::make_unique<ProbeActivity>("reader");
  auto* ptr = root.get();
  activityManager.replaceActivity(std::move(root));
  frame();
  frame();
  check(lastLoop == "reader", "replace activates the root activity");
  return ptr;
}

ProbeActivity* pushMenu(ProbeActivity& root) {
  auto menu = std::make_unique<ProbeActivity>("menu");
  auto* ptr = menu.get();
  root.startActivityForResult(std::move(menu), [](const ActivityResult&) { ++results; });
  frame();
  frame();
  check(lastLoop == "menu", "menu is pushed above reader");
  return ptr;
}

void testInitialScreenPreservesBootInput() {
  gpio.held = 1 << HalGPIO::BTN_BACK;
  gpio.pressed = gpio.held;
  input.update();
  activityManager.replaceActivity(std::make_unique<ProbeActivity>("boot"));
  check(input.isPressed(MappedInputManager::Button::Back), "initial Boot screen preserves the Home startup shortcut");
  frame();
}

void testNestedReturnAndHold() {
  auto* root = openRoot();
  auto* menu = pushMenu(*root);
  const int before = exits;
  const int beforeResults = results;
  menu->startActivityForResult(std::make_unique<ProbeActivity>("settings", true), [](const ActivityResult& result) {
    check(result.isCancelled, "child cancellation result reaches its parent");
    ++results;
  });
  frame();
  frame();
  check(lastLoop == "settings", "settings is nested under menu");

  const uint8_t back = 1 << HalGPIO::BTN_BACK;
  frame(back, back);
  check(exits == before + 1, "Back press closes exactly one child");
  gpio.heldTime = 5000;
  for (int i = 0; i < 5; ++i) frame(back);
  check(lastLoop == "menu", "parent loop keeps running during a held navigation key");
  frame(0, 0, back);
  check(exits == before + 1, "the child's key release does not close the menu");
  check(results == beforeResults + 1, "reader's result handler was not overwritten by the child");

  frame();
  frame(back, back);
  frame(0, 0, back);
  check(exits == before + 2, "a fresh Back closes the menu");
  frame();
  check(lastLoop == "reader", "reader remains after returning from menu");
  check(results == beforeResults + 2, "reader receives the menu result once");
}

void testPressOpenedChildDoesNotConfirmOnRelease() {
  auto* root = openRoot();
  const uint8_t confirm = 1 << HalGPIO::BTN_CONFIRM;
  root->startActivityForResult(std::make_unique<ProbeActivity>("slider"), [](const ActivityResult&) {});
  frame(confirm, confirm);
  const int before = confirms;
  frame(confirm);
  frame(0, 0, confirm);
  check(lastLoop == "slider", "child loop runs while its opening gesture is suppressed");
  check(confirms == before, "opening Confirm release is not accepted in the child");
  frame();
  frame(confirm, confirm);
  frame(0, 0, confirm);
  check(confirms == before + 2, "a fresh child Confirm gesture is accepted normally");
}

void testChainedPopsAndExplicitReplace() {
  auto* root = openRoot();
  auto* menu = pushMenu(*root);
  menu->startActivityForResult(std::make_unique<ProbeActivity>("navigation", true),
                               [menu](const ActivityResult&) { menu->finish(); });
  frame();
  frame();
  const int before = exits;
  const uint8_t back = 1 << HalGPIO::BTN_BACK;
  frame(back, back);
  check(exits == before + 2, "successful navigation can pop child and menu together");
  frame(0, 0, back);
  check(exits == before + 2, "chained pops do not leak the release to reader");
  frame();
  check(lastLoop == "reader", "chained pop returns to reader");

  menu = pushMenu(*root);
  menu->startActivityForResult(std::make_unique<ProbeActivity>("nested"), [](const ActivityResult&) {});
  frame();
  frame();
  const int beforeReplace = exits;
  activityManager.replaceActivity(std::make_unique<ProbeActivity>("home"));
  frame(back, back);
  check(exits == beforeReplace + 3, "explicit Home clears all three stacked activities");
  frame(0, 0, back);
  frame();
  check(lastLoop == "home", "explicit replace remains on its intended destination");
}
}  // namespace

int main() {
  testInitialScreenPreservesBootInput();
  testNestedReturnAndHold();
  testPressOpenedChildDoesNotConfirmOnRelease();
  testChainedPopsAndExplicitReplace();
  std::printf("%d navigation stack checks, %d failures\n", checks, failures);
  std::fflush(stdout);
  // The firmware singleton intentionally asserts if destroyed. Exit the host
  // process without invoking its static destructor, just like the device.
  std::_Exit(failures ? 1 : 0);
}
