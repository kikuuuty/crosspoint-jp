#pragma once

template <typename... Args>
inline void zipTestLog(Args&&...) {}

#define LOG_DBG(...) zipTestLog(__VA_ARGS__)
#define LOG_ERR(...) zipTestLog(__VA_ARGS__)
