#pragma once

#include <vector>

#include "Epub/converters/ImageToFramebufferDecoder.h"

inline struct TestImageProbe {
  bool noDecoder = false;
  int failuresBeforeSuccess = 0;
  int calls = 0;
  bool afterFlushAndClose = true;
  ImageDimensions dimensions = {120, 80};
  std::vector<size_t> delayCounts;
} imageProbe;
