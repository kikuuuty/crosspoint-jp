#pragma once

#include <atomic>
#include <ctime>

namespace TimeSync {
constexpr time_t MIN_VALID_TIME = 1704067200;  // 2024-01-01 UTC
enum class ClockSource { None, Internal, Rtc };

constexpr ClockSource selectClockSource(time_t internalTime, time_t rtcTime) {
  if (internalTime >= MIN_VALID_TIME) return ClockSource::Internal;
  if (rtcTime >= MIN_VALID_TIME) return ClockSource::Rtc;
  return ClockSource::None;
}

// The lwIP callback only publishes flags. RTC I2C access belongs to the main
// task under RenderLock, never to the network callback or a temporary activity.
class State {
  std::atomic<bool> synced{false};
  std::atomic<bool> rtcWritePending{false};

 public:
  void beginSync() { synced.store(false); }
  void onSynchronized() {
    rtcWritePending.store(true);
    synced.store(true);
  }
  bool hasSynced() const { return synced.load(); }
  bool hasPendingRtcWrite() const { return rtcWritePending.load(); }
  bool takeRtcWrite() { return rtcWritePending.exchange(false); }
  void retryRtcWrite() { rtcWritePending.store(true); }
};

void start();
bool hasSynced();
void process();
// Stop the SNTP client and flush any pending RTC write before going to sleep.
void prepareForSleep();
}  // namespace TimeSync
