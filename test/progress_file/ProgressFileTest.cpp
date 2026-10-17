#include <cstdlib>
#include <iostream>

#include "FsHelpers.h"
#include "ProgressFile.h"
#include "ReaderProgressHarness.h"
#include "ReadingStatusHelper.h"

namespace {
const std::string CACHE = "/.crosspoint/epub_123";
const std::string FINAL = CACHE + "/progress.bin";
const std::string TEMP = CACHE + "/progress.bin.tmp";
const std::vector<uint8_t> OLD = {1, 0, 2, 0, 10, 0, 0, 20};
const std::vector<uint8_t> NEXT = {3, 0, 4, 0, 20, 0, 1, 100};

void check(bool value, const char* message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void reset(bool existing = true) {
  check(Storage.openPaths.empty(), "all progress handles must be closed");
  Storage = {};
  if (existing) Storage.files[FINAL] = OLD;
}

bool save() { return ProgressFile::writeAtomic(CACHE, NEXT.data(), NEXT.size()); }

void expectOld() {
  check(Storage.files.at(FINAL) == OLD, "failure before replacement must preserve the old record byte-for-byte");
  check(Storage.openPaths.empty(), "failure must close temporary handles");
  for (const auto& path : Storage.writePaths) check(path == TEMP, "canonical progress must never be opened for write");
  check(std::find(Storage.events.begin(), Storage.events.end(), "remove:" + FINAL) == Storage.events.end() ||
            Storage.failRemove,
        "old progress must not be removed before a complete, closed temp file exists");
}

void testSave() {
  reset();
  check(save(), "replacing an existing record must succeed");
  check(Storage.files.at(FINAL) == NEXT && !Storage.files.contains(TEMP),
        "successful replacement must publish all bytes");
  const std::vector<std::string> expected = {"open:" + TEMP,    "write:" + TEMP,   "flush:" + TEMP, "close:" + TEMP,
                                             "exists:" + FINAL, "remove:" + FINAL, "rename:" + TEMP};
  check(Storage.events == expected, "must finish write/flush/close before remove/rename");

  reset(false);
  check(save() && Storage.files.at(FINAL) == NEXT, "first save must not require an old file");

  for (const size_t staleSize : {2, 40}) {
    reset();
    Storage.files[TEMP] = std::vector<uint8_t>(staleSize, 0xff);
    check(save() && Storage.files.at(FINAL) == NEXT, "stale temp must be truncated, not appended or recovered");
  }
}

void testFailures() {
  reset();
  Storage.failOpen = true;
  check(!save(), "temp-open failure must be reported");
  expectOld();

  for (size_t len = 0; len < NEXT.size(); ++len) {
    reset();
    Storage.writeLimit = len;
    check(!save(), "every short write must be rejected");
    expectOld();
    Storage.writeLimit = NEXT.size();
    check(save(), "retry must overwrite a partial temp and succeed");
  }

  reset();
  Storage.failClose = true;
  check(!save(), "close/sync failure must be reported before removing old progress");
  expectOld();

  reset();
  Storage.failRemove = true;
  check(!save(), "old-file remove failure must be reported");
  expectOld();
  check(Storage.files.at(TEMP) == NEXT, "failed replacement keeps the complete temp file");

  reset();
  Storage.failRename = true;
  check(!save(), "rename failure must not be reported as a successful save");
  check(!Storage.files.contains(FINAL) && Storage.files.at(TEMP) == NEXT,
        "FAT remove/rename gap can lose progress, but cannot publish partial bytes");
  Storage.failRename = false;
  check(save(), "retry after rename failure must succeed even when the final path is absent");
}

std::string bookCache(const std::string& book, const char* prefix) {
  return "/.crosspoint/" + std::string(prefix) + std::to_string(FsHelpers::pathHash(book));
}

void testFinishedRecords() {
  for (const bool epub : {true, false}) {
    const std::string book = epub ? "/Books/test.epub" : "/Books/test.xtc";
    const std::string cache = bookCache(book, epub ? "epub_" : "xtc_");
    const std::string path = cache + "/progress.bin";
    const size_t size = epub ? 8 : 6;
    const size_t flag = epub ? 6 : 4;
    // Legacy and current records, plus a book without a progress file.
    const std::vector<size_t> oldSizes = epub ? std::vector<size_t>{0, 4, 6, 7, 8} : std::vector<size_t>{0, 4, 5, 6};
    for (const size_t oldSize : oldSizes) {
      reset(false);
      std::vector<uint8_t> expected(size, 0);
      if (oldSize) {
        Storage.files[path] = std::vector<uint8_t>(OLD.begin(), OLD.begin() + oldSize);
        std::copy_n(OLD.begin(), oldSize, expected.begin());
      }
      expected[flag] = 1;
      expected[flag + 1] = 100;
      check(markAsFinished(book, "/.crosspoint"), "mark finished must succeed for legacy and current records");
      check(Storage.files.at(path) == expected, "mark finished must preserve position bytes and extend the record");
      check(Storage.writePaths == std::vector<std::string>{cache + "/progress.bin.tmp"},
            "mark finished must stage its write, not truncate progress.bin");
      const auto progress = getReadingProgress(book, "/.crosspoint");
      check(progress.status == ReadingStatus::Finished && progress.percent == 100,
            "finished status must remain readable");
    }

    reset(false);
    Storage.files[path] = std::vector<uint8_t>(OLD.begin(), OLD.begin() + size);
    const auto original = Storage.files.at(path);
    Storage.writeLimit = 1;
    check(!markAsFinished(book, "/.crosspoint"), "mark finished must propagate a failed save");
    check(Storage.files.at(path) == original, "failed mark finished must preserve the original position/status");
  }
}

void testStatusIgnoresTemp() {
  const std::string book = "/Books/test.epub";
  const std::string cache = bookCache(book, "epub_");
  reset(false);
  Storage.files[cache + "/progress.bin.tmp"] = NEXT;
  check(getReadingProgress(book, "/.crosspoint").status == ReadingStatus::Unread,
        "a leftover temp must never be used as reading progress");
  Storage.files[cache + "/progress.bin"] = OLD;
  const auto progress = getReadingProgress(book, "/.crosspoint");
  check(progress.status == ReadingStatus::Reading && progress.percent == 20,
        "a complete temp must not override existing canonical progress");
}

void testReaderSaves() {
  ReaderTestBook book{CACHE, 200000};
  reset(false);
  EpubReaderActivity epub;
  epub.epub = &book;
  const std::vector<uint8_t> epubRecord = {0x23, 1, 0x34, 2, 0x45, 3, 0, 37};
  epub.saveProgress(0x123, 0x234, 0x345, false, 37);
  check(Storage.files.at(FINAL) == epubRecord && epub.progressSaved, "EPUB record format must stay unchanged");
  const auto writes = Storage.writePaths.size();
  epub.saveProgress(0x123, 0x234, 0x345, false, 37);
  check(Storage.writePaths.size() == writes, "clock redraw must not rewrite an unchanged EPUB position");
  Storage.failRename = true;
  epub.saveProgress(0x123, 0x235, 0x345, false, 38);
  check(std::equal(epubRecord.begin(), epubRecord.end(), epub.lastSavedProgress),
        "EPUB deduplication state must not advance after failed replacement");
  Storage.failRename = false;
  epub.saveProgress(0x123, 0x235, 0x345, false, 38);
  check(Storage.files.at(FINAL)[2] == 0x35 && Storage.files.at(FINAL)[7] == 38,
        "EPUB must retry the identical position after rename failure");
  epub.saveProgress(0x123, 0x235, 0x345, true, 100);
  check(Storage.files.at(FINAL)[6] == 1 && Storage.files.at(FINAL)[7] == 100, "EPUB finished state must be preserved");

  reset(false);
  TxtReaderActivity txt;
  txt.txt = &book;
  const std::vector<uint8_t> txtRecord = {9, 0, 0, 0, 10};
  txt.saveProgress();
  check(Storage.files.at(FINAL) == txtRecord && txt.progressSaved, "TXT record format must stay unchanged");
  Storage.writeLimit = 1;
  txt.currentPage = 10;
  txt.saveProgress();
  check(Storage.files.at(FINAL) == txtRecord && std::equal(txtRecord.begin(), txtRecord.end(), txt.lastSavedProgress),
        "failed TXT save must preserve both file and deduplication state");
  Storage.writeLimit = 5;
  txt.saveProgress();
  check(Storage.files.at(FINAL) == std::vector<uint8_t>({10, 0, 0, 0, 11}), "TXT must retry after a short write");
  const auto txtWrites = Storage.writePaths.size();
  txt.saveProgress();
  check(Storage.writePaths.size() == txtWrites, "clock redraw must not rewrite an unchanged TXT position");

  reset(false);
  XtcReaderActivity xtc;
  xtc.xtc = &book;
  xtc.currentPage = 0x12345;
  xtc.saveProgress();
  check(Storage.files.at(FINAL) == std::vector<uint8_t>({0x45, 0x23, 1, 0, 0, 37}),
        "XTC must keep its 32-bit page number and percentage");
  const auto xtcRecord = Storage.files.at(FINAL);
  Storage.writeLimit = 1;
  xtc.currentPage++;
  xtc.saveProgress();
  check(Storage.files.at(FINAL) == xtcRecord, "failed XTC save must not truncate existing progress");
  Storage.writeLimit = 6;
  xtc.saveProgress();
  check(Storage.files.at(FINAL)[0] == 0x46, "XTC must retry after a short write");
  xtc.saveProgress(true);
  check(Storage.files.at(FINAL)[4] == 1 && Storage.files.at(FINAL)[5] == 100, "XTC finished state must be preserved");
  for (const auto& path : Storage.writePaths) check(path == TEMP, "XTC must stage every progress write");
}
}  // namespace

int main() {
  testSave();
  testFailures();
  testFinishedRecords();
  testStatusIgnoresTemp();
  testReaderSaves();
  check(Storage.openPaths.empty(), "no test may leak a file handle");
  std::cout << "Progress file tests passed (staged writes, fault injection, legacy records, finished status).\n";
}
