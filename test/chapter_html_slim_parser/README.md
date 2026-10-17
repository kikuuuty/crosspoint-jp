# Chapter XHTML parser regression tests

```sh
python test/run_chapter_html_slim_parser_test.py
```

Requires Python 3 and C/C++ compilers (GCC or Clang, configurable via `CC` and
`CXX`). No external packages or device are required. Temporary build files are
removed from the ignored `build/chapter_html_slim_parser/` directory.

The test runs **production Expat, ChapterHtmlSlimParser, CssParser, ParsedText,
TextBlock, TableRowBlock and Page** against complete XHTML documents. The EPUB
archive, SD storage, fixed font metrics, hyphenation and image decoding are test
doubles. The staged production source/header copies are unchanged; only the
archive-boundary Epub header is substituted. `-fno-access-control` allows the
test to inspect parser state/table output without adding production test hooks.
Exceptions and RTTI are disabled, as in the firmware.

## Hidden attributes (upstream #3390)

- Hidden paragraphs, headers, inline spans, containers, preformatted text, ruby,
  tables and lists suppress their descendants and resume at the visible sibling.
- Presence is what matters: `hidden=""`, `hidden="hidden"`, `hidden="false"` and
  `hidden="until-found"` are all hidden. As in upstream, CSS display:block does
  not reveal them. This does not implement browser search/reveal behavior.
- Hiding works with and without a CSS parser. Existing CSS display:none and
  ordinary/data-hidden elements are checked as controls.
- Hidden images neither extract their files nor emit alt-text placeholders.
- Hidden rows/cells/inline table content cannot bypass skipping via table
  buffering; a hidden nested table cannot close the visible outer table.
- Hidden IDs cannot overwrite a visible pending anchor. Hidden list items do
  not advance numbering or damage list nesting.
- Japanese text and ruby association survive in horizontal and vertical layout.
- Parser depth, skip, table and list state are balanced after each document.

## Image dimension retries (upstream #2434)

The dimension-probe test double can fail zero to three times. With PNG, JPEG,
horizontal and vertical layout, the tests verify:

- First probe follows extraction flush/close, without a fixed delay.
- Success on attempt one/two/three causes zero/one/two 50ms waits respectively.
- Persistent failure stops after three probes and two waits (100ms total), then
  removes the failed cache and emits the existing alt-text fallback.
- Retrying never re-extracts an image, changes its size, or drops surrounding text.
- Successful images retain their cache. Multiple immediately readable images
  introduce no waits.
- Missing decoders, unsupported formats, failed cache writes/extractions and
  placeholder/suppression settings introduce no pointless probe/retry waits.

This exercises the production retry loop and callback integration, not physical
SD-card synchronization or real JPEG/PNG decoding.

## Device smoke test

Existing chapter caches must be cleared/rebuilt (or use a new EPUB filename) to
see parser changes. In a disposable EPUB, include visible paragraphs surrounding
hidden paragraphs, headings, inline spans, images and table rows. Confirm only
the visible text/images remain, with CSS enabled and disabled. Check Japanese
text/ruby and numbered lists in both orientations. Real fonts, SD timings and
pixel rendering still require device verification; these tests do not emulate
the ESP32 hardware.

For images, rebuild a chapter with several PNG/JPEG illustrations (including one
with alt text), check that each is displayed at the same size, and navigate back
and forth in both orientations. Check display, placeholder and suppress image
settings. Rebuild the cache to measure indexing time; existing caches do not run
the changed extraction/probe path. Do not corrupt an SD card to simulate latency.
