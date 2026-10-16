#include "ReaderClock.h"

#include "components/UITheme.h"

void ReaderClock::render(const GfxRenderer& renderer) {
  const time_t now = time(nullptr);
  char text[6];
  formatTime(now, text);
  GUI.drawReaderHeader(renderer, text);
  recordRender(now);
}
