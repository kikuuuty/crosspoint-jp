# Web file-transfer safety tests

```sh
python test/run_web_transfer_test.py
```

Requires Python 3, a C++20 compiler (`CXX` may select it) and Node.js 18+.
No npm/Python packages or device are needed. Generated sources/fixtures are
removed from the ignored `build/web_transfer/` directory after the run.

## Coverage and limits

- Compile production `FsHelpers.cpp` and the verbatim `normalizeWebPath()`
  function with a minimal Arduino String stub. Test single-component rejection,
  embedded NUL, repeated dots in valid names, terminal `.`/`..`, root traversal,
  canonical paths, and 343 component-stack reference cases.
- Source-wiring checks verify that HTTP/WS handlers use normalization and reject
  unsafe filenames before their storage calls. These do **not** emulate the
  actual Arduino HTTP/multipart/WebSocket parser or SD operations.
- Execute the complete FilesPage inline script in Node's VM with a minimal DOM.
  Test delegated move/rename/delete calls and repeated hydration, then parse the
  generated rows with Python's HTMLParser. Verify that Japanese, quotes, HTML
  entities, `<...>` names and literal `%` survive as text, never row markup or
  event-handler attributes. The mock DOM is not a full browser.

Based on upstream CrossPoint
[#3353](https://github.com/crosspoint-reader/crosspoint-reader/pull/3353).
This fork additionally needs trailing-dot-component normalization, NUL rejection,
escaping in its file-type column, and removal of an extra URL decode in the page.
The normalizer retains its relative-result API for EPUB/WebDAV callers. The
validator examines a string view; normalization owns only its returned path
string, reserving input-sized capacity rather than adding fixed stack buffers
or a component/string allocation per segment.

This change is not authentication, a complete file-access sandbox or a redesign
of hidden-file permissions. In particular, direct font uploads to
`/.crosspoint/fonts/<family>/` remain supported. No new UI libraries, retained
buffers or device-side network dependencies are added.

## Device/browser smoke test

1. Back up the test SD card. Install the firmware and enter WiFi file transfer.
2. Hard-reload the Files page (or use a private window) to avoid an old HTML cache.
3. In a dedicated test folder, upload a Japanese EPUB named `O'Brien & 本文.epub`
   and `volume..2.epub`. Check listing, download, rename, move and single/batch
   deletion. Test folder navigation with a literal `%` or `%20` in the name.
   Only use names accepted by the SD card's filesystem; some XSS test fixtures
   intentionally contain FAT-invalid characters and are host-only.
4. Verify an invalid folder/file component such as `../outside` is rejected
   without creating a file outside the test folder. Use disposable names only;
   do not probe delete/download against real system or root paths.
5. Check font uploads from the Fonts page and sleep-image uploads. Check the
   opt-in Aozora TXT-to-EPUB upload conversion still produces a readable EPUB.
6. Repeat normal file upload via the HTTP fallback if used by your browser.

All firmware builds still need the usual PlatformIO build check. These host tests
do not replace browser/device validation of network parsing and actual storage.
