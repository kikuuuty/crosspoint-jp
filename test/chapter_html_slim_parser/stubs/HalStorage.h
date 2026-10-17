#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

class FsFile {
 public:
  std::shared_ptr<std::vector<uint8_t>> bytes = std::make_shared<std::vector<uint8_t>>();
  size_t cursor = 0;
  bool open = true;
  bool flushed = false;
  size_t write(const void* data, size_t count) {
    bytes->resize(std::max(bytes->size(), cursor + count));
    if (count) std::memcpy(bytes->data() + cursor, data, count);
    cursor += count;
    return count;
  }
  size_t write(uint8_t byte) { return write(&byte, 1); }
  size_t read(void* data, size_t count) {
    const size_t actual = std::min(count, available());
    if (actual) std::memcpy(data, bytes->data() + cursor, actual);
    cursor += actual;
    return actual;
  }
  int read() {
    uint8_t byte;
    return read(&byte, 1) ? byte : -1;
  }
  size_t available() const { return bytes->size() - cursor; }
  size_t size() const { return bytes->size(); }
  bool seek(size_t pos) {
    cursor = std::min(pos, bytes->size());
    return cursor == pos;
  }
  void flush() { flushed = true; }
  void close() { open = false; }
  explicit operator bool() const { return open; }
};

inline struct TestStorage {
  std::map<std::string, std::shared_ptr<std::vector<uint8_t>>> files;
  std::vector<std::string> removed;
  bool failWrite = false;
  void put(const std::string& path, const std::string& content) {
    files[path] = std::make_shared<std::vector<uint8_t>>(content.begin(), content.end());
  }
  bool openFileForRead(const char*, const std::string& path, FsFile& file) {
    if (!files.count(path)) return false;
    file = {};
    file.bytes = files.at(path);
    return true;
  }
  bool openFileForWrite(const char*, const std::string& path, FsFile& file) {
    if (failWrite) return false;
    file = {};
    files[path] = file.bytes;
    return true;
  }
  bool exists(const char* path) const { return files.count(path) != 0; }
  bool remove(const char* path) {
    removed.emplace_back(path);
    return files.erase(path) != 0;
  }
} Storage;
