#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define LOG_ERR(...) ((void)0)

struct FontCacheManager {
  bool scanning = false;
  struct Scope {
    FontCacheManager& manager;
    void endScanAndPrewarm() { manager.scanning = false; }
  };
  Scope createPrewarmScope() {
    scanning = true;
    return {*this};
  }
};
struct GfxRenderer {
  enum RenderMode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB };
  enum class GlyphInk { Sharp, GrayscaleBase };
  uint8_t fontWeight = 0;
  FontCacheManager manager;
  std::vector<uint8_t> weights;
  int displays = 0, grayDisplays = 0, stores = 0, restores = 0;
  int getLineHeight(int) { return 20; }
  int getTextWidth(int, const char*) { return 40; }
  void setFontWeight(uint8_t value) { fontWeight = value; }
  void setGlyphInk(GlyphInk) {}
  void drawText(int, int, int, const char*) {
    if (!manager.scanning) weights.push_back(fontWeight);
  }
  FontCacheManager* getFontCacheManager() { return &manager; }
  bool storeBwBuffer() {
    ++stores;
    return true;
  }
  void restoreBwBuffer() { ++restores; }
  void clearScreen(int) {}
  void setRenderMode(RenderMode) {}
  void copyGrayscaleLsbBuffers() {}
  void copyGrayscaleMsbBuffers() {}
  void displayGrayBuffer() { ++grayDisplays; }
};
struct CrossPointSettings {
  enum { LEFT_ALIGN, CENTER_ALIGN, RIGHT_ALIGN, JUSTIFIED };
  struct Direction {
    uint8_t fontWeight = 0;
    bool textAntiAliasing = false;
  } horizontal;
  float getReaderLineCompression(bool) { return 1.0f; }
} settings;
#define SETTINGS settings
namespace ReaderUtils {
void displayWithRefreshCycle(GfxRenderer& renderer, int&) { ++renderer.displays; }
#include "RenderAntiAliased.inc"
}  // namespace ReaderUtils
struct TxtReaderActivity {
  GfxRenderer renderer;
  int cachedFontId = 1, viewportWidth = 400, cachedOrientedMarginTop = 10, cachedOrientedMarginLeft = 10;
  int cachedParagraphAlignment = 0, pagesUntilFullRefresh = 1;
  std::vector<std::string> currentPageLines{"first", "", "second"};
  void renderStatusBar() { assert(renderer.fontWeight == 0); }
  void renderPage();
};
#include "TxtRenderPage.inc"

int main() {
  for (uint8_t weight = 0; weight <= 2; ++weight) {
    for (const bool aa : {false, true}) {
      for (int alignment = 0; alignment < 4; ++alignment) {
        settings.horizontal.fontWeight = weight;
        settings.horizontal.textAntiAliasing = aa;
        TxtReaderActivity reader;
        reader.cachedParagraphAlignment = alignment;
        reader.renderPage();
        const auto& r = reader.renderer;
        assert(r.fontWeight == 0 && r.displays == 1);
        assert(r.stores == aa && r.restores == aa && r.grayDisplays == aa);
        assert(r.weights.size() == (aa ? 6 : 2));
        for (const auto applied : r.weights) assert(applied == weight);
      }
    }
  }
  std::puts("TXT font weight: all weights/AA/alignments, status isolation and refresh counts passed");
}
