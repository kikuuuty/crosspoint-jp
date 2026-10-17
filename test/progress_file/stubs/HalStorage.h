#pragma once

// Host-only FAT-like storage: rename cannot overwrite or rename an open file.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

constexpr int O_RDONLY = 0;
class HalFile;

struct StubStorage {
  std::map<std::string, std::vector<uint8_t>> files;
  std::set<std::string> openPaths;
  std::vector<std::string> events;
  std::vector<std::string> writePaths;
  bool failOpen = false;
  bool failClose = false;
  bool failRemove = false;
  bool failRename = false;
  size_t writeLimit = std::numeric_limits<size_t>::max();

  bool openFileForWrite(const char*, const std::string& path, HalFile& file);
  bool openFileForRead(const char*, const std::string& path, HalFile& file);
  HalFile open(const char* path, int = O_RDONLY);
  bool mkdir(const char*) { return true; }

  bool exists(const char* path) {
    events.push_back(std::string("exists:") + path);
    return files.contains(path);
  }
  bool remove(const char* path) {
    events.push_back(std::string("remove:") + path);
    return !failRemove && !openPaths.contains(path) && files.erase(path) == 1;
  }
  bool rename(const char* from, const char* to) {
    events.push_back(std::string("rename:") + from);
    if (failRename || openPaths.contains(from) || files.contains(to) || !files.contains(from)) return false;
    files[to] = std::move(files[from]);
    files.erase(from);
    return true;
  }
};

inline StubStorage Storage;

class HalFile {
 public:
  HalFile() = default;
  explicit HalFile(std::string path) : path(std::move(path)), opened(true) { Storage.openPaths.insert(this->path); }
  ~HalFile() {
    if (opened) close();
  }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;
  HalFile(HalFile&& other) noexcept { *this = std::move(other); }
  HalFile& operator=(HalFile&& other) noexcept {
    if (opened) close();
    path = std::move(other.path);
    cursor = other.cursor;
    opened = std::exchange(other.opened, false);
    return *this;
  }

  size_t write(const void* data, size_t len) {
    Storage.events.push_back("write:" + path);
    if (!opened) return 0;
    const size_t written = std::min(len, Storage.writeLimit);
    const auto* bytes = static_cast<const uint8_t*>(data);
    Storage.files[path].assign(bytes, bytes + written);
    return written;
  }
  int read(void* data, size_t len) {
    if (!opened) return -1;
    const auto& bytes = Storage.files.at(path);
    const size_t count = std::min(len, bytes.size() - cursor);
    if (count > 0) std::memcpy(data, bytes.data() + cursor, count);
    cursor += count;
    return static_cast<int>(count);
  }
  void flush() { Storage.events.push_back("flush:" + path); }
  bool close() {
    if (!opened) return false;
    Storage.events.push_back("close:" + path);
    Storage.openPaths.erase(path);
    opened = false;
    return !Storage.failClose;
  }
  explicit operator bool() const { return opened; }

  // Directory scanning is not exercised by these save/status tests.
  bool isDirectory() const { return false; }
  void rewindDirectory() {}
  HalFile openNextFile() { return {}; }
  size_t getName(char*, size_t) { return 0; }

 private:
  std::string path;
  size_t cursor = 0;
  bool opened = false;
};

using FsFile = HalFile;

inline bool StubStorage::openFileForWrite(const char*, const std::string& path, HalFile& file) {
  events.push_back("open:" + path);
  writePaths.push_back(path);
  if (failOpen) return false;
  files[path].clear();  // Model O_CREAT | O_TRUNC, including stale temp files.
  file = HalFile(path);
  return true;
}

inline bool StubStorage::openFileForRead(const char*, const std::string& path, HalFile& file) {
  if (!files.contains(path)) return false;
  file = HalFile(path);
  return true;
}

inline HalFile StubStorage::open(const char* path, int) {
  if (!files.contains(path)) return {};
  return HalFile(path);
}
