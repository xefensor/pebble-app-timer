# Timer validation

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

Before publication, complete the actual phone/watch procedure in the
[README](../README.md#actual-phone-and-watch-checks): upgrade an existing
Activity Timer installation; verify plugin permissions and delivery to Dynamic;
restart the phone app; reuse a slot while disconnected; test nearby deadlines,
snooze, dismissal, Quiet Time and competing system Wakeups. With Bluetooth
actually disabled and the phone absent, verify automatic launch and physical
alarm vibration. Measure power use separately if required.

No physical watch or phone was accessible in this execution environment; USB
and ADB access were unavailable. Emulator and packaged-runtime results do not
certify Bluetooth behavior, physical vibration, phone delivery or battery use.
