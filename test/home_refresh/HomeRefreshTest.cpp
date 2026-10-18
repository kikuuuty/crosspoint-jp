#include <algorithm>
#include <cassert>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

struct Rect {
  int x, y, width, height;
};
struct RenderLock {};
enum UIIcon { Folder, Recent, Transfer, Settings, Library, Book };
enum StrId {
  STR_BROWSE_FILES,
  STR_MENU_RECENT_BOOKS,
  STR_FILE_TRANSFER,
  STR_SETTINGS_TITLE,
  STR_OPDS_BROWSER,
  STR_AOZORA_BUNKO,
  STR_SELECT,
  STR_DIR_UP,
  STR_DIR_DOWN
};
const char* tr(StrId id) {
  static constexpr const char* labels[] = {"files",  "recents", "transfer", "settings", "opds",
                                           "aozora", "select",  "up",       "down"};
  return labels[id];
}
struct Renderer {
  int displays = 0;
  void clearScreen() {}
  void displayBuffer() { ++displays; }
};
struct Metrics {
  int topPadding = 10, homeTopPadding = 50, verticalSpacing = 8, homeCoverTileHeight = 250;
  int menuRowHeight = 24, menuSpacing = 4, homeCoverHeight = 200;
};
struct Theme {
  Metrics metrics;
  Rect area{0, 0, 528, 750};
  std::vector<std::string> labels;
  void drawHeader(Renderer&, Rect, const char*) {}
  template <typename Fn>
  void drawRecentBookCover(Renderer&, Rect, const std::vector<int>&, const std::vector<int>&, int, bool&, bool&, bool&,
                           Fn) {}
  template <typename TextFn, typename IconFn>
  void drawButtonMenu(Renderer&, Rect, int count, int, TextFn text, IconFn icon) {
    labels.clear();
    for (int i = 0; i < count; ++i) {
      labels.push_back(text(i));
      (void)icon(i);
    }
  }
  void drawButtonHints(Renderer&, const char*, const char*, const char*, const char*) {}
} theme;
#define GUI theme
struct UITheme {
  static UITheme& getInstance() {
    static UITheme instance;
    return instance;
  }
  const Metrics& getMetrics() const { return theme.metrics; }
  static Rect getContentArea(const Renderer&) { return theme.area; }
};
struct Input {
  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };
  Labels mapLabels(const char* a, const char* b, const char* c, const char* d) { return {a, b, c, d}; }
};
struct HomeActivity {
  Renderer renderer;
  Input mappedInput;
  bool coverBufferStored = false, coverRendered = false, hasOpdsUrl = false;
  bool recentsLoaded = false, recentsLoading = false;
  bool missingCover = false, queued = true;
  int loadCalls = 0, selectorIndex = 0, coverRectX = 0, coverRectY = 0, coverRectW = 0, coverRectH = 0;
  std::vector<int> recentBooks{1}, recentBookProgress{1};
  bool restoreCoverBuffer() { return true; }
  bool storeCoverBuffer() { return true; }
  void requestUpdate() { queued = true; }
  void loadRecentCovers(int) {
    ++loadCalls;
    recentsLoaded = true;
    recentsLoading = false;
    if (missingCover) requestUpdate();
  }
  void render(RenderLock&&);
  void drain() {
    int count = 0;
    while (queued) {
      queued = false;
      render(RenderLock{});
      assert(++count <= 2);
    }
  }
};

// Extract the real method at test time rather than duplicating its logic.
#include "HomeRender.inc"

int main() {
  for (bool missing : {false, true}) {
    for (bool opds : {false, true}) {
      for (bool landscape : {false, true}) {
        theme.area = landscape ? Rect{0, 0, 792, 480} : Rect{0, 0, 528, 750};
        HomeActivity home;
        home.missingCover = missing;
        home.hasOpdsUrl = opds;
        home.drain();
        const int expected = 1 + (missing ? 1 : 0);
        assert(home.renderer.displays == expected);
        assert(home.loadCalls == 1 && home.recentsLoaded && !home.recentsLoading);
        const std::vector<std::string> labels =
            opds ? std::vector<std::string>{"files", "recents", "opds", "aozora", "transfer", "settings"}
                 : std::vector<std::string>{"files", "recents", "aozora", "transfer", "settings"};
        assert(theme.labels == labels);
        // Returning from a child must still recheck covers, without a
        // duplicate update when the cover is unchanged.
        home.recentsLoaded = false;
        home.queued = true;
        home.drain();
        assert(home.renderer.displays == expected * 2);
        assert(home.loadCalls == 2);
      }
    }
  }
  std::puts("Home refresh: cached/missing covers, child return, OPDS and portrait/landscape checks passed");
}
