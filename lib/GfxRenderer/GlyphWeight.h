#pragma once

#include <algorithm>
#include <cstdint>

// Streaming glyph coverage, before monochrome thresholding or grayscale plane
// encoding. No temporary glyph/frame buffer, font mutation, or metric changes.
namespace GlyphWeight {
constexpr uint8_t STANDARD = 0;
constexpr uint8_t MEDIUM = 1;
constexpr uint8_t HEAVY = 2;

struct Bitmap {
  const uint8_t* data;
  int width;
  int height;
  bool is2Bit;
  int rowStride = 0;  // 0 = tightly packed EpdGlyph; otherwise row-packed 1-bit
  int left = 0;       // left trim for row-packed ExternalFont

  uint8_t at(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return 0;
    if (rowStride) {
      x += left;
      return ((data[y * rowStride + x / 8] >> (7 - x % 8)) & 1) * 3;
    }
    const int pos = y * width + x;
    return is2Bit ? ((data[pos / 4] >> ((3 - pos % 4) * 2)) & 3) : ((data[pos / 8] >> (7 - pos % 8)) & 1) * 3;
  }

  // Area averaging on shrink preserves thin strokes. Enlargement is nearest
  // neighbor. Sampling outside the destination is transparent, including the
  // extra output row/column used for dilation.
  uint8_t scaled(int x, int y, int drawW, int drawH) const {
    if (drawW <= 0 || drawH <= 0 || x < 0 || x >= drawW || y < 0 || y >= drawH) return 0;
    if (drawW == width && drawH == height) return at(x, y);
    const int x0 = x * width / drawW;
    const int y0 = y * height / drawH;
    const int x1 = std::max(x0 + 1, (x + 1) * width / drawW);
    const int y1 = std::max(y0 + 1, (y + 1) * height / drawH);
    unsigned sum = 0;
    for (int sy = y0; sy < y1; ++sy) {
      for (int sx = x0; sx < x1; ++sx) sum += at(sx, sy);
    }
    const unsigned count = (x1 - x0) * (y1 - y0);
    return (sum + count / 2) / count;
  }
};

inline uint8_t coverage(const Bitmap& bitmap, int x, int y, int drawW, int drawH, uint8_t weight,
                        bool syntheticBold = false) {
  uint8_t ink = bitmap.scaled(x, y, drawW, drawH);
  if (weight == HEAVY || syntheticBold) {
    ink = std::max(ink, bitmap.scaled(x - 1, y, drawW, drawH));
  }
  if (weight == HEAVY) {
    ink = std::max(ink, bitmap.scaled(x, y - 1, drawW, drawH));
    if (syntheticBold) {
      ink = std::max(ink, bitmap.scaled(x - 2, y, drawW, drawH));
      ink = std::max(ink, bitmap.scaled(x - 1, y - 1, drawW, drawH));
    }
  }
  // Preserve transparency and black; promote both gray coverages one step.
  return weight != STANDARD && ink > 0 ? std::min(3, ink + 1) : ink;
}
}  // namespace GlyphWeight
