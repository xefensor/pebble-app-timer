# Timer 0.14.0 — Pebble-native redesign for review

This is a local alternative on `design/pebble-native-redesign`, based on
`6a2c6f074623779532104a066439c85264827d8d`. It is ready for Xef to compare, not
an accepted change to the canonical shared standard or a published release.

The main timer remains recognizably Timer: Fredoka countdown, blue hourglass,
proportional sand and clear physical actions. Utility text now uses Pebble's
Gothic family, with stronger menu and confirmation roles. Larger native menu
ink fits through shorter labels such as “Edit remaining”; Delete still names
its target and starts safely on Keep. Long list details retain saved duration
with a compact state word when needed. The persistent local clock, 34px action
rail, critical instructions and explicit Reset/Start flow remain.

Selection stretches toward the new row and settles, following Pebble's
100+250ms menu pattern. Editor values receive a small cell/value response
(107+214ms); advancing fields takes 107+179ms. The button icon moves 5px into
the rail during a physical press and returns over 144ms. These public SDK
equivalents keep logical targets, field values and owner operations immediate.
Select, repeat input, covering windows and failed timer allocation cannot leave
delayed actions behind. Countdown digits never bounce; the meaningful
glass-only rotation remains 300ms.

The phone page keeps one Watch appearance choice plus help. A short blue
identity line, clearer spacing, neutral headings and a Save button matched to
each phone palette align it with the other apps. Only controls and user-opened
help receive short feedback, respecting reduced motion. Save, drafts, unknown
writes, active-runtime ownership and reopening retain their existing contracts;
appearance never edits timer state or alarms.

## Fresh native previews

These are actual Emery QEMU captures of the new distribution-native payload,
with a test AppMessage bridge. They are not physical-watch evidence.

| Screen | Light | Dark |
| --- | --- | --- |
| Timers | [List](qa/0.14.0/native-ui-system/list-light.png) | [List](qa/0.14.0/native-ui-system/list-dark.png) |
| Countdown | [Running](qa/0.14.0/native-ui-system/running-light.png) | [Running](qa/0.14.0/native-ui-system/running-dark.png) |
| Paused | [Paused](qa/0.14.0/native-ui-system/paused-light.png) | [Paused](qa/0.14.0/native-ui-system/paused-dark.png) |
| Editor | [Remaining](qa/0.14.0/native-ui-system/remaining-editor-light.png) | [Remaining](qa/0.14.0/native-ui-system/remaining-editor-dark.png) |
| Delete | [Safe default](qa/0.14.0/native-ui-system/delete-light-safe-default.png) | [Safe default](qa/0.14.0/native-ui-system/delete-dark-safe-default.png) |

Actual sampled native frames: [elastic list](qa/0.14.0/native-motion/elastic-list.gif),
[editor value](qa/0.14.0/native-motion/editor-value-bump.gif),
[field advance](qa/0.14.0/native-motion/editor-field-advance.gif).
GIF dwell times aid comparison; screenshot RPCs do not measure real frame rate.

[Open the offline phone layout](qa/0.14.0/phone-preview.html). It uses the exact
generated config HTML with a labelled mock bridge and no phone/watch storage.
Fresh Chromium validation is unavailable in the current sandbox; automated
packaged-handler tests cover storage contracts, not current browser pixels.
Earlier 0.13 screenshots remain historical and are not new evidence.

## Verification

See [the machine-readable record](qa/0.14.0/validation.json),
[native appearance/storage results](qa/0.14.0/native-ui-system/report.json) and
[motion/input results](qa/0.14.0/native-motion/report.json).
Each physical action is also measured independently in
[the exact-build action check](qa/0.14.0/native-actions/report.json): Up,
Select and Down retain their icon shape, move exactly 5px right while held and
return to the identical idle pixels after release. Fixed per-button handlers
make this independent of raw recognizer identity.
Portable tests cover finite geometry, reverse presses, fixed digits, owner
state, storage, alarms and sharing. Native QA exercises safe Delete, critical
warnings, both appearances, persisted reopen, real Glance/preference
coexistence, Select during travel, held entry input, editor cancellation,
press/release icons, covering windows and motion-free paused screens.

No real phone, physical watch or battery measurement was performed. The local
PBW is `dist/Timer-0.14.0.pbw`; older versioned artifacts remain intact.
