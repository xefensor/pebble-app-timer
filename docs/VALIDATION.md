# Timer validation

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
