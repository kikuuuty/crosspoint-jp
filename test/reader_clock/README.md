# Reader clock checks

Run `python test/run_reader_clock_test.py` with GCC or Clang available.
This tests the production clock's HH:MM formatting, unset-time placeholder,
minute boundaries, default/manual refresh mode, duplicate-request suppression,
page-render baseline changes, midnight and forwards/backwards time corrections.
It also compiles the production NTP persistence service with fake SNTP/RTC/HAL
implementations: non-consuming completion reads, late replies, deferred and
render-locked I2C writes, retry throttling, pre-sleep flushing, X4 without RTC,
and internal/RTC boot-source selection (including the observed five-hour rollback).
The production header layout is also checked on X3/X4, all four orientations
and all three themes. Home/reader share typography and edge padding, not button-
hint reservations. The reader stays at the logical top with a compact header,
even when Home has a 56px inverted/landscape hint band. Clock and percentage
share font/vertical alignment and the clock stays screen-centered.

## Device checks (EPUB and TXT)

- Open a book: clock centered at the top, battery at the top right, neither
  overlapping text. The bottom bar keeps its page count/title/progress but no
  battery or battery-sized gap. Hide all bottom items: no empty bar remains.
- Repeat in Portrait, Inverted, Landscape CW/CCW, light/dark mode, all themes,
  horizontal/vertical EPUB, smallest/largest margins, and with antialiasing on/off.
  Existing layout/index caches should rebuild automatically when needed.
- Compare Home and reader: normal Portrait battery position and size must
  match. Home avoids its physical button hints; the reader has no hint band
  and must not reserve one. Inverted reader clock/battery stay close to the
  logical top (no empty 56px band); landscape reader battery stays close to the
  logical right edge. Clock text aligns with the battery percentage. Page count/
  title/progress remain at the logical bottom unchanged.
- Set Hide Battery % to Never / In Reader / Always: the top-right icon remains;
  the reader shows the percentage only for Never. Test USB plug/unplug as well.
- Existing settings without `readerClockUpdate` use On Page Turn. Leave a page
  open across a minute boundary: no clock-driven refresh. Turn a page: current
  time appears. Unsynchronized time is shown as `--:--`.
- Settings -> Reader -> Clock Update -> Every Minute: leave a page open over
  multiple minute boundaries. Time updates once per minute without moving the
  reading position, modifying progress.bin, or losing grayscale/antialiasing.
  A full-page refresh is intentional: not all supported panels can safely
  update just the header after grayscale rendering.
- Change the option back, restart, and check persistence. Menus/settings must
  not be overwritten by clock refreshes while the reader is underneath them.
- Set auto-sleep to one minute and make no input: the clock must not keep the
  device awake. Check both modes, plus manual sleep during a clock refresh.
- Monitor free heap while leaving Every Minute on: no progressive loss.
- After Wi-Fi connection, verify `NTP time synced (JST)` and
  `NTP synced; RTC saved: ... UTC` (X3). The saved UTC hour is nine hours behind
  JST. If the initial wait expires, a later reply must still produce the RTC
  save log. Wake after sleeping: `Keeping ESP-IDF time` must preserve the
  correct internal clock even if the external RTC is stale. A cold boot with
  invalid internal time should still restore from DS3231.

XTC/XTCH files contain pre-rendered full-screen pages (including their own
status bars) and are unchanged by this feature.
