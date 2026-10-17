#pragma once

// Host-only file storage with injectable signed read errors. The ZIP parser and
// decompressor themselves are compiled from the production sources.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(const uint8_t* data, size_t size) = 0;
};

struct ReadFault {
  size_t begin = std::numeric_limits<size_t>::max();
  size_t end = std::numeric_limits<size_t>::max();
  int result = -1;
  size_t hits = 0;
  size_t maxRequest = std::numeric_limits<size_t>::max();
};

class FsFile {
 public:
  std::vector<uint8_t> data;
  ReadFault* fault = nullptr;
  size_t maxRead = std::numeric_limits<size_t>::max();
  size_t readCalls = 0;
  size_t lastRequest = 0;

  FsFile() = default;
  explicit FsFile(std::vector<uint8_t> bytes, ReadFault* readFault = nullptr)
      : data(std::move(bytes)), fault(readFault), opened(true) {}

  int read(void* buffer, size_t count) {
    ++readCalls;
    lastRequest = count;
    if (!opened) return -1;
    if (fault && cursor >= fault->begin && cursor < fault->end && count <= fault->maxRequest) {
      ++fault->hits;
      return fault->result;
    }
    const size_t bytes = std::min({count, data.size() - cursor, maxRead});
    if (bytes > 0) std::memcpy(buffer, data.data() + cursor, bytes);
    cursor += bytes;
    return static_cast<int>(bytes);
  }

  bool seek(size_t position) {
    if (position > data.size()) return false;
    cursor = position;
    return true;
  }
  bool seekCur(int64_t offset) { return seek(static_cast<size_t>(static_cast<int64_t>(cursor) + offset)); }
  size_t size() const { return data.size(); }
  size_t position() const { return cursor; }
  int available() const { return opened ? static_cast<int>(data.size() - cursor) : 0; }
  void close() { opened = false; }
  explicit operator bool() const { return opened; }

 private:
  size_t cursor = 0;
  bool opened = false;
};

struct StubStorage {
  ReadFault fault;

  bool openFileForRead(const char*, const std::string& path, FsFile& file) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    file = FsFile(std::move(bytes), &fault);
    return true;
  }
};

inline StubStorage Storage;
