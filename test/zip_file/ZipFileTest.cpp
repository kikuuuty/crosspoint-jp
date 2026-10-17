#include <cstdlib>
#include <iostream>
#include <string>

// Include the unchanged production translation unit to exercise its internal
// callback as well as the public streaming API, without adding test-only hooks.
#include "ZipFile.cpp"

// Vendored uzlib omits the optional checksum helpers. Firmware link-time GC
// drops their unused wrapper; host linkers may retain it. These fail-fast stubs
// must never be called: production InflateReader uses raw uzlib_uncompress().
extern "C" uint32_t uzlib_adler32(const void*, unsigned int, uint32_t) { std::abort(); }
extern "C" uint32_t uzlib_crc32(const void*, unsigned int, uint32_t) { std::abort(); }

namespace {
void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void testCallbackFailure(int readResult) {
  ReadFault fault{0, 4, readResult, 0};
  FsFile file({1, 2, 3, 4}, &fault);
  uint8_t buffer[4] = {};
  ZipInflateCtx ctx;
  ctx.file = &file;
  ctx.fileRemaining = 4;
  ctx.readBuf = buffer;
  ctx.readBufSize = sizeof(buffer);
  auto* raw = ctx.reader.raw();
  raw->source = buffer + 2;
  raw->source_limit = buffer + 3;

  check(zipReadCallback(raw) == -1, "callback must report EOF/error, not return an unread byte");
  check(ctx.fileRemaining == 4, "failed read must not change the remaining compressed size");
  check(raw->source == buffer + 2 && raw->source_limit == buffer + 3,
        "failed read must not publish an invalid input range");
  check(fault.hits == 1, "callback fault must be exercised");
}

void testCallbackReads() {
  FsFile file({0x80, 2, 3, 4, 5, 6, 7, 8});
  uint8_t buffer[4] = {};
  ZipInflateCtx ctx;
  ctx.file = &file;
  ctx.fileRemaining = 8;
  ctx.readBuf = buffer;
  ctx.readBufSize = sizeof(buffer);
  auto* raw = ctx.reader.raw();

  check(zipReadCallback(raw) == 0x80, "callback must return the first byte without sign extension");
  check(file.lastRequest == 4 && ctx.fileRemaining == 4, "read must be capped to the buffer size");
  check(raw->source == buffer + 1 && raw->source_limit == buffer + 4, "full read input range");

  file.maxRead = 2;
  check(zipReadCallback(raw) == 5, "positive short read must succeed");
  check(ctx.fileRemaining == 2 && raw->source_limit == buffer + 2, "short read must use its actual size");
  file.maxRead = 4;
  check(zipReadCallback(raw) == 7, "final read must succeed");
  check(file.lastRequest == 2 && ctx.fileRemaining == 0, "final read must be capped to the remaining data");
  const auto calls = file.readCalls;
  check(zipReadCallback(raw) == -1 && file.readCalls == calls, "exhausted input must not read again");
}

class Output : public Print {
 public:
  std::string data;
  size_t writes = 0;
  bool oversizedWrite = false;

  size_t write(const uint8_t* buffer, size_t count) override {
    ++writes;
    // Reject invalid sizes before touching buffer, so the old stored-ZIP bug
    // fails the test without making the host read out of bounds.
    if (count > 1024) {
      oversizedWrite = true;
      return 0;
    }
    data.append(reinterpret_cast<const char*>(buffer), count);
    return count;
  }
};

void testStream(const std::string& path, const std::string& expected) {
  for (const size_t chunkSize : {1, 7, 1024}) {
    Storage.fault = {};
    ZipFile zip(path);
    Output output;
    check(zip.readFileToStream("content.xhtml", output, chunkSize), "valid ZIP must stream successfully");
    check(output.data == expected && !output.oversizedWrite, "stream output must match the original UTF-8 bytes");
    check(!zip.isOpen(), "stream must close the file it opened");
  }
}

void testStreamFailure(const std::string& path, size_t dataOffset, size_t compressedSize, bool stored = false) {
  for (const int result : {-1, -7, 0}) {
    // Fail at the first refill and after one successful refill. Metadata reads
    // remain healthy, so this tests failures in the actual ZIP data path.
    for (const size_t after : {0, 7}) {
      // The 1KB EOCD scan may overlap stored data; do not inject faults there.
      Storage.fault = {dataOffset + after, dataOffset + compressedSize, result, 0, 7};
      ZipFile zip(path);
      Output output;
      check(!zip.readFileToStream("content.xhtml", output, 7), "read error/early EOF must fail the stream");
      check(Storage.fault.hits > 0, "stream fault must be exercised");
      check(!output.oversizedWrite, "read error must never reach the output as a huge write");
      if (after == 0) check(output.writes == 0, "failed first read must not write output");
      if (stored) check(output.data.size() == after, "stored stream must stop after the last successful read");
      check(!zip.isOpen(), "failed stream must close the file it opened");
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  check(argc == 5, "usage: ZipFileTest deflated.zip stored.zip expected.txt compressed-size");
  // Run the original regression first; it fails safely before a decoder can
  // dereference the invalid pointers created by the unfixed callback.
  for (const int result : {-1, -7, 0}) testCallbackFailure(result);
  testCallbackReads();

  std::ifstream input(argv[3], std::ios::binary);
  check(static_cast<bool>(input), "expected text fixture must exist");
  const std::string expected((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  testStream(argv[1], expected);
  testStream(argv[2], expected);
  const size_t dataOffset = 30 + std::strlen("content.xhtml");
  testStreamFailure(argv[1], dataOffset, std::strtoul(argv[4], nullptr, 10));
  testStreamFailure(argv[2], dataOffset, expected.size(), true);
  std::cout << "ZIP callback and streaming tests passed (deflated/stored, signed errors, EOF, short reads).\n";
}
