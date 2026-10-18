#!/usr/bin/env python3
"""Test the production HomeActivity::render body without hardware or SDK."""

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
    build = root / "build" / "home_refresh_test"
    build.mkdir(parents=True, exist_ok=True)
    source = (root / "src/activities/home/HomeActivity.cpp").read_text(encoding="utf-8")
    body = source[source.index("void HomeActivity::render("):source.index("void HomeActivity::onSelectBook(")]
    with tempfile.TemporaryDirectory(dir=build) as tmp:
        tmp = Path(tmp)
        (tmp / "HomeRender.inc").write_text(body, encoding="utf-8")
        binary = tmp / ("home_refresh.exe" if os.name == "nt" else "home_refresh")
        subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-I", str(tmp), str(root / "test/home_refresh/HomeRefreshTest.cpp"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
