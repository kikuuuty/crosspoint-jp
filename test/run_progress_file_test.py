#!/usr/bin/env python3
"""Test production progress persistence and reading status on Windows or Unix."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parent.parent
    tests = root / "test" / "progress_file"
    compiler = os.environ.get("CXX") or next(
        (path for name in ("c++", "g++", "clang++") if (path := shutil.which(name))),
        None,
    )
    if compiler is None:
        raise SystemExit("Set CXX to a C++ compiler (GCC or Clang).")

    build = root / "build" / "progress_file"
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as directory:
        directory = Path(directory)
        binary = directory / ("ProgressFileTest.exe" if os.name == "nt" else "ProgressFileTest")
        # Compile the save methods verbatim with tiny owner classes, avoiding the
        # embedded UI dependency graph. The firmware build checks full integration.
        methods = []
        for name in ("EpubReaderActivity", "TxtReaderActivity", "XtcReaderActivity"):
            source = (root / "src" / "activities" / "reader" / f"{name}.cpp").read_text(encoding="utf-8")
            start = source.index(f"void {name}::saveProgress(")
            end = source.index("\n}", start) + 2
            methods.append(source[start:end])
        writers = directory / "ReaderProgressWriters.cpp"
        writers.write_text(
            '#include <algorithm>\n#include <cstring>\n#include "ProgressFile.h"\n'
            '#include "ReadingStatusHelper.h"\n#include "ReaderProgressHarness.h"\n' + "\n".join(methods),
            encoding="utf-8",
        )
        subprocess.run(
            [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
             "-I", str(tests / "stubs"), "-I", str(tests),
             "-I", str(root / "src"), "-I", str(root / "lib" / "FsHelpers"),
             str(tests / "ProgressFileTest.cpp"), str(root / "src" / "ReadingStatusHelper.cpp"),
             str(writers), str(root / "lib" / "FsHelpers" / "FsHelpers.cpp"), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
