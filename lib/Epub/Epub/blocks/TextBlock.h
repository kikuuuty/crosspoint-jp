#pragma once
#include <EpdFontFamily.h>
#include <HalStorage.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "../Ruby.h"
#include "Block.h"
#include "BlockStyle.h"

// Represents a line of text on a page
class TextBlock final : public Block {
 private:
  std::vector<std::string> words;
  std::vector<int16_t> wordXpos;
  std::vector<EpdFontFamily::Style> wordStyles;
  BlockStyle blockStyle;
  std::vector<int16_t> wordYpos;  // vertical layout: y position within column
  bool isVertical = false;        // true when this block was laid out vertically
  std::vector<RubySpan> rubySpans;

  int wordExtent(const GfxRenderer& renderer, int fontId, size_t index, int columnWidth) const;
  void renderRuby(GfxRenderer& renderer, int fontId, int x, int y, int columnWidth, int viewportWidth,
                  int viewportX) const;

 public:
  explicit TextBlock(std::vector<std::string> words, std::vector<int16_t> word_xpos,
                     std::vector<EpdFontFamily::Style> word_styles, const BlockStyle& blockStyle = BlockStyle(),
                     std::vector<int16_t> word_ypos = {}, bool vertical = false, std::vector<RubySpan> ruby_spans = {})
      : words(std::move(words)),
        wordXpos(std::move(word_xpos)),
        wordStyles(std::move(word_styles)),
        blockStyle(blockStyle),
        wordYpos(std::move(word_ypos)),
        isVertical(vertical),
        rubySpans(std::move(ruby_spans)) {}
  ~TextBlock() override = default;
  void setBlockStyle(const BlockStyle& blockStyle) { this->blockStyle = blockStyle; }
  const BlockStyle& getBlockStyle() const { return blockStyle; }
  const std::vector<std::string>& getWords() const { return words; }
  const std::vector<int16_t>& getWordYpos() const { return wordYpos; }
  bool getIsVertical() const { return isVertical; }
  bool hasRuby() const;
  const std::vector<RubySpan>& getRubySpans() const { return rubySpans; }
  static int rubyFontId;  // アプリ層から設定されるルビフォントID（0=ルビ描画しない）
  // <sup> / <sub> を描くための小さいフォントID。アプリ層が設定する。
  // 0 のときは本文フォントのまま描く（位置もずらさない＝従来どおり）。
  static int smallFontId;
  bool isEmpty() override { return words.empty(); }
  size_t wordCount() const { return words.size(); }
  // given a renderer works out where to break the words into lines
  // renderer は非 const で受ける（インライン画像の描画が ImageBlock 経由で GfxRenderer の
  // 状態を触るため。呼び出し元の PageLine::render は元々非 const）。
  // viewportX / viewportWidth はビューポート（余白を除いた描画領域）の左端 x と幅。
  // 横書きルビは実際の本文の先頭〜末尾に収め、さらにこのビューポート内に制限する。
  // viewportWidth が 0 のときも本文の端を越える突出は許さない。
  void render(GfxRenderer& renderer, int fontId, int x, int y, int viewportWidth = 0, int viewportX = 0) const;
  void collectCodepoints(std::vector<uint32_t>& out, size_t max) const;
  BlockType getType() override { return TEXT_BLOCK; }
  bool serialize(FsFile& file) const;
  static std::unique_ptr<TextBlock> deserialize(FsFile& file);
};
