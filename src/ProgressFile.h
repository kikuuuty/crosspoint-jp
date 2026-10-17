#pragma once

#include <HalStorage.h>
#include <Logging.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace ProgressFile {

// Stage the complete record before replacing progress.bin (upstream #2305).
// FAT replacement is remove + rename, NOT a metadata-atomic operation: a reset
// between them can lose the saved position, but never publishes a partial record.
// Callers serialize writes for the same book; leftover .tmp files are overwritten
// on the next save and are never used as reading progress.
inline bool writeAtomic(const std::string& cachePath, const uint8_t* data, size_t len) {
  // Only two temporary path strings; no persistent buffer or copy of the record.
  // Paths can have variable length, so avoid large fixed-size stack buffers.
  const std::string finalPath = cachePath + "/progress.bin";
  const std::string tmpPath = cachePath + "/progress.bin.tmp";

  HalFile file;
  if (!Storage.openFileForWrite("PRG", tmpPath, file)) {
    LOG_ERR("PRG", "Could not open temp progress file: %s", tmpPath.c_str());
    return false;
  }
  const size_t written = file.write(data, len);
  if (written != len) {
    LOG_ERR("PRG", "Short progress write: %u/%u bytes", static_cast<unsigned>(written), static_cast<unsigned>(len));
    file.close();
    return false;
  }
  file.flush();
  // Explicit close also checks SdFat's final sync, which the void flush() API
  // cannot report. Never rename an open file or remove the old record on failure.
  if (!file.close()) {
    LOG_ERR("PRG", "Could not close temp progress file: %s", tmpPath.c_str());
    return false;
  }

  if (Storage.exists(finalPath.c_str()) && !Storage.remove(finalPath.c_str())) {
    LOG_ERR("PRG", "Could not remove old progress file: %s", finalPath.c_str());
    return false;
  }
  if (!Storage.rename(tmpPath.c_str(), finalPath.c_str())) {
    LOG_ERR("PRG", "Could not rename temp progress file: %s", finalPath.c_str());
    return false;
  }
  return true;
}

}  // namespace ProgressFile
