#include <cstdlib>
#include <iostream>
#include <vector>

#include "FsHelpers.h"

String normalizeWebPath(const String& inputPath);

namespace {
void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void testComponents() {
  for (const auto name : {"book.epub", "volume..2.epub", "notes...txt", ".hidden", "a.b", "100%.epub",
                          "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e.epub", "O'Brien & Co.epub"}) {
    check(FsHelpers::isSafePathComponent(std::string_view(name)), "ordinary single-component names must remain valid");
    check(FsHelpers::isSafePathComponent(String(name)), "Arduino String overload must agree");
  }
  for (const auto name : {"", ".", "..", "/", "\\", "../x", "x/..", "a/b", "a\\b", "..\\x", "./x"}) {
    check(!FsHelpers::isSafePathComponent(std::string_view(name)), "a filename must not contain a path");
    check(!FsHelpers::isSafePathComponent(String(name)), "Arduino String overload must reject paths");
  }
  const std::string nul("book\0.epub", 10);
  check(!FsHelpers::isSafePathComponent(std::string_view(nul)),
        "embedded NUL must not be truncated at a C API boundary");
  check(!FsHelpers::isSafePathComponent(String(nul)), "String validation must use its length, not strlen");
}

void testPaths() {
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"", ""},
      {"/", ""},
      {"///", ""},
      {"Books/book.epub", "Books/book.epub"},
      {"//Books///book.epub/", "Books/book.epub"},
      {"/Books/../.crosspoint/x", ".crosspoint/x"},
      {"/../../etc", "etc"},
      {"/Books/..", ""},
      {"/Books/../", ""},
      {"/Books/.", "Books"},
      {"/Books/./book.epub", "Books/book.epub"},
      {"/a/b/../../c/../d", "d"},
      {"/a/../../..", ""},
      {"/Books/volume..2.epub", "Books/volume..2.epub"},
      {"/Books/%2e%2e/file.epub", "Books/%2e%2e/file.epub"},
      {"/.crosspoint/fonts/Family/../Family/", ".crosspoint/fonts/Family"},
      {"/sleep/./", "sleep"},
  };
  for (const auto& [input, expected] : cases) {
    check(FsHelpers::normalisePath(input) == expected, "relative canonical path mismatch");
    const std::string absolute = expected.empty() ? "/" : "/" + expected;
    check(std::string(normalizeWebPath(String(input)).c_str()) == absolute, "web path must be absolute and canonical");
    check(FsHelpers::normalisePath(expected) == expected, "normalization must be idempotent");
  }

  // Differential check with a simple component-stack reference, including all
  // final-component cases the old implementation handled differently.
  const std::vector<std::string> parts = {"", ".", "..", "a", "b", "volume..2", "%20"};
  for (const auto& a : parts) {
    for (const auto& b : parts) {
      for (const auto& c : parts) {
        std::vector<std::string> stack;
        stack.reserve(3);
        for (const auto& part : {a, b, c}) {
          if (part == "..") {
            if (!stack.empty()) stack.pop_back();
          } else if (!part.empty() && part != ".") {
            stack.push_back(part);
          }
        }
        std::string expected;
        for (const auto& part : stack) {
          if (!expected.empty()) expected += '/';
          expected += part;
        }
        const auto path = "/" + a + "/" + b + "/" + c;
        check(FsHelpers::normalisePath(path) == expected, "normalization must match the component-stack reference");
      }
    }
  }
}
}  // namespace

int main() {
  testComponents();
  testPaths();
  std::cout << "Web path tests passed (components, canonical paths, font/sleep destinations, 343 reference cases).\n";
}
