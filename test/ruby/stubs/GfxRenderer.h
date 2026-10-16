#pragma once

#include <EpdFontFamily.h>
#include <Utf8.h>

#include <string>
#include <vector>

#include "FontCacheManager.h"
#include "RubyPlacement.h"

// Fixed metrics and captured ruby calls isolate production line extraction,
// bounds calculation, and serialization from hardware/font I/O.
class GfxRenderer {
 public:
  struct RubyCall {
    std::string text;
    bool vertical;
    int baseStart, baseEnd, limitStart, limitEnd;
    RubyPlacement placed;
  };
  mutable std::vector<RubyCall> calls;
  mutable FontCacheManager cache;
  bool isSdCardFont(int) const { return false; }
  void ensureSdCardFontReady(int, const char*, uint8_t) const {}
  int getTextAdvanceX(const int font, const char* text, EpdFontFamily::Style) const {
    int width = 0;
    const auto* ptr = reinterpret_cast<const uint8_t*>(text);
    while (const auto cp = utf8NextCodepoint(&ptr)) width += cp == ' ' ? 5 : (font == 2 ? 10 : 20);
    return width;
  }
  int getTextWidth(int font, const char* text, EpdFontFamily::Style style) const {
    return getTextAdvanceX(font, text, style);
  }
  int getSpaceWidth(int, EpdFontFamily::Style) const { return 5; }
  int getLineHeight(int font) const { return font == 2 ? 12 : 20; }
  int getFontAscenderSize(int font) const { return getLineHeight(font); }
  int getScreenWidth() const { return 480; }
  int getScreenHeight() const { return 800; }
  int getVerticalCharSpacing() const { return 10; }
  FontCacheManager* getFontCacheManager() const { return &cache; }
  void drawText(int, int, int, const char*, bool, EpdFontFamily::Style) const {}
  void drawTextVertical(int, int, int, const char*, bool, EpdFontFamily::Style) const {}
  void drawTextSideways(int, int, int, const char*, bool, EpdFontFamily::Style, int) const {}
  void drawLine(int, int, int, int, bool) const {}
  void drawRubyText(int font, int, int, const char* text, bool vertical, int baseStart, int baseEnd, int limitStart,
                    int limitEnd) const {
    const int length = getTextAdvanceX(font, text, EpdFontFamily::REGULAR);
    calls.push_back({text, vertical, baseStart, baseEnd, limitStart, limitEnd,
                     placeRuby(baseStart, baseEnd, length, limitStart, limitEnd)});
  }
};
