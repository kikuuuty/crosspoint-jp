#include <GfxRenderer.h>

#include <string>

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

bool ImageDecoderFactory::isFormatSupported(const std::string& path) { return path.ends_with(".png"); }
ImageToFramebufferDecoder* ImageDecoderFactory::getDecoder(const std::string&) { return nullptr; }
bool CssSelectorUsage::matches(const std::string&) const { return true; }

std::vector<Hyphenator::BreakInfo> Hyphenator::breakOffsets(const std::string&, bool) { return {}; }
