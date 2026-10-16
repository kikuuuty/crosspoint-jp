#pragma once

#include "Ruby.h"

// Streaming ruby association state, independent of XML chunk boundaries.
// Explicit <rb> boundaries need a small variable-size queue; implicit ruby
// needs no queue allocation. The queue is bounded by the parser's word budget.
class RubyParser {
  struct Base {
    uint16_t start;
    uint16_t count;
  };
  std::vector<Base> bases;
  size_t nextBase = 0;
  size_t implicitStart = 0;
  size_t explicitStart = 0;
  size_t readingStart = 0;
  size_t readingCount = 0;
  std::string reading;

 public:
  static constexpr size_t MAX_READING_BYTES = Ruby::MAX_READING_BYTES;

  void begin(const size_t word) {
    bases.clear();
    nextBase = 0;
    implicitStart = word;
    reading.clear();
  }
  void beginBase(const size_t word) { explicitStart = word; }
  void endBase(const size_t word) {
    if (word <= explicitStart || word > UINT16_MAX) return;
    if (bases.capacity() == 0) bases.reserve(8);
    bases.push_back({static_cast<uint16_t>(explicitStart), static_cast<uint16_t>(word - explicitStart)});
  }
  void beginReading(const size_t word, const size_t baseSpan = 1) {
    reading.clear();
    readingStart = implicitStart;
    readingCount = word >= implicitStart ? word - implicitStart : 0;
    if (nextBase < bases.size()) {
      const size_t last = std::min(bases.size(), nextBase + std::max(size_t{1}, baseSpan)) - 1;
      readingStart = bases[nextBase].start;
      readingCount = bases[last].start + bases[last].count - readingStart;
      nextBase = last + 1;
    }
  }
  bool append(const char* text, const size_t bytes) {
    if (bytes > MAX_READING_BYTES - reading.size()) return false;
    reading.append(text, bytes);
    return true;
  }
  void endReading(std::vector<RubySpan>& out, const size_t word) {
    // HTML formatting whitespace around <rt> is not part of the reading.
    const size_t first = reading.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      reading.clear();
    } else {
      reading.erase(reading.find_last_not_of(" \t\r\n") + 1);
      reading.erase(0, first);
      if (readingStart <= word && readingCount <= word - readingStart) {
        Ruby::add(out, readingStart, readingCount, std::move(reading));
      }
    }
    implicitStart = word;
  }
};
