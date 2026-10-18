#!/usr/bin/env python3
"""Run production glyph-weight/plane encoding and settings persistence tests."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    compiler = os.environ.get("CXX") or next(
        (path for name in ("c++", "g++", "clang++") if (path := shutil.which(name))), None)
    if not compiler:
        raise SystemExit("Set CXX to a C++ compiler")
    build = root / "build" / "font_weight"
    build.mkdir(parents=True, exist_ok=True)
    source = (root / "lib/GfxRenderer/GfxRenderer.cpp").read_text(encoding="utf-8")
    header = (root / "lib/GfxRenderer/GfxRenderer.h").read_text(encoding="utf-8")
    body = source[source.index("void GfxRenderer::renderWeightedGlyph("):
                  source.index("void GfxRenderer::getOrientedViewableTRBL(")]
    ink = header[header.index("  bool isGlyphInk("):header.index("  // Orientation control")]
    settings = (root / "src/JsonSettingsIO.cpp").read_text(encoding="utf-8")
    save = settings[settings.index("  auto saveDirection ="):settings.index("  saveDirection(doc[")]
    load = settings[settings.index("  auto loadDirection ="):settings.index("\n  if (!loadDirection(")]
    ui = (root / "src/activities/settings/DirectionSettingsActivity.cpp").read_text(encoding="utf-8")
    assert ui.index("StrId::STR_FONT_SIZE") < ui.index("StrId::STR_FONT_WEIGHT") < ui.index("StrId::STR_LINE_SPACING")
    arduinojson = root / ".pio/libdeps/default/ArduinoJson/src"
    if not (arduinojson / "ArduinoJson.h").exists():
        raise SystemExit("Install build dependencies first: pio pkg install -e default")
    with tempfile.TemporaryDirectory(dir=build) as tmp:
        tmp = Path(tmp)
        (tmp / "WeightedGlyph.inc").write_text(body, encoding="utf-8")
        (tmp / "GlyphInk.inc").write_text(ink, encoding="utf-8")
        binary = tmp / ("font_weight.exe" if os.name == "nt" else "font_weight")
        subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-I", str(tmp), "-I", str(root / "lib/GfxRenderer"),
                        str(root / "test/font_weight/FontWeightTest.cpp"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        (tmp / "DirectionSettingsIO.inc").write_text(save + load, encoding="utf-8")
        settings_binary = tmp / ("font_weight_settings.exe" if os.name == "nt" else "font_weight_settings")
        subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-D_CRT_SECURE_NO_WARNINGS",
                        "-I", str(tmp), "-I", str(root / "test/font_weight/stubs"),
                        "-I", str(root / "src"), "-I", str(arduinojson),
                        str(root / "test/font_weight/FontWeightSettingsTest.cpp"),
                        "-o", str(settings_binary)], check=True)
        subprocess.run([str(settings_binary)], check=True)
        txt = (root / "src/activities/reader/TxtReaderActivity.cpp").read_text(encoding="utf-8")
        txt_body = txt[txt.index("void TxtReaderActivity::renderPage()"):
                       txt.index("void TxtReaderActivity::renderStatusBar()")]
        reader_utils = (root / "src/activities/reader/ReaderUtils.h").read_text(encoding="utf-8")
        aa = reader_utils[reader_utils.index("template <typename RenderFn>"):
                          reader_utils.index("}  // namespace ReaderUtils")]
        (tmp / "TxtRenderPage.inc").write_text(txt_body, encoding="utf-8")
        (tmp / "RenderAntiAliased.inc").write_text(aa, encoding="utf-8")
        txt_binary = tmp / ("txt_font_weight.exe" if os.name == "nt" else "txt_font_weight")
        subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-I", str(tmp), str(root / "test/font_weight/TxtFontWeightTest.cpp"),
                        "-o", str(txt_binary)], check=True)
        subprocess.run([str(txt_binary)], check=True)


if __name__ == "__main__":
    main()
