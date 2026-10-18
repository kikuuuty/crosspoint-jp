#include <ArduinoJson.h>
#include <CrossPointSettings.h>

#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
  // The exact direction save/load lambdas extracted from JsonSettingsIO.cpp.
#include "DirectionSettingsIO.inc"
  for (uint8_t horizontal = 0; horizontal <= 2; ++horizontal) {
    for (uint8_t vertical = 0; vertical <= 2; ++vertical) {
      DirectionSettings h, v;
      assert(h.fontWeight == 0 && v.fontWeight == 0);
      h.fontWeight = horizontal;
      v.fontWeight = vertical;
      h.fontSize = 1;
      v.fontSize = 2;
      h.textAntiAliasing = 0;
      v.textAntiAliasing = 1;
      JsonDocument doc;
      saveDirection(doc["horizontal"].to<JsonObject>(), h);
      saveDirection(doc["vertical"].to<JsonObject>(), v);
      std::string json;
      serializeJson(doc, json);
      JsonDocument restored;
      assert(!deserializeJson(restored, json));
      DirectionSettings loadedH, loadedV;
      assert(loadDirection(restored["horizontal"].as<JsonObject>(), loadedH));
      assert(loadDirection(restored["vertical"].as<JsonObject>(), loadedV));
      assert(loadedH.fontWeight == horizontal && loadedV.fontWeight == vertical);
      assert(loadedH.fontSize == 1 && loadedV.fontSize == 2);
      assert(loadedH.textAntiAliasing == 0 && loadedV.textAntiAliasing == 1);
    }
  }
  // An existing JSON object without the new field defaults to Standard.
  JsonDocument legacy;
  auto obj = legacy.to<JsonObject>();
  obj["fontSize"] = 1;
  obj["lineSpacing"] = 185;
  DirectionSettings loaded;
  loaded.fontWeight = 2;
  assert(loadDirection(obj, loaded) && loaded.fontWeight == 0);
  for (const int invalid : {3, 100, 255, -1}) {
    obj["fontWeight"] = invalid;
    assert(loadDirection(obj, loaded) && loaded.fontWeight == 0);
  }
  std::puts("Font weight settings: direction independence, JSON round-trip, old defaults, invalid values passed");
}
