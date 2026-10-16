#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/ruby"
mkdir -p "$BUILD_DIR"

"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -pedantic \
  -I"$ROOT_DIR/lib/Epub" -I"$ROOT_DIR/lib/GfxRenderer" \
  "$ROOT_DIR/test/ruby/RubyTest.cpp" -o "$BUILD_DIR/RubyTest"
"$BUILD_DIR/RubyTest"

"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -pedantic \
  -Wno-unused-parameter -Wno-unused-function \
  -I"$ROOT_DIR/test/ruby/stubs" -I"$ROOT_DIR/lib/Epub" \
  -I"$ROOT_DIR/lib/GfxRenderer" -I"$ROOT_DIR/lib/EpdFont" \
  -I"$ROOT_DIR/lib/Utf8" -I"$ROOT_DIR/lib/Serialization" \
  "$ROOT_DIR/test/ruby/RubyLayoutTest.cpp" \
  "$ROOT_DIR/lib/Epub/Epub/ParsedText.cpp" "$ROOT_DIR/lib/Epub/Epub/blocks/TextBlock.cpp" \
  "$ROOT_DIR/lib/Utf8/Utf8.cpp" -o "$BUILD_DIR/RubyLayoutTest"
"$BUILD_DIR/RubyLayoutTest"
