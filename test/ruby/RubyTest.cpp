#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "Epub/Ruby.h"
#include "Epub/RubyParser.h"
#include "RubyPlacement.h"
#include "RubyRaster.h"

static void sourceAssociations() {
  RubyParser parser;
  std::vector<RubySpan> spans;
  // <ruby>漢<rt>かん</rt>字<rt>じ</rt></ruby>
  parser.begin(3);
  parser.beginReading(4);
  assert(parser.append("\xe3\x81\x8b\xe3\x82\x93", 6));
  parser.endReading(spans, 4);
  parser.beginReading(5);
  assert(parser.append("\xe3\x81\x98", 3));
  parser.endReading(spans, 5);
  assert(spans.size() == 2);
  assert(spans[0].start == 3 && spans[0].count == 1 && spans[0].text.size() == 6);
  assert(spans[1].start == 4 && spans[1].count == 1 && spans[1].text.size() == 3);

  // <ruby>今日<rt>きょう</rt></ruby> -- never split into invented readings.
  parser.begin(5);
  parser.beginReading(7);
  assert(parser.append(" \n\xe3\x81", 4));  // XML chunk ends inside UTF-8
  assert(parser.append("\x8d\xe3\x82\x87\xe3\x81\x86\t ", 9));
  parser.endReading(spans, 7);
  assert(spans.size() == 3);
  assert(spans.back().start == 5 && spans.back().count == 2);
  assert(spans.back().text == "\xe3\x81\x8d\xe3\x82\x87\xe3\x81\x86");
}

static void explicitBases() {
  RubyParser parser;
  std::vector<RubySpan> spans;
  parser.begin(10);
  parser.beginBase(10);
  parser.endBase(11);
  parser.beginBase(11);
  parser.endBase(13);
  // All bases precede all readings, as in <rbc> / <rtc>.
  parser.beginReading(13);
  assert(parser.append("first", 5));
  parser.endReading(spans, 13);
  parser.beginReading(13);
  assert(parser.append("second", 6));
  parser.endReading(spans, 13);
  assert(spans.size() == 2);
  assert(spans[0].start == 10 && spans[0].count == 1);
  assert(spans[1].start == 11 && spans[1].count == 2);

  spans.clear();
  parser.begin(0);
  for (size_t i = 0; i < 3; ++i) {
    parser.beginBase(i);
    parser.endBase(i + 1);
  }
  parser.beginReading(3, 2);  // rbspan="2"
  assert(parser.append("group", 5));
  parser.endReading(spans, 3);
  parser.beginReading(3);
  assert(parser.append("mono", 4));
  parser.endReading(spans, 3);
  assert(spans.size() == 2 && spans[0].count == 2 && spans[1].start == 2 && spans[1].count == 1);
}

static void sparseOwnershipAndFlush() {
  std::vector<RubySpan> spans;
  auto empty = Ruby::takeRange(spans, 0, 400);
  assert(empty.empty() && empty.capacity() == 0 && spans.capacity() == 0);

  Ruby::add(spans, 390, 3, std::string(100, 'a'));
  Ruby::add(spans, 398, 2, "group");
  const char* owned = spans[0].text.data();
  auto first = Ruby::takeRange(spans, 380, 398);
  assert(first.size() == 1 && first[0].start == 10 && first[0].count == 3);
  assert(first[0].text.data() == owned);  // transfer, not copy, of a non-SSO reading
  Ruby::consume(spans, 398);
  assert(spans.size() == 1 && spans[0].start == 0 && spans[0].count == 2);
  auto last = Ruby::takeRange(spans, 0, 22);  // trailing words without ruby
  assert(last.size() == 1 && last[0].text == "group");
  Ruby::consume(spans, 22);
  assert(spans.empty());

  // Oversized base: whole reading belongs to the first wrapped fragment only.
  Ruby::add(spans, 0, 50, "whole-reading");
  first = Ruby::takeRange(spans, 0, 20);
  assert(first.size() == 1 && first[0].count == 20 && first[0].text == "whole-reading");
  Ruby::consume(spans, 20);
  assert(Ruby::takeRange(spans, 0, 20).empty());
}

static void groupWrapping() {
  std::vector<RubySpan> spans;
  Ruby::add(spans, 3, 4, "reading");
  const auto measure = [](size_t start, size_t end) { return static_cast<int>(end - start) * 20; };
  assert(Ruby::adjustBreak(spans, 5, 0, 100, measure) == 3);  // move whole group
  assert(Ruby::adjustBreak(spans, 5, 3, 100, measure) == 7);  // restore intact group after kinsoku
  assert(Ruby::adjustBreak(spans, 5, 3, 40, measure) == 5);   // emergency wrap, guaranteed progress
  assert(Ruby::adjustBreak(spans, 7, 0, 200, measure) == 7);
  assert(Ruby::containsWord(spans, 3) && Ruby::containsWord(spans, 6));
  assert(!Ruby::containsWord(spans, 2) && !Ruby::containsWord(spans, 7));
}

static void boundedReadings() {
  RubyParser parser;
  std::vector<RubySpan> spans;
  parser.begin(0);
  parser.beginReading(1);
  const std::string longReading(RubyParser::MAX_READING_BYTES, 'x');
  assert(parser.append(longReading.data(), longReading.size()));
  assert(!parser.append("x", 1));
  parser.endReading(spans, 1);
  assert(spans.size() == 1 && spans[0].text == longReading);
  parser.begin(1);
  parser.beginReading(2);
  assert(parser.append(" \n\t", 3));
  parser.endReading(spans, 2);
  assert(spans.size() == 1);  // empty annotation creates no span
}

static void placement() {
  auto p = placeRuby(40, 80, 20, 0, 200);
  assert(p.start == 50 && p.length == 20);  // normal centered group
  p = placeRuby(20, 40, 60, 20, 200);
  assert(p.start == 20 && p.length == 60);  // actual indented line start
  p = placeRuby(180, 200, 60, 20, 200);
  assert(p.start == 140 && p.length == 60);
  p = placeRuby(20, 40, 200, 20, 40);
  assert(p.start == 20 && p.length == 20);  // single-character short line
  p = placeRuby(0, 20, 60, 0, 21);
  const auto adjacent = placeRuby(22, 42, 60, 21, 42);
  assert(p.start + p.length <= adjacent.start);
  assert(placeRuby(0, 20, 100, 20, 10).length == 0);

  // The same axis-only geometry is used for horizontal and vertical text.
  for (int start = 0; start < 40; ++start) {
    for (int available = 1; available < 100; ++available) {
      for (int natural = 1; natural < 200; ++natural) {
        p = placeRuby(start, start + available / 2, natural, start, start + available);
        assert(p.start >= start && p.start + p.length <= start + available);
        assert(p.length > 0 && p.length <= natural);
      }
    }
  }
}

static void rasterization() {
  const uint8_t solid[] = {0xff, 0xff, 0xff, 0xff};
  const RubyBitmapView oneBit{solid, 4, 4, false};
  const RubyBitmapView twoBit{solid, 4, 4, true};
  for (const bool vertical : {false, true}) {
    for (const auto& bitmap : {oneBit, twoBit}) {
      for (int size = 1; size <= 8; ++size) {
        int pixels = 0;
        drawRubyBitmap(bitmap, 10, 10, size, size, vertical, 10, 10 + size, [&](int x, int y, uint8_t opacity) {
          assert(x >= 10 && x < 10 + size && y >= 10 && y < 10 + size && opacity == 3);
          ++pixels;
        });
        assert(pixels == size * size);
      }
      int pixels = 0;
      drawRubyBitmap(bitmap, 9, 9, 4, 4, vertical, 10, 12, [&](int x, int y, uint8_t) {
        const int axis = vertical ? y : x;
        assert(axis >= 10 && axis < 12);
        ++pixels;
      });
      assert(pixels == 8);  // hard guarantee even for a font's unexpected overhang
    }
  }
  // A one-pixel stroke must contribute to area averaging, not disappear because
  // nearest-neighbor happened to sample an adjacent white pixel.
  const uint8_t stroke[] = {0x0c};  // 2-bit pixels: white, white, black, white
  uint8_t opacity = 0;
  drawRubyBitmap({stroke, 4, 1, true}, 0, 0, 1, 1, false, 0, 1, [&](int, int, uint8_t ink) { opacity = ink; });
  assert(opacity == 1);
  // Row-padded external font, trimming the first three columns.
  const uint8_t padded[] = {0x1f, 0x80, 0x1f, 0x80};
  int pixels = 0;
  drawRubyBitmap({padded, 6, 2, false, 2, 3}, 0, 0, 6, 2, false, 0, 6, [&](int, int, uint8_t ink) {
    assert(ink == 3);
    ++pixels;
  });
  assert(pixels == 12);
}

int main() {
  sourceAssociations();
  explicitBases();
  sparseOwnershipAndFlush();
  groupWrapping();
  boundedReadings();
  placement();
  rasterization();
  std::puts("Ruby tests passed (source associations, sparse ownership, wrapping, placement, rasterization).");
}
