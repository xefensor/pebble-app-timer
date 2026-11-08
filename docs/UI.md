# Timer: current UI

The current release is 0.14.5. Earlier downloads and frozen evidence retain
their original versions and interfaces.

Watch timer controls stay on the watch. The published phone page has one Light/Dark selector. The development candidate adds independent Text Size and Appearance controls, stays open after Save, and distinguishes saved phone choices from watch-confirmed application. The 34 px contrasting action rail spans every screen row; headers and footers stay inside 166 px. All development watch screens use built-in Pebble Gothic roles and LECO number fonts; no custom fonts are bundled in the candidate. The development Timer checkpoint uses native MenuLayer selection and measured rows, with all eight slots reachable. Long elapsed/counter values and warnings stay complete.

## Shared UI

The current release uses the pinned `pebble-xoce-ui` package for the
32 px header, 8 px common text inset, measured-ink centering, canonical icons,
full-height action rail and physical-button feedback. On Emery the three rail
centers are y=54/114/174 with one 18 px icon grid. A held button moves its icon
5 px toward the content; release settles over a bounded 144 ms quadratic
EaseOut curve at a 33 ms cadence, matching Pebble's Emery action bar.
Options and confirmation use measured native MenuLayer rows and complete scrollable context. Confirmation always enters on the safe "Keep timer" choice.
An ordinary one-timer list uses the same first-row anchor as a longer list;
adding another timer does not recenter the first row.

Timer still owns its fonts, layers, input handlers, animation timers and state.
The library installs no input callbacks. Native MenuLayer owns list motion; editor
feedback, native numeric fitting and the animated hourglass remain
Timer-specific. Shared drawing cannot delay an owner operation or change alarm
handling. Earlier published downloads remain unchanged.

The editor follows Pebble's own Timer
[SelectionLayer](https://github.com/coredevices/pebble-apps/blob/f8ce1857422b048dcfa8df18a69d866d5b9b06fb/timer/src/selection_layer.c):
field changes slide the whole box for 107 ms with EaseIn, add only the 3 px
field gap, then settle that gap for 179 ms with EaseOut. A value edit moves the
previous glyph toward Up/Down for 107 ms, then shows the new glyph while a 6 px
extension on that edge settles over 214 ms. Logical edits remain immediate;
repeating input and Select settle this visual feedback before consuming current
state. Options and confirmation now use native MenuLayer motion; the bounded
hourglass illustration remains Timer's own animation.

Native state, verified banks, deadlines and Wakeups are authoritative. Preserve eight slots, saved repeat duration, acknowledgement and migration compatibility. A transport ACK is not a saved-state receipt. Paused sharing expires after five minutes without renewing its observation. Zero does not establish completion. Keep one AppMessage queue. Appearance keys are separate from timer/Glance keys; appearance never changes a timer. Back, critical warnings, safe Keep-first Delete and immediate owner operations remain available.

## Development navigation

The production list is a `MenuLayer` below the existing first-row anchor at y=50.
A single timer and a multi-timer list use that same anchor. Small/Medium/Large
resolve the per-app override or actual launch-time `preferred_content_size()` (Emery SDK values
Medium/Large/ExtraLarge). Font roles, UTF-8 wrapping, value fitting and row
measurement share one layout. Stock role fonts keep their actual bitmap sizes;
state and saved duration share one line only when those native glyphs fit. Critical warnings reserve their full bottom panel;
the menu scrolls every selected timer completely into the remaining viewport.

The app's click provider owns taps, 150 ms repeats and raw rail feedback exactly
once, stepping through the exported animated MenuLayer APIs. Colors come from
`menu_cell_layer_is_highlighted()` during SDK animation. The New/Replace position
above the first timer keeps its existing Up/Down/Select behavior. All eight slots,
safe Keep-first deletion and immediate pause/resume remain available.

List, Details, Add/Edit, Options, confirmation and Alarm each have a real Window. Normal navigation uses animated stack push/pop. Both surfaces
remain alive during transitions; menu motion settles on cover. The selected
production timer slot, rather than a previous row number, restores focus after
Back and after state-driven sorting. The windows/layers/fonts are app-owned and
are released at app exit. Alarm attention immediately replaces ordinary navigation without waiting for decorative motion. Add/Edit returns to its actual originating window. Native numeric fields, units and help share measured editor geometry, moving optional artwork before reducing available control space. Options and confirmation use their own native menus; their complete context and warnings remain scrollable at Large.

The left header label fits/truncates after reserving the measured full local clock.
The existing full-height 34 px XOCE rail, canonical icons, button positions and
5 px held displacement remain unchanged. No native rail replacement is proposed
at this checkpoint. Preference overrides and watch-confirmed delivery are documented in the
[preferences checkpoint](ui-preferences-checkpoint.md). Touch opt-in remains outside
this navigation checkpoint.

Build with `npm run build`, then `python3 scripts/build-fixtures.py` and
`python3 scripts/build-native-reference.py`. Run
`python3 scripts/emulator-timer-checkpoint.py --baseline /path/to/starting.pbw`.
The baseline argument is optional. Evidence is disposable under
`build/qa/timer-checkpoint/<PBW hash>/<run>/`, including raw PNGs, sampled GIFs,
comparison boards and a hash-bound JSON report. The reference harness uses SDK
stock menu cells and an XOCE center-focus trial; it is not the full stock Timer.
Run `python3 scripts/emulator-timer-navigation.py` for Add/Edit, Options, safe confirmation, saved repeat duration and alarm interruption. Its optional `--pbw` and `--label` identify separate locale or diagnostic fixtures. Separate locale-lookup and heap-log fixtures must never be described as final
candidate bytes. The SDK emulator's content-size command changes the OS preference;
the read-only probe confirms it through `preferred_content_size()`. Its SDK shell
omits Settings and stock Timer, so their physical checks stay pending.

## Superseded custom-type checkpoint evidence — 2026-11-08

This record belongs to the first custom-type candidate. Its interaction and
preference checks passed, but its font scales did not match Pebble's type roles.
The native-type correction below supersedes it; the old evidence keeps its
original PBW hash.

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

## Native typography correction

The first draft's custom 22/30/36 value sizes and 18/20/22 supporting sizes were
removed from the list/details flow. Pebble compatibility uses actual built-in
fonts, not custom fonts resized to similar nominal points. The app calls only
public `fonts_get_system_font()`, `graphics_draw_text()`, text measurement and
watch-firmware APIs. It does not call PebbleOS's private theme functions.

[The pinned role data](native-type.json) records the source tables for v4.33.2
and v4.44.0/v4.44.4. On the tested 4.33.2 firmware, the exact Emery profiles are:

| Watch setting | Header | Menu value | Menu supporting text | Footer |
| --- | --- | --- | --- | --- |
| Small | Gothic 18 Bold | Gothic 24 Bold | Gothic 18 | Gothic 18 |
| Medium | Gothic 24 Bold | Gothic 24 Bold | Gothic 24 | Gothic 18 |
| Large | Gothic 28 Bold | Gothic 28 | Gothic 24 Bold | Gothic 24 |

Firmware 4.44 and newer completes the Large menu roles with Gothic 28 Bold for
the value and Gothic 28 for supporting text. The public firmware-version API
selects that table without changing SDK/minimum-firmware requirements. The
portable regression test compares every key in both profiles against the pinned
source data. Current-firmware rendering remains a separate device check.

Details uses the SDK's built-in LECO numeric fonts, starting at 42 and falling
back to the next available native size only if a complete value cannot fit.
Those bitmap fonts are never rescaled and the dominant readout is independent
of menu/body Text Size, as in stock Timer's separate numeric treatment. This is
not a claim to reproduce stock Timer's app-bundled custom 48-point font. The
clock/header, rows, footer, warnings and wrapping in this checkpoint use native
role fonts. The hourglass yields space before text is shrunk or clipped.

The independent font fixture draws directly with public SDK font constants,
without Timer's type helpers. The emulator suite compares its menu-value,
header and footer bitmap pixels against the real candidate at each OS size;
only outer whitespace, position and foreground color are normalized. No
scaling is allowed in that comparison. Legacy custom 18/20 font resources added
by the first draft were removed; existing 22/24/26 resources serve the deferred
legacy controls.

### Native-type verification — 2026-11-08

Implementation: `f448cfb8c1475224f079be97e4360b9c1d0c4772`. Local candidate PBW:
`build/candidate/Timer-0.14.5.pbw`, SHA-256
`123c5563c12bf866710de98adcf997128f8d14d4abbe0f23f8680b68b3e24bda`.
SDK 4.33.1, CLI 5.0.40 and actual Emery firmware v4.33.2 are unchanged.
`npm test`, `npm run build` and `npm run check:artifact` passed, including the
independent stock-role regression test. Resources are 52,484 bytes; static RAM
is 57,357 bytes, leaving 73,715 bytes before runtime heap allocations.

| OS size | Native measured row | Complete standard rows | Single/multi first y |
| --- | --- | --- | --- |
| Small | 50 px | 3 | 50 / 50 |
| Medium | 56 px | 3 | 50 / 50 |
| Large | 86 px | 2 | 50 / 50 |

These are the same eight paused timers at every size, with unchanged native
bank checksums during navigation. Native 24-hour and 12-hour clock measurements
use every hour/minute combination: widest header clocks are 36/44/49 px at the
three sizes. Menu values, headers and footers match the independent direct-SDK
font fixture's bitmap pixels at every size. The full-height rail remains
pixel-identical to the previous checkpoint; the existing anchor is retained.
No custom font scaling is used to obtain the row-count difference.

The exact candidate passed forward/reverse access to all eight slots, holds,
rapid reversals, Select during movement, exactly-once immediate owner actions,
identity restoration after sorting, light/dark, complete 24-hour values and
warnings, and alarm preemption. The diagnostic fixture completed 40 open/Back
cycles (80 samples): minimum 70,348 heap bytes, last 20 samples 70,348–70,384.
The exact candidate completed another 20 cycles. Czech/German renders were
inspected in their separately identified locale fixtures. No phone runtime or
physical-device verification is claimed.

Current disposable proof:

- `build/qa/timer-checkpoint/123c5563c12b/1794134390697643578/report.json`
  binds the candidate/native hashes, OS font keys, bitmap comparisons and 227
  raw captures. That directory contains `sizes-list.png`, `sizes-details.png`,
  `sizes-back.png`, `baseline-comparison.png`, native reference specimens and
  sampled motion GIFs.
- `build/qa/timer-checkpoint/123c5563c12b/1794134498546359128/anchors-report.json`
  binds the all-size single/multi anchor captures and retained replacement notice.

Pending: current-firmware rendering on a physical Time 2, Settings UI and actual
language-pack checks, paired-phone delivery/upgrade, real notification covers,
Quiet Time/offline Wakeup and physical button feel. The 4.44 font-key profile is
source-verified and regression-tested, but only the actual 4.33.2 emulator was
rendered here. Full stock Timer app reproduction, including its bundled custom
numeric font, remains separate from the verified native role-font comparison.
Touch remains disabled and unverified at this checkpoint. Native Add/Edit,
Options, confirmation and alarm typography remain later checkpoints. Nothing
was pushed or published.

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

## Native navigation verification — 2026-11-08

The complete development navigation and native type changes are recorded in
[native navigation checkpoint](native-navigation-checkpoint.md). This supersedes
only the active rendering/navigation descriptions above; older hash-bound records
below retain their original scope. Preference delivery is a separate pending stage.

## Development preferences

Text Size offers Follow Pebble, Small, Medium and Large. Manual choices use the
same native role profiles as the matching Emery setting and survive relaunch.
Appearance offers Follow Pebble, Light and Dark. This SDK has no supported system
appearance API: the page explains that limitation, retains a saved Follow choice
and uses its last explicit Light/Dark fallback. Fresh defaults follow Pebble text
size and retain Timer's light appearance; existing explicit appearance migrates.

The page shows Saved on phone / Pending on watch until an exact native application
receipt arrives. Transport success alone cannot change that status. Pending choices
survive reopening and reconcile when Timer starts or reconnects. Draft edits remain
untouched by status polling. Settings never create, pause, reset or edit a timer.
