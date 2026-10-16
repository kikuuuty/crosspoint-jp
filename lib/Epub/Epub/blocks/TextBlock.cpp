#include "TextBlock.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <Serialization.h>
#include <Utf8.h>
#include <VerticalTextUtils.h>

#include <algorithm>
#include <cstdlib>

#include "../InlineImage.h"
#include "ImageBlock.h"

int TextBlock::rubyFontId = 0;
int TextBlock::smallFontId = 0;

void TextBlock::collectCodepoints(std::vector<uint32_t>& out, size_t max) const {
  if (max == 0 || out.size() >= max) {
    return;
  }

  for (const auto& word : words) {
    if (InlineImage::isInlineImage(word)) continue;  // 画像語に対応するグリフは無い
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
    uint32_t cp;
    while ((cp = utf8NextCodepoint(&ptr))) {
      // Check if already exists (simple linear search, OK for small sets)
      bool exists = false;
      for (uint32_t existing : out) {
        if (existing == cp) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        out.push_back(cp);
        if (out.size() >= max) {
          return;
        }
      }
    }
  }
}

bool TextBlock::hasRuby() const { return !rubySpans.empty(); }

int TextBlock::wordExtent(const GfxRenderer& renderer, const int fontId, const size_t index,
                          const int columnWidth) const {
  const auto style = wordStyles[index];
  const int wordFontId = ((style & EpdFontFamily::SCRIPT_MASK) != 0 && smallFontId != 0) ? smallFontId : fontId;
  if (isVertical) {
    if (InlineImage::isInlineImage(words[index])) {
      // Geometry needs only the height, not an allocated copy of the image path.
      const size_t comma = words[index].find(',');
      if (comma != std::string::npos) {
        char* end = nullptr;
        const long height = std::strtol(words[index].c_str() + comma + 1, &end, 10);
        if (end && *end == ',' && height > 0 && height <= INT16_MAX) return static_cast<int>(height);
      }
    }
    const auto& word = words[index];
    if (word.size() <= 2 && !word.empty() && word.find_first_not_of("0123456789") == std::string::npos) {
      return columnWidth;  // tate-chu-yoko is one full-height cell
    }
  }
  return renderer.getTextAdvanceX(wordFontId, words[index].c_str(), style);
}

void TextBlock::renderRuby(GfxRenderer& renderer, const int fontId, const int x, const int y, const int columnWidth,
                           const int viewportWidth, const int viewportX) const {
  if (rubyFontId == 0 || rubySpans.empty() || words.empty()) return;
  if (renderer.getFontCacheManager() && renderer.getFontCacheManager()->isScanning()) {
    for (const auto& span : rubySpans) {
      renderer.drawRubyText(rubyFontId, 0, 0, span.text.c_str(), isVertical, 0, 0, 0, 0);
    }
    return;
  }
  const auto& positions = isVertical ? wordYpos : wordXpos;
  if (positions.size() != words.size()) return;
  const int origin = isVertical ? y : x;
  // Use the actual text extent, NOT the viewport, and exclude trailing vertical
  // character spacing. Indents and short final lines retain their own bounds.
  int lineStart = origin + positions.front();
  int lineEnd = origin + positions.back() + wordExtent(renderer, fontId, words.size() - 1, columnWidth);
  if (isVertical) {
    lineStart = std::max(lineStart, 0);
    lineEnd = std::min(lineEnd, renderer.getScreenHeight());
  } else if (viewportWidth > 0) {
    lineStart = std::max(lineStart, viewportX);
    lineEnd = std::min(lineEnd, viewportX + viewportWidth);
  }
  for (size_t i = 0; i < rubySpans.size(); ++i) {
    const auto& span = rubySpans[i];
    if (span.count == 0 || span.end() > words.size() || span.text.empty()) continue;
    const size_t last = span.end() - 1;
    const int baseStart = origin + positions[span.start];
    const int baseEnd = origin + positions[last] + wordExtent(renderer, fontId, last, columnWidth);
    int limitStart = lineStart;
    int limitEnd = lineEnd;
    if (i > 0) {
      const size_t prevLast = rubySpans[i - 1].end() - 1;
      const int prevEnd = origin + positions[prevLast] + wordExtent(renderer, fontId, prevLast, columnWidth);
      limitStart = std::max(limitStart, prevEnd + (baseStart - prevEnd) / 2);
    }
    if (i + 1 < rubySpans.size()) {
      const int nextStart = origin + positions[rubySpans[i + 1].start];
      limitEnd = std::min(limitEnd, baseEnd + (nextStart - baseEnd) / 2);
    }
    const int rubyX = isVertical ? x + wordXpos[span.start] + columnWidth + 2 : 0;
    const int rubyY = isVertical ? 0 : y - renderer.getLineHeight(rubyFontId) - 1;
    renderer.drawRubyText(rubyFontId, rubyX, rubyY, span.text.c_str(), isVertical, baseStart, baseEnd, limitStart,
                          limitEnd);
  }
}

void TextBlock::render(GfxRenderer& renderer, const int fontId, const int x, const int y, const int viewportWidth,
                       const int viewportX) const {
  // Validate iterator bounds before rendering
  if (words.size() != wordXpos.size() || words.size() != wordStyles.size()) {
    LOG_ERR("TXB", "Render skipped: size mismatch (words=%u, xpos=%u, styles=%u)\n", (uint32_t)words.size(),
            (uint32_t)wordXpos.size(), (uint32_t)wordStyles.size());
    return;
  }

  const int effectiveFontId = (blockStyle.fontId != 0) ? blockStyle.fontId : fontId;

  // Ruby is recorded directly by drawRubyText during the normal prewarm scan.
  // No concatenated temporary string or per-render advance table is needed.

  // <sup> / <sub> 用の小さいフォントのグリフをプリロード（SDカードフォントの場合）。
  // 本文フォントとは別のフォントIDなので、本文ぶんのプリロードでは用意されない。
  if (smallFontId != 0 && smallFontId != effectiveFontId && renderer.isSdCardFont(smallFontId)) {
    std::string scriptText;
    for (size_t i = 0; i < words.size(); i++) {
      if ((wordStyles[i] & EpdFontFamily::SCRIPT_MASK) != 0) scriptText += words[i];
    }
    if (!scriptText.empty()) {
      renderer.ensureSdCardFontReady(smallFontId, scriptText.c_str(), 1u << EpdFontFamily::REGULAR);
    }
  }

  // Compute column width once for Sideways/TateChuYoko centering
  int columnWidth = 0;
  if (isVertical) {
    // Use advance of CJK reference character "一" (U+4E00) as column width
    columnWidth = renderer.getTextAdvanceX(effectiveFontId, "\xe4\xb8\x80", EpdFontFamily::REGULAR);
    if (columnWidth <= 0) columnWidth = renderer.getLineHeight(effectiveFontId);
  }

  for (size_t i = 0; i < words.size(); i++) {
    const EpdFontFamily::Style currentStyle = wordStyles[i];

    // <sup> / <sub>: 小さいフォントに切り替え、行の上端／下端に寄せる。
    // drawText の y は文字セルの上端なので、上付きはそのまま（小さい字が上に付く）、
    // 下付きは本文と小フォントの行高の差だけ下げれば、余計な定数無しで収まる。
    // smallFontId が 0（小さいフォントを用意できない）ときは本文フォントのまま描く。
    const bool isScript = (currentStyle & EpdFontFamily::SCRIPT_MASK) != 0;
    const int wordFontId = (isScript && smallFontId != 0) ? smallFontId : effectiveFontId;
    int scriptYOffset = 0;
    if (isScript && wordFontId != effectiveFontId && (currentStyle & EpdFontFamily::SUBSCRIPT) != 0) {
      scriptYOffset = renderer.getLineHeight(effectiveFontId) - renderer.getLineHeight(wordFontId);
      if (scriptYOffset < 0) scriptYOffset = 0;
    }

    // インライン画像（本文中に 1 文字ぶんの大きさで置かれた外字など）。
    // パーサは縦書きのときだけこの語を作る（ChapterHtmlSlimParser の <img> 処理）。
    if (InlineImage::isInlineImage(words[i])) {
      std::string imagePath;
      int imageWidth = 0;
      int imageHeight = 0;
      if (isVertical && i < wordYpos.size() && InlineImage::decode(words[i], imagePath, imageWidth, imageHeight)) {
        const int wx = x + wordXpos[i];
        const int wy = y + wordYpos[i];
        // 列の幅に対して中央に寄せる。パーサが幅を列幅以内に縮めているが、安全網として
        // 画面内に収める（ImageBlock::render は画面外だと描画自体を捨てる）
        int imageX = wx + (columnWidth - imageWidth) / 2;
        const int maxX = renderer.getScreenWidth() - imageWidth;
        if (imageX > maxX) imageX = maxX;
        if (imageX < 0) imageX = 0;
        ImageBlock(imagePath, static_cast<int16_t>(imageWidth), static_cast<int16_t>(imageHeight))
            .render(renderer, imageX, wy);
      }
      continue;
    }

    if (isVertical && i < wordYpos.size()) {
      // 縦書きモード: VerticalBehaviorに応じて描画方法を分岐
      const char* w = words[i].c_str();
      const int wx = x + wordXpos[i];
      const int wy = y + wordYpos[i];

      // Classify: replicate the logic from ChapterHtmlSlimParser::flushPartWordBuffer
      const auto* p = reinterpret_cast<const unsigned char*>(w);
      uint32_t firstCp = utf8NextCodepoint(&p);
      bool isSingleCjk = (firstCp != 0 && *p == '\0' && VerticalTextUtils::isUprightInVertical(firstCp));

      if (isSingleCjk) {
        renderer.drawTextVertical(wordFontId, wx, wy, w, true, currentStyle);
      } else {
        bool allDigits = true;
        int asciiCount = 0;
        for (const char* c = w; *c; c++) {
          if ((static_cast<uint8_t>(*c) & 0xC0) != 0x80) asciiCount++;
          if (*c < '0' || *c > '9') allDigits = false;
        }
        if (allDigits && asciiCount <= 2) {
          // TateChuYoko: draw horizontally, centered in the column
          const int textW = renderer.getTextAdvanceX(wordFontId, w, currentStyle);
          const int centerOffset = (columnWidth - textW) / 2;
          renderer.drawText(wordFontId, wx + centerOffset, wy, w, true, currentStyle);
        } else {
          // Sideways: draw rotated 90° CW, centered in the column.
          // Gap asymmetry: CJK rendering adds ascender offset (~11px from cell top
          // via drawText), while Sideways uses glyph->left (~1px). This creates
          // 0px gap before and ~12px gap after. Shift down by ascender/6 ≈ 6px
          // to equalize (derived from tracing actual pixel positions).
          const int vertShift = renderer.getFontAscenderSize(wordFontId) / 3;
          renderer.drawTextSideways(wordFontId, wx, wy + vertShift, w, true, currentStyle, columnWidth);
        }
      }
    } else {
      const int wordX = wordXpos[i] + x;
      renderer.drawText(wordFontId, wordX, y + scriptYOffset, words[i].c_str(), true, currentStyle);

      if ((currentStyle & EpdFontFamily::UNDERLINE) != 0) {
        const std::string& w = words[i];
        const int fullWordWidth = renderer.getTextWidth(wordFontId, w.c_str(), currentStyle);
        // y is the top of the text line; add ascender to reach baseline, then offset 2px below
        const int underlineY = y + scriptYOffset + renderer.getFontAscenderSize(wordFontId) + 2;

        int startX = wordX;
        int underlineWidth = fullWordWidth;

        // if word starts with em-space ("\xe2\x80\x83"), account for the additional indent before drawing the line
        if (w.size() >= 3 && static_cast<uint8_t>(w[0]) == 0xE2 && static_cast<uint8_t>(w[1]) == 0x80 &&
            static_cast<uint8_t>(w[2]) == 0x83) {
          const char* visiblePtr = w.c_str() + 3;
          const int prefixWidth = renderer.getTextAdvanceX(wordFontId, "\xe2\x80\x83", currentStyle);
          const int visibleWidth = renderer.getTextWidth(wordFontId, visiblePtr, currentStyle);
          startX = wordX + prefixWidth;
          underlineWidth = visibleWidth;
        }

        renderer.drawLine(startX, underlineY, startX + underlineWidth, underlineY, true);
      }
    }
  }

  renderRuby(renderer, effectiveFontId, x, y, columnWidth, viewportWidth, viewportX);

  // Draw full-width separator line below the block (used for h1/h2 headings).
  // Suppressed in vertical mode: horizontal lines are inappropriate for tategaki.
  // コードブロックの枠線。行ごとに左右の縦線を引き、先頭行と最終行で上辺・下辺を閉じる。
  // 縦書きでは横組み用の枠になってしまうので描かない（drawSeparatorBelow と同じ扱い）。
  if (blockStyle.frameEdges != 0 && viewportWidth > 0 && !isVertical) {
    const int frameTop = y;
    const int frameHeight =
        blockStyle.frameHeight > 0 ? blockStyle.frameHeight : renderer.getLineHeight(effectiveFontId);
    const int frameBottom = frameTop + frameHeight;
    const int left = viewportX;
    const int right = viewportX + viewportWidth - 1;
    if ((blockStyle.frameEdges & BlockStyle::FRAME_SIDES) != 0) {
      renderer.drawLine(left, frameTop, left, frameBottom, true);
      renderer.drawLine(right, frameTop, right, frameBottom, true);
    }
    if ((blockStyle.frameEdges & BlockStyle::FRAME_TOP) != 0) {
      renderer.drawLine(left, frameTop, right, frameTop, true);
    }
    if ((blockStyle.frameEdges & BlockStyle::FRAME_BOTTOM) != 0) {
      renderer.drawLine(left, frameBottom, right, frameBottom, true);
    }
  }

  if (blockStyle.drawSeparatorBelow && viewportWidth > 0 && !isVertical) {
    const int separatorY = y + renderer.getLineHeight(effectiveFontId) + 2;
    // ビューポート（余白を除いた描画領域）の幅いっぱいに引く。以前は画面左端の 0 から
    // 引いていたので、左が余白にはみ出し右が余白の手前で終わる非対称な線になっていた。
    renderer.drawLine(viewportX, separatorY, viewportX + viewportWidth, separatorY, true);
  }
}

bool TextBlock::serialize(FsFile& file) const {
  if (words.size() != wordXpos.size() || words.size() != wordStyles.size()) {
    LOG_ERR("TXB", "Serialization failed: size mismatch (words=%u, xpos=%u, styles=%u)\n", words.size(),
            wordXpos.size(), wordStyles.size());
    return false;
  }

  // Word data
  serialization::writePod(file, static_cast<uint16_t>(words.size()));
  for (const auto& w : words) serialization::writeString(file, w);
  for (auto x : wordXpos) serialization::writePod(file, x);
  for (auto s : wordStyles) serialization::writePod(file, s);

  // Style (alignment + margins/padding/indent)
  serialization::writePod(file, blockStyle.alignment);
  serialization::writePod(file, blockStyle.textAlignDefined);
  serialization::writePod(file, blockStyle.marginTop);
  serialization::writePod(file, blockStyle.marginBottom);
  serialization::writePod(file, blockStyle.marginLeft);
  serialization::writePod(file, blockStyle.marginRight);
  serialization::writePod(file, blockStyle.paddingTop);
  serialization::writePod(file, blockStyle.paddingBottom);
  serialization::writePod(file, blockStyle.paddingLeft);
  serialization::writePod(file, blockStyle.paddingRight);
  serialization::writePod(file, blockStyle.textIndent);
  serialization::writePod(file, blockStyle.textIndentDefined);
  serialization::writePod(file, blockStyle.fontId);
  serialization::writePod(file, blockStyle.drawSeparatorBelow);
  serialization::writePod(file, blockStyle.frameEdges);
  serialization::writePod(file, blockStyle.frameHeight);
  serialization::writePod(file, blockStyle.isListItem);
  serialization::writePod(file, blockStyle.isHeading);

  // Vertical layout data
  serialization::writePod(file, isVertical);
  if (isVertical) {
    for (auto y : wordYpos) serialization::writePod(file, y);
  }

  // Sparse ruby associations. Write the owned string by reference (a ternary
  // with std::string() would copy the non-empty branch into a temporary).
  serialization::writePod(file, static_cast<uint16_t>(rubySpans.size()));
  for (const auto& span : rubySpans) {
    serialization::writePod(file, span.start);
    serialization::writePod(file, span.count);
    serialization::writeString(file, span.text);
  }

  return true;
}

std::unique_ptr<TextBlock> TextBlock::deserialize(FsFile& file) {
  uint16_t wc;
  std::vector<std::string> words;
  std::vector<int16_t> wordXpos;
  std::vector<EpdFontFamily::Style> wordStyles;
  BlockStyle blockStyle;

  // Word count
  serialization::readPod(file, wc);

  // Sanity check: prevent allocation of unreasonably large vectors (max 10000 words per block)
  if (wc > 10000) {
    LOG_ERR("TXB", "Deserialization failed: word count %u exceeds maximum", wc);
    return nullptr;
  }

  // Word data
  words.resize(wc);
  wordXpos.resize(wc);
  wordStyles.resize(wc);
  for (auto& w : words) serialization::readString(file, w);
  for (auto& x : wordXpos) serialization::readPod(file, x);
  for (auto& s : wordStyles) serialization::readPod(file, s);

  // Style (alignment + margins/padding/indent)
  serialization::readPod(file, blockStyle.alignment);
  serialization::readPod(file, blockStyle.textAlignDefined);
  serialization::readPod(file, blockStyle.marginTop);
  serialization::readPod(file, blockStyle.marginBottom);
  serialization::readPod(file, blockStyle.marginLeft);
  serialization::readPod(file, blockStyle.marginRight);
  serialization::readPod(file, blockStyle.paddingTop);
  serialization::readPod(file, blockStyle.paddingBottom);
  serialization::readPod(file, blockStyle.paddingLeft);
  serialization::readPod(file, blockStyle.paddingRight);
  serialization::readPod(file, blockStyle.textIndent);
  serialization::readPod(file, blockStyle.textIndentDefined);
  serialization::readPod(file, blockStyle.fontId);
  serialization::readPod(file, blockStyle.drawSeparatorBelow);
  serialization::readPod(file, blockStyle.frameEdges);
  serialization::readPod(file, blockStyle.frameHeight);
  serialization::readPod(file, blockStyle.isListItem);
  serialization::readPod(file, blockStyle.isHeading);

  // Vertical layout data
  bool vertical = false;
  serialization::readPod(file, vertical);
  std::vector<int16_t> wordYpos;
  if (vertical) {
    wordYpos.resize(wc);
    for (auto& y : wordYpos) serialization::readPod(file, y);
  }

  uint16_t rubyCount = 0;
  serialization::readPod(file, rubyCount);
  if (rubyCount > wc) return nullptr;
  std::vector<RubySpan> rubySpans;
  // A variable number of annotations must be owned by the deserialized page;
  // allocate exactly that many, and nothing for a page without ruby.
  rubySpans.reserve(rubyCount);
  for (uint16_t i = 0; i < rubyCount; ++i) {
    RubySpan span;
    serialization::readPod(file, span.start);
    serialization::readPod(file, span.count);
    uint32_t bytes = 0;
    serialization::readPod(file, bytes);
    if (span.count == 0 || span.end() > wc || (!rubySpans.empty() && span.start < rubySpans.back().end()) ||
        bytes == 0 || bytes > Ruby::MAX_READING_BYTES || bytes > file.available()) {
      LOG_ERR("TXB", "Invalid ruby association in cache");
      return nullptr;
    }
    span.text.resize(bytes);
    if (file.read(span.text.data(), bytes) != bytes) return nullptr;
    rubySpans.push_back(std::move(span));
  }

  return std::unique_ptr<TextBlock>(new TextBlock(std::move(words), std::move(wordXpos), std::move(wordStyles),
                                                  blockStyle, std::move(wordYpos), vertical, std::move(rubySpans)));
}
