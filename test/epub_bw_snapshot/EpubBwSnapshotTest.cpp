#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
unsigned long millis() {
  static unsigned long time = 0;
  return ++time;
}

struct HalDisplay {
  enum RefreshMode { FAST_REFRESH, HALF_REFRESH };
};
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
  enum class GlyphInk { Sharp, GrayscaleBase };
  enum RenderMode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB };
  FontCacheManager fcm;
  int stores = 0, restores = 0, cleanups = 0, grayDisplays = 0, renders = 0;
  bool failStore = false, validBw = true, controllerSynced = false;
  int getScreenWidth() const { return 528; }
  FontCacheManager* getFontCacheManager() { return &fcm; }
  void setVerticalCharSpacing(int) {}
  void setGlyphInk(GlyphInk) {}
  void fillRect(int, int, int, int, bool) {}
  void displayBuffer(HalDisplay::RefreshMode = HalDisplay::FAST_REFRESH) {}
  bool storeBwBuffer() {
    ++stores;
    assert(validBw);
    return !failStore;
  }
  void restoreBwBuffer() {
    ++restores;
    assert(stores == 1 && !failStore);
    validBw = true;
    cleanupGrayscaleWithFrameBuffer();
  }
  void cleanupGrayscaleWithFrameBuffer() {
    assert(validBw);
    ++cleanups;
    controllerSynced = true;
  }
  void clearScreen(int) { validBw = false; }
  void setRenderMode(RenderMode) {}
  void copyGrayscaleLsbBuffers() {}
  void copyGrayscaleMsbBuffers() {}
  void displayGrayBuffer() { ++grayDisplays; }
};
struct Page {
  void collectCodepoints(std::vector<uint32_t>&, size_t) {}
  void render(GfxRenderer& renderer, int, int, int, int) { ++renderer.renders; }
  bool hasImages() const { return false; }
  bool getImageBoundingBox(int16_t&, int16_t&, int16_t&, int16_t&) const { return false; }
};
struct DirectionSettings {
  bool textAntiAliasing = false;
};
struct Settings {
  DirectionSettings ds;
  const DirectionSettings& getDirectionSettings(bool) const { return ds; }
  int getReaderFontId(bool) const { return 1; }
  int getVerticalCharSpacingPercent() const { return 100; }
} settings;
#define SETTINGS settings
struct ExternalFont {
  size_t getCacheCapacity() const { return 128; }
  void preloadGlyphs(const uint32_t*, size_t) {}
};
struct FontManager {
  bool external = false;
  static FontManager& getInstance() {
    static FontManager manager;
    return manager;
  }
  bool isExternalFontEnabled() const { return external; }
  ExternalFont* getActiveFont() const { return nullptr; }
};
namespace ReaderUtils {
void displayWithRefreshCycle(GfxRenderer&, int&) {}
}  // namespace ReaderUtils
struct EpubReaderActivity {
  GfxRenderer renderer;
  bool verticalMode = false;
  int pagesUntilFullRefresh = 0;
  void renderStatusBar() {}
  void renderContents(std::unique_ptr<Page>, int, int, int, int);
};

// Extract the real method at test time rather than duplicating its logic.
#include "RenderContents.inc"

int main() {
  for (const bool aa : {false, true}) {
    for (const bool external : {false, true}) {
      for (const bool oom : {false, true}) {
        for (const bool vertical : {false, true}) {
          settings.ds.textAntiAliasing = aa;
          FontManager::getInstance().external = external;
          EpubReaderActivity reader;
          reader.verticalMode = vertical;
          reader.renderer.failStore = oom;
          reader.renderContents(std::make_unique<Page>(), 0, 0, 0, 0);
          const bool gray = aa && !external;
          const auto& r = reader.renderer;
          assert(r.stores == (gray ? 1 : 0));
          assert(r.restores == (gray && !oom ? 1 : 0));
          assert(r.cleanups == 1 && r.controllerSynced && r.validBw);
          assert(r.grayDisplays == (gray && !oom ? 1 : 0));
          assert(r.renders == (gray && !oom ? 4 : 2));
        }
      }
    }
  }
  std::puts("EPUB BW snapshot: 16 AA/external/OOM/direction combinations passed");
}
