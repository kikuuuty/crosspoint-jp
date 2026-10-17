#include "FsHelpers.h"

#include <cctype>
#include <cstring>

namespace FsHelpers {

std::string normalisePath(const std::string& path) {
  // Keep the existing relative-result contract, but process the final component
  // exactly like every other one (including trailing "." and ".."). Only the
  // returned string owns storage; components are views, not heap allocations.
  const std::string_view input(path);
  std::string result;
  result.reserve(path.size());
  for (size_t start = 0; start < path.size();) {
    const size_t end = path.find('/', start);
    const size_t length = (end == std::string::npos ? path.size() : end) - start;
    const std::string_view component = input.substr(start, length);
    if (component == "..") {
      const size_t slash = result.find_last_of('/');
      result.resize(slash == std::string::npos ? 0 : slash);
    } else if (!component.empty() && component != ".") {
      if (!result.empty()) result += '/';
      result.append(component.data(), component.size());
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return result;
}

bool isSafePathComponent(std::string_view name) {
  return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\") == std::string_view::npos &&
         name.find('\0') == std::string_view::npos;
}

bool checkFileExtension(std::string_view fileName, const char* extension) {
  const size_t extLen = strlen(extension);
  if (fileName.length() < extLen) {
    return false;
  }

  const size_t offset = fileName.length() - extLen;
  for (size_t i = 0; i < extLen; i++) {
    if (tolower(static_cast<unsigned char>(fileName[offset + i])) !=
        tolower(static_cast<unsigned char>(extension[i]))) {
      return false;
    }
  }
  return true;
}

bool hasJpgExtension(std::string_view fileName) {
  return checkFileExtension(fileName, ".jpg") || checkFileExtension(fileName, ".jpeg");
}

bool hasPngExtension(std::string_view fileName) { return checkFileExtension(fileName, ".png"); }

bool hasBmpExtension(std::string_view fileName) { return checkFileExtension(fileName, ".bmp"); }

bool hasGifExtension(std::string_view fileName) { return checkFileExtension(fileName, ".gif"); }

bool hasEpubExtension(std::string_view fileName) { return checkFileExtension(fileName, ".epub"); }

bool hasXtcExtension(std::string_view fileName) {
  return checkFileExtension(fileName, ".xtc") || checkFileExtension(fileName, ".xtch");
}

bool hasTxtExtension(std::string_view fileName) { return checkFileExtension(fileName, ".txt"); }

bool hasMarkdownExtension(std::string_view fileName) { return checkFileExtension(fileName, ".md"); }

std::string extractFolderPath(const std::string& filePath) {
  const auto lastSlash = filePath.find_last_of('/');
  if (lastSlash == std::string::npos || lastSlash == 0) {
    return "/";
  }
  return filePath.substr(0, lastSlash);
}

}  // namespace FsHelpers
