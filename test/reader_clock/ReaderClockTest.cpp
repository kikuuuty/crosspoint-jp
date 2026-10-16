#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "activities/reader/ReaderClock.h"

namespace {
constexpr time_t JAN_1_2024 = 1704067200;

void expectTime(time_t now, const char* expected) {
  char text[6];
  ReaderClock::formatTime(now, text);
  assert(strcmp(text, expected) == 0);
}
}  // namespace

int main() {
  // Match the firmware's timezone, independently of the host configuration.
#ifdef _WIN32
  _putenv_s("TZ", "JST-9");
  _tzset();
#else
  setenv("TZ", "JST-9", 1);
  tzset();
#endif
  expectTime(0, "--:--");
  expectTime(JAN_1_2024 - 1, "--:--");
  expectTime(JAN_1_2024, "09:00");
  expectTime(JAN_1_2024 + 5 * 60, "09:05");
  expectTime(JAN_1_2024 + 15 * 3600 - 1, "23:59");
  expectTime(JAN_1_2024 + 15 * 3600, "00:00");

  ReaderClock clock;
  assert(!clock.shouldRefresh(JAN_1_2024, true));  // No page has been drawn yet.
  clock.recordRender(JAN_1_2024 + 30);
  assert(!clock.shouldRefresh(JAN_1_2024 + 59, true));
  assert(!clock.shouldRefresh(JAN_1_2024 + 60, false));  // Default: page turns only.
  assert(clock.shouldRefresh(JAN_1_2024 + 60, true));
  for (int i = 0; i < 1000; ++i) {
    assert(!clock.shouldRefresh(JAN_1_2024 + 61, true));  // One request per minute.
  }
  clock.recordRender(JAN_1_2024 + 62);
  assert(!clock.shouldRefresh(JAN_1_2024 + 119, true));
  assert(clock.shouldRefresh(JAN_1_2024 + 120, true));

  // Turning a page updates the baseline even when automatic updates are off.
  clock.recordRender(JAN_1_2024 + 180);
  assert(!clock.shouldRefresh(JAN_1_2024 + 181, true));
  assert(!clock.shouldRefresh(JAN_1_2024 + 240, false));
  assert(clock.shouldRefresh(JAN_1_2024 + 240, true));

  // Synchronization / RTC corrections may move time forwards or backwards.
  assert(clock.shouldRefresh(JAN_1_2024 + 3600, true));
  assert(clock.shouldRefresh(JAN_1_2024, true));
  clock.recordRender(0);
  assert(!clock.shouldRefresh(3600, true));  // Invalid time stays --:--.
  assert(clock.shouldRefresh(JAN_1_2024, true));
  clock.recordRender(JAN_1_2024);
  assert(clock.shouldRefresh(0, true));  // A lost clock must also update the UI.

  std::puts("Reader clock tests passed");
}
