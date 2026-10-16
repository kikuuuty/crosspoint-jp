#pragma once

#include <atomic>
#include <cstdint>
#include <ctime>

class GfxRenderer;

// Shared by the EPUB/TXT readers. Only a minute counter crosses the main/render
// task boundary; no timer task, heap buffer or change to the inactivity timer.
class ReaderClock {
  static constexpr time_t MIN_VALID_TIME = 1704067200;  // 2024-01-01 UTC
  static constexpr int32_t NOT_RENDERED = -1;
  std::atomic<int32_t> lastMinute{NOT_RENDERED};

  static int32_t minuteFor(time_t now) { return now >= MIN_VALID_TIME ? static_cast<int32_t>(now / 60) : 0; }

 public:
  void render(const GfxRenderer& renderer);

  static void formatTime(time_t now, char (&text)[6]) {
    text[0] = '-';
    text[1] = '-';
    text[2] = ':';
    text[3] = '-';
    text[4] = '-';
    text[5] = '\0';
    if (now < MIN_VALID_TIME) return;
    struct tm local = {};
#ifdef _WIN32
    if (localtime_s(&local, &now) != 0) return;
#else
    if (!localtime_r(&now, &local)) return;
#endif
    strftime(text, sizeof(text), "%H:%M", &local);
  }

  void recordRender(time_t now) { lastMinute.store(minuteFor(now)); }

  bool shouldRefresh(time_t now, bool everyMinute) {
    if (!everyMinute) return false;
    const int32_t minute = minuteFor(now);
    int32_t previous = lastMinute.load();
    if (previous == NOT_RENDERED || previous == minute) return false;
    // Claim this minute once so a busy render task is not flooded with requests.
    return lastMinute.compare_exchange_strong(previous, minute);
  }
};
