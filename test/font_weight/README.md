# Reader font weight tests

```sh
pio pkg install -e default
python test/run_font_weight_test.py
python test/run_epub_bw_snapshot_test.py
```

Requires a C++20 compiler (`CXX`, or `c++`/`g++`/`clang++`). The settings test
uses the project's ArduinoJson dependency from `.pio/libdeps/default`.

Tests compile production code: the coverage helper, weighted glyph renderer,
direction JSON save/load lambdas, TXT page renderer, AA helper, and EPUB content
renderer. They cover packing/trim, resampling, outline bounds, synthetic bold,
both grayscale planes, dark mode, rotations, independent direction persistence,
old defaults, invalid values, and unchanged refresh counts for all three weights.
The status bar must always see Standard; weight must not leak out of page content.

Hardware verification: use BIZ UD Mincho at Medium in horizontal/vertical books,
compare all three weights with AA off/on (six combinations per direction), and
check dense kanji, punctuation, sideways Latin, ruby, and images. Ruby and UI
should remain unchanged. Reopen/reboot to verify persistence. Compare serial
`bw_render`, grayscale pass and `total` timings, and free heap before/after repeated
page turns; outline correction adds CPU work but no frame buffers or refreshes.
