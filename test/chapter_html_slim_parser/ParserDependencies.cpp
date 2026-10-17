#include <Arduino.h>
#include <GfxRenderer.h>
#include <TestImageDecoder.h>

#include <string>

#include "Epub.h"
#include "Epub/blocks/ImageBlock.h"
#include "Epub/converters/ImageDecoderFactory.h"
#include "Epub/css/CssSelectorUsage.h"
#include "Epub/hyphenation/Hyphenator.h"

// Hardware/ZIP decoding is not part of these tests. Production parser, CSS,
// ruby, layout, page construction and table buffering remain real.
namespace FsHelpers {
std::string normalisePath(const std::string& path) { return path; }
}  // namespace FsHelpers

ImageBlock::ImageBlock(const std::string& path, int16_t w, int16_t h) : imagePath(path), width(w), height(h) {}
void ImageBlock::render(GfxRenderer&, int, int) {}
bool ImageBlock::serialize(FsFile&) { return false; }
std::unique_ptr<ImageBlock> ImageBlock::deserialize(FsFile&) { return nullptr; }

namespace {
class TestDecoder : public ImageToFramebufferDecoder {
 public:
  bool decodeToFramebuffer(const std::string&, GfxRenderer&, const RenderConfig&) override { return false; }
  const char* getFormatName() const override { return "test"; }
  bool getDimensions(const std::string&, ImageDimensions& dimensions) const override {
    ++imageProbe.calls;
    imageProbe.delayCounts.push_back(testDelays.size());
    imageProbe.afterFlushAndClose &= Epub::extractedFile && Epub::extractedFile->flushed && !Epub::extractedFile->open;
    if (imageProbe.calls <= imageProbe.failuresBeforeSuccess) return false;
    dimensions = imageProbe.dimensions;
    return true;
  }
} decoder;
}  // namespace

bool ImageDecoderFactory::isFormatSupported(const std::string& path) {
  return path.ends_with(".png") || path.ends_with(".jpg") || path.ends_with(".jpeg");
}
ImageToFramebufferDecoder* ImageDecoderFactory::getDecoder(const std::string&) {
  return imageProbe.noDecoder ? nullptr : &decoder;
}
bool CssSelectorUsage::matches(const std::string&) const { return true; }

std::vector<Hyphenator::BreakInfo> Hyphenator::breakOffsets(const std::string&, bool) { return {}; }
