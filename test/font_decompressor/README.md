# Font decompressor OOM tests

```sh
python test/run_font_decompressor_test.py
```

Requires Python 3 and C/C++ compilers (`CC` / `CXX` may select GCC or Clang).
No Python packages or device are needed. Fixtures and executables are removed
from the ignored `build/font_decompressor/` directory after the run.

The test compiles production `FontDecompressor.cpp`, `InflateReader.cpp`, uzlib
and the UTF-8 decoder with C++ exceptions/RTTI disabled, as on the device.
Python's zlib generates real raw-DEFLATE groups and an independent pixel-packing
reference for glyphs with different row widths, plus an empty glyph. Both
contiguous and frequency-grouped font layouts are exercised.

Only the decompressor translation unit's malloc/free calls are intercepted.
The host test injects allocation failures and tracks live buffers, frees and
requested-byte peaks. During OOM probes, unchecked C++ `new` exits the test
with code 86; outside those probes it uses the host allocator. There are no
test allocator hooks or special build flags in production firmware.

Coverage:

- Initial hot-group and glyph-scratch allocation failures return `nullptr`.
- Growth failures free the old buffer and invalidate/recover cache identity.
- A failed scratch allocation can retry without re-decoding/reallocating the
  successfully loaded group.
- Smaller/same-sized requests reuse capacity. Growth frees dead storage before
  allocating its replacement, rather than requiring both buffers to coexist.
- Decode failure never leaves an old identity attached to overwritten data.
- Pixel output matches the independent fixture for both group layouts.
- Blank compressed glyphs do not allocate zero-sized buffers; uncompressed fonts
  still return their direct bitmap pointer.
- Prewarm hits do not allocate; page/lookup/temp allocation failures can use
  the hot-group fallback once allocations succeed again.
- `clearCache()`, `deinit()`, reinitialization and destruction release buffers;
  the allocation registry detects leaks and double frees.

Based on upstream CrossPoint
[#2526](https://github.com/crosspoint-reader/crosspoint-reader/pull/2526).
This only protects the two fallback buffers of built-in compressed fonts. It
does not redesign SD-card font caches, change font formats, or eliminate all
possible low-memory failures elsewhere. On OOM the affected glyph may be omitted
for that draw instead of aborting the firmware; it can be retried on a later draw.

## Device smoke test

1. Install the firmware built with `pio run -e default` and confirm menus render.
2. If available, read a Latin/mixed-language EPUB using a built-in font such as
   Noto Serif/Sans. Change font size, bold/italic styles and orientation, and
   repeatedly navigate pages/chapters. Check that text/spacing remain unchanged.
3. Check Japanese SD-font text and ruby as a regression test. They use a separate
   font path; this patch is not an SD-font memory/performance redesign.
4. Alternate reading, home/settings and WiFi transfer, then sleep/wake. Watch for
   restarts, missing text that persists after redraw, or heap loss between cycles.
   If serial logging is available, compare free heap and the largest allocatable
   block, not just the firmware's static RAM report.

Do not deliberately exhaust device memory or damage font files to trigger OOM.
Host fault injection covers those branches without making the reader unstable;
real-device verification checks integration, not simulated ESP32 heap behavior.
