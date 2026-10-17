#pragma once

// Reuse the fixed font metrics used by the production ruby/layout host tests.
#define GfxRenderer RubyTestRenderer
#include "../../ruby/stubs/GfxRenderer.h"
#undef GfxRenderer

class GfxRenderer : public RubyTestRenderer {
 public:
  std::string truncatedText(int font, const char* text, int width, EpdFontFamily::Style style) const {
    std::string result(text);
    while (!result.empty() && getTextWidth(font, result.c_str(), style) > width) utf8RemoveLastChar(result);
    return result;
  }
};
