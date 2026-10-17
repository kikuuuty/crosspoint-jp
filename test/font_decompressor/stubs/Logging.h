#pragma once

template <typename... Args>
inline void fontTestLog(Args&&...) {}

#define LOG_DBG(...) fontTestLog(__VA_ARGS__)
#define LOG_ERR(...) fontTestLog(__VA_ARGS__)
