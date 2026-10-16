#include <Arduino.h>
#include <HalRTC.h>
#include <esp_sntp.h>

#include <cassert>
#include <cstdio>

#include "network/TimeSync.h"

int main() {
  using TimeSync::ClockSource;
  const time_t valid = TimeSync::MIN_VALID_TIME;
  assert(TimeSync::selectClockSource(0, 0) == ClockSource::None);
  assert(TimeSync::selectClockSource(valid - 1, valid - 1) == ClockSource::None);
  assert(TimeSync::selectClockSource(0, valid) == ClockSource::Rtc);
  assert(TimeSync::selectClockSource(valid, 0) == ClockSource::Internal);
  // Exact regression: a correct retained 02:11 must not become the RTC's 21:11.
  assert(TimeSync::selectClockSource(1792170685, 1792152689) == ClockSource::Internal);
  // NTP may correct time backwards too: select the trusted source, not max().
  assert(TimeSync::selectClockSource(valid, valid + 18000) == ClockSource::Internal);

  TimeSync::State state;
  assert(!state.hasSynced());
  assert(!state.takeRtcWrite());
  state.onSynchronized();
  assert(state.hasSynced());
  assert(state.hasSynced());  // Reading completion must not consume it.
  state.beginSync();
  assert(!state.hasSynced());
  assert(state.takeRtcWrite());  // Restarting SNTP cannot discard an unsaved result.
  state.retryRtcWrite();
  assert(state.takeRtcWrite());
  assert(!state.takeRtcWrite());

  TimeSync::start();
  assert(testSntpEnabled && testSyncCallback);
  assert(!TimeSync::hasSynced());
  TimeSync::process();
  assert(halRTC.writes == 0);

  // A reply after the activity's initial three-second wait is still persisted.
  testMillis += 5000;
  testSyncCallback(nullptr);
  assert(TimeSync::hasSynced());
  assert(TimeSync::hasSynced());
  assert(halRTC.writes == 0);  // The callback must not touch the I2C bus.
  testRendererBusy = true;
  TimeSync::process();
  assert(halRTC.writes == 0);
  testRendererBusy = false;
  TimeSync::process();
  assert(halRTC.writes == 1);
  const time_t now = time(nullptr);
  tm utc = {};
  gmtime_r(&now, &utc);
  assert(halRTC.saved.tm_year == utc.tm_year);
  assert(halRTC.saved.tm_hour == utc.tm_hour);
  TimeSync::process();
  assert(halRTC.writes == 1);  // No repeated RTC writes after success.

  // Retry failed I2C writes, at most once per second.
  testMillis += 1000;
  halRTC.failWrite = true;
  testSyncCallback(nullptr);
  TimeSync::process();
  assert(halRTC.writes == 2);
  TimeSync::process();
  assert(halRTC.writes == 2);
  halRTC.failWrite = false;
  testMillis += 1000;
  TimeSync::process();
  assert(halRTC.writes == 3);

  // A final notification while stopping SNTP must be flushed before sleeping,
  // even if the normal retry interval has not elapsed yet.
  testNotifyDuringStop = true;
  TimeSync::prepareForSleep();
  assert(!testSntpEnabled);
  assert(halRTC.writes == 4);
  TimeSync::process();
  assert(halRTC.writes == 4);

  // X4 has no external RTC, but NTP and the persistent completion flag work.
  halRTC.available = false;
  TimeSync::start();
  assert(!TimeSync::hasSynced());
  testMillis += 1000;
  testSyncCallback(nullptr);
  TimeSync::process();
  assert(TimeSync::hasSynced());
  assert(halRTC.writes == 4);
  TimeSync::prepareForSleep();
  assert(!testSntpEnabled);
  assert(!testRenderLocked);

  std::puts("NTP persistence and clock restore tests passed");
}
