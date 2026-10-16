#include "TimeSync.h"

#include <Arduino.h>
#include <HalPowerManager.h>
#include <HalRTC.h>
#include <Logging.h>
#include <esp_sntp.h>

#include <cstdlib>

#include "activities/RenderLock.h"

namespace {
TimeSync::State syncState;
unsigned long lastRtcAttemptMs = 0;
constexpr unsigned long RTC_RETRY_MS = 1000;

void onTimeSynchronized(struct timeval*) { syncState.onSynchronized(); }

void persistPendingTime(bool waitForRenderer) {
  if (!syncState.hasPendingRtcWrite()) return;
  if (!waitForRenderer &&
      (RenderLock::peek() || (lastRtcAttemptMs != 0 && millis() - lastRtcAttemptMs < RTC_RETRY_MS))) {
    return;
  }

  RenderLock lock;
  if (!syncState.takeRtcWrite()) return;
  if (!halRTC.isAvailable()) {
    LOG_INF("TIME", "NTP time synced; no external RTC");
    return;
  }

  HalPowerManager::Lock powerLock;
  lastRtcAttemptMs = millis();
  const time_t now = time(nullptr);
  struct tm utc = {};
  if (now < TimeSync::MIN_VALID_TIME || !gmtime_r(&now, &utc) || !halRTC.writeTime(utc)) {
    syncState.retryRtcWrite();
    LOG_ERR("TIME", "RTC time save failed; will retry");
    return;
  }
  LOG_INF("TIME", "NTP synced; RTC saved: %04d-%02d-%02d %02d:%02d:%02d UTC", utc.tm_year + 1900, utc.tm_mon + 1,
          utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
}
}  // namespace

void TimeSync::start() {
  if (esp_sntp_enabled()) esp_sntp_stop();
  syncState.beginSync();
  setenv("TZ", "JST-9", 1);
  tzset();
  esp_sntp_set_time_sync_notification_cb(onTimeSynchronized);
  esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();
}

bool TimeSync::hasSynced() { return syncState.hasSynced(); }

void TimeSync::process() { persistPendingTime(false); }

void TimeSync::prepareForSleep() {
  // Serialize with lwIP before the final flush so a late reply cannot update
  // only the internal clock after we have saved the external RTC.
  if (esp_sntp_enabled()) esp_sntp_stop();
  persistPendingTime(true);
}
