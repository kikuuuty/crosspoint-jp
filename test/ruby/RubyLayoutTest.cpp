#include <GfxRenderer.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include "Epub/ParsedText.h"
#include "Epub/blocks/ImageBlock.h"
#include "Epub/hyphenation/Hyphenator.h"

// Non-ruby dependencies are deliberately inert; ParsedText and TextBlock below
// are the actual production translation units, not copies of their algorithms.
ImageBlock::ImageBlock(const std::string& path, int16_t w, int16_t h) : imagePath(path), width(w), height(h) {}
void ImageBlock::render(GfxRenderer&, int, int) {}
std::vector<Hyphenator::BreakInfo> Hyphenator::breakOffsets(const std::string& word, bool fallback) {
  std::vector<BreakInfo> out;
  if (fallback) {
    out.reserve(word.size());
    for (size_t i = 1; i < word.size(); ++i) {
      if ((static_cast<uint8_t>(word[i]) & 0xC0) != 0x80) out.push_back({i, false});
    }
  }
  return out;
}

static BlockStyle style() {
  BlockStyle s;
  s.alignment = CssTextAlign::Left;
  s.textIndentDefined = true;
  return s;
}

static void addCjk(ParsedText& parsed, const size_t count, const bool vertical) {
  for (size_t i = 0; i < count; ++i) {
    if (vertical) {
      parsed.addWord("\xe5\xad\x97", EpdFontFamily::REGULAR, VerticalTextUtils::VerticalBehavior::Upright);
    } else {
      parsed.addWord("\xe5\xad\x97", EpdFontFamily::REGULAR);
    }
  }
}

static std::vector<std::shared_ptr<TextBlock>> extract(ParsedText& parsed, GfxRenderer& renderer, const bool vertical,
                                                       const int extent, const bool last = true) {
  std::vector<std::shared_ptr<TextBlock>> lines;
  lines.reserve(100);
  const auto emit = [&](std::shared_ptr<TextBlock> block) { lines.push_back(std::move(block)); };
  if (vertical)
    parsed.layoutVerticalColumns(renderer, 1, extent, emit, last);
  else
    parsed.layoutAndExtractLines(renderer, 1, extent, emit, last);
  return lines;
}

static void wrappingAndCache(const bool vertical, const bool hyphenation) {
  GfxRenderer renderer;
  ParsedText parsed(hyphenation, style());
  addCjk(parsed, 12, vertical);
  Ruby::add(parsed.getRubySpans(), 3, 4, "whole-group");
  Ruby::add(parsed.getRubySpans(), 10, 1, "mono");
  auto lines = extract(parsed, renderer, vertical, vertical ? 110 : 100);
  assert(parsed.isEmpty());
  assert(lines.size() == 3);
  assert(lines[0]->wordCount() == 3 && !lines[0]->hasRuby());
  assert(lines[1]->wordCount() == 5 && lines[1]->getRubySpans().size() == 1);
  assert(lines[1]->getRubySpans()[0].start == 0 && lines[1]->getRubySpans()[0].count == 4);
  assert(lines[2]->getRubySpans()[0].start == 2 && lines[2]->getRubySpans()[0].text == "mono");

  for (const auto& line : lines) {
    FsFile file;
    assert(line->serialize(file));
    file.cursor = 0;
    auto restored = TextBlock::deserialize(file);
    assert(restored && file.available() == 0);
    assert(restored->getWords() == line->getWords());
    assert(restored->getIsVertical() == vertical);
    const auto& spans = restored->getRubySpans();
    assert(spans.size() == line->getRubySpans().size());
    if (spans.empty()) assert(spans.capacity() == 0);
    for (size_t i = 0; i < spans.size(); ++i) {
      assert(spans[i].start == line->getRubySpans()[i].start);
      assert(spans[i].count == line->getRubySpans()[i].count);
      assert(spans[i].text == line->getRubySpans()[i].text);
    }
  }
}

static void intermediateFlush(const bool vertical) {
  GfxRenderer renderer;
  ParsedText partial(false, style());
  addCjk(partial, 4, vertical);
  assert(extract(partial, renderer, vertical, vertical ? 110 : 100, false).empty());
  assert(partial.size() == 4);  // one partial line must never be emitted early

  ParsedText parsed(false, style());
  addCjk(parsed, 400, vertical);
  Ruby::add(parsed.getRubySpans(), 398, 2, "last-pair");
  auto lines = extract(parsed, renderer, vertical, vertical ? 110 : 100, false);
  assert(lines.size() == 79 && parsed.size() == 5);
  assert(parsed.getRubySpans().size() == 1 && parsed.getRubySpans()[0].start == 3);
  addCjk(parsed, 20, vertical);
  lines = extract(parsed, renderer, vertical, vertical ? 110 : 100);
  assert(lines.size() == 5 && parsed.isEmpty());
  assert(lines[0]->getRubySpans().size() == 1 && lines[0]->getRubySpans()[0].text == "last-pair");
}

static void actualLineBounds(const bool vertical) {
  GfxRenderer renderer;
  ParsedText parsed(false, style());
  addCjk(parsed, 4, vertical);
  Ruby::add(parsed.getRubySpans(), 0, 1, std::string(30, 'a'));
  Ruby::add(parsed.getRubySpans(), 3, 1, std::string(30, 'b'));
  auto lines = extract(parsed, renderer, vertical, 500);
  lines[0]->render(renderer, 1, 10, 40, 400, 10);
  assert(renderer.calls.size() == 2);
  const int start = vertical ? 40 : 10;
  const int end = start + (vertical ? 86 : 80);  // no final 2px vertical spacing
  assert(renderer.calls[0].limitStart == start);
  assert(renderer.calls[1].limitEnd == end);
  assert(renderer.calls[0].placed.start >= start);
  assert(renderer.calls[1].placed.start + renderer.calls[1].placed.length <= end);
  assert(renderer.calls[0].placed.start + renderer.calls[0].placed.length <= renderer.calls[1].placed.start);
}

static void oversizedGroup(const bool vertical) {
  GfxRenderer renderer;
  ParsedText parsed(false, style());
  addCjk(parsed, 30, vertical);
  Ruby::add(parsed.getRubySpans(), 0, 30, std::string(100, 'r'));
  auto lines = extract(parsed, renderer, vertical, vertical ? 110 : 100);
  assert(lines.size() == 6);
  assert(lines[0]->getRubySpans().size() == 1 && lines[0]->getRubySpans()[0].text.size() == 100);
  for (size_t i = 1; i < lines.size(); ++i) assert(!lines[i]->hasRuby());
}

static void invalidCache() {
  GfxRenderer renderer;
  ParsedText parsed(false, style());
  addCjk(parsed, 1, false);
  Ruby::add(parsed.getRubySpans(), 0, 1, "reading");
  auto lines = extract(parsed, renderer, false, 100);
  FsFile file;
  assert(lines[0]->serialize(file));
  const uint32_t huge = 4097;
  std::memcpy(file.bytes.data() + file.bytes.size() - 7 - sizeof(huge), &huge, sizeof(huge));
  file.cursor = 0;
  assert(!TextBlock::deserialize(file));
}

static void latinEmergencyWrapping() {
  for (const bool annotated : {false, true}) {
    for (const bool hyphenation : {false, true}) {
      GfxRenderer renderer;
      ParsedText parsed(hyphenation, style());
      parsed.addWord(std::string(20, 'a'), EpdFontFamily::REGULAR);
      addCjk(parsed, 2, false);
      Ruby::add(parsed.getRubySpans(), annotated ? 0 : 1, annotated ? 1 : 2, "intact-reading");
      auto lines = extract(parsed, renderer, false, 100);
      assert(lines.size() == 5);
      size_t annotations = 0;
      for (size_t i = 0; i < lines.size(); ++i) {
        if (!lines[i]->hasRuby()) continue;
        ++annotations;
        assert(i == (annotated ? 0 : 4));
        assert(lines[i]->getRubySpans()[0].start == 0);
        assert(lines[i]->getRubySpans()[0].text == "intact-reading");
      }
      assert(annotations == 1);
    }
  }
}

static void groupAndKinsoku(const bool vertical) {
  GfxRenderer renderer;
  ParsedText parsed(false, style());
  addCjk(parsed, 3, vertical);
  if (vertical) {
    parsed.addWord("\xe3\x80\x8c", EpdFontFamily::REGULAR, VerticalTextUtils::VerticalBehavior::Upright);
  } else {
    parsed.addWord("\xe3\x80\x8c", EpdFontFamily::REGULAR);
  }
  addCjk(parsed, 6, vertical);
  Ruby::add(parsed.getRubySpans(), 4, 3, "group-after-opening-bracket");
  auto lines = extract(parsed, renderer, vertical, vertical ? 110 : 100);
  assert(lines.front()->wordCount() == 3);  // do not leave the opening bracket behind
  assert(lines[1]->getWords()[0] == "\xe3\x80\x8c");
  assert(lines[1]->getRubySpans()[0].start == 1 && lines[1]->getRubySpans()[0].count == 3);
}

static void randomizedWrapping(const bool vertical) {
  uint32_t seed = 12345;
  const auto random = [&]() {
    seed = seed * 1664525 + 1013904223;
    return seed;
  };
  GfxRenderer renderer;
  for (int trial = 0; trial < 500; ++trial) {
    ParsedText parsed((trial & 1) != 0, style());
    const size_t words = 20 + random() % 380;
    addCjk(parsed, words, vertical);
    for (size_t start = random() % 4; start < words;) {
      const size_t count = std::min(size_t{1} + random() % 10, words - start);
      Ruby::add(parsed.getRubySpans(), start, count, "reading-" + std::to_string(start));
      start += count + random() % 4;
    }
    const auto source = parsed.getRubySpans();
    auto lines = extract(parsed, renderer, vertical, vertical ? 110 : 100);
    size_t consumed = 0;
    size_t annotation = 0;
    for (const auto& line : lines) {
      assert(line->wordCount() > 0 && line->wordCount() <= 5);
      for (const auto& span : line->getRubySpans()) {
        assert(annotation < source.size());
        assert(consumed + span.start == source[annotation].start);
        assert(span.text == source[annotation].text);
        assert(span.end() <= line->wordCount());
        if (source[annotation].count <= 5) assert(span.count == source[annotation].count);
        ++annotation;
      }
      consumed += line->wordCount();
    }
    assert(consumed == words && annotation == source.size());
  }
}

static void paragraphIndentAcrossFlush(const bool vertical) {
  for (const bool indent : {false, true}) {
    GfxRenderer renderer;
    BlockStyle s;
    s.alignment = CssTextAlign::Left;
    ParsedText whole(false, s, indent);
    ParsedText streamed(false, s, indent);
    addCjk(whole, 405, vertical);
    addCjk(streamed, 400, vertical);
    Ruby::add(whole.getRubySpans(), 398, 2, "boundary");
    Ruby::add(streamed.getRubySpans(), 398, 2, "boundary");
    auto expected = extract(whole, renderer, vertical, vertical ? 110 : 100);
    auto actual = extract(streamed, renderer, vertical, vertical ? 110 : 100, false);
    addCjk(streamed, 5, vertical);
    auto tail = extract(streamed, renderer, vertical, vertical ? 110 : 100);
    actual.insert(actual.end(), tail.begin(), tail.end());
    assert(actual.size() == expected.size());
    for (size_t i = 0; i < actual.size(); ++i) {
      FsFile a, b;
      assert(actual[i]->serialize(a) && expected[i]->serialize(b));
      assert(a.bytes == b.bytes);  // includes coordinates, styles, and ruby ranges
    }
  }
}

int main() {
  TextBlock::rubyFontId = 2;
  for (const bool vertical : {false, true}) {
    wrappingAndCache(vertical, false);
    wrappingAndCache(vertical, true);
    intermediateFlush(vertical);
    actualLineBounds(vertical);
    oversizedGroup(vertical);
    groupAndKinsoku(vertical);
    randomizedWrapping(vertical);
    paragraphIndentAcrossFlush(vertical);
  }
  invalidCache();
  latinEmergencyWrapping();
  std::puts("Ruby layout/cache tests passed (production ParsedText and TextBlock).");
}
