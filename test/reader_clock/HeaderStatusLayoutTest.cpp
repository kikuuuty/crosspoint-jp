#include <algorithm>
#include <cassert>
#include <cstdio>

#include "components/themes/lyra/Lyra3CoversTheme.h"
#include "components/themes/lyra/LyraTheme.h"
#include "components/themes/lyra/VegaTheme.h"

namespace {
int cases = 0;

void checkLayout(const ThemeMetrics& metrics, int width, int height, Rect area, int viewableTop, int viewableRight,
                 int expectedUiX, int expectedUiTextY) {
  for (const int clockWidth : {0, 49, 50, 75}) {
    const auto home = HeaderStatusLayout::calculate(metrics, HeaderStatusContext::Ui, area, width, viewableTop,
                                                    viewableRight, 20, 24, 0);
    const auto reader = HeaderStatusLayout::calculate(metrics, HeaderStatusContext::Reader, area, width, viewableTop,
                                                      viewableRight, 20, 24, clockWidth);
    assert(home.battery.x == expectedUiX);
    assert(home.textY == expectedUiTextY);
    // The reading direction never creates a physical button-hint reservation.
    // Keep these expected coordinates independent of the supplied UI rectangle.
    assert(reader.battery.x == width - 12 - metrics.batteryWidth);
    assert(reader.textY == 10);
    assert(home.fontId == reader.fontId);
    assert(reader.battery.y == reader.textY);
    assert(reader.fontId == metrics.headerStatusFontId);
    assert(reader.clockX == (width - clockWidth) / 2);
    assert(reader.battery.width == metrics.batteryWidth);
    assert(reader.battery.height == metrics.batteryHeight);
    assert(reader.battery.x + reader.battery.width <= width - viewableRight);
    assert(reader.textY >= viewableTop && reader.textY < height / 2);
    assert(reader.bounds.y == 0 && reader.bounds.x == 0);
    assert(reader.bounds.width == width);
    const int rowBottom = reader.bounds.y + reader.bounds.height;
    assert(rowBottom == 39);  // 10px top + 24px row + 5px gap, never 56px extra.
    assert(rowBottom >= reader.textY + 24 + metrics.topPadding);
    assert(rowBottom >= reader.battery.y + metrics.batteryIconYOffset + metrics.batteryHeight + metrics.topPadding);

    // Match the EPUB/TXT top-margin convention: the bezel is added once only.
    const int reservedHeaderHeight = rowBottom - viewableTop;
    for (const int bodyMargin : {0, 5, 10, 30, 60}) {
      const int bodyTop = viewableTop + std::max(bodyMargin, reservedHeaderHeight);
      assert(bodyTop == std::max(viewableTop + bodyMargin, rowBottom));
    }
    if (area.y == 0 && area.x + area.width == width) {
      assert(home.battery.x == reader.battery.x);
      assert(home.battery.y == reader.battery.y);
    }
    if (area.y > 0) {
      // Regression: Home keeps its inverted hint band, the reader must not.
      assert(home.textY - reader.textY == area.y);
      assert(home.bounds.y + home.bounds.height - rowBottom == area.y);
    }
    if (area.x + area.width < width) {
      assert(reader.battery.x - home.battery.x == width - area.x - area.width);
    }
    ++cases;
  }
}
}  // namespace

int main() {
  for (const auto& metrics : {LyraMetrics::values, Lyra3CoversMetrics::values, VegaMetrics::values}) {
    assert(metrics.headerStatusFontId == SMALL_FONT_ID);
    const int hints = metrics.buttonHintsHeight + metrics.verticalSpacing;
    for (const bool x3 : {false, true}) {
      const int portraitWidth = x3 ? 528 : 480;
      const int portraitHeight = x3 ? 792 : 800;
      // Normal portrait: preserve the original Home placement exactly.
      checkLayout(metrics, portraitWidth, portraitHeight, Rect{0, 0, portraitWidth, portraitHeight - hints}, 9, 3,
                  portraitWidth - 12 - metrics.batteryWidth, 10);
      // Inverted Home avoids its hints; the reader stays near the logical top.
      checkLayout(metrics, portraitWidth, portraitHeight, Rect{0, hints, portraitWidth, portraitHeight - hints}, 3, 3,
                  portraitWidth - 12 - metrics.batteryWidth, hints + 10);
      // Landscape CW: only Home avoids the button hints on the left.
      checkLayout(metrics, portraitHeight, portraitWidth, Rect{hints, 0, portraitHeight - hints, portraitWidth}, 3, 9,
                  portraitHeight - 12 - metrics.batteryWidth, 10);
      // Landscape CCW: only Home avoids right hints. Reader uses the full width.
      checkLayout(metrics, portraitHeight, portraitWidth, Rect{0, 0, portraitHeight - hints, portraitWidth}, 3, 3,
                  portraitHeight - hints - 12 - metrics.batteryWidth, 10);
    }
  }

  // Font metrics, icon size and bezel clearance must all contribute to reservation.
  auto metrics = LyraMetrics::values;
  metrics.batteryHeight = 40;
  const auto largeIcon =
      HeaderStatusLayout::calculate(metrics, HeaderStatusContext::Reader, Rect{0, 56, 528, 736}, 528, 9, 3, 20, 24, 50);
  assert(largeIcon.bounds.height >= largeIcon.textY + metrics.batteryIconYOffset + 40 + metrics.topPadding);
  const auto largeFont = HeaderStatusLayout::calculate(metrics, HeaderStatusContext::Reader, Rect{0, 56, 528, 736}, 528,
                                                       9, 3, 48, 52, 120);
  assert(largeFont.bounds.height >= largeFont.textY + 52 + metrics.topPadding);
  const auto wideBezel = HeaderStatusLayout::calculate(metrics, HeaderStatusContext::Reader, Rect{0, 56, 528, 736}, 528,
                                                       18, 24, 20, 24, 50);
  assert(wideBezel.textY == 18);
  assert(wideBezel.battery.x + wideBezel.battery.width == 528 - 24);
  std::printf("Header status layout: %d orientation/theme/clock-width cases passed\n", cases);
}
