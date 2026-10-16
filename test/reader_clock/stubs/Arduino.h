#pragma once

#include <cstdlib>
#include <ctime>

inline unsigned long testMillis = 1000;
inline unsigned long millis() { return testMillis; }

#ifdef _WIN32
inline tm* gmtime_r(const time_t* value, tm* result) { return gmtime_s(result, value) == 0 ? result : nullptr; }
inline int setenv(const char* name, const char* value, int) { return _putenv_s(name, value); }
#define tzset _tzset
#endif
