#pragma once

#include <cstddef>

// Only needed to compile FsHelpers' unused Arduino String overloads.
class String {
 public:
  const char* c_str() const { return ""; }
  size_t length() const { return 0; }
};
