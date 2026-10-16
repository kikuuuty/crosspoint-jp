#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// One source-level base/reading association. Indices are word indices (CJK
// characters are individual words), not UTF-8 byte offsets. A single base word
// may also be Latin text, tate-chu-yoko, or an inline image.
struct RubySpan {
  uint16_t start = 0;
  uint16_t count = 0;
  std::string text;

  RubySpan() = default;
  RubySpan(const uint16_t start, const uint16_t count, std::string&& text)
      : start(start), count(count), text(std::move(text)) {}

  size_t end() const { return static_cast<size_t>(start) + count; }
};

namespace Ruby {

inline constexpr size_t MAX_READING_BYTES = 4096;

inline bool containsWord(const std::vector<RubySpan>& spans, const size_t index) {
  for (const auto& span : spans) {
    if (index < span.start) break;
    if (index < span.end()) return true;
  }
  return false;
}

// Sparse storage: no allocation at all for text without ruby. Reserve a small
// batch only on the first annotation, rather than sizeof(string) per base word.
inline void add(std::vector<RubySpan>& spans, const size_t start, const size_t count, std::string&& text) {
  if (count == 0 || text.empty() || start > UINT16_MAX || count > UINT16_MAX - start) return;
  if (!spans.empty() && start < spans.back().end()) return;  // no secondary/nested ruby layers
  if (spans.capacity() == 0) spans.reserve(8);
  spans.emplace_back(static_cast<uint16_t>(start), static_cast<uint16_t>(count), std::move(text));
}

inline std::vector<RubySpan> takeRange(std::vector<RubySpan>& spans, const size_t start, const size_t end) {
  const auto count = std::count_if(spans.begin(), spans.end(), [start, end](const RubySpan& span) {
    return span.start >= start && span.start < end && !span.text.empty();
  });
  std::vector<RubySpan> out;
  if (count == 0) return out;
  out.reserve(count);
  for (auto& span : spans) {
    if (span.start < start || span.start >= end || span.text.empty()) continue;
    // Emergency wrapping of a base longer than a whole line: keep the entire
    // reading on the first fragment. Never invent per-character readings.
    out.emplace_back(static_cast<uint16_t>(span.start - start),
                     static_cast<uint16_t>(std::min(span.end(), end) - span.start), std::move(span.text));
  }
  return out;
}

inline void consume(std::vector<RubySpan>& spans, const size_t words) {
  const auto first =
      std::find_if(spans.begin(), spans.end(), [words](const RubySpan& span) { return span.start >= words; });
  spans.erase(spans.begin(), first);
  for (auto& span : spans) span.start = static_cast<uint16_t>(span.start - words);
}

// Keep a complete association together whenever it fits on an otherwise empty
// line. The caller supplies the measured extent, including real word spacing.
template <typename Measure>
size_t adjustBreak(const std::vector<RubySpan>& spans, const size_t breakAt, const size_t lineStart,
                   const int available, Measure measure) {
  for (const auto& span : spans) {
    if (span.start >= breakAt) break;
    if (breakAt >= span.end()) continue;
    if (span.start > lineStart) return span.start;
    if (span.start == lineStart && measure(span.start, span.end()) <= available) return span.end();
    break;  // oversized base: make progress with emergency wrapping
  }
  return breakAt;
}

}  // namespace Ruby
