# ZIP read-error regression tests

Run on Windows, Linux or macOS with Python 3 and a C/C++ compiler:

```sh
python test/run_zip_file_test.py
```

Set `CC` / `CXX` if the compiler is not on `PATH`. No device or Python packages
are needed. Temporary ZIP fixtures and executables are created under the ignored
`build/zip_file/` directory and removed after the run.

The test compiles the production `ZipFile.cpp`, `InflateReader.cpp` and uzlib
decoder. Only storage and logging are replaced by host stubs. It checks:

- Negative read results (`-1`, `-7`) and EOF (`0`) never change the callback's
  remaining-size counter or publish invalid input pointers.
- Full, short and final reads respect the buffer/input bounds; exhausted input
  does not read the file again.
- Deflated and stored ZIP entries stream byte-for-byte UTF-8 content with 1-,
  7- and 1024-byte buffers.
- Read errors and early EOF, both immediately and after one successful refill,
  return failure without oversized output writes; files opened by the operation
  are closed.

## Backport notes

Based on upstream CrossPoint PR
[#3244](https://github.com/crosspoint-reader/crosspoint-reader/pull/3244).
This fork still uses the uzlib byte callback, whose EOF/error sentinel is `-1`,
not the `0` byte count used by upstream's newer callback. The same signed-read
guard also protects this fork's stored-entry streaming path.

## Device smoke test

1. Back up the test SD card's `.crosspoint/` directory (including reading progress).
2. Build with `pio run -e default` and install using the usual USB or SD update
   procedure. The firmware is `.pio/build/default/firmware.bin`.
3. Open a new copy of a Japanese EPUB so that its chapter cache is generated.
   Check the first page, chapter changes, ruby and images. If available, also use
   an EPUB with stored (uncompressed) ZIP entries.
4. Sleep/wake and reopen the book; check that reading still works normally.

On Windows, if PlatformIO stops on a CP932 `UnicodeEncodeError`, use UTF-8 for
the build process (no project configuration change is needed):

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
pio run -e default
```

Do not unplug the SD card or interrupt power to provoke failures. Read faults
are covered by the host tests; the device check verifies normal operation on the
actual SD/HAL/display stack.
