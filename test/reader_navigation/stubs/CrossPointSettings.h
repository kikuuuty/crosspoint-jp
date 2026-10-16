#pragma once

#include <cstdint>

struct CrossPointSettings {
  enum SIDE_BUTTON_LAYOUT { PREV_NEXT, NEXT_PREV };
  uint8_t sideButtonLayout = PREV_NEXT;
  uint8_t frontButtonBack = 0;
  uint8_t frontButtonConfirm = 1;
  uint8_t frontButtonLeft = 2;
  uint8_t frontButtonRight = 3;
};

extern CrossPointSettings testSettings;
#define SETTINGS testSettings
