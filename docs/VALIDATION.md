# Timer validation

## 0.13.0 shared UI — local candidate

The native app uses neutral Light/Dark surfaces, consistent inverted selection,
the existing blue hourglass and a target-specific Delete confirmation with Keep
selected initially. Appearance lives in the bundled phone settings by Xef's
decision; timer creation, editing and alarm ownership remain on the watch.

The [versioned validation record](qa/0.13.0/validation.json) identifies the exact
candidate PBW and links the native, font and phone-page evidence. Nine portable
C suites, 34 JavaScript checks and 45 Python release regressions pass. The SDK
build reports 36,988 bytes RAM footprint and 22,802 resource bytes. The only
build warning is the existing SDK linker's RWX segment warning.

Packaged optimized config/PKJS/plugin checks cover exact phone persistence,
independent Android DOM/native stores, Save staying open, close/reopen and full
browser restart, uncertain acknowledgements, recovery, EN/CS and constrained
layouts. These use a simulated SDK bridge in actual Chromium, not a real phone.
Fresh stock-derived Emery QEMU captures cover 26 native compositions. Every
capture requires visible Timer title, local clock and contextual sidebar with
the expected palette; all 26 were also visually inspected. A prior run using a
reused flash was invalidated after an OS missed-Wakeup overlay intercepted its
inputs. Its separate report is explicitly marked INVALIDATED and is not passing
evidence. Native phone delivery is simulated with direct AppMessages; persisted
appearance is checked by reopening without phone JavaScript. A read-only
same-UUID fixture probes actual Glance/checkpoint and Appearance records between
native launches, including both write orders. Portable tests link both modules
against one shared store and protect all native owner/bank keys; a source-derived
namespace audit independently prevents collisions. The earlier key 2104 local
candidate is invalidated and superseded by Appearance 2105; 2104 remains AppGlance.

The candidate preserves the UUID, eight timers, model/bank versions, native
Wakeup ownership and Activities 0.4.0 contracts. Theme uses isolated native key
2105 and appended message keys 1024/1025. This work has not published a release,
measured battery use or verified a physical watch or real phone WebView.

## 0.12.0 Reset without starting

More options now resets the selected timer to its saved duration as Ready.
Select starts it explicitly. The cancelled owner record stays in the same slot
and run, and withdraws its displayable activity; only Start creates a new run.
Ready, edited remaining time, Snooze, process reopen, sibling timers and native
alarm ownership are covered by the focused checks in [versioned QA](qa/0.12.0/).

All 31 JavaScript checks, eight portable C suites and 42 Python regressions pass.
The changed model/collection suites also pass AddressSanitizer and UBSan with
leak scanning disabled for the sandbox. The packaged optimized PKJS/provider
checks verify Reset withdrawal through summary/live/card/session, unchanged
sibling clocks, stale/native-terminal rejection and explicit new-run Start.
Reset records do not acquire a new observation or lease on polling/reopen.

The SDK build passes with 36,123 bytes RAM footprint and 22,802 resource bytes.
The font/license audit confirms unchanged source fonts, native outlines and SDK
bitmap resources. Published PBW SHA-256:
`2ff486c1af98f9f62325d49d61f92ab43abe20d2159f3e9bfb03c032abc30560`.
Both focused native phases pass with 21 captures: 19 Timer screens, one stock OS
screen at the withdrawn deadline and one labelled storage seed fixture. Reset
survives reopening, the withdrawn deadline does not launch Timer, and an untouched
sibling still wakes and alarms. An actually refused native bank save preserves
the old timer/alarm, shows the warning and emits no unsaved withdrawal. Both
contact sheets were visually reviewed. Exact candidate/native payload identity
is recorded in the versioned QA directory; older captures retain their scope.
No real phone or physical-watch validation is claimed.

The persisted layout, keys and record sizes remain unchanged, and the current
decoder retains v1/v2 input support. V2 additionally accepts a visible cancelled
run as Ready; the old binaries do not support this representation. Already Ready
Reset and unchanged Ready edits preserve every model byte, and the native UI
does not save or republish a no-op Reset. The Activities publisher/provider pin
remains 0.4.0; source schemas, message keys and app UUID are unchanged.

The [0.12.0 development prerelease](https://github.com/xefensor/pebble-app-timer/releases/tag/v0.12.0)
was published by the passing GitHub Actions release workflow from tested commit
`2c5a9ef2b2f8b0da3e6643abe87ec07f149442b8`. Published PBW, source, debug and
checksum assets match the prepared bytes. Source ZIP SHA-256 is
`c843da6a2dbc8be7b265930a4866efee871f3af59764ec5d179bf65147fbb710`;
debug ZIP SHA-256 is
`04057ff2c180db8e785c5d49259eeb655b1f504e6f24f4938a7deef4ef6c63cb`.
Later documentation commits keep that release source snapshot, tag and runtime
unchanged. Release preparation pins 0.12.0 to the tested source commit and checks
the committed artifacts without rebuilding or replacing the published source ZIP.

## 0.11.2 activity transport reliability

All 28 JavaScript checks, eight portable C suites and 42 Python regressions pass.
SDK build, font/license identity and the actual packaged PKJS/provider handoff
pass. Six new regressions cover thrown sends, bounded bootstrap retries,
duplicate ready/callback handling, acceptance cancellation and stale rejection.

The [actual packaged transport comparison](qa/0.11.2/transport-work.json) shows
five duplicate ready events falling from five requests to one. A thrown first
send previously escaped and wedged the queue; the candidate recovers with two
requests and an accepted receipt. Duplicate callbacks previously allow two
simultaneous sends in the simulation; the candidate holds one queue owner.
No retry timer remains after recovery.

The [identity report](qa/0.11.2/ownership-identity.json) verifies unchanged native
rendering/clock, model, storage, alarm, Glance and provider sources. The existing
0.11.1 native captures retain their historical scope; this transport pass does
not claim another native layout capture batch. Shared Activities remains 0.4.0
because the separately reviewed 0.4.3 changes affect host consumers only.
Evidence and limits are indexed in [versioned QA](qa/0.11.2/).

## 0.11.1 clock, falling sand and repeating feedback

Eight portable C suites, 22 JS checks and 42 Python regressions pass. The new
fake-SDK alarm tests verify repeated pulses, Quiet Time, final cancellation,
multiple pending alarms, failed acknowledgement, reopening and timer-allocation
failure recovery. Native storage, model, Wakeup broker and phone/provider source
identity is retained; this pass adds feedback without handing off alarm ownership.

The final SDK build uses 35,831 bytes of RAM footprint and 22,802 resource bytes.
Exact font/license identity and the actual packaged phone runtime pass. Candidate
PBW SHA-256:
`2b4c9ce770cb5a08b1a22da13c7788e77570158ebd15cd99ab7b87a8c0292652`.
See [versioned evidence](qa/0.11.1/).

Focused native QA covers wall-clock headers on all six screens and their warning
compositions, actual 12/24-hour switching, moving grains, static pause, native
Snooze/Done with multiple pending alarms and idle minute updates. The running
stream contains 83 blue pixels in its measured lane versus 15 in the preserved
0.11.0 capture; alternating grain phases move 80 native pixels. These are
screen-pixel measurements, not physical display brightness or battery results.
Both focused phases pass with 30 actual QEMU captures, including one labelled
seed fixture. Empty, paused and editor screens cross real minute boundaries
with identical body pixels; their headers update. A font audit verifies all
2,160 unique 12/24-hour clock strings fit the header, and a pixel audit confirms
visible clock ink on all 29 Timer screenshots.

Physical vibration feel and phone delivery are untested. Repeating feedback
requires the Timer process; exiting stops it, and a saved pending alarm resumes
feedback on reopening. The broader [0.11.0 native report](qa/0.11.0/native/ux.json)
and [provenance](qa/0.11.0/asset-provenance.json) retain their historical scope.

## 0.11.0 local hourglass candidate

This version changes native presentation and short decorative motion. Timer
models, collection/banks, Wakeup broker, Glance and phone publisher/provider
remain independent. Dashboard Pause/Resume is deferred by user choice; the
[platform investigation](design/dashboard-timer-control.md) records the scope.

All seven portable C suites, 22 JS checks and 42 Python regressions pass. The
portable checks cover conserved sand, every second of a 24-hour countdown,
every integer orientation from 0 to 180 degrees at the used scales, state poses
and finite monotone easing, including interrupted rotations. SDK build,
packaged-runtime, font identity and Python release/optimization checks are
recorded in [versioned QA](qa/0.11.0/). The final SDK build reports 35,243 bytes
of static RAM and 22,802 resource bytes. Final candidate SHA-256:
`4fb9ef3ab3581338842ae57bf2610abdef665886d1612f515c45a4eeeddebbbd`.

Six actual native Emery phases pass with 76 captures, including explicitly
labelled OS/fixture surfaces. They cover the hourglass poses and intermediate
motion, partial sand proportions, static pause, OS cover-return, existing-run
opening, real 23:59:59 entry, invalid 0/4 seconds versus valid 5 seconds,
eight-slot capacity/reuse, editing/cancellation, background Wakeup, Snooze,
Restart, durable Done/reopening and actual refusal to save an exhausted bank.
Contact sheets preserve the raw native screenshots and identify enlargement;
the three-state product overview keeps native screenshots unscaled. All checks
use the exact final candidate's native binary and resources, with Python
transport/receipts explicitly separated from real phone delivery.

The separate [scheduling-error run](qa/0.11.0/native/errors/errors.json) also
passes with seven captures, including two labelled external-fixture surfaces.
Timer shows Alarm not set in Detail and Options, recovers after the external
Wakeup is cleared, and returns to the same empty list after deletion. Across
both reports, all 83 PNG hashes match and the native payload is identical.
Six labelled contact sheets and focused originals were visually reviewed.

The [identity report](qa/0.11.0/ownership-identity.json) verifies the retained
timer models, storage, alarm broker, Glance, phone sources, provider metadata
and packaged fonts/scripts against 0.10. Repeated migration, equal/nearby-deadline
and launcher tests retain their historical scope; this pass does not claim
those unchanged paths were exercised again on 0.11.

The first native candidate exposed a rasterization defect: its one-pixel sand
polygons disappeared at the horizontal paused pose. Its rejected hash, captures
and observations remain in [failed-candidate evidence](qa/0.11.0/candidate-1-failed/).
Portable geometry tests alone did not detect that SDK raster behavior.

This remains emulator/host evidence, not a real phone, physical-watch, battery
or measured watch rendering-time claim. The published 0.10 artifacts and their
[historical provenance](qa/0.10.0/asset-provenance.json) remain unchanged.

## Publication — 3 November 2026

The [0.10.0 development prerelease](https://github.com/xefensor/pebble-app-timer/releases/tag/v0.10.0)
was published from tested source
[`e652563356cdbc8b1a85a328eb9a3e9695b4bd63`](https://github.com/xefensor/pebble-app-timer/tree/e652563356cdbc8b1a85a328eb9a3e9695b4bd63).
The documentation follow-up preserves the tag, PBW, source/debug ZIPs and checksums.
Repeated release checks use `prepare --commit` with that tested commit; the
original validation below is unchanged.

## 0.10.0 optimization candidate

The [optimization review](OPTIMIZATION-REVIEW.md) records the implementation,
measured before/after sizes, preserved behavior and remaining device checks.
All 22 JS tests, seven portable C suites and 42 Python regressions pass. The SDK
build uses 34,393 bytes of static RAM and 22,802 resource bytes; the distribution
PBW is 74,892 bytes. Exact font/license identity and the actual optimized
PKJS/provider cold handoff pass. Debug maps and original scripts live in a
separate versioned ZIP, reproduced and verified during release preparation.

Six native QEMU phases pass with 59 captures, including explicitly labelled
fixture/OS-launcher frames. They cover unchanged layouts, offscreen clocks,
storage refusal, native offline alarms, nearby/equal deadlines and app-list
countdown clearing. Twenty-four complete native frames match approved 0.9.0
pixels exactly. Source/payload hashes, limits and reviewed frames are in
[0.10 provenance](qa/0.10.0/asset-provenance.json) and [the native report](qa/0.10.0/native/ux.json).
The old 0.9 provenance is retained [with its historical QA](qa/0.9.0/asset-provenance.json).

The host-only raster profile informs a decision to retain existing bounded
scratch; it does not justify extra watch RAM for a phase-sensitive glyph cache.
No battery, physical-watch timing, real-phone startup or peak-memory claim is
made. Native timer models, verified persistence, expiry and launch sharing
remain independent of this rendering/packaging pass.

## 0.9.0 complete watch UI/UX pass

This pass improves list orientation, Options context, editor affordances and
critical error layouts while keeping the normal hourglass/detail composition.
Full lists focus an existing timer; unavailable navigation icons are omitted.
The selected timer stays visible with a larger position footer and scroll thumb.
Invalid seconds and locked 24-hour fields match the available Select/Up/Down
actions. Options retains duration/state and selected-timer context. Two-line
critical warnings leave Delete choices and alarm Done/Snooze hints readable.

- **22 JavaScript checks, six portable C suites and 38 Python regressions pass.**
  The actual packaged PKJS/provider also pass numeric/symbolic messages, schema4,
  acknowledgement, original leases, compatibility sources and cold snapshots in
  isolated VMs. Font/resource and license identity checks pass.
- **SDK 4.33.1 Emery build passes:** resources remain 26,719 bytes; the SDK RAM
  footprint is 36,501 bytes with 94,571 available heap bytes before startup
  allocation. These are static SDK figures, not runtime peak/battery measurements.
  The only compiler/linker warning is the existing SDK RWX load segment notice.
- **53 actual final-candidate native frames pass** across focused UI, settings,
  all editor fields, normal native UX and eight-slot capacity phases. Checks
  cover mixed states, equal-duration Options context, first/last list position,
  invalid 0/4 seconds versus valid 5 seconds, unchanged locked 24-hour fields,
  cancellation/Keep return paths, Ready replacement, Full opening an actual
  selected timer, background Wakeup, Snooze, no-op edit, elapsed-preserving edits,
  original-duration Restart, durable Done and explicit repeat.
- **Seven actual Timer storage-error frames pass.** An isolated fixture writes a
  valid V1 bank at generation `UINT32_MAX`. The unmodified final Timer binary
  then executes its real refusal to commit another bank. List/Detail/Options
  warnings fit, Delete keeps both choices visible, native completion retains
  Done/Snooze hints, failed Done stays pending, and Back remains available.
  The fixture screen is separately labelled and is not a Timer capture.
- **Five actual Timer scheduling-error/recovery frames pass.** A different-UUID
  fixture owns a conflicting native Wakeup. Timer displays Alarm not set / Keep
  Timer open in Detail and Options, then clears the warning after the fixture
  explicitly cancels its Wakeup. Both fixture screens are labelled separately.
- All **65 Timer frames** use the exact final native binary/resources and were
  visually reviewed; the three fixture frames retain their separate scope.
  Contact sheets and the current eight-frame product overview arrange raw
  screenshots with captions outside. Native pixels are not altered.
- Timer models, collection/banks, Glance, fonts/hourglass, phone publisher/provider
  and the Activities 0.4.0 pin match 0.8.0 source bytes. Packaged phone bundles and
  resources are byte-identical to 0.8.0. Only native UI drawing, issue labels and
  Full-list navigation change; UUID, keys, leases and native alarm ownership stay.
- The final suite handoff executes Dynamic 0.15.0, Timer 0.9.0 and Open Activity
  0.1.4 scripts. Six privacy/role cases cover 48 selected launches, 48 expired
  closures and 336 unselected attempts without a subscription or invocation.
  SDK/native presentation/gesture/confirmation and clocks are simulated.

Evidence: [checks](qa/0.9.0/checks.json),
[normal/UI/native report](qa/0.9.0/native/ux.json),
[storage refusal](qa/0.9.0/native-storage/ux.json),
[scheduling failure/recovery](qa/0.9.0/native-error/errors/errors.json),
[focused review](qa/0.9.0/review-focused.png),
[font audit](qa/0.9.0/font-audit.json),
[packaged runtime](qa/0.9.0/packaged-runtime.json),
[packaged suite handoff](qa/0.9.0/packaged-handoff.json),
[owner/Glance/bridge identity](qa/0.9.0/native-identity.json) and
[UX review](UX-REVIEW.md).

Final PBW SHA-256:
`44a2b09ce5a72455ae65694b8e22b05d5b5ab5276e0bd4891fc743b4e7ae0e3a`.

Native tests run in isolated Emery QEMU with synthetic saved data and direct
Python requests/receipts. They establish neither physical button feel nor real
phone/plugin delivery, vibration or battery use. Prior migration/Glance evidence
retains its original version scope unless explicitly repeated above. Earlier
release archives and QA remain preserved.

## 0.8.0 on-watch Options and editor UX

This update improves list guidance and cancellation paths. Timer has no
phone configuration form: Options and the duration editor are its settings
surface. The native model, collection, Glance and phone bridge sources retain
their exact 0.7.0 bytes. Activities stays pinned at 0.4.0; UUIDs, keys, banks,
alarms and all eight slots keep their existing ownership.

- **22 JavaScript cases, six portable C suites and 38 Python regressions pass.**
  The packaged PKJS and cold provider pass their numeric/symbolic key, schema4,
  compatibility, acknowledgement, original-lease and multi-slot checks with
  simulated SDK surfaces. The exact font/resource audit also passes.
- **SDK 4.33.1 Emery distribution build passes:** 26,719 resource bytes and a
  35,581-byte SDK RAM footprint. The SDK reports 95,491 available heap bytes
  before startup allocation; no runtime peak or physical battery claim follows.
- **19 actual native settings/navigation captures pass** on an isolated Emery
  emulator. New timer, all three editor fields, running/paused detail, Options,
  safe Delete confirmation, eight-Ready replacement and eight-running Full
  states fit the real 200×228 surface.
- **17 additional native UX captures pass** on the final candidate. Background
  completion, Snooze, unchanged editor saves, elapsed-preserving remaining edits,
  original-duration Restart, durable Done acknowledgement, reopen and Ready edits
  retain their behavior. Independent visual review covers all 36 current frames.
- Cancelled new edits keep the list empty. Direct detail cancellation preserves
  run/deadline; Options editor cancellation restores Options. Delete Keep and
  Back restore its selected Delete row and preserve the current run/deadline.
  Native selection-region checks confirm those return screens and cursors.
- A remaining-time edit keeps the saved ten-minute identity. Restart then
  creates a new run with the saved 600-second duration, matching its visible
  label. Cancelling replacement keeps all eight Ready entries; Select on the
  eight-running Full state preserves every run and deadline.
- Replacement is explained while New timer is selected in the list and again
  in the editor. Back guidance covers Hours, Minutes and Seconds. The app's
  deliberate hold guards, alarm controls and critical error messages are
  unchanged; no extra confirmation or phone setting was added.
- Native review found the alarm's Snooze hint touching the last display row.
  Raising both hint baselines eight pixels preserves the hourglass and actions.
  Both recaptured alarm frames have a verified eight-pixel black bottom margin
  across the full content width, with no hint clipping.

Evidence: [native navigation/capacity report](qa/0.8.0/native/ux.json),
[review contact sheet](qa/0.8.0/native/review-settings.png),
[native alarm/repeat UX](qa/0.8.0/native-ux/ux.json),
[packaged runtime](qa/0.8.0/packaged-runtime.json),
[packaged cross-app handoff](qa/0.8.0/packaged-handoff.json),
[font audit](qa/0.8.0/font-audit.json),
[unchanged owner/bridge source identity](qa/0.8.0/native-identity.json) and
[UX review](UX-REVIEW.md).

The emulator uses synthetic saved timers and direct Python snapshot requests/
receipts while executing actual native Timer operations. This is separate from
real phone/plugin delivery and physical-watch testing. Earlier migration,
launcher, failure and hold captures remain historical unless explicitly
repeated for this version.

Version 0.8.0 is a [GitHub development prerelease](https://github.com/xefensor/pebble-app-timer/releases/tag/v0.8.0)
from commit `984e05690684a3676124dd358760979726bd9c26`. GitHub's portable Checks
and release validation/publication workflows passed. Downloaded PBW, source ZIP
and checksums match the prepared artifacts; published bytes and the artifact
tag are preserved separately from later documentation updates.

## 0.7.0 typed summary snapshots and app navigation

This local version publishes `activity/summary` schema 4 through the pinned
Activities 0.4.0 package. It retains live/session/card compatibility sources.
It was validated as a local candidate; its source changes are now included in
the published 0.8.0 release. No separate 0.7.0 release was published.

- **22 JavaScript cases, six portable C suites and 38 Python release-helper
  regressions pass.** New cold-plugin cases preserve the native countdown,
  revision, observation and lease; preserve the original paused expiry; omit
  acknowledged results; and sequence a complete empty reply after expiry.
- **SDK 4.33.1 Emery distribution build passes.** The actual packaged PKJS and
  independently callable provider execute with simulated SDK surfaces. The
  check covers numeric/symbolic keys, all four declared source items, optional
  app destination, multi-slot replacement, acknowledgement and cold reload.
  Timer advertises no invented Pause/Cancel actions.
- **Native C and PKJS sources remain unchanged from 0.6.0.** Native bytes match
  after excluding the process metadata and verified GNU build-ID note. Resource
  bytes are identical. Existing Timer UI, banks, native alarms, Wakeup broker and
  app-list countdown therefore retain their previous implementation. Earlier
  native captures remain historical; no new Timer native runtime run is claimed.
- The exact font audit passes for the same seven bitmap resources, linked
  Fredoka outlines and required notices. App and provider identity are unchanged.

Evidence: [packaged scripts](qa/0.7.0/packaged-runtime.json),
[native payload identity](qa/0.7.0/native-identity.json) and
[font audit](qa/0.7.0/font-audit.json). The exact dependency commit and archive
checksums are recorded in [the pin](../vendor/ACTIVITIES.json).

These portable, SDK and packaged-script checks do not establish physical-watch
appearance, live mobile-plugin transport or battery cost. Opening Timer by its
app destination does not select a specific saved timer.

## 0.6.0 simple activity publishing — 3 November 2026

Timer now publishes the notification-style `activity/live` schema 3 source
through Activities 0.3.0. It supplies app identity and one countdown; the host
chooses normal or compact presentation. The publishing API has no required
visible field labels, main/support tree, meter or host layout selection.
The existing session/card sources remain available to older hosts.

- **18 JavaScript cases, six portable C suites and 38 Python release-helper
  regressions pass.** New cold-runtime cases cover the live countdown, fixed
  paused seconds and original five-minute expiry, acknowledged hidden terminal
  revisions and compatibility source declarations. Polling does not renew data.
- **SDK 4.33.1 Emery distribution build passes.** The final PBW includes native
  payloads, PKJS, independently callable provider, exact symbolic/numeric keys,
  the pinned Activities 0.3.0 package and required notices. The packaged scripts
  execute through their actual receipt, multi-slot, live/session/card and cold
  reload checks with simulated SDK surfaces.
- The native Timer C and PKJS sources are unchanged from 0.5.0. A comparison of
  both PBWs shows identical native payload bytes after excluding only the SDK
  process header and verified GNU build-ID note. Resources are byte-identical.
  Existing timer banks, alarms, Wakeup broker, application UI and launcher-glance
  coalescing code therefore retain their prior implementation. The 0.5.0 native
  UI/alarm/glance captures remain explicitly historical; no new native run is
  claimed for this publishing-only change.
- The release auditor now verifies the exact additional simple-API module and
  its compiled imports against the pinned package. Regressions accept the valid
  graph and reject an unpinned or modified module.
- The final font audit verifies the same seven resources, linked outlines and
  required licenses. Its current PBW hash and the native metadata ranges are
  recorded alongside the packaged-runtime check.

Evidence: [native payload identity](qa/0.6.0/native-identity.json),
[actual packaged scripts](qa/0.6.0/packaged-runtime.json) and
[font/resource audit](qa/0.6.0/font-audit.json). The dependency pin records exact
source `dad422a3384864966b60acf6dc3e52ddec296a09` and archive SHA-256.

Final PBW SHA-256: `f5ce5d3c7f16668046113fbf20a3b790b2c42d44b784f08d021c9be284123814`.

This GitHub development prerelease is for sideload testing. Physical-watch and
live mobile-app/plugin checks remain separate. Earlier published PBWs, source
archives and tags remain unchanged.

## 0.5.0 activity acknowledgement, timer UX and app-list countdown

This development prerelease implements the user's review of the watchface and
Timer together. Published 0.4.0 and older tags, PBWs and source archives stay
unchanged. Both app identities, the provider namespace, existing keys, eight
stable slots, native Wakeup broker and five-minute paused visibility remain.

- Six strict portable C suites and 15 JavaScript tests pass. New cases cover
  original/repeat duration versus current-run span, snooze, remaining-time edits,
  no-op deadlines, acknowledged completion, terminal reconfirmation, active-first
  stable-slot ordering, Ready reuse, arithmetic overflow and launcher deadlines.
- The original 44-byte singleton and 372-byte collection decode into version 2
  models/banks, preserving pending versus acknowledged alarms. In-place and
  unaligned migration, invalid input and failure atomicity have regression cases.
  Native banks remain verified in bounded chunks before their header commits.
- The SDK 4.33.1 Emery distribution build passes. Its final PBW contains native
  code, PKJS, independently callable provider, matching symbolic/numeric keys,
  the exact Activities 0.2.1 package and licences. All 35 Python release-helper
  tests pass. Packaged scripts verify compact projection, completion visibility,
  cold reload and stale-completion rejection using the actual bundled scripts.
- The [exact font audit](qa/0.5.0/font-audit.json) checks all seven native font
  resources and linked Fredoka outlines against the reviewed sources/licences.
  The [static stack audit](qa/0.5.0/stack-audit.json) compares ARM compiler frames
  with 12 actual ELF prologues; all selected frames agree. The actual native
  header SDK tuple is (5,106), classified as 4.x with a nominal 4 KiB Emery
  stack in the identified PebbleOS source, independently of `PBL_SDK_3`.
  Those totals exclude SDK, event-loop, libc and syscall overhead and are not a
  runtime stack high-water measurement.
- SDK static RAM footprint is 35,421 bytes; resources are 26,719 bytes. The SDK's
  95,651-byte pre-startup heap figure does not measure live free heap or battery
  use. Migration/save/completion scratch avoids nested full-collection copies.

Focused actual native evidence is recorded in
[the 0.5.0 Emery report](qa/0.5.0/native/ux.json), with exact final PBW/native
hashes and unmodified raw screenshots. Its Python AppMessage harness requests
native state and sends matching receipts; this is explicitly separate from
phone JavaScript and the real phone/provider transport. Native alarms run under
Timer's own Wakeup scheduling. App Glance uses OS countdown templates rather
than a worker or repeated phone updates; launcher position remains OS-owned.

All six native phases pass and all **48 raw screenshots** were hash-verified and
visually reviewed. They cover old singleton/bank migration, pending and
acknowledged results, snooze/repeat/edit behaviour, all-eight Ready reuse,
actual native background alarms and equal/nearby Wakeups. The OS app-list
countdown ticks after Timer closes; paused, deleted and finished entries clear.
A real same-UTC-second start/resume and pause followed by Back also clears it.
The public App Glance database rejects equal creation timestamps, so unchanged
projections skip writes and changed projections coalesce until the next second.
Normal Back permits a bounded one-second flush; failure or clock rollback never
traps navigation. The derived checkpoint at key 2104 is not an alarm or source
delivery receipt. Fake-SDK tests explicitly model timestamp rejection, limited
slice capacity, timer/persistence/slice failures and rollback.

Final PBW SHA-256:
`1f08e804ec8c4246fb4e9dbf0aacd07cb5aafc54de0f7fc3277049350a7b5351`.

Physical-watch appearance, vibration, Quiet Time transitions, real Bluetooth
loss/reconnection, live plugin delivery and battery use remain device checks.
An offline acknowledgement is durable locally and shared when the existing
phone path is available; it cannot directly message another native app.

## 0.4.0 native hourglass prerelease — 2 November 2026

The approved [interactive concept](design/Timer-UI-Preview.html) is implemented
in the native Emery watchapp. Its browser images remain the design reference;
the actual native evidence is recorded separately under `qa/0.4.0/`.
Version 0.4.0 is published as a GitHub development prerelease from commit
`c907ef39b5defc01e4217ef8b3cea98779a4d115`. Both GitHub workflows passed and all
three downloaded release assets match the prepared bytes and checksums.
Publication-status documentation updates leave those artifacts unchanged.
Older PBWs, source archives and their validation retain their original scope.

The native composition uses the reviewed Fredoka outlines and Roboto Condensed
bitmap fonts in a 166px content area with a 34px action bar. Main countdowns
use fixed character cells, with a separate size for hour-long values through
24 hours. Sand transfers between the curved chambers using the authoritative
remaining/configured duration; it is not simulated elapsed time. The chamber
contains 1,073 discrete pixels, conserved across the two halves.

Pause freezes the stream and countdown. Screen transitions use a bounded
180ms animation and are cancelled under a covering window or on exit. Scheduled
second-by-second redraws cover changing visible content; native deadline checks and
Wakeup ownership remain independent of decorative motion. Initial foreground
launch, pause/resume and covered-screen lifecycle are handled explicitly.
The three-row list, highlighted duration fields, dark alarm, Options and safe
Delete confirmation all use the same native typography. Sharing status lives
in Options; storage and alarm errors retain a prominent warning footer.

| Check | Result and scope |
| --- | --- |
| `npm test` | Four strict C99 suites and all 14 JavaScript handoff tests pass |
| Font corpus | Every second from zero through 24 hours: 86,401 countdown layouts, fixed-cell bounds and digit ink; all ten digit raster samples pass |
| Sand geometry | Exact conserved area, curved silhouette bounds, monotonic transfer, start/end and long-duration arithmetic pass |
| Native ownership | Restart gets a new run ID in the same slot; paused visibility, restored timers, per-slot isolation and failure immutability pass |
| Release regressions | All 35 Python release-helper tests pass, including exact mandatory font notices and resource declarations |
| SDK build | SDK 4.33.1 builds the Emery native app, PKJS, callable provider and notices |
| Exact native font audit | [Font audit](qa/0.4.0/font-audit.json) verifies all seven PFO resources against standalone SDK generation, linked Fredoka points/contours/glyphs, source hashes, generated IDs and both SIL licences |

The SDK reports a 29,389-byte RAM footprint and 101,683 bytes of heap before
startup allocations. Native process metadata reports 26,712 loaded bytes and a
29,392-byte aligned virtual extent. The complete binary is 26,828 bytes,
including 29 four-byte relocation entries. Resources are 26,719 bytes. These
figures do not measure runtime free heap, stack high-water or battery use.
The rasterizer uses bounded static scratch; drawing does not allocate per frame.

Final distribution PBW SHA-256:

`769a7690fca5cc7930e22f728fd9fde4ad2aca3d6a937f98785129dced2eae5d`

The final native capture reports are
[UI, ticking and background smoke](qa/0.4.0/native/emulator.json),
[simultaneous/nearby deadlines](qa/0.4.0/native/deadlines/deadlines.json), and
[actual scheduling error/recovery](qa/0.4.0/native/errors/errors.json).
Their source PBW, exact native payload and raw 200 × 228 screenshot hashes bind
them to this build. Phone JavaScript is omitted from native capture installs;
packaged scripts and both provider schemas are checked separately.

All **67 native capture hashes** were verified: 46 UI/motion/background frames,
seven actual scheduling-error/recovery frames and 14 deadline frames. Direct
visual review covers 43 representative frames, with an additional independent
review of 20 capture records. The long countdown, 24-hour limit hint, three-row
list, safe confirmation, elapsed footer, dark alarm and critical warnings fit
and remain readable. The confirmation was shortened to “Delete timer?” after
checking the actual native font width.

No-input proofs show the countdown changing, the sand stream changing in its
own small region and a paused screen remaining pixel-identical. Holding Down
from detail does not scroll the newly opened Options screen; holding alarm
dismissal does not dismiss a second pending alarm. The full eight-slot header
and count remain stable when Select cannot create a ninth timer. Background
Wakeup works with phone JavaScript omitted, and equal/five-second-separated
deadlines each produce both alarm states before exact empty-list cleanup.
The external-Wakeup fixture produces an actual scheduling failure: the amber
warning stays visible in detail and Options, then clears after explicit fixture
cleanup and successful native recovery. This is separate from a flash-failure
injection test; no native storage-failure injection is claimed.

[Asset provenance](asset-provenance.json) records exact final native payloads,
source/font hashes, report hashes and unmodified product screenshot copies.

Actual phone/plugin delivery, physical vibration, display appearance under
real lighting, button feel and battery use remain phone/watch checks. No
physical-device result is implied by the browser concept or emulator captures.
The README procedure includes the unchanged five-minute paused visibility
behavior and both the live Timer-to-Dynamic sharing path and native alarms.

## 0.3.1 paused visibility candidate — historical

Paused timers advertise a five-minute native validity window. Both session and
card sources stop returning them at that original expiry; Dynamic's existing
native expiry scheduler removes an already received card without needing a
new phone update. A paused timer remains saved and resumable in Timer. Resume
publishes its new running deadline. Explicit paused edits/extensions create a
fresh five-minute window; passive polling, reopening and sharing do not.

Paused reconfirmation advances the revision while preserving the saved
observation and expiry. This also caps an older day-long lease from the saved
observation, without changing the 44-byte model or deleting an expired pause.
No library or Dynamic source/binary changes were required. The existing
Dynamic 0.9.0 native expiry evidence retains its original scope.

- Two strict C99 suites pass. New cases cover repeated reconfirmation before,
  at and long after expiry, old lease migration, independent saved slots,
  restored hidden pauses, resume, explicit edits/extensions, revision overflow,
  backward RTC and saturating timestamp arithmetic.
- All **14 JavaScript handoff tests** pass. The new tests execute the separate
  cold provider for both schemas just before, exactly at and after expiry;
  repeated native reconfirmation cannot renew it. A later native resume makes
  both sources available again with the correct remaining/elapsed baseline.
- All **33 release-helper tests** pass. The SDK 4.33.1 Emery distribution build
  passes with native app, PKJS, callable provider, original identity and pinned
  Activities 0.2.0. Resources are 4,092 bytes; prestartup RAM footprint is
  15,480 bytes, with 115,592 bytes of SDK-reported heap before runtime use.
- [29 native smoke captures](qa/0.3.1/native/emulator.json) cover migration,
  running/paused detail, edit, resume, 24 hours, eight slots/reuse and native
  background Wakeup. Thirteen representative frames were inspected directly.
  The four background/cleanup frames were refreshed in a focused replay;
  both runs used isolated Emery QEMU FIFOs with phone JavaScript omitted.
  The native-only installation matches the final binary/resources/manifest,
  and cleanup returns to the identical empty-list frame. These captures check
  native regression behavior; the exact five-minute boundary is exercised by
  the C and JavaScript tests above, rather than claimed as a five-minute
  physical or emulator watchface session.

Candidate PBW SHA-256:

`af6eaf21a2ee12a524537d3ae274acfdfc0b55826ee1877a4b5e7bf36998a00d`

These checks establish native lifetime and source behavior. The actual
phone/watch procedure in the README also checks expiry and resume through live
Timer-to-Dynamic delivery. Physical vibration, radio behavior and battery use
remain device checks. The 0.3.0 native deadline evidence below is historical;
the running deadline and Wakeup broker paths were unchanged in 0.3.1.

## 0.3.0 local candidate — historical

The local `feature/activity-views-v2-2026-11-02` branch adds native Remaining and
Elapsed detail, immutable configured-duration publication metadata and a schema 2
card source alongside schema 1. Both source items share one `activity` category
declaration with the union of their 46 properties. The native persisted model
remains 44 bytes; bank storage, run identity and Wakeup ownership are unchanged.
This candidate is unpublished. Historical 0.2.0 evidence below belongs to that
release and is preserved separately.

| Check | Result and scope |
| --- | --- |
| Pinned dependency | Activities 0.2.0 archive, lockfile and installed package; source commit and exact checksum in [vendor/ACTIVITIES.json](../vendor/ACTIVITIES.json) |
| `npm test` | Two strict C99 suites and all 12 JavaScript handoff tests pass |
| `python3 -m unittest discover -s tests -p 'test_*release.py'` | All 33 release-helper tests pass, including the new ACT2 compiled-module/source regression |
| Distribution build | SDK 4.33.1 built the Emery 0.3.0 native app, PKJS, callable provider and notices |
| `node scripts/verify-timer-bundle.js EXTRACTED_PBW_DIRECTORY` | Repacked distribution scripts pass numeric/symbolic keys, multiple-slot receipts, offline replacement and cold session/card provider reads; all emitted shapes are declared |
| Working-tree release audit | The release validator passes against current local source: manifest, locked dependency, notices, native CRC/UUID/version, exact PKJS modules, rebuilt provider and packaged runtime; release preparation separately verifies the archived committed snapshot |
| Native smoke | [29 fresh captures](qa/0.3.0/native/emulator.json), including migration, Remaining/Elapsed detail, pause/resume, eight slots, cleanup and bridge-free background Wakeup |
| Deadline broker | [14 fresh captures](qa/0.3.0/native/deadlines/deadlines.json), including equal deadlines, deadlines five seconds apart and eight-slot cleanup |

The C suites cover elapsed pause exclusion, unchanged source revision while
reading a clock, terminal boundaries and Edit baseline reset. The JavaScript
checks cover both source schemas, independent slot reads, original native
observation/expiry, conflicting duration metadata and bounded authoritative
completed results. Saved older snapshots still work without invented elapsed
time. The local Node runner adapter described below was also used for these
portable checks; production code and assertions were unchanged.
The release auditor now includes `view.js` when that module exists in the
pinned Activities package. Source-map presence, source bytes and actual compiled
module bytes remain mandatory; missing or altered ACT2 modules are rejected.

After the native captures, only `appinfo.json`'s `plugin.sources` field was
repacked to declare the two source items under one category. Every other ZIP
entry retains identical payload and compressed bytes, including PKJS, provider,
native binary, resources and native manifest. No SDK rebuild was performed for
this metadata correction. Both QA reports record the previous PBW hash, the
manifest-only repack and verified identical native payload hashes. The native-only
deadline PBW was also checked against those exact native payloads, and all 43
recorded capture hashes were verified.

The final local `dist/Timer-0.3.0.pbw` SHA-256 is:

`fbf6de60e1704132a973f5c608279cc4710710f6438133320df22cc95991be16`

All 14 deadline captures and the representative running, paused and 24-hour
detail captures were visually reviewed. Countdown, Remaining/Elapsed labels,
state, progress bubbles, controls, alarm screens and cleanup fit the native
200×228 screen. These are local artifact and emulator results. Real phone/plugin
delivery, physical vibration, actual Bluetooth loss/reconnection and battery use
remain device checks. The release workflow archives this final source and
documentation, then validates the committed candidate before publication.
Those local packaging checks do not publish a release.

## 0.2.0 testing prerelease — historical

Xef authorized publishing 0.2.0 as a GitHub development prerelease for actual
phone/watch testing on 2 November 2026. The distribution PBW is unchanged from
the package and native evidence below. A dedicated quick Extend action is
deferred; the existing Edit control changes the duration. No new gesture or
increment is implied. Real-device testing is the next phase of this prerelease.

## 0.2.0 local recovery — 2 November 2026

This is an unpublished candidate on `recovery/0.2.0-2026-11-02`. GitHub was
checked through ordinary Git/GitHub CLI: `main` and v0.1.1 still identify
`23f8c369973aeade578ced5a07e6e4346e456995`. The recovery patch passed
`git apply --check` at that exact base and reproduced every manifest-listed
snapshot file. The recovered implementation was committed before further work.
No release, tag or remote branch was changed.

### Checks executed locally

| Check | Result and scope |
| --- | --- |
| `npm ci --ignore-scripts` | Installs the pinned Activities 0.1.2 tarball; no sibling checkout |
| `npm test` | Two strict C99 suites and six JavaScript handoff tests pass |
| `python3 -m unittest discover -s tests -p 'test_*release.py'` | 32 release-helper tests pass |
| `npm run build` | SDK 4.33.1 builds an Emery distribution PBW with manifestVersion 2, native app, PKJS, callable `plugin.js` and notices |
| `node scripts/verify-timer-bundle.js EXTRACTED_PBW_DIRECTORY` | Actual packaged scripts pass symbolic/numeric keys, matching receipts, multiple slots, offline replacement and cold provider reads |
| Native smoke | 29 fresh Screenshot-service captures cover migration, list/editor/detail/alarm navigation, 24 hours, pause/resume, snooze, dismissal, reopening, eight slots and slot reuse |

The local execution environment denies Node's captured synchronous subprocess
pipes. Portable suites were rerun with a runner adapter using regular temporary
files and `--test-isolation=none`; production code and assertions were unchanged.
The SDK/tool state was isolated in the common recovery workspace. The SDK reports
4,092 resource bytes, a 15,267-byte static RAM footprint and 115,805 available
heap bytes before runtime allocations. The packaged native binary is 14,700
bytes. These are build measurements, not battery or runtime-heap measurements.

Release preparation also accepts the SDK-injected source module when its
source-map name has a directory prefix. Regression checks retain duplicate
rejection and exact compiled-module/source comparison. This portability fix
changes release validation, not the packaged runtime.

### Current native evidence

[Fresh emulator report](qa/0.2.0/local-recovery/emulator.json) records the exact
distribution PBW and native payload hashes, plus all 29 PNG hashes. The PBW's
SHA-256 is:

`4e5d6e0caa1b92e36016da753c6a6d68d4f6651e6a222343040b1431eea1c7f2`

The emulator is SDK 4.33.1's Emery firmware, run headlessly over local FIFOs
because this environment prohibits socket listeners. No phone JavaScript runtime
was attached. Migration was seeded with the repository's isolated legacy fixture:
44-byte paused model at key 2100, 298 seconds remaining, retaining publication
ticket key 2102. The final Timer then displayed the migrated timer and completed
the normal button workflow.

For the background alarm check, an emulator-only diagnostic PBW omitted all
phone JavaScript while retaining the distribution's exact native payloads.
A five-second timer was started, the app was exited, and native Wakeup returned
to the alarm screen automatically. The diagnostic PBW is a build output and is
excluded from the release. It must never be installed as the distributable PBW.
Requesting QEMU's disconnected Bluetooth state leaves its SDK transport open;
this does not establish behavior during a real Bluetooth disconnection.

The additional [deadline broker captures](qa/0.2.0/local-recovery/deadlines/deadlines.json)
exercise two equal deadlines and two deadlines five seconds apart, leaving the
app before expiry and dismissing each pending alarm. An
[emulator-only persistence fixture](../tests/deadline-fixture/README.md) seeds
these deadlines before the actual Timer native payload schedules its Wakeup;
the watch clock stays unchanged while alarms are scheduled. The probe rejects
launcher screens in place of alarms. It also replays eight-slot cleanup and
the isolated background alarm after correcting the smoke script's navigation.
Portable collection tests
also check simultaneous completion, bounded slots, revision order, expiry and
snooze. Neither a phone receipt nor a displayed zero proves native completion.

The watch-owned implementation was reviewed alongside the tests: persistence
uses checked, bounded chunks in a new bank before committing its header;
legacy state is retired only after a verified save. One native Wakeup follows
the earliest running deadline, and every due slot is reconciled at callback.
Scheduling failure remains visible and Back does not depend on phone sharing.

### Historical evidence

The parent [emulator report](qa/0.2.0/emulator.json) and its 29 captures came from
the recovered Work session. Their PNG hashes were checked without altering them.
Their recorded candidate PBW was excluded from the supplied ZIP by the ignored
`dist/` directory, despite the handoff saying it was included. The new SDK build
has different native metadata/build identity, so that old report is retained as
historical evidence. The fresh report above binds the current candidate to its
own captures. Historical `releases/Timer-0.1.1*` bytes remain unchanged.

### Release preparation and remaining device checks

After the artifact and native checks, `python3 scripts/package-release.py`
freezes the candidate in `releases/`, including the final documentation and
source. The committed candidate is checked with
`python3 scripts/github-release.py prepare --output /tmp/timer-release`.
Source or documentation edits require regenerating the source archive and
checksums, committing them, and repeating committed-HEAD preparation.

Before treating the release as device-validated, complete the actual phone/watch procedure in the
[README](../README.md#actual-phone-and-watch-checks): upgrade an existing
Activity Timer installation; verify plugin permissions and delivery to Dynamic;
restart the phone app; reuse a slot while disconnected; test nearby deadlines,
snooze, dismissal, Quiet Time and competing system Wakeups. With Bluetooth
actually disabled and the phone absent, verify automatic launch and physical
alarm vibration. Measure power use separately if required.

No physical watch or phone was accessible in this execution environment; the
final ADB check listed no attached devices. Emulator and packaged-runtime results do not
certify Bluetooth behavior, physical vibration, phone delivery or battery use.

## Local 0.14.2 whole-composition revision

The candidate remains unpublished on `design/pebble-native-redesign`, after
`fad44ed`/0.14.1. [The current validation record](qa/0.14.2/validation.json)
binds every final native report to the same distribution PBW. The original
approved eight-screen composition set is frozen separately under its preview
candidate hash; a later one-row list-index correction changed the final native
binary without changing those approved representative compositions.

Robustness checks include the normal portable owner/model, collection, Glance,
appearance, alarm feedback, outline/raster and motion tests;34 explicitly
reported JavaScript tests;46 Python provenance/release tests; an integrated
SDK4.33.1 distribution build; optimized packaged settings/provider execution;
compiled PFO/linked-outline/source/license checks; and previous-artifact
immutability. The three extended Roboto resources total39620bytes. SDK static
RAM is46844bytes with84228bytes available heap; these are SDK estimates, not
runtime peak-memory or battery measurements. A clean repeat build reproduced
every executable/resource/script byte; only the SDK manifest's `generatedAt`
changed. [The rebuild record](qa/0.14.2/rebuild-identity.json) proves that exact
difference. The final PBW retains the original tested manifest and hash.

Exact distribution-native Emery QEMU checks produced26 layout/theme/storage
captures,47 motion/input frames,9 physical-button-feedback captures and34
all-eight viewport captures. A further38 workflow captures cover normal edits,
Cancel/Keep/Back restoration, Reset/Ready/Start, full/reusable collections,
actual23:59:59 editor entry and real exhausted-generation failures through
native completion. Twenty-seven actual PNG-ink checks verify complete groups,
selected boxes, warnings, units, footers and the19px visible clock. Critical
copy stays whole and all8 normal/critical selected indices stay visible in
both directions. Navigation preserves owner identities/deadlines; a read-only
probe preserves the critical-bank checksum. Selection remains rigid120ms;
labels and owner actions remain immediate. The screenshot RPC samples frames
and does not measure physical frame rate or motor behavior.

All native fixtures use isolated stock-derived local FIFO emulator data and
shipping native bytes. The UI scripts simulate AppMessage receipt/theme
boundaries; they do not run the real phone application. Source-only phone CSS
inspection and the labelled mock preview remain separate from browser testing.
Fresh Chromium startup remains blocked by the existing sandbox restriction;
real phone/WebView persistence, physical-watch readability, vibration and
battery checks remain unperformed. Main and prior versioned artifacts remain
unchanged. No tag, commit, push or publication is performed by this validation.
