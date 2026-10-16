# Reader navigation regression checks

Run `python test/run_reader_navigation_test.py` with GCC or Clang available.
This compiles the unchanged production `MappedInputManager.cpp` and its
`InputTransitionGuard` against a fake HAL. It checks press/release-triggered
transitions, long holds, all hardware buttons, raw power-management access,
X3/X4 and all four orientations, remapping, chained pops, and acceptance of the
next gesture.
It also compiles the production `ActivityManager.cpp` / `Activity.cpp` with
probe activities to check nested returns, result-handler ownership, held-key
suppression while activity loops continue, chained pops, and explicit Home-like
stack replacement. Rendering tasks and hardware screens are stubbed; this is
not a substitute for the following visual/device checks. The initial Boot
screen is also checked to preserve the held-Back shortcut used by setup().

## Device / simulator checks

Use an EPUB with a table of contents and footnotes where available. Repeat with
Portrait, Inverted, Landscape CW/CCW, and remapped front buttons.

| From the book menu | Action | Expected destination |
| --- | --- | --- |
| Settings | Back to the category row, then Back | Same book menu and selected item |
| Status bar | Back | Same book menu and selected item |
| Line spacing | Cancel or select | Same book menu; only select saves the value |
| QR display | Back or Confirm | Same book menu and selected item |
| Chapter / percent / footnotes | Cancel | Same book menu and selected item |
| Chapter / percent / footnotes | Select | Reader at the selected destination |
| Sync | Cancel, including Wi-Fi cancellation | Same book menu and selected item |
| Sync | Apply / upload successfully | Reader with the returned position |
| First-line indent / invert images / tilt | Toggle | Stay in the book menu |
| Screenshot | Select | Reader; capture the book, not the menu |
| Home / delete cache | Select | Home (intentional exit) |
| Book menu | Back | Reader; remain there after key release |

Hold Back longer than one second when dismissing a child. Its release must not
close the parent or trigger the reader's file browser. Hold Confirm on entry to
a child: releasing it must not toggle a setting, accept a slider, or close it.
Check nested Settings → direction/font/status-bar screens as well as Settings
opened from Home, where Back must still return to Home.
Boot while holding Back: it must still choose Home instead of resuming the book.

Change fonts, writing mode, margins and status-bar visibility, then dismiss the
menu with Back. Check that reflow happens and the reading position is retained.
Change rotation in the menu, visit Settings without changing reader orientation,
then return: the pending rotation must be retained. Change reader orientation
inside Settings: it must not be overwritten by the older menu value. Toggle
auto-turn, visit/cancel a child, and confirm its menu selection is retained.

On hardware, monitor the existing MEM log while repeating nested transitions:
free heap and maximum allocatable block must recover without progressive loss.
The menu is retained, not recreated; it holds only its existing title/items and
a non-owning reference to the reader, not another EPUB or framebuffer. The input
guard adds a boolean and does not allocate or block background activity loops.
