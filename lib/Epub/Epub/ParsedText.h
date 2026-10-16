#pragma once

#include <EpdFontFamily.h>
#include <VerticalTextUtils.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "blocks/BlockStyle.h"
#include "blocks/TextBlock.h"

class GfxRenderer;

class ParsedText {
  std::vector<std::string> words;
  std::vector<EpdFontFamily::Style> wordStyles;
  std::vector<bool> wordContinues;  // true = word attaches to previous (no space before it)
  // 原文でこの語の直前に空白があったか。CJK は 1 文字ずつを語に割っているため
  // wordContinues は CJK 語では常に false で、「空白があったか」を区別できない。
  // これが無いと CJK どうしの字間をゼロにする規則が、原文の空白まで飲んでしまう。
  std::vector<bool> wordSpaceBefore;
  std::vector<RubySpan> rubySpans;  // sparse source-level base/reading associations
  std::vector<VerticalTextUtils::VerticalBehavior> wordVerticalBehaviors;
  BlockStyle blockStyle;
  bool firstLineIndent;
  bool hyphenationEnabled;
  // Ruby-safe intermediate flushes must not reapply paragraph-first-line
  // indentation to the retained partial line/column.
  bool hasEmittedLine = false;
  bool paragraphIndentApplied = false;
  // リスト項目のぶら下げ幅をマーカー語の実測幅に合わせ直したか。
  // 中間 flush で同じブロックに対して複数回レイアウトが走るので、1 度だけ行う。
  bool hangIndentAligned = false;
  // コードブロックの枠の上辺を既に出したか。1 つの <pre> 行が折り返して複数行になったとき、
  // 上辺・下辺を全ての行に引くと枠の中に横線が並んでしまう。
  bool frameTopEmitted = false;

  void applyParagraphIndent();
  std::vector<size_t> computeLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth, int spaceWidth,
                                        std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec,
                                        std::vector<bool>& cjkAdjVec);
  std::vector<size_t> computeHyphenatedLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                                  int spaceWidth, std::vector<uint16_t>& wordWidths,
                                                  std::vector<bool>& continuesVec, std::vector<bool>& cjkAdjVec);
  bool hyphenateWordAtIndex(size_t wordIndex, int availableWidth, const GfxRenderer& renderer, int fontId,
                            std::vector<uint16_t>& wordWidths, bool allowFallbackBreaks,
                            std::vector<bool>* continuesVec = nullptr, std::vector<bool>* cjkAdjVec = nullptr);
  void extractLine(size_t breakIndex, int pageWidth, int spaceWidth, const std::vector<uint16_t>& wordWidths,
                   const std::vector<bool>& continuesVec, const std::vector<bool>& cjkAdjVec,
                   const std::vector<size_t>& lineBreakIndices,
                   const std::function<void(std::shared_ptr<TextBlock>)>& processLine, const GfxRenderer& renderer,
                   int fontId);
  std::vector<uint16_t> calculateWordWidths(const GfxRenderer& renderer, int fontId);
  size_t adjustRubyBreak(size_t breakAt, size_t lineStart, int available, const std::vector<uint16_t>& extents,
                         int spaceWidth = 0, const std::vector<bool>* tight = nullptr) const;

 public:
  explicit ParsedText(const bool hyphenationEnabled = false, const BlockStyle& blockStyle = BlockStyle(),
                      const bool firstLineIndent = false)
      : blockStyle(blockStyle), firstLineIndent(firstLineIndent), hyphenationEnabled(hyphenationEnabled) {}
  ~ParsedText() = default;

  void addWord(std::string word, EpdFontFamily::Style fontStyle, bool underline = false, bool attachToPrevious = false,
               bool spaceBefore = false);
  void addWord(std::string word, EpdFontFamily::Style fontStyle, VerticalTextUtils::VerticalBehavior vBehavior,
               bool underline = false, bool attachToPrevious = false, bool spaceBefore = false);
  void setBlockStyle(const BlockStyle& blockStyle) { this->blockStyle = blockStyle; }
  BlockStyle& getBlockStyle() { return blockStyle; }
  size_t size() const { return words.size(); }
  std::vector<RubySpan>& getRubySpans() { return rubySpans; }
  bool isEmpty() const { return words.empty(); }
  void layoutAndExtractLines(const GfxRenderer& renderer, int fontId, uint16_t viewportWidth,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                             bool includeLastLine = true);
  void layoutVerticalColumns(const GfxRenderer& renderer, int fontId, uint16_t columnHeight,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processColumn,
                             bool includeLastColumn = true);
};
