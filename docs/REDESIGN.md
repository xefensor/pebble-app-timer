# Timer 0.14.1 — readable redesign revision

This local revision stays on `design/pebble-native-redesign`, after the original
0.14.0 trial (`4d98c82`). Main and all older artifacts remain unchanged. It is
an alternative under review, not a published release.

Xef rejected the smaller top clock/date, high-looking icons and slow, distorted
up/down choice animation. Readability takes priority over imitating Pebble's
smaller utility type. Timer has no inline icon/text pairs: its action icons
follow the physical button positions. Those anchors and the 5px press/144ms
release feedback remain; standalone illustrations retain clear text spacing.

The top clock now uses Gothic 28 Bold with **18px of actual visible ink**.
Header titles and all secondary/cell/status text use bold 24 instead of the
small 18 or thin regular face. The measured title and clock share the same
15.5px vertical ink centre. Header spacing uses actual measured clock width;
long context labels shorten rather than silently shrink.

List countdown/detail groups move up 3px for short digits and 5px for long ones,
leaving balanced 5/6px and 7/8px ink insets. Option and Keep/Delete labels account
for descenders instead of sharing an arbitrary point-size baseline. Editor
HRS/MIN/SEC, selected digits and help have separate clear bands. Larger critical
warnings have two readable lines with padding on both edges. These are verified
built-in English/numeric strings, not a claim about arbitrary Unicode glyphs.

## Selection bug

The old menu highlight stretched its leading edge for 100ms and recovered its
trailing edge over another 250ms, temporarily covering both rows. A single
choice therefore remained distorted for 350ms. That was the slow/weird behavior.

Vertical list/options/confirmation feedback now moves a **rigid surface for
120ms**, with ease-out and no overshoot or stretch. Labels stay anchored. Scroll
window changes, skipped rows and held repeats snap to the target. Select settles
before activating the immediate logical target. No owner operation waits for
motion. The meaningful 300ms glass rotation and editor field/value feedback
remain independent; countdown digits do not bounce.

Compare actual emulator frames of
[the old 0.14.0 option switch](qa/0.14.1/previous-option-motion/previous-0.14.0-options-switch.gif)
with [the fixed option switch](qa/0.14.1/native-motion/quick-options.gif) and
[the quick list step](qa/0.14.1/native-motion/quick-list.gif).
The GIF transitions use host capture intervals, with explicit before/after dwell;
screenshot RPCs do not measure physical-watch smoothness or frame rate.

## Review and checks

- Native [list](qa/0.14.1/native-ui-system/list-light.png),
  [editor](qa/0.14.1/native-ui-system/new-editor-dark.png),
  [options](qa/0.14.1/native-ui-system/options-dark.png),
  [safe Delete](qa/0.14.1/native-ui-system/delete-light-safe-default.png) and
  [critical warning](qa/0.14.1/native-ui-system/critical-delete-light.png).
- [Ink measurements](qa/0.14.1/centering.json) check 22 roles against real native
  PNG pixels: complete list text groups, option/confirmation labels, selected
  digits, all three editor units, warning bands and ordinary/descender header roles.
- [Appearance/storage QA](qa/0.14.1/native-ui-system/report.json): 26 captures;
  [motion/input QA](qa/0.14.1/native-motion/report.json): 47 captures, including
  fixed-height option frames, rapid reverse, immediate safe Delete, held entry,
  preserved owner state, covered views and cold reopen;
  [physical icon QA](qa/0.14.1/native-actions/report.json): all three independently
  move exactly 5px and return to identical idle pixels.
- [Validation](qa/0.14.1/validation.json) records portable JS/C, Python audits,
  exact package/runtime/font checks and immutable previous-artifact verification.

The phone page still has one appearance selector and disclosed help. Source
inspection found symmetric field/button padding and natural wrapping. Help now
has explicit 10px top/bottom padding, so its 44px target does not rely on newer
block `align-content` support. No Save, storage, draft or active-writer logic
changed. [The local phone preview](qa/0.14.1/phone-preview.html) uses the exact
page with a labelled mock bridge; it cannot write to a phone or watch.
Fresh browser/phone WebView and physical-watch checks remain unperformed because
browser startup is sandbox-blocked. No battery or vibration measurement is claimed.

The current local PBW is `dist/Timer-0.14.1.pbw`; versioned review artifacts are
also stored under `releases/Timer-0.14.1*` with matching checksums. The canonical
shared standard remains the baseline until Xef chooses the alternative.
