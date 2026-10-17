#!/usr/bin/env python3
"""Test production ZipFile/InflateReader/uzlib with read faults on Windows or Unix."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile


def main():
    root = Path(__file__).resolve().parent.parent
    tests = root / "test" / "zip_file"
    compiler = os.environ.get("CXX") or next(
        (path for name in ("c++", "g++", "clang++") if (path := shutil.which(name))),
        None,
    )
    if compiler is None:
        raise SystemExit("Set CXX to a C++ compiler (GCC or Clang).")

    build = root / "build" / "zip_file"
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as directory:
        directory = Path(directory)
        binary = directory / ("ZipFileTest.exe" if os.name == "nt" else "ZipFileTest")
        # Compile uzlib as C, as on the device, rather than relying on C++'s
        # treatment of C input files (which differs between GCC and Clang).
        c_compiler = os.environ.get("CC") or next(
            (path for name in ("cc", "gcc", "clang") if (path := shutil.which(name))),
            None,
        )
        if c_compiler is None:
            raise SystemExit("Set CC to a C compiler (GCC or Clang).")
        uzlib = directory / "tinflate.o"
        subprocess.run(
            [c_compiler, "-c", str(root / "lib" / "uzlib" / "src" / "tinflate.c"), "-o", str(uzlib)],
            check=True,
        )
        subprocess.run(
            [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
             "-I", str(tests / "stubs"), "-I", str(root / "lib" / "ZipFile"),
             "-I", str(root / "lib" / "InflateReader"), "-I", str(root / "lib" / "uzlib" / "src"),
             str(tests / "ZipFileTest.cpp"), str(root / "lib" / "InflateReader" / "InflateReader.cpp"),
             str(uzlib), "-o", str(binary)],
            check=True,
        )

        payload = ("<p>ZIP read test: \u65e5\u672c\u8a9e\u306e\u672c\u6587\u3002</p>\n" * 100).encode("utf-8")
        expected = directory / "expected.txt"
        expected.write_bytes(payload)
        compressed_size = 0
        for name, compression in (("deflated.zip", zipfile.ZIP_DEFLATED), ("stored.zip", zipfile.ZIP_STORED)):
            with zipfile.ZipFile(directory / name, "w", compression=compression) as fixture:
                fixture.writestr("content.xhtml", payload)
                if compression == zipfile.ZIP_DEFLATED:
                    compressed_size = fixture.getinfo("content.xhtml").compress_size
        subprocess.run(
            [str(binary), str(directory / "deflated.zip"), str(directory / "stored.zip"),
             str(expected), str(compressed_size)],
            check=True,
        )


if __name__ == "__main__":
    main()
