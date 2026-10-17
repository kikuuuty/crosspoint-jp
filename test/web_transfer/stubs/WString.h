#pragma once

// Only the Arduino String operations used by FsHelpers and normalizeWebPath.
#include <string>
#include <utility>

class String {
 public:
  String() = default;
  // Intentionally implicit, matching Arduino's String conversion API.
  // cppcheck-suppress noExplicitConstructor
  String(const char* text) : value(text) {}
  // cppcheck-suppress noExplicitConstructor
  String(std::string text) : value(std::move(text)) {}
  const char* c_str() const { return value.c_str(); }
  size_t length() const { return value.size(); }
  bool isEmpty() const { return value.empty(); }
  bool startsWith(const char* prefix) const { return value.starts_with(prefix); }
  bool endsWith(const char* suffix) const { return value.ends_with(suffix); }
  String substring(size_t begin, size_t end) const { return value.substr(begin, end - begin); }
  bool operator==(const char* other) const { return value == other; }
  friend String operator+(const char* left, const String& right) { return std::string(left) + right.value; }

 private:
  std::string value;
};
