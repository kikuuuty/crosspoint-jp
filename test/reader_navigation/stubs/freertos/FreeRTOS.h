#pragma once

#include <cstdint>

using TaskHandle_t = void*;
using SemaphoreHandle_t = void*;
constexpr int pdTRUE = 1;
constexpr uint32_t portMAX_DELAY = 0xffffffff;
constexpr int eIncrement = 0;

#define taskENTER_CRITICAL(...) ((void)0)
#define taskEXIT_CRITICAL(...) ((void)0)

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void*>(1); }
inline void xSemaphoreTake(SemaphoreHandle_t, uint32_t) {}
inline void xSemaphoreGive(SemaphoreHandle_t) {}
inline TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t) { return nullptr; }
inline TaskHandle_t xTaskGetCurrentTaskHandle() { return reinterpret_cast<void*>(2); }
inline int xQueuePeek(SemaphoreHandle_t, void*, int) { return pdTRUE; }
inline void xTaskCreate(void (*)(void*), const char*, int, void*, int, TaskHandle_t* handle) {
  *handle = reinterpret_cast<void*>(3);
}
inline void xTaskNotify(TaskHandle_t, int, int) {}
inline void ulTaskNotifyTake(int, uint32_t) {}
