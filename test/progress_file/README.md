# Progress-file persistence tests

```sh
python test/run_progress_file_test.py
```

Requires Python 3 and a C++20 compiler on Windows, Linux or macOS (`CXX` may
select the compiler). No Python packages or device are needed. Generated sources
and executables are removed from the ignored `build/progress_file/` directory
after the test.

The tests compile the production `ProgressFile.h`, `ReadingStatusHelper.cpp` and
`FsHelpers.cpp` against a FAT-like storage stub. They also compile the three
readers' `saveProgress()` method bodies verbatim with minimal owner classes;
these test-only classes do not emulate the rest of the activities. A firmware
build is still required to check full integration.

Coverage:

- First save, replacement and stale/partial temporary-file replacement.
- Write/flush/close ordering before remove/rename, and closed handles on failure.
- Temporary-file open failure, every short-write length of an EPUB record,
  close/sync failure, remove failure, rename failure and retries.
- Existing progress is not truncated on staging failure. A failed FAT rename
  after removal is reported as failure (it can lose progress; it is not atomic).
- Legacy/current EPUB and XTC records keep their position bytes when marked
  finished. Reader records remain EPUB=8, XTC=6, TXT=5 bytes, with their existing
  finished/percentage fields unchanged.
- EPUB/TXT failure retries and unchanged-page write suppression (clock redraws).
- Status readers ignore leftover `.tmp` files.

Based on upstream CrossPoint
[#2305](https://github.com/crosspoint-reader/crosspoint-reader/pull/2305), adapted
to this fork's extended records and file-browser "mark finished" writer. This
version also checks close and old-file removal failures before proceeding.

## Device smoke test

1. Back up `.crosspoint/` on a test SD card, including progress for your books.
2. Install the firmware built with `pio run -e default`.
3. Open a partly read EPUB, change chapters/pages, return home and check the
   displayed percentage. Sleep/wake and reopen it: the new position must persist.
4. Read to the end, or use "mark finished" in the file browser: check the finished
   icon/100% status and that marking finished does not reset the saved position.
5. If available, repeat saving/reopening with an XTC/XTCH book. TXT/Markdown are
   normally disabled in this fork; their save code is covered by host tests.
6. When no save is in progress, the updated book's cache directory should contain
   `progress.bin`, not `progress.bin.tmp`. Existing unrelated cache files need
   not be removed.

Do not unplug the SD card or cut power during a write to provoke failures. The
host tests inject those failure results without risking your actual card. The
storage stub verifies software behavior, not physical FAT power-loss durability.
