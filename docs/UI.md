# Timer: current UI

The accepted UI is merged into main; implementation version 0.14.4 remains
unpublished. Published downloads retain their original versions and interfaces.

Watch timer controls stay on the watch. The phone has one Light/Dark appearance selector and confirmed Save-and-stay. The 34 px contrasting action rail spans every screen row; headers and footers stay inside 166 px. Header title and clock 26, primary utility 24 and secondary 22 use Roboto Condensed Bold; dominant values use Fredoka. Keep two 80 px list rows or one above a complete critical warning, all eight selections visible and rigid 120 ms choices. Long elapsed/counter values and warnings stay complete.

## Shared UI development branch

The current development branch uses the pinned `pebble-xoce-ui` package for the
32 px header, 8 px common text inset, measured-ink centering, canonical icons,
full-height action rail and physical-button feedback. On Emery the three rail
centers are y=54/114/174 with one 18 px icon grid. A held button moves its icon
5 px toward the content; release settles over a bounded 144 ms cubic curve.
Confirmation choices are full-column rectangles, 30 px high on a 36 px stride,
with left-aligned 24-point labels and an initial safe "Keep timer" choice.

Timer still owns its fonts, layers, input handlers, animation timers and state.
The library installs no input callbacks. The rigid 120 ms vertical selection,
editor feedback, custom Fredoka digit rendering and animated hourglass remain
Timer-specific. Shared drawing cannot delay an owner operation or change alarm
handling. This branch does not change the app version or published downloads.

Native state, verified banks, deadlines and Wakeups are authoritative. Preserve eight slots, saved repeat duration, acknowledgement and migration compatibility. A transport ACK is not a saved-state receipt. Paused sharing expires after five minutes without renewing its observation. Zero does not establish completion. Keep one AppMessage queue. Appearance keys are separate from timer/Glance keys; appearance never changes a timer. Back, critical warnings, safe Keep-first Delete and immediate owner operations remain available.

## Native/phone verification

Inspect actual affected compositions, full text, glyph/icon alignment, edges,
light/dark and relevant critical states. Keep the actual packaged native payload
and report hashes bound together. Preserve the accepted complete UI rather than
reintroducing a historical trial. Phone palette is independent of watch appearance.
Read [maintenance contracts](MAINTENANCE.md) before changing persistence, routing
or source lifecycle. Current build/QA commands are in [CONTRIBUTING](../CONTRIBUTING.md).

## Frozen evidence

[Version 0.14.4 evidence](qa/0.14.4/validation.json) belongs to its original
frozen build at `559f2cac638294fcdefed0fc31cd6bdc895789e2`. Routine development uses build/candidate and build/qa;
that output does not overwrite these records. Physical phone/watch checks and
fresh browser checks remain explicitly separate.
