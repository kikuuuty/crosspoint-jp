#pragma once

#include <HalStorage.h>

#include <string>

class Epub {
 public:
  inline static FsFile* extractedFile = nullptr;
  int extracts = 0;
  bool extractSuccess = true;
  bool getItemSize(const std::string&, size_t*) { return false; }
  bool readItemContentsToStream(const std::string&, FsFile& file, size_t) {
    ++extracts;
    extractedFile = &file;
    file.write("image", 5);
    return extractSuccess;
  }
};
