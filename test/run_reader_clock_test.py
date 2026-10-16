#!/usr/bin/env python3
"""Test the production reader clock on Windows or Unix without device libraries."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parent.parent
    compiler = os.environ.get("CXX") or next(
        (path for name in ("c++", "g++", "clang++") if (path := shutil.which(name))),
        None,
    )
    if compiler is None:
        raise SystemExit("Set CXX to a C++ compiler (GCC or Clang).")

    build = root / "build" / "reader_clock"
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as directory:
        binary = Path(directory) / ("ReaderClockTest.exe" if os.name == "nt" else "ReaderClockTest")
        subprocess.run(
            [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
             "-I", str(root / "src"), str(root / "test" / "reader_clock" / "ReaderClockTest.cpp"),
             "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)

        sync_binary = Path(directory) / ("TimeSyncTest.exe" if os.name == "nt" else "TimeSyncTest")
        subprocess.run(
            [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable",
             "-I", str(root / "test" / "reader_clock" / "stubs"), "-I", str(root / "src"),
             str(root / "src" / "network" / "TimeSync.cpp"),
             str(root / "test" / "reader_clock" / "TimeSyncTest.cpp"), "-o", str(sync_binary)],
            check=True,
        )
        subprocess.run([str(sync_binary)], check=True)

        layout_binary = Path(directory) / ("HeaderStatusLayoutTest.exe" if os.name == "nt" else "HeaderStatusLayoutTest")
        subprocess.run(
            [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
             "-I", str(root / "src"), str(root / "src" / "components" / "themes" / "HeaderStatusLayout.cpp"),
             str(root / "test" / "reader_clock" / "HeaderStatusLayoutTest.cpp"), "-o", str(layout_binary)],
            check=True,
        )
        subprocess.run([str(layout_binary)], check=True)


if __name__ == "__main__":
    main()
