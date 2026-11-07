# Timer 0.14.2 — readable whole compositions

This local revision stays on `design/pebble-native-redesign`, after the
0.14.1 review (`fad44ed`). Main and all previous versioned artifacts remain
unchanged. The candidate has not been published.

Xef rejected making the header clock larger while leaving the rest tiny and
unbalanced. Each screen now uses one deliberate hierarchy: Roboto Condensed
Bold26 for the header and clock,24 for primary labels and22 for secondary text,
help and status. Actual visible caps are about19,17–18 and16px respectively;
descenders and accents get their measured bounds. Fredoka countdowns remain
visually dominant. Font point names alone do not establish readability.

The header spans the full200px. The physical-button rail retains its hardware
anchors below it; full-width footers give complete help/status text space.
Option/confirmation labels, editor fields and units use actual fitted ink
centering. No text is anchored to an arbitrary point-size baseline. Tight
clock/unit strings are explicitly advance-anchored and painted without SDK
word reflow after fitting to their logical columns.

The list shows two padded80px rows with an8px gap, using a single left-aligned
column for remaining value, state and saved duration. A critical warning uses
one row above its complete two-line message. An oldest-Ready replacement hint
also gets its own space. The shared renderer/tick window keeps every selected
timer visible, forward and backward. There are still eight saved timers.
Long running durations separate the hours (`23 h`) from a large MM:SS line;
list rows keep the full remaining value and saved duration. Editor digits,
HRS/MIN/SEC, Back help and Select help have separate bands. Critical copy uses
complete concise equivalents (`Not saved`, `Keep Timer open`, `Cannot undo`)
and preserves both safe Delete choices and Done/Snooze alarm instructions.

Three SDK bitmap resources contain322 glyphs each. `extended:true` prevents
the SDK's default256-glyph limit from truncating Czech letters. The compiled
PFOs verify all printable Latin through017F plus apostrophe/ellipsis, and their
exact ink tables are linked into the native app. Original font sources, SIL
notices and Fredoka outlines are unchanged. The build derives these tables
from its exact SDK output and relinks when needed. This coverage is not a
claim about arbitrary Unicode or localized watch strings; current watch copy
remains built-in English/numeric text.

The fixed **rigid120ms ease-out** selection response remains: no stretch,
overshoot or deferred model update. Scroll-window changes, skipped rows,
repeats and Select settle immediately. The independent editor feedback,
300ms hourglass rotation and5px physical-icon press/144ms release stay intact.
Native deadlines, Wakeups, storage formats, transport identity, phone settings
and the safe Keep-first Delete behavior are unchanged.

Native review links:

- [List](qa/0.14.2/native-ui-system/list-light.png),
  [editor](qa/0.14.2/native-ui-system/new-editor-dark.png),
  [options](qa/0.14.2/native-ui-system/options-dark.png),
  [running](qa/0.14.2/native-ui-system/running-light.png),
  [long countdown](qa/0.14.2/native/legibility-235959-running-detail.png) and
  [long list rows](qa/0.14.2/native/legibility-235959-running-list.png).
- [Critical Delete](qa/0.14.2/native-ui-system/critical-delete-light.png) and
  [critical alarm](qa/0.14.2/native/storage-not-saved-native-alarm-actions-visible.png).
- [Eight-position viewport QA](qa/0.14.2/native-list/report.json):34 captures,
  all8 normal/critical positions in both directions, boundary/Back restore,
  unchanged owner identities/deadlines and read-only critical-bank checksums.
- [Appearance/storage QA](qa/0.14.2/native-ui-system/report.json):26 captures;
  [motion/input QA](qa/0.14.2/native-motion/report.json):47 frames and actual
  [quick option-switch clip](qa/0.14.2/native-motion/quick-options.gif);
  [physical icon QA](qa/0.14.2/native-actions/report.json): all3 icons shift5px
  and return to identical idle pixels.
- [Native workflows](qa/0.14.2/native/ux.json): settings navigation/cancel,
  remaining edits versus saved repeat, Reset/Ready/restart, eight-Ready reuse,
  full capacity, actual23:59:59 editor entry, invalid seconds and exhausted-bank
  native completion/failure behavior.
- [Ink checks](qa/0.14.2/centering.json):27 roles using unmodified Emery PNGs;
  [font audit](qa/0.14.2/font-audit.json) and
  [validation](qa/0.14.2/validation.json) bind checks to the final exact PBW.

The phone page still has one appearance selector, disclosed help and the same
confirmed Save/draft/uncertain-readback controller. Its
[offline preview](qa/0.14.2/phone-preview.html) uses the generated page with a
labelled mock bridge. Browser startup remains blocked by this sandbox; no
fresh browser/WebView or physical-watch test is claimed. Native screenshots
and sampled clips prove emulator pixels and behavior, not hardware smoothness,
readability, battery consumption or vibration.

Review artifacts are `dist/Timer-0.14.2.pbw` and `releases/Timer-0.14.2*`.
The canonical shared standard remains the baseline until Xef chooses the design.
