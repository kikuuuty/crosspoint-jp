#pragma once

template <typename... Args>
inline void progressTestLog(Args&&...) {}

#define LOG_DBG(...) progressTestLog(__VA_ARGS__)
#define LOG_ERR(...) progressTestLog(__VA_ARGS__)
