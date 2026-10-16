#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

// In-memory SD file for the production TextBlock serializer tests.
class FsFile {
 public:
  std::vector<uint8_t> bytes;
  size_t cursor = 0;
  size_t write(const uint8_t* data, const size_t count) {
    bytes.resize(std::max(bytes.size(), cursor + count));
    if (count) std::memcpy(bytes.data() + cursor, data, count);
    cursor += count;
    return count;
  }
  size_t read(void* data, const size_t count) {
    const size_t actual = std::min(count, available());
    if (actual) std::memcpy(data, bytes.data() + cursor, actual);
    cursor += actual;
    return actual;
  }
  size_t available() const { return bytes.size() - cursor; }
};
