#include <Arduino.h>
#include <FontDecompressor.h>
#include <Logging.h>
#include <Utf8.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FontFixture.generated.h"

namespace {
void check(bool value, const char* message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

struct Allocation {
  void* pointer = nullptr;
  size_t size = 0;
};
struct Memory {
  Allocation live[64];
  size_t calls = 0;
  size_t failures = 0;
  size_t frees = 0;
  size_t bytes = 0;
  size_t peak = 0;
  int failAfter = -1;  // One-shot failure after N successful allocation calls.
} memory;
bool rejectCppAllocations = false;

void* fontTestMalloc(size_t size) {
  ++memory.calls;
  if (memory.failAfter == 0) {
    memory.failAfter = -1;
    ++memory.failures;
    return nullptr;
  }
  if (memory.failAfter > 0) --memory.failAfter;
  void* pointer = std::malloc(size);
  check(pointer != nullptr, "host allocator unexpectedly failed");
  for (auto& allocation : memory.live) {
    if (!allocation.pointer) {
      allocation = {pointer, size};
      memory.bytes += size;
      if (memory.bytes > memory.peak) memory.peak = memory.bytes;
      return pointer;
    }
  }
  check(false, "allocation registry exhausted");
  return nullptr;
}

void fontTestFree(void* pointer) {
  if (!pointer) return;
  for (auto& allocation : memory.live) {
    if (allocation.pointer == pointer) {
      memory.bytes -= allocation.size;
      allocation = {};
      ++memory.frees;
      std::free(pointer);
      return;
    }
  }
  check(false, "double free or untracked buffer");
}

size_t liveCount() {
  size_t count = 0;
  for (const auto& allocation : memory.live) {
    if (allocation.pointer) ++count;
  }
  return count;
}

void resetMemory() {
  check(liveCount() == 0 && memory.bytes == 0, "all font buffers must be released between tests");
  memory = {};
}
}  // namespace

// During OOM probes, an unchecked C++ allocation must fail fast, as it does in
// the device's -fno-exceptions build. The fixed path uses checked malloc instead.
void* operator new(size_t size) {
  if (rejectCppAllocations) {
    std::fputs("Unchecked C++ allocation during font OOM probe\n", stderr);
    std::fflush(stderr);
    std::_Exit(86);  // Deterministic failure, without a Windows crash dialog.
  }
  if (void* pointer = std::malloc(size ? size : 1)) return pointer;
  std::abort();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, size_t) noexcept { std::free(pointer); }

// Intercept only the production decompressor's malloc/free calls, not host STL
// allocations or uzlib. All included headers were loaded before these macros.
// No allocator hooks or test conditionals are added to production firmware.
#define malloc fontTestMalloc
#define free fontTestFree
#include "FontDecompressor.cpp"
#undef free
#undef malloc

// Optional checksum wrappers are not used by raw InflateReader. The firmware
// linker drops them; fail-fast stubs satisfy host linkers that retain them.
extern "C" uint32_t uzlib_adler32(const void*, unsigned int, uint32_t) { std::abort(); }
extern "C" uint32_t uzlib_crc32(const void*, unsigned int, uint32_t) { std::abort(); }

namespace {
struct FontFixture {
  EpdFontData data = {};

  explicit FontFixture(bool frequency = false) {
    data.bitmap = frequency ? FREQUENCY_BITMAP : CONTIGUOUS_BITMAP;
    data.glyph = GLYPHS;
    data.intervals = INTERVALS;
    data.intervalCount = 2;
    data.is2Bit = true;
    data.groups = frequency ? FREQUENCY_GROUPS : CONTIGUOUS_GROUPS;
    data.groupCount = 2;
    data.glyphToGroup = frequency ? GLYPH_TO_GROUP : nullptr;
  }
};

void expectGlyph(FontDecompressor& decoder, const FontFixture& font, uint32_t index) {
  const uint8_t* bitmap = decoder.getBitmap(&font.data, &GLYPHS[index], index);
  if (GLYPHS[index].dataLength == 0) {
    check(bitmap == nullptr, "blank compressed glyph must not expose a bitmap");
    return;
  }
  check(bitmap != nullptr, "glyph must be available after recovery");
  check(std::memcmp(bitmap, EXPECTED[index], GLYPHS[index].dataLength) == 0,
        "decompressed/compacted pixels must match the independent fixture");
}

const uint8_t* oomBitmap(FontDecompressor& decoder, const FontFixture& font, uint32_t index) {
  rejectCppAllocations = true;
  const uint8_t* bitmap = decoder.getBitmap(&font.data, &GLYPHS[index], index);
  rejectCppAllocations = false;
  return bitmap;
}

void testPixelsAndReuse() {
  resetMemory();
  {
    FontFixture font;
    FontDecompressor decoder;
    check(decoder.init(), "init must succeed");
    expectGlyph(decoder, font, 0);
    check(memory.calls == 2 && liveCount() == 2, "fallback must own one group and one glyph buffer");
    expectGlyph(decoder, font, 1);
    check(memory.calls == 2, "same-size glyph in the cached group must not allocate");
    expectGlyph(decoder, font, 3);
    check(memory.calls == 4 && liveCount() == 2, "larger group and scratch must grow without leaking");
    const auto retained = memory.bytes;
    check(memory.peak == retained, "growing buffers must free old storage before allocating replacements");
    expectGlyph(decoder, font, 0);
    expectGlyph(decoder, font, 4);
    check(memory.calls == 4 && memory.bytes == retained, "smaller requests must reuse high-water capacity");
    decoder.clearCache();
    check(liveCount() == 0, "clearCache must release both buffers");
    decoder.deinit();
    check(decoder.init(), "reinitialization after clear/deinit must succeed");
    expectGlyph(decoder, font, 0);
  }
  check(liveCount() == 0, "destructor must release buffers");

  resetMemory();
  {
    FontFixture font(true);
    FontDecompressor decoder;
    for (uint32_t index = 0; index < 5; ++index) expectGlyph(decoder, font, index);
  }
  resetMemory();
}

void testAllocationFailures() {
  for (const int failAfter : {0, 1}) {
    resetMemory();
    {
      FontFixture font;
      FontDecompressor decoder;
      memory.failAfter = failAfter;
      check(oomBitmap(decoder, font, 0) == nullptr, "initial buffer OOM must return nullptr");
      check(memory.failures == 1, "the requested allocation failure must occur");
      const auto calls = memory.calls;
      expectGlyph(decoder, font, 0);
      check(memory.calls == calls + (failAfter == 0 ? 2 : 1), "retry must reuse any successfully decoded group");
    }
    check(liveCount() == 0, "destructor after initial OOM must not leak");
  }

  for (const int failAfter : {0, 1}) {
    resetMemory();
    {
      FontFixture font;
      FontDecompressor decoder;
      expectGlyph(decoder, font, 0);
      memory.failAfter = failAfter;
      check(oomBitmap(decoder, font, 3) == nullptr, "buffer-growth OOM must return nullptr");
      check(memory.failures == 1 && liveCount() == 1, "failed growth must free the old buffer, not leak it");
      const auto calls = memory.calls;
      expectGlyph(decoder, font, failAfter == 0 ? 0 : 3);
      check(memory.calls == calls + 1, "retry must recover without an unnecessary second allocation");
      expectGlyph(decoder, font, 0);
      decoder.clearCache();
      check(liveCount() == 0, "clearCache after growth OOM must release all storage");
    }
  }
  resetMemory();
}

void testFailedDecodeAndBlankGlyph() {
  resetMemory();
  {
    FontFixture font;
    FontDecompressor decoder;
    expectGlyph(decoder, font, 2);
    check(memory.calls == 0, "blank glyph must not allocate zero-sized buffers");
    expectGlyph(decoder, font, 0);
    const auto calls = memory.calls;

    const uint8_t invalidDeflate[] = {0x07};  // Reserved BTYPE=3: deterministic decode failure.
    EpdFontGroup group = CONTIGUOUS_GROUPS[0];
    group.compressedOffset = 0;
    group.compressedSize = sizeof(invalidDeflate);
    FontFixture broken;
    broken.data.bitmap = invalidDeflate;
    broken.data.groups = &group;
    broken.data.groupCount = 1;
    check(decoder.getBitmap(&broken.data, &GLYPHS[0], 0) == nullptr, "decode failure must return nullptr");
    expectGlyph(decoder, font, 0);
    check(memory.calls == calls, "failed decode must invalidate identity while retaining reusable storage");
  }
  resetMemory();
}

void testPrewarmAndFallback() {
  const char* text = "AA\xe6\x97\xa5\xe6\x97\xa5";  // A and U+65E5, duplicated.
  for (const bool frequency : {false, true}) {
    resetMemory();
    {
      FontFixture font(frequency);
      FontDecompressor decoder;
      check(decoder.prewarmCache(&font.data, text) == 0, "prewarm must handle Latin and UTF-8 glyphs");
      const auto calls = memory.calls;
      memory.failAfter = 0;
      expectGlyph(decoder, font, 0);
      expectGlyph(decoder, font, 3);
      check(memory.calls == calls, "prewarmed hits must not allocate");
      memory.failAfter = -1;
    }
  }
  for (const int failure : {0, 1, 2}) {
    resetMemory();
    {
      FontFixture font;
      FontDecompressor decoder;
      memory.failAfter = failure;  // Page buffer, lookup table, then first group temp.
      check(decoder.prewarmCache(&font.data, text) > 0, "prewarm allocation failure must be reported");
      check(memory.failures == 1, "prewarm failure injection must be exercised");
      expectGlyph(decoder, font, 0);
      expectGlyph(decoder, font, 3);
    }
  }
  resetMemory();
}

void testUncompressedFont() {
  FontFixture font;
  font.data.groups = nullptr;
  font.data.groupCount = 0;
  font.data.bitmap = EXPECTED[0];
  FontDecompressor decoder;
  memory.failAfter = 0;
  check(decoder.getBitmap(&font.data, &GLYPHS[0], 0) == EXPECTED[0], "uncompressed path must remain a direct pointer");
  check(memory.calls == 0, "uncompressed path must not allocate");
  memory.failAfter = -1;
}
}  // namespace

int main() {
  testAllocationFailures();
  testPixelsAndReuse();
  testFailedDecodeAndBlankGlyph();
  testPrewarmAndFallback();
  testUncompressedFont();
  resetMemory();
  std::puts("Font decompressor tests passed (pixels, checked OOM/retries, capacity reuse, prewarm, cleanup).");
}
