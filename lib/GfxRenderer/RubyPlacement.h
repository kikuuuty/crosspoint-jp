#pragma once

#include <algorithm>

struct RubyPlacement {
  int start;
  int length;
};

// Coordinates are along the writing axis. The caller partitions adjacent ruby
// at the midpoint of their base gap, so these limits prevent both line-edge
// protrusion and collisions without a per-frame layout allocation.
inline RubyPlacement placeRuby(const int baseStart, const int baseEnd, const int naturalLength, const int limitStart,
                               const int limitEnd) {
  const int length = std::min(std::max(0, naturalLength), std::max(0, limitEnd - limitStart));
  const int centered = baseStart + (baseEnd - baseStart - length) / 2;
  return {std::clamp(centered, limitStart, std::max(limitStart, limitEnd - length)), length};
}
