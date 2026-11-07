# Timer shared UI — local 0.14.1 review branch

The shared reference remains in the local sibling `xoce` checkout at
`src/pages/docs/design/pebble-ui-system.md`, also available through
[the local preview](http://127.0.0.1:4321/docs/design/pebble-ui-system/).
The canonical standard remains the baseline. This branch is an alternative for
Xef to compare, documented in [REDESIGN.md](REDESIGN.md). No website or
release has been published by this work.

## User correction in 0.14.1

Readability takes priority over imitating Pebble's smaller utility text. The clock
is now Gothic 28 Bold; utility/cell/status text uses bold 24. Native ink and padding
are checked in every captured screen, with a shared optical header centre and
larger warning line spacing. Vertical choices move rigidly for 120ms; they no
longer stretch for 350ms. Scroll changes/skips/repeats snap, and Select stays
immediate. See [REDESIGN](REDESIGN.md) and current [validation](qa/0.14.1/validation.json).

The remaining 0.14.0 record below is historical; its elastic vertical-menu timings
were explicitly rejected. Editor/hourglass feedback remains meaningful and bounded.

## Implemented in the previous candidate

- Neutral native light/dark surfaces with the existing blue hourglass,
  Fredoka numbers, stock Gothic utility/menu text and 166/34px composition.
  Larger Gothic 24 Bold menu/confirmation roles fit actual native ink; longer
  list details retain the saved duration with a compact state word. Unused
  Roboto bitmaps are no longer packaged; original source/notices are preserved.
- Strong inverted selection in menus and the duration editor; title-left and
  local-time-right headers remain visible. Essential critical instructions
  use the 18px interface role.
- Delete identifies saved duration and, when needed, the position among current
  timers. Keep is the initial choice; cancellation retains Options selection.
- Native selection stretches/recoveries use 100+250ms; editor value feedback
  uses 107+214ms and field advance uses 107+179ms. Logical targets and owner
  operations change immediately. Select settles pending motion; held repeat
  omits flourish. Focus loss/screen changes cancel it without replay on reveal.
  Physical action icons move 5px right on press and return in 144ms.
  Countdown digits remain fixed; the existing glass-only 300ms rotation remains.
- Appearance settings live on the phone by Xef's decision. The page has one
  Watch appearance Light/Dark selector plus help, with EN/CS interface text.
  It uses the shared phone title/intro and short blue identity line, neutral
  section headings, 16px body/14px hints, 16px insets and 24px section gaps.
  Controls remain at least 44px, Save is 48px with theme-aware accent contrast,
  and status stays left aligned. Small user-triggered control/disclosure
  transitions follow reduced-motion preference; Saved status never animates.
- Save remains open with Saving… → ✓ Saved. Another edit shows Unsaved changes.
  A failed/uncertain write keeps the draft. Reload saved is a readback, not an
  attempt to undo a write. There is no routine keyboard focus or scroll reset.
- The active phone runtime owns writes to its separate Android storage view.
  A closed config-plugin save rechecks for a newly started runtime before
  confirming the exact settings. Save confirmation means phone persistence.
- Native appearance is verified and stored under independent key 2105 (2104 remains AppGlance) before
  a new palette is applied. Startup uses that saved choice while awaiting the
  current phone setting; no theme-setting row is added to the watch menus.

Timer creation, Pause/Resume, explicit Reset-to-Ready, eight independent slots,
five-minute paused-activity visibility, acknowledgment and native alarms are
preserved. The phone settings have no timer editor or action proxy. Dashboard
Timer controls remain deferred. The native alarm screen keeps its deliberate
high-contrast dark treatment in either selected appearance.

## Validation

The 0.14.0 [validation record](qa/0.14.0/validation.json) links exact artifacts,
26 new native screenshots and sampled motion/input evidence. Checks include:

- Portable owner/model/storage/expiry, eight slots, native feedback, conserved
  hourglass geometry, fixed digits and finite elastic motion (including reverse
  presses and field bounds).
- Exact optimized plugin/PKJS, source/config/Fredoka outline identity, retained
  notices, zero unused bitmap resources, append-only keys and release auditors.
- Actual distribution-native Emery appearance/storage QA: both palettes,
  persisted appearance on reopen, unchanged deadlines/identity, static pause,
  safe Delete, critical warnings and actual Glance/preference coexistence.
- Actual native motion/input QA: Select during travel activates the correct
  slot; held entry cannot leak editor repeats; editor cancellation preserves
  owner state; raw action press/release settles; stock OS cover cancels feedback
  and return remains still; cold reopen works.
- Independent exact-native Up/Select/Down pixel checks prove the same icon shape
  moves exactly 5px right during press and returns to the original idle pixels.
  Fixed per-button raw handlers avoid implicit recognizer identity; each release
  affects only its own button. See [the action record](qa/0.14.0/native-actions/report.json).

Fresh Chromium validation is blocked by sandbox startup restrictions found in
this session. Packaged-handler tests pass, but do not verify current browser
pixels. The offline phone layout uses the exact generated page with an explicit
mock/no-storage banner. The [0.13.0 record](qa/0.13.0/validation.json), including
its browser screenshots and close/restart simulations, remains historical;
it is never presented as a new capture.

These are browser/SDK simulations and native emulator checks. Real phone
WebView/storage/permission behavior, physical-watch readability/button feel,
vibration and battery measurements still require device testing. No release
was published by these checks.

## Maintaining the shared design

Keep common rules in the canonical website standard. This repository owns its
actual settings controller, native rendering, separate appearance preference,
resources and validation. Preserve all published tags/assets and original
source identity. The reviewed pre-alignment baseline was Timer 0.12.0 at
`a21cc63877c9ba3fd4f14ffb671df6768a275486`; its evidence stays historical.
