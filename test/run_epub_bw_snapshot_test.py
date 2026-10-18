#!/usr/bin/env python3
"""Test the production EPUB renderContents body with a fake display."""

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
    build = root / "build" / "epub_bw_snapshot"
    build.mkdir(parents=True, exist_ok=True)
    source = (root / "src/activities/reader/EpubReaderActivity.cpp").read_text(encoding="utf-8")
    body = source[source.index("void EpubReaderActivity::renderContents("):
                  source.index("void EpubReaderActivity::renderStatusBar(")]
    with tempfile.TemporaryDirectory(dir=build) as tmp:
        tmp = Path(tmp)
        (tmp / "RenderContents.inc").write_text(body, encoding="utf-8")
        binary = tmp / ("epub_bw_snapshot.exe" if os.name == "nt" else "epub_bw_snapshot")
        subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-Wno-unused-variable", "-Wno-unused-parameter", "-I", str(tmp),
                        str(root / "test/epub_bw_snapshot/EpubBwSnapshotTest.cpp"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
