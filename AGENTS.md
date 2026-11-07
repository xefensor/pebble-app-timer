# Timer maintainer notes

This repository owns the native Timer watchapp and its bundled phone-side
PKJS/provider plugin. It targets stock PebbleOS on Emery (Pebble Time 2).
`pebble-activities` is a pinned shared dependency, not a separate installed
service. Keep timer behaviour independent of Dynamic's presentation.

## Identity and native ownership

- Preserve app UUID `50e9ef12-a5ab-4369-a3b5-3447bfba5151`, native persistence
  keys/model compatibility, plugin UUID and shared storage namespace when
  updating Activity Timer installations.
- The native timer owns saved state, deadlines and Wakeup alarms. Phone receipt
  means accepted shared state, not an alarm or a disk-flush guarantee.
- Preserve source/run identity, revisions, transport handshakes and observation
  time. Polling a saved snapshot must not renew its native observation. Zero is
  not proof of native completion.
- A paused activity is shared for `ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS`
  (five minutes), then hidden from consumers while remaining saved in Timer.
  Reconfirmation advances its revision without renewing observation/expiry.
  Only explicit paused edits/extensions or Resume establish a fresh window.
  Clamp old paused leases from the saved observation when upgrading.
- Back remains available when disconnected, storage fails or sharing is pending.
  Scheduling failure must remain visible; do not claim an alarm was secured.
- Keep a single AppMessage queue. A transport ACK is not an application receipt.
- Acknowledgement preserves the completed run and saved repeat duration, advances
  its source revision and hides its result. Reconfirmation must never renew a
  terminal observation or validity. Persist acknowledgement and alarm-mask changes
  in the same verified bank before publication.
- Keep the saved repeat duration separate from the current run's progress span.
  Snooze uses a new one-minute run; remaining-time edits preserve elapsed and a
  no-op editor save preserves the deadline. Ready edits do not start alarms.

## Dependencies and packaging

- The Activities source repository is `xefensor/pebble-lib-activities`; its
  package/import/header name remains `pebble-activities`. Keep the pinned
  tarball's bytes, source commit and checksums when changing only its canonical
  repository location. See `docs/REPOSITORY-MIGRATION.md`.
- Install with `npm ci --ignore-scripts`. Use the versioned tarball in `vendor/`;
  never rely on a sibling library checkout. Record updated provenance and hashes.
  Version 0.9.0 keeps Activities 0.4.0; `vendor/ACTIVITIES.json` records
  its exact reviewed source commit and archive checksum.
- Use `npm run build` for distribution. Its manifestVersion 2 PBW must contain
  native app, PKJS, independently callable `plugin.js`, licences and notices.
  A plain SDK development PBW lacks the additional provider packaging.
- Do not copy upstream Pebble Timer code or assets without checking its actual
  reuse terms. It is currently a design reference, not a bundled dependency.
- Preserve published artifacts and tags. Changed bytes require a new version.

## Verification

Run `npm test`, build the app and run the release artifact checks. Check the
actual packaged PKJS/provider handoff, symbolic and numeric message keys, native
identity and resource integrity. Record emulator results separately from real
phone and physical-watch checks; never describe unperformed checks as passed.
Check affected layouts on an actual emulator when changing native rendering.

The approved hourglass reference is in `docs/design/`; those browser images
remain design evidence, not native captures. `src/c/timer_ui.c` owns the native
166px content/34px action-bar composition. `timer_fonts.c` uses the reviewed
Dynamic Fredoka outlines and native Roboto bitmap fonts; preserve their SIL
notices in both the source and PBW. `scripts/check-timer-fonts.py` verifies
packaged resources and linked outlines against their exact source assets.
Keep digits stable, sand proportional, paused views still and transitions
bounded. Preserve initial launch focus and cancel decorative work under a
covering window. All eight saved timers retain native deadline ownership.
More options owns Reset and confirmed Delete; Keep is the initial choice.
Cancel returns to the originating screen: an Options edit returns to Options,
a direct detail edit returns to detail, and a new timer returns to the list.
Delete Keep/Back returns to Options with its Delete row still selected.
Reset names the saved repeat duration, independent of remaining/snoozed time.
When New timer will reuse the oldest acknowledged Ready slot, warn in the list
before entry and in the editor before starting. Eight active slots remain Full.
Held buttons must not act again after their first press changes screens.
Full lists skip the disabled Add row; normal lists retain New timer focus.
Keep the selected row visible with a position label and static scroll indicator.
Only advertise actions available in the current state: hide boundary arrows,
the invalid seconds commit check and 24-hour locked-field plus/minus icons.
Critical warnings must preserve list rows, both Delete choices and the alarm's
Done/Snooze hints. Error fixtures may exercise an exhausted bank generation or
an external Wakeup conflict, but shipping native code must remain unmodified.
Normal sharing status belongs in More options; alarm/storage failures stay
visible. Reset leaves the timer Ready; explicit Select starts a new run in the
same slot with the saved repeat duration.

The app has eight stable native timer slots and an independent list/editor/detail/
alarm interface. ACTIVITY_SLOT (1021) is an internal bridge key;
ACTIVITY_TOTAL_DURATION (1022) adds immutable configured-duration metadata without
renumbering prior keys. ACTIVITY_SHOW_TERMINAL (1023) explicitly publishes result
visibility. Version 2 native models add saved repeat duration and acknowledgement;
keep the exact version 1 record decoder and atomic bank migration. Do not stack
full collection copies in nested migration/save calls on the app stack; retain a conservative budget for older SDK classifications.
Prefer activity/summary schema 4: a standard typed countdown and separate optional
openApp destination, without provider-selected layouts or required display
labels. The snapshot provider preserves native owner truth and gives complete
responses an independently managed sequence, including empty responses.
Retain live schema 3, session schema 1 and card schema 2 for older hosts.
The native publication/receipt path remains
schema 1 plus the optional bridge metadata. Source revisions advance globally
and dirty slots are published in ascending order.
Declare the `activity` source category once, with items `summary`, `live`,
`session` and `card`, and the union of their properties. Keep the native alarm,
timer models, bank formats and app-list countdown independent of these source
projection choices.
Keep the earliest-deadline Wakeup broker: separate nearby Wakeups conflict on
stock PebbleOS. Versioned storage chunks must stay below the SDK’s per-key limit;
write and verify a new bank before committing it or retiring legacy state.
App Glance is a launcher subtitle only: queue native running deadlines, respect
the runtime slice limit, and let the OS tick/expire them. It must not change
Wakeup ownership, start a worker, reorder the launcher or refresh source validity.

The accompanying PolyForm Noncommercial licence and required attribution apply.
Preserve third-party notices, including the SDK's Apache 2.0 notice.

Version 0.10.0 preserves the 0.9 UI while reducing work. `timer_ui_state.c` owns
the portable row-window and visible sharing-status invalidation decisions.
Both drawing and clock ticking must use the same list window, including the
two-row critical-warning case. Offscreen timers still reconcile and own alarms.
Do not use transport receipt status as a reason to repaint other screens.
Keep the six used body fonts and exact eleven digit/colon outlines; regenerate
with `scripts/generate-clock-glyphs.py` using the pinned font dependencies.

Distribution-only minification uses locked Terser 5.46.0 with compression off.
The debug ZIP owns SDK originals/maps and chained minified maps; no source map
or dangling sourceMappingURL is installed in the PBW. Release preparation must
verify original module/source identity, reproduce optimized code/map bytes and
exercise the actual optimized runtime. Keep development SDK output unchanged.
Publish/debug/source artifacts need matching versioned checksums. Host raster
profiling counts portable work; it is not watch render timing or battery proof.

Version 0.11.0 replaces the canvas bounce with a 300ms glass-only rotation.
`timer_hourglass.h` owns portable geometry, state poses and bounded easing;
`main.c` starts motion only after an explicit successful timer operation.
Ready/setup is 180 degrees, Running/Finished is 0 and Paused is 90. Keep
the countdown fixed, preserve proportional sand, stop decorative timers under
a cover and do not replay motion on ordinary refresh or reopening. The alarm
deadline starts immediately, independently of decorative motion.
Dashboard control remains a proposal in `docs/design/dashboard-timer-control.md`.
Do not add a remote Pause/Resume action without an owner-side verified operation;
neither a queued command nor a changed phone snapshot cancels a native Wakeup.

Version 0.11.1 keeps the local watch time visible in each screen's existing
header band. Preserve 12/24-hour preference, short readable context titles and
critical warning clearance. Use minute ticks when no timer runs, second ticks
when any native timer runs, and repaint idle headers only at minute/format
changes. The SDK's first callback after subscribing can report zero changed
units; do not skip it. Covering the app suppresses paints, not reconciliation.
Running sand has a bold stream/grains on the existing second tick; do not add
a perpetual high-frequency particle timer or animate paused/Ready glass.
Repeating alarm feedback follows the real pending alarm mask. Clear/cancel it
only after a successful acknowledgement/Snooze or process teardown; other
pending alarms continue. Check Quiet Time before every pulse. Native model,
saved alarm mask and Wakeup ownership remain authoritative. Repeating feedback
requires the Timer process; never claim background motor behavior after exit.

Version 0.11.2 hardens the single PKJS queue: settle callbacks once, release
the slot on a thrown SDK send and bound failed startup requests to three total
attempts. Cancel pending bootstrap retries only after publisher acceptance,
not a transport ACK or rejected snapshot. Duplicate ready/connection events
must not trigger another full reconfirmation/publication. Keep native receipt
tickets, storage, observations and alarm ownership intact. Use the packaged
transport benchmark for work/recovery evidence; it does not measure real radio
delivery, battery life or native vibration.

Version 0.12.0 replaces More options' immediate Restart with Reset. Persist a
cancelled tombstone in the same run/slot, retain that row through visible_mask,
and render it as Ready through activity_timer_ready(). Preserve repeat_seconds;
Ready editing changes only the saved repeat, and Select starts a new run. Reset
must not report completion, create a deadline or affect sibling alarms. Only a
verified save permits cancelling its Wakeup and withdrawing its activity. No-op
Ready resets do not write or renew observation/revision. Keep v1 visibility
decoding strict; v2 additionally accepts visible cancelled records with run>0.
Existing records upgrade without a format rewrite; older binaries do not support
the new visible-cancelled Ready representation. Preserve the unchanged paused
lease, terminal-history, publisher receipt and single-queue rules.

## Shared UI/UX alignment — local 0.13.0 candidate

Read [the UI alignment record](docs/UX-ALIGNMENT.md) and the canonical sibling
XOCE standard before interface changes. Version 0.13.0 applies neutral native
light/dark roles, inverted selection, target-specific Delete and a minimal
phone Appearance page. Timer editing and alarms remain watch-owned.

- The user chose phone settings for appearance. Do not add an on-watch theme
  row or duplicate timer controls on the phone. The page has exactly one
  Light/Dark selector plus disclosed help, using EN/CS labels.
- Native appearance key 2105 is independent of every timer bank, migration,
  Wakeup and publication key. Write and read back before applying a new theme;
  preserve the previous palette on failure. Restore it before the first frame.
  Key 2104 remains the existing AppGlance checkpoint: never reuse it. Keep the
  source-derived namespace audit and actual shared-store Glance/appearance
  coexistence regression; separate per-module storage mocks cannot prove this.
- Append-only keys TIMER_APPEARANCE (1024) and TIMER_APPEARANCE_VERSION (1025)
  carry the version-1 appearance update. They do not project or control timers.
- `src/pkjs/settings.js` owns phone appearance defaults, validation and exact
  storage acknowledgement. `settings-page.js` owns the one shared template;
  generate `config.html` through `scripts/generate-config.cjs` and bundle it.
- Save stays open with Saving…/✓ Saved/Unsaved changes. Do not automatically
  focus fields or scroll to the top. Known rejection and an uncertain write
  remain distinct; uncertain state preserves the draft and explicit readback.
- Use the live PKJS writer when present, and recheck topology after a plugin
  save. Keep separate Android DOM/native-store regressions. Appearance shares
  the existing PKJS queue with bounded retries and coalescing; no second queue.
- `scripts/github-release.py` must verify the exact local settings module,
  generated config HTML and plugin rebuild inputs as well as the Activities pin.
- `scripts/check-settings.cjs` runs actual optimized PBW pages/handlers in local
  Chromium with a simulated SDK bridge. `scripts/emulator-ui-system.py` runs
  actual distribution native bytes/resources in isolated QEMU with direct QA
  AppMessages. Keep those evidence boundaries explicit.

Existing timer/model/privacy/release rules above still apply. The candidate is
local; published versions and immutable release assets remain unchanged.

## Native redesign — local 0.14.0 review branch

`design/pebble-native-redesign` is an alternative for Xef to review, documented
in [REDESIGN.md](docs/REDESIGN.md). The canonical XOCE standard remains the
baseline; this branch does not silently replace it or publish a release.

- Use OS Gothic 18 Bold for dense utility roles and Gothic 24/24 Bold for menu
  and confirmation roles. Fit actual native ink within the 166px content area;
  long list details use a compact state word while retaining the saved duration.
- Keep the exact Fredoka digit/colon outlines and both original SIL notices.
  The six Roboto bitmaps above are historical 0.10–0.13 requirements: 0.14 uses
  no custom bitmap resources or custom-font allocations. Preserve original
  source assets/provenance for older releases. Audit the exact linked outlines,
  notices, OS font roles and absence of unused bitmap resources.
- `timer_ui_motion.h` owns finite portable feedback geometry. Menu stretch and
  recovery take 100+250ms; editor value feedback 107+214ms and field advance
  107+179ms. Held repeat skips flourish; Select settles pending selection before
  activating its immediate logical target. Native timer mutations never wait.
- Moving selection inverts only neutral pixels in a bounded framebuffer region
  on Emery, preserving blue/amber meaning and readable labels across its edges.
  Release the captured framebuffer in the same draw callback. Do not allocate
  a separate screen bitmap. Do not run motion while covered or replay on reveal.
- Raw action presses move the relevant icon 5px right, followed by 144ms release
  feedback. Screen change, focus loss and teardown settle it; timer allocation
  failure settles every feedback family. Running countdowns stay fixed.
  Fixed wrappers identify Up/Select/Down independently of raw recognizers;
  releases settle only their own button. Verify all three icons independently
  with `scripts/emulator-action-feedback.py`, keeping the action available so
  a screen change or hidden boundary icon cannot invalidate pixel comparisons.
- Keep the one-selector phone page, confirmed Save/draft/uncertain readback and
  active Android writer. Use reduced-motion-aware 160ms disclosures/144ms
  controls. Do not animate or dismiss Saved status. The offline phone preview
  is explicitly a mock layout and proves no real settings write.
- Run `scripts/emulator-redesign.py` after the fixture/build procedure for real
  distribution-native frame samples and input/focus lifecycle checks. Existing
  native appearance/storage checks remain required. Browser restrictions and
  real-device gaps must be recorded separately from passed native tests.

## Readability correction — local 0.14.1

Xef rejected shrinking the top time/date and the long/deformed up/down choice
transition. Readability takes priority over matching Pebble's small text.
Use Gothic 28 Bold for the top clock and 24Bold for utility text; compare actual
ink, not font point names. Header title/time visible ink shares an optical centre.
All boxes must retain readable inset ink, including critical warnings and editor.
Vertical menu/list/confirmation choices use a rigid120ms ease-out move, without
stretch/overshoot. Snap when a list window changes, multiple rows are skipped,
repeat input arrives or Select activates. Keep independent editor field/value
feedback and 300ms hourglass behavior. The old 0.14.0 stretch specification above
is historical. Physical action icons keep their hardware-button anchors; Timer
has no inline text/icon pairs. Preserve exact native screenshots/provenance and
separate phone CSS source audits from browser rendering, which remains blocked.

## Whole-composition correction — local 0.14.2

The user rejected enlarging only the clock while other text stays tiny. Treat
each screen as one composition: Roboto Condensed Bold26 header/clock (19px
digit ink),24 primary labels (17–18px caps) and22 secondary/help (16px caps,
with appropriate descender clearance). Preserve dominant Fredoka digits. Check
actual visible ink, complete copy and comfortable group/box padding together;
do not shrink a label to rescue an otherwise cramped layout.

The header uses all200px; the34px physical action rail starts below it. Complete
footers may use the full width. The list has two80px rows with8px gap; critical
warnings and oldest-Ready replacement use one readable row. Both the renderer
and tick policy share that window, and the selected item must remain visible in
both directions at every index. Critical copy is complete: `Not saved`,
`Keep Timer open`, and `Cannot undo` must not receive an ellipsis. Long running
timers split hours from MM:SS; list rows retain the full remaining/saved duration
in a single left-aligned column. Keep editor units, selected digits and help in
separate bands. Keep-first Delete and immediate owner actions remain unchanged.

Three22/24/26pt custom bitmap resources replace the previous OS Gothic roles.
Set `extended:true` for declared Latin017F coverage: the SDK's default256-glyph
limit silently truncates Czech letters. `scripts/generate-body-metrics.py`
derives actual ink from SDK-built PFOs; the distribution build regenerates and
relinks only if needed. `check-timer-fonts.py` verifies exact sources/licences,
compiled printable Latin coverage, native alignment tables and Fredoka outlines.
Fit text to its logical column, anchor its advance explicitly and paint Left
into a wide rectangle so tight clock/unit fields cannot trigger SDK reflow.
This is not arbitrary Unicode coverage or physical-watch legibility proof.

The rigid120ms selection and independent editor/hourglass behavior above remain.
After visual approval, run current-geometry native suites plus
`emulator-list-readability.py`: all8 normal/critical positions forward/reverse,
boundary/Back restore and unchanged owner/bank checks. Keep immutable preview
sets under candidate-hash directories and bind final QA to the exact finalPBW.
Package standalone source last. Preserve all0.14.1 artifacts and prior evidence.

## Action-rail correction — local 0.14.3

Xef rejected the running screen's right-side treatment. The current candidate
uses the page background for the34px action rail, with a neutral1px divider at
x166 and inset ends. Alarm uses black with white icons. Do not reintroduce the
floating inverted block between full-width header/footer bands from0.14.2.
All screens use larger bold action glyphs (about20px painted bounds,3px strokes),
24px slots centered at48/114/180 and3px press travel/144ms return. Preserve at
least3px right-edge clearance even while pressed. Ellipsis dots must remain
separate; their small natural height is intentional. Idle and pressed glyphs
must retain their full shape. Screen-changing presses may immediately show the
new action and must cancel old-screen feedback; do not delay timer operations
for a screenshot or decorative motion. Body typography/layout, rigid120ms choices,
owner/alarm/storage and phone settings remain unchanged. Verify actual native
pixels in both themes, editor/menu/list/confirmation/alarm and critical states.
