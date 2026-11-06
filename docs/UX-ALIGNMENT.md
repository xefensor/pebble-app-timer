# Timer shared UI — local 0.13.0 candidate

The shared reference remains in the local sibling `xoce` checkout at
`src/pages/docs/design/pebble-ui-system.md`, also available through
[the local preview](http://127.0.0.1:4321/docs/design/pebble-ui-system/).
The website and this candidate have not been published by this work.

## Implemented in this candidate

- Neutral native light/dark surfaces with the existing blue hourglass,
  Fredoka numbers, Roboto interface text and 166/34px composition.
- Strong inverted selection in menus and the duration editor; title-left and
  local-time-right headers remain visible. Essential critical instructions
  use the 18px interface role.
- Delete identifies saved duration and, when needed, the position among current
  timers. Keep is the initial choice; cancellation retains Options selection.
- Appearance settings live on the phone by Xef's decision. The page has one
  Watch appearance Light/Dark selector plus help, with EN/CS interface text.
  It uses the shared phone layout, semantic neutral palette and blue accent.
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

The 0.13.0 [validation record](qa/0.13.0/validation.json) links the exact native,
font and packaged settings evidence. The checks include:

- Portable owner lifecycle/storage/expiry, timer slots, native alarm feedback,
  conserved hourglass geometry, clock bounds and visible UI-work decisions.
- Appearance preference write failure/readback/unchanged-write suppression,
  source-derived disjoint storage namespaces, actual Glance/preference shared-store
  writes and cold reopen, and separate Android DOM/native settings stores.
- Exact optimized plugin/PKJS, append-only keys, source/font/resources/config
  identity, artifact checks and release-auditor regression tests.
- Actual packaged config HTML and handlers in Chromium: confirmed Save stays
  open; live/closed writers; close/reopen and browser restart; lost/mismatched
  acknowledgements; explicit readback; load errors and repair; EN/CS; 320/360px
  light/dark layouts, short viewports and doubled interface text.
- Actual native Emery captures of normal and critical states in both palettes,
  persisted appearance on reopening without phone JavaScript, unchanged timer
  identity/deadlines through appearance changes, static pause and safe Delete.

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
