# Timer: current UI

The current release is 0.14.5. Earlier downloads and frozen evidence retain
their original versions and interfaces.

Watch timer controls stay on the watch. The phone has one Light/Dark appearance selector and confirmed Save-and-stay. The 34 px contrasting action rail spans every screen row; headers and footers stay inside 166 px. Roboto Condensed Bold supplies utility text and the size-aware header; dominant values use Fredoka. The development Timer checkpoint uses native MenuLayer selection and measured rows, with all eight slots reachable. Long elapsed/counter values and warnings stay complete.

## Shared UI

The current release uses the pinned `pebble-xoce-ui` package for the
32 px header, 8 px common text inset, measured-ink centering, canonical icons,
full-height action rail and physical-button feedback. On Emery the three rail
centers are y=54/114/174 with one 18 px icon grid. A held button moves its icon
5 px toward the content; release settles over a bounded 144 ms quadratic
EaseOut curve at a 33 ms cadence, matching Pebble's Emery action bar.
Confirmation choices are full-column rectangles, 30 px high on a 36 px stride,
with left-aligned 24-point labels and an initial safe "Keep timer" choice.
An ordinary one-timer list uses the same first-row anchor as a longer list;
adding another timer does not recenter the first row.

Timer still owns its fonts, layers, input handlers, animation timers and state.
The library installs no input callbacks. Native MenuLayer owns list motion; editor
feedback, custom Fredoka digit rendering and the animated hourglass remain
Timer-specific. Shared drawing cannot delay an owner operation or change alarm
handling. Earlier published downloads remain unchanged.

The editor follows Pebble's own Timer
[SelectionLayer](https://github.com/coredevices/pebble-apps/blob/f8ce1857422b048dcfa8df18a69d866d5b9b06fb/timer/src/selection_layer.c):
field changes slide the whole box for 107 ms with EaseIn, add only the 3 px
field gap, then settle that gap for 179 ms with EaseOut. A value edit moves the
previous glyph toward Up/Down for 107 ms, then shows the new glyph while a 6 px
extension on that edge settles over 214 ms. Logical edits remain immediate;
repeating input and Select settle this visual feedback before consuming current
state. Options and confirmation retain their existing 120 ms feedback until their
own checkpoint; the bounded hourglass illustration remains Timer's own animation.

Native state, verified banks, deadlines and Wakeups are authoritative. Preserve eight slots, saved repeat duration, acknowledgement and migration compatibility. A transport ACK is not a saved-state receipt. Paused sharing expires after five minutes without renewing its observation. Zero does not establish completion. Keep one AppMessage queue. Appearance keys are separate from timer/Glance keys; appearance never changes a timer. Back, critical warnings, safe Keep-first Delete and immediate owner operations remain available.

## Development checkpoint: list → details → Back

The production list is a `MenuLayer` below the existing first-row anchor at y=50.
A single timer and a multi-timer list use that same anchor. Small/Medium/Large
read the actual launch-time `preferred_content_size()` (Emery SDK values
Medium/Large/ExtraLarge). Font roles, UTF-8 wrapping, value fitting and row
measurement share one layout. Eight standard paused timers fit three complete
Small rows and two Large rows. Critical warnings reserve their full bottom panel;
the menu scrolls every selected timer completely into the remaining viewport.

The app's click provider owns taps, 150 ms repeats and raw rail feedback exactly
once, stepping through the exported animated MenuLayer APIs. Colors come from
`menu_cell_layer_is_highlighted()` during SDK animation. The New/Replace position
above the first timer keeps its existing Up/Down/Select behavior. All eight slots,
safe Keep-first deletion and immediate pause/resume remain available.

Details uses a second real Window with animated stack push/pop. Both surfaces
remain alive during transitions; menu motion settles on cover. The selected
production timer slot, rather than a previous row number, restores focus after
Back and after state-driven sorting. The windows/layers/fonts are app-owned and
are released at app exit. Alarm attention preempts the ordinary content window.
Add/Edit, Options and confirmation still use their existing renderer and input
inside that content window; their native navigation is a later checkpoint.

The left header label fits/truncates after reserving the measured full local clock.
The existing full-height 34 px XOCE rail, canonical icons, button positions and
5 px held displacement remain unchanged. No native rail replacement is proposed
at this checkpoint. Text-size overrides, phone preference delivery and touch
opt-in are also deferred to the later checkpoints.

Build with `npm run build`, then `python3 scripts/build-fixtures.py` and
`python3 scripts/build-native-reference.py`. Run
`python3 scripts/emulator-timer-checkpoint.py --baseline /path/to/starting.pbw`.
The baseline argument is optional. Evidence is disposable under
`build/qa/timer-checkpoint/<PBW hash>/<run>/`, including raw PNGs, sampled GIFs,
comparison boards and a hash-bound JSON report. The reference harness uses SDK
stock menu cells and an XOCE center-focus trial; it is not the full stock Timer.
Separate locale-lookup and heap-log fixtures must never be described as final
candidate bytes. The SDK emulator's content-size command changes the OS preference;
the read-only probe confirms it through `preferred_content_size()`. Its SDK shell
omits Settings and stock Timer, so their physical checks stay pending.

## Checkpoint verification — 2026-11-08

Starting Timer main: `23028676172e2dab0d0057a11e411673f13995be` (also confirmed
through GitHub). UI implementation: `09a1740d137d3faefca0a638dd93b81a09f419e8`.
Shared UI documentation only: `80e05792e933f44bd3f2116bb85598ec3ccd77d1`;
the bundled UI 0.1.1 archive and consumer pins did not change. No push, release,
version bump or Dashboard implementation was performed.

`npm test`, `npm run build` and `npm run check:artifact` passed. Source tests ran
with Python 3.13.16 and Node 26.12.0, and passed again with available Node 22.14.0.
The exact contributor pins (Node 22.23.4 / Python 3.12.15) were not available.
The shared UI `npm test` also passed. The native build used SDK 4.33.1 / CLI
5.0.40; the running Emery firmware reports **v4.33.2**, separately verified in
its startup log. Native resources are 71,672 bytes; static RAM is 60,568 bytes
with 70,504 bytes available for the heap before runtime allocations.

Candidate: `build/candidate/Timer-0.14.5.pbw` (local candidate, same version label
as the frozen release). SHA-256:
`6176fb349290804489fc8d8dc2f1fab19caf7263c137f5b547d16863f5fdc08f`.

| OS Text Size | SDK preference | Measured standard row | Complete rows | First-row y |
| --- | --- | --- | --- | --- |
| Small | Medium (1) | 58 px | 3 | 50 |
| Medium | Large (2) | 72 px | 2 | 50 |
| Large | ExtraLarge (3) | 85 px | 2 | 50 |

The same eight paused production timers and authoritative bank checksum were
retained across the size matrix and ordinary navigation. All eight slots were
reached in both directions and restored after Details/Back. Single- and
multi-timer y=50 were separately checked at all three sizes. Headers reserve
50/55/59 px for the widest 12- or 24-hour clock at 22/24/26 points. Native
center focus moved the first selected XOCE row to y=97, so the y=50 anchor stays.
The 34 px full-height rail is pixel-identical to the starting build on the
checked list/details screens; held feedback moves 5 px toward content.

Exact candidate checks also passed for holds, fast reversals, Select during
motion, exactly one immediate owner action on slot 3, identity restoration
after state sorting, light/dark, 12-hour time, complete 24-hour values, actual
storage warnings and alarm interruption of Details. A distinct instrumented
build completed 40 open/Back cycles: 80 heap samples, minimum 67,012 bytes,
last 20 samples bounded at 67,012–67,048 bytes. The uninstrumented exact
candidate completed another 20 cycles. Czech and German row/header/footer
renders passed visual inspection in explicitly separate locale-lookup fixtures;
all 17 catalogues passed packaged glyph coverage and 935 measured text fits.

Disposable evidence, bound to the PBW and native payload hashes:

- `build/qa/timer-checkpoint/6176fb349290/1794132084657217416/report.json`:
  224 raw captures, sampled native selection/push/pop GIFs, `sizes-list.png`,
  `sizes-details.png`, `sizes-back.png`, `alignment-comparison.png` and
  `baseline-comparison.png`.
- `build/qa/timer-checkpoint/6176fb349290/1794132281463428939/anchors-report.json`:
  all-size single/multi captures and the retained Oldest ready notice.

The PNGs are real emulator frames. GIFs use fixed review cadence and before/after
dwell; `framesMs` records actual host acquisition intervals. They do not measure
physical button feel. Stock cells and center-focus references are public SDK
control fixtures on the same firmware, not the complete stock Timer app.

Pending device checks: physical Time 2 on current supported firmware, actual
Settings UI selection of Text Size, paired-phone delivery and upgrade from the
published PBW, real notification covers/focus, Quiet Time/offline Wakeup and
hardware button feel. ADB listed no attached device. The SDK shell omits Settings
and stock Timer; its official content-size protocol was used and the actual OS
preference was independently read back. Actual OS Czech/German language packs
and a full stock Timer side-by-side capture remain pending. Touch drag/tap/Back
and mixed input are pending; this checkpoint retains the existing button-only
flow and does not opt in to touch. Add/Edit, Options/confirmation and per-app
preference delivery belong to later checkpoints.

Physical check sequence (do not install the data-writing QA fixtures on a watch):

1. Install the candidate over the published Timer with saved timers; confirm
   their state and original deadline survive. Prepare eight paused timers once.
2. Choose Small, Medium and Large in watch Settings, closing and reopening Timer
   for each. Verify 3/2/2 complete standard rows, full header clock, y=50 anchor
   with one/eight timers and the full-height rail.
3. Navigate all eight slots with taps, holds and reversals. Select during motion,
   pause/resume, then Back; confirm the exact timer remains selected after sorting.
4. While in Details, test a short timer alarm and a real notification cover;
   repeat with Quiet Time and the phone disconnected to check the native Wakeup.

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
