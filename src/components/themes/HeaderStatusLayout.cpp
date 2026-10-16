#include <algorithm>

#include "BaseTheme.h"

HeaderStatusLayout HeaderStatusLayout::calculate(const ThemeMetrics& metrics, HeaderStatusContext context,
                                                 Rect uiContentArea, int screenWidth, int viewableTop,
                                                 int viewableRight, int textHeight, int lineHeight, int clockWidth) {
  // Keep the normal Home placement (topPadding + 5, right inset 12) without
  // adding the reader's bezel/body margins a second time. Only UI screens
  // reserve physical button hints; applying those insets to a reader would
  // introduce an empty 56px band in inverted mode (or a side inset in landscape).
  const Rect contentArea = context == HeaderStatusContext::Reader ? Rect{0, 0, screenWidth} : uiContentArea;
  const int textY = std::max(viewableTop, contentArea.y + metrics.topPadding + metrics.headerStatusTopPadding);
  const int rightEdge =
      std::min(contentArea.x + contentArea.width - metrics.headerStatusRightPadding, screenWidth - viewableRight);
  const int rowHeight = std::max(std::max(textHeight, lineHeight), metrics.batteryIconYOffset + metrics.batteryHeight);
  const int bottom = textY + rowHeight + metrics.topPadding;
  return {Rect{contentArea.x, contentArea.y, contentArea.width, bottom - contentArea.y},
          Rect{rightEdge - metrics.batteryWidth, textY, metrics.batteryWidth, metrics.batteryHeight},
          metrics.headerStatusFontId, textY, (screenWidth - clockWidth) / 2};
}
