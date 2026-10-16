#pragma once

#include <algorithm>
#include <cstdint>

struct RubyBitmapView {
  const uint8_t* data;
  int width;
  int height;
  bool twoBit;
  int rowStride = 0;  // external/UI fonts: padded 1-bit rows
  int leftTrim = 0;
};

// Area-average directly from the native glyph, including at scales below one
// pixel per source stroke. The callback receives opacity 1..3, never white.
// No scratch framebuffer is needed. Limits are in logical writing coordinates,
// before the renderer rotates pixels into any of the four panel orientations.
template <typename Pixel>
void drawRubyBitmap(const RubyBitmapView& bitmap, const int x, const int y, const int width, const int height,
                    const bool vertical, const int limitStart, const int limitEnd, Pixel pixel) {
  if (!bitmap.data || bitmap.width <= 0 || bitmap.height <= 0 || width <= 0 || height <= 0) return;
  for (int dy = 0; dy < height; ++dy) {
    const int sy0 = dy * bitmap.height / height;
    const int sy1 = std::max(sy0 + 1, (dy + 1) * bitmap.height / height);
    for (int dx = 0; dx < width; ++dx) {
      const int axis = vertical ? y + dy : x + dx;
      if (axis < limitStart || axis >= limitEnd) continue;
      const int sx0 = dx * bitmap.width / width;
      const int sx1 = std::max(sx0 + 1, (dx + 1) * bitmap.width / width);
      unsigned ink = 0;
      unsigned samples = 0;
      for (int sy = sy0; sy < sy1; ++sy) {
        for (int sx = sx0; sx < sx1; ++sx) {
          const int pp = sy * bitmap.width + sx;
          if (bitmap.rowStride > 0) {
            const int bitX = sx + bitmap.leftTrim;
            ink += ((bitmap.data[sy * bitmap.rowStride + bitX / 8] >> (7 - bitX % 8)) & 1) * 3;
          } else {
            ink += bitmap.twoBit ? ((bitmap.data[pp / 4] >> ((3 - pp % 4) * 2)) & 3)
                                 : (((bitmap.data[pp / 8] >> (7 - pp % 8)) & 1) * 3);
          }
          ++samples;
        }
      }
      const uint8_t opacity = (ink + samples / 2) / samples;
      if (opacity) pixel(x + dx, y + dy, opacity);
    }
  }
}
