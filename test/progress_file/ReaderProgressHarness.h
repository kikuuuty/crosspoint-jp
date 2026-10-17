#pragma once

#include <cstdint>
#include <string>

// Minimal owners for the verbatim production saveProgress() method bodies.
// UI/rendering dependencies are not needed to check the record format or retries.
struct ReaderTestBook {
  std::string cachePath;
  uint32_t pageCount = 100;
  const std::string& getCachePath() const { return cachePath; }
  uint32_t getPageCount() const { return pageCount; }
};

class EpubReaderActivity {
 public:
  ReaderTestBook* epub = nullptr;
  bool progressSaved = false;
  uint8_t lastSavedProgress[8] = {};
  void saveProgress(int spineIndex, int currentPage, int pageCount, bool isFinished = false, int percent = -1);
};

class TxtReaderActivity {
 public:
  ReaderTestBook* txt = nullptr;
  int totalPages = 100;
  int currentPage = 9;
  bool progressSaved = false;
  uint8_t lastSavedProgress[5] = {};
  void saveProgress();
};

class XtcReaderActivity {
 public:
  ReaderTestBook* xtc = nullptr;
  uint32_t currentPage = 0;
  void saveProgress(bool isFinished = false) const;
};
