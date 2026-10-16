#pragma once

#include <cassert>
#include <cstring>

struct timeval;
using sntp_sync_time_cb_t = void (*)(timeval*);
inline sntp_sync_time_cb_t testSyncCallback = nullptr;
inline bool testSntpEnabled = false;
inline bool testNotifyDuringStop = false;
constexpr int SNTP_SYNC_MODE_IMMED = 0;
constexpr int ESP_SNTP_OPMODE_POLL = 0;

inline bool esp_sntp_enabled() { return testSntpEnabled; }
inline void esp_sntp_stop() {
  if (testNotifyDuringStop) {
    testNotifyDuringStop = false;
    assert(testSyncCallback);
    testSyncCallback(nullptr);
  }
  testSntpEnabled = false;
}
inline void esp_sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t cb) { testSyncCallback = cb; }
inline void esp_sntp_set_sync_mode(int mode) { assert(mode == SNTP_SYNC_MODE_IMMED); }
inline void esp_sntp_setoperatingmode(int mode) { assert(mode == ESP_SNTP_OPMODE_POLL); }
inline void esp_sntp_setservername(int index, const char* name) {
  assert(index == 0);
  assert(strcmp(name, "pool.ntp.org") == 0);
}
inline void esp_sntp_init() { testSntpEnabled = true; }
