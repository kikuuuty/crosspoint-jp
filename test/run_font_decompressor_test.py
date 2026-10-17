#!/usr/bin/env python3
"""Test the production font decompressor and raw inflate with checked heap faults."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zlib


def fixture_header():
    # Independent reference: 2-bit pixels, each source row aligned to one byte.
    glyphs = [(3, 2, bytes.fromhex("6c b4")), (4, 2, bytes.fromhex("1b e4")), (0, 0, b""),
              (5, 4, bytes.fromhex("1b c0 e4 40 6c 80 93 00")), (2, 4, bytes.fromhex("60 b0 c0 10"))]
    lines = ['#pragma once', '#include "EpdFontData.h"']
    expected = []
    metadata = []
    for index, (width, height, aligned) in enumerate(glyphs):
        stride = (width + 3) // 4
        bits = ''.join(f'{(aligned[y * stride + x // 4] >> (6 - 2 * (x % 4))) & 3:02b}'
                       for y in range(height) for x in range(width))
        bits += '0' * (-len(bits) % 8)
        packed = bytes(int(bits[start:start + 8], 2) for start in range(0, len(bits), 8))
        lines.append(f'inline constexpr uint8_t EXPECTED_{index}[] = {{{", ".join(map(str, packed)) or "0"}}};')
        expected.append(f'EXPECTED_{index}')
        metadata.append(f'{{{width}, {height}, 0, 0, 0, {len(packed)}, 0}}')
    lines += [f'inline constexpr const uint8_t* EXPECTED[] = {{{", ".join(expected)}}};',
              f'inline constexpr EpdGlyph GLYPHS[] = {{{", ".join(metadata)}}};',
              'inline constexpr EpdUnicodeInterval INTERVALS[] = {{65, 67, 0}, {0x65e5, 0x65e6, 3}};']
    for name, groups in [('CONTIGUOUS', [[0, 1, 2], [3, 4]]), ('FREQUENCY', [[0, 2, 3], [1, 4]])]:
        bitmap = bytearray()
        metadata = []
        for indices in groups:
            raw = b''.join(glyphs[index][2] for index in indices)
            compressor = zlib.compressobj(wbits=-15)
            compressed = compressor.compress(raw) + compressor.flush()
            metadata.append(f'{{{len(bitmap)}, {len(compressed)}, {len(raw)}, {len(indices)}, {indices[0]}}}')
            bitmap.extend(compressed)
        lines += [f'inline constexpr uint8_t {name}_BITMAP[] = {{{", ".join(map(str, bitmap))}}};',
                  f'inline constexpr EpdFontGroup {name}_GROUPS[] = {{{", ".join(metadata)}}};']
    lines.append('inline constexpr uint16_t GLYPH_TO_GROUP[] = {0, 1, 0, 0, 1};')
    return '\n'.join(lines) + '\n'


def main():
    root = Path(__file__).resolve().parent.parent
    tests = root / "test/font_decompressor"
    compiler = os.environ.get("CXX") or next(
        (path for name in ("c++", "g++", "clang++") if (path := shutil.which(name))), None,
    )
    c_compiler = os.environ.get("CC") or next(
        (path for name in ("cc", "gcc", "clang") if (path := shutil.which(name))), None,
    )
    if not compiler or not c_compiler:
        raise SystemExit("Set CC/CXX to C and C++ compilers (GCC or Clang).")
    build = root / "build/font_decompressor"
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as directory:
        directory = Path(directory)
        (directory / "FontFixture.generated.h").write_text(fixture_header(), encoding="utf-8")
        uzlib = directory / "tinflate.o"
        subprocess.run([c_compiler, "-c", str(root / "lib/uzlib/src/tinflate.c"), "-o", str(uzlib)], check=True)
        binary = directory / ("FontDecompressorTest.exe" if os.name == "nt" else "FontDecompressorTest")
        subprocess.run(
            [compiler, "-std=c++20", "-fno-exceptions", "-fno-rtti", "-Wall", "-Wextra", "-Werror",
             "-I", str(tests / "stubs"), "-I", str(directory), "-I", str(root / "lib/EpdFont"),
             "-I", str(root / "lib/InflateReader"), "-I", str(root / "lib/uzlib/src"), "-I", str(root / "lib/Utf8"),
             str(tests / "FontDecompressorTest.cpp"), str(root / "lib/InflateReader/InflateReader.cpp"),
             str(root / "lib/Utf8/Utf8.cpp"), str(uzlib), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
