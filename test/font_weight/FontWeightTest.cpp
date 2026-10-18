#include <array>
#include <cassert>
#include <cstdio>

#include "GlyphWeight.h"

// Compile the production weighted renderer and BW predicate, not copies. The
// harness records pixels in memory; no display refresh API is even available.
class GfxRenderer {
 public:
  enum RenderMode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB };
  enum class GlyphInk { Sharp, GrayscaleBase };
  RenderMode renderMode = BW;
  GlyphInk glyphInk = GlyphInk::Sharp;
  uint8_t fontWeight = 0;
  bool darkMode = false;
  mutable std::array<int, 64 * 64> pixels;
  GfxRenderer() { pixels.fill(-1); }
  void drawPixel(int x, int y, bool state) const {
    assert(x >= 0 && x < 64 && y >= 0 && y < 64);
    assert(pixels[y * 64 + x] == -1);  // each output pixel encoded only once
    pixels[y * 64 + x] = state;
  }
  void renderWeightedGlyph(const GlyphWeight::Bitmap&, int, int, int, int, bool, int = 0, bool = false) const;
#include "GlyphInk.inc"
};
#include "WeightedGlyph.inc"

static void coverageAndPacking() {
  // Coverage 0,1,2,3 in one tightly packed row.
  const uint8_t ramp[] = {0x1B};
  const GlyphWeight::Bitmap bitmap{ramp, 4, 1, true};
  for (int x = 0; x < 4; ++x) {
    assert(bitmap.at(x, 0) == x);
    assert(GlyphWeight::coverage(bitmap, x, 0, 4, 1, GlyphWeight::STANDARD) == x);
    assert(GlyphWeight::coverage(bitmap, x, 0, 4, 1, GlyphWeight::MEDIUM) == (x ? std::min(3, x + 1) : 0));
  }
  assert(bitmap.at(-1, 0) == 0 && bitmap.at(4, 0) == 0 && bitmap.at(0, 1) == 0);
  assert(bitmap.scaled(0, 0, 2, 1) == 1);
  assert(bitmap.scaled(1, 0, 2, 1) == 3);
  assert(bitmap.scaled(4, 0, 8, 1) == 2);
  assert(bitmap.scaled(0, 0, 0, 1) == 0);
  assert(GlyphWeight::coverage(bitmap, 4, 0, 4, 1, GlyphWeight::HEAVY) == 3);
  assert(GlyphWeight::coverage(bitmap, 3, 1, 4, 1, GlyphWeight::HEAVY) == 3);
  assert(GlyphWeight::coverage(bitmap, 4, 1, 4, 1, GlyphWeight::HEAVY) == 0);
  // Synthetic bold is unioned before grayscale encoding and retains its edge.
  assert(GlyphWeight::coverage(bitmap, 5, 0, 4, 1, GlyphWeight::HEAVY, true) == 3);
  const uint8_t packed[] = {0xA8, 0x00};  // 3x3, rows aren't byte-aligned
  const GlyphWeight::Bitmap oneBit{packed, 3, 3, false};
  assert(oneBit.at(0, 0) == 3 && oneBit.at(1, 0) == 0 && oneBit.at(2, 0) == 3);
  assert(oneBit.at(1, 1) == 3 && oneBit.at(2, 2) == 0);
  const uint8_t rows[] = {0x05, 0x80, 0x02, 0x00};
  const GlyphWeight::Bitmap trimmed{rows, 4, 2, false, 2, 5};
  assert(trimmed.at(0, 0) == 3 && trimmed.at(1, 0) == 0 && trimmed.at(3, 0) == 3);
  assert(trimmed.at(1, 1) == 3 && trimmed.at(3, 1) == 0);
}

static void rendering() {
  const uint8_t data[] = {0x1B, 0xE4};  // 4x2, every coverage and transparency
  const GlyphWeight::Bitmap bitmap{data, 4, 2, true};
  for (uint8_t weight = 0; weight <= 2; ++weight) {
    for (const bool dark : {false, true}) {
      for (const bool aa : {false, true}) {
        for (const bool synth : {false, true}) {
          for (const int rotation : {-1, 0, 1}) {
            for (const int size : {2, 4, 8}) {
              GfxRenderer renderer;
              renderer.fontWeight = weight;
              renderer.darkMode = dark;
              renderer.glyphInk = aa ? GfxRenderer::GlyphInk::GrayscaleBase : GfxRenderer::GlyphInk::Sharp;
              for (const auto mode : {GfxRenderer::BW, GfxRenderer::GRAYSCALE_LSB, GfxRenderer::GRAYSCALE_MSB}) {
                renderer.renderMode = mode;
                renderer.pixels.fill(-1);
                renderer.renderWeightedGlyph(bitmap, 20, 20, size, size / 2, true, rotation, synth);
                int expectedPixels = 0;
                const int grow = weight == 2 ? 1 : 0;
                for (int gy = 0; gy < size / 2 + grow; ++gy) {
                  for (int gx = 0; gx < size + grow + synth; ++gx) {
                    const auto ink = GlyphWeight::coverage(bitmap, gx, gy, size, size / 2, weight, synth);
                    const int x = rotation == 0 ? 20 + gx : 20 - rotation * gy;
                    const int y = rotation == 0 ? 20 + gy : 20 + rotation * gx;
                    const int actual = renderer.pixels[y * 64 + x];
                    if (!ink || (mode == GfxRenderer::BW && !renderer.isGlyphInk(3 - ink))) {
                      assert(actual == -1);
                    } else {
                      const int val = dark ? ink : 3 - ink;
                      const int expected = mode == GfxRenderer::BW
                                               ? 1
                                               : !(mode == GfxRenderer::GRAYSCALE_LSB ? val & 1 : (val >> 1) & 1);
                      assert(actual == expected);
                      ++expectedPixels;
                    }
                  }
                }
                int actualPixels = 0;
                for (const auto pixel : renderer.pixels) actualPixels += pixel != -1;
                assert(actualPixels == expectedPixels);
              }
            }
          }
        }
      }
    }
  }
}

int main() {
  coverageAndPacking();
  rendering();
  std::puts("Font weight: packing, scaling, dilation, synthetic bold, BW/AA, dark mode, rotations passed");
}
