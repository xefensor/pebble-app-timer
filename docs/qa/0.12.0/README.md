# Timer 0.12.0 Reset to Ready verification

The local `feature/reset-to-ready-0.12.0` branch starts from Timer 0.11.2 commit
`0c378ec8d037822d65ed3df7c48a4c542a91539b`. More options now offers Reset with
the saved repeat duration. Reset stops the current run and returns the same saved
timer to Ready; Select starts a new run only when explicitly pressed afterward.
The native Ready countdown and bottom-filled hourglass remain still.

The model keeps the existing slot, origin, run identity and saved repeat duration.
A successful reset advances the revision and persists a cancelled withdrawal,
without a deadline or pending alarm. Already-Ready Reset is a no-op. Remaining
edits and Snooze retain the separate saved repeat duration; editing that duration
while Ready does not start a run. Existing bank sizes, legacy migration and app
identity are retained. The model/collection tests cover these ownership rules,
including invalid/boundary operations and decoder/alarm-mask compatibility.

| Check | Result |
| --- | --- |
| Source handoff JavaScript | 31 passed; active/paused Reset, cold reads, sibling preservation, stale/new-run safety and failed saved withdrawal |
| Portable native | 8 C suites passed; model/collection ASan/UBSan passed |
| Release/optimization Python | 42 passed |
| Final optimized PBW scripts | Numeric/symbolic keys, cold summary/live/session/card, Reset withdrawal and explicit new-run Start passed |
| Native Emery | Focused `reset` and `storage` phases passed against the exact final payload |
| Native frames | 21 captures: 19 Timer UI, 1 stock OS foreground and 1 explicitly labelled storage fixture |
| Fonts/resources | Packaged font identity passed; SDK reports 36,123 bytes RAM and 22,802 resource bytes |

The [native report](native/ux.json) records the exact distribution/native hashes,
direct QA AppMessage records, fixture hashes, separate isolated flash paths and
firmware log hashes. Its required phases are only `reset,storage`; the older full
UX, capacity, launcher, migration and clock batches were not rerun or claimed.
The existing UX/settings harnesses were updated to require Reset followed by
explicit Start instead of assuming that the Options command starts immediately.

The [Reset contact sheet](native/reset-contact-sheet.png) shows:

- A running 20-second timer returning to Ready with its full saved duration.
- No Timer foreground launch at that timer's former deadline; its untouched
  25-second sibling still launches the real native alarm. Foreground UUIDs and
  unchanged sibling run/deadline are checked independently of screenshot color.
- A paused timer with edited remaining time resetting to its original ten-minute
  repeat, and a one-minute Snooze resetting to the original ten-second repeat.
- A Ready duration edited to twenty seconds, repeated Ready Reset leaving its
  revision/observation/lease unchanged, and a process reopen retaining Ready.
- Explicit Select alone creating the new twenty-second run with a native deadline.

The [storage contact sheet](native/storage-contact-sheet.png) uses the existing
exhausted-generation V1 bank fixture. The shipping native binary is unmodified.
Reset fails visibly, keeps the existing Running timer through forced process
reopen and does not publish an unsaved withdrawal after a QA snapshot request.
The original deadline still triggers its native alarm; failed Done cannot hide
that unsaved alarm, and Back remains available. The critical warning leaves the
clock, both Delete choices and alarm Done/Snooze actions readable.

The first storage attempt exposed an obsolete harness assertion: it expected the
Delete warning at the top-left pixel, before 0.11.1 moved it below the clock.
[The preserved first-attempt evidence](native/storage-before-harness-update/failure.json)
contains the original seven images. Correcting the assertion to the existing
warning band required no production change. The whole storage phase then passed
again on a fresh isolated flash. Final native PNGs retain unmodified 200×228
source pixels; contact sheets are labelled 2× nearest-neighbor derivatives.
Root independently reviewed both final sheets, including the failed Reset,
reopen, Delete choices and alarm warnings.

Cold provider tests distinguish withdrawal from deleting compatibility records.
Schema4 summary omits the reset timer and advances its complete-response header
while its sibling remains. Live/card retain a cancelled tombstone with
`show_terminal:false`; legacy session retains cancelled state without that flag.
Original observation/expiry stay finite through reconfirmation and reads. An old
queued active snapshot is rejected; even a newer active revision under the same
terminal run is rejected. Explicit Start uses a new run ID and atomically
replaces the prior slot record. A failed phone save keeps the previously accepted
sources and emits no successful receipt; a later native resend can recover.

Final PBW SHA-256:
`2ff486c1af98f9f62325d49d61f92ab43abe20d2159f3e9bfb03c032abc30560`.
Debug ZIP SHA-256:
`04057ff2c180db8e785c5d49259eeb655b1f504e6f24f4938a7deef4ef6c63cb`.
No later SDK rebuild or payload inheritance is needed for these native captures.

The emulator runs the real native candidate with synthetic fixtures and a Python
snapshot/receipt peer. It does not execute phone JavaScript. The packaged checks
execute actual optimized scripts in isolated SDK models. No real phone,
physical watch, live plugin action, physical vibration or battery measurement is
claimed. No watch-time reset occurs during the native deadline checks.

```sh
source ../recovery-validation/env.sh
npm test
TIMER_QA_PIPE=1 TIMER_QA_PHASE=reset TIMER_QA_EXPECT_PHASES=reset,storage \
  TIMER_QA_EXPECT_SHA256=2ff486c1af98f9f62325d49d61f92ab43abe20d2159f3e9bfb03c032abc30560 \
  TIMER_QA_NATIVE_PBW=/tmp/timer-0120-native.pbw \
  PEBBLE_QA_STATE=/tmp/timer-0120-reset PEBBLE_QA_FLASH=/tmp/timer-0120-reset/spi-flash.bin \
  python3 scripts/emulator-ux.py
TIMER_QA_PIPE=1 TIMER_QA_PHASE=storage TIMER_QA_EXPECT_PHASES=reset,storage \
  TIMER_QA_EXPECT_SHA256=2ff486c1af98f9f62325d49d61f92ab43abe20d2159f3e9bfb03c032abc30560 \
  TIMER_QA_NATIVE_PBW=/tmp/timer-0120-native.pbw \
  PEBBLE_QA_STATE=/tmp/timer-0120-storage-final PEBBLE_QA_FLASH=/tmp/timer-0120-storage-final/spi-flash.bin \
  python3 scripts/emulator-ux.py
```

These commands require the existing UX fixture variants. Use fresh temporary
state directories for a clean replay; never point the harness at a physical
watch. Published artifacts are preserved in the parent release verification.
