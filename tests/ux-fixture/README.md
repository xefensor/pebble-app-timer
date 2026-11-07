# Isolated Timer UX fixture

This emulator-only app uses Timer's UUID to replace its isolated QEMU data.
Never install it on a physical watch. It writes the exact pre-0.5.0 44-byte
model / 372-byte collection format; it does not schedule alarms or implement
Timer behavior. Install the actual Timer candidate immediately afterward.

`UX_FIXTURE_MODE=0 pebble build` creates an empty V1 bank. Mode 1 seeds a pending
completion, an acknowledged expired completion and a running timer. Mode 2
seeds eight acknowledged completions. Modes 3 and 4 seed two running native
deadlines five seconds apart and equal, respectively. The actual Timer must
migrate the bank and schedule/handle its own stock PebbleOS Wakeup.
Mode 5 seeds eight mixed running/paused/Ready timers, including equal-duration
timers. Mode 6 seeds a valid 30-second V1 timer at the maximum bank generation.
The exact Timer candidate then encounters its actual exhausted-generation
write refusal while retaining native alarm/Back behavior.
Mode 9 combines mode 5's eight mixed timers with the exhausted bank generation,
so actual critical-warning layouts can prove every selected item remains visible
in the one-row readable viewport. Mode 8 is the read-only Glance/appearance/bank
checksum probe and never writes timer data.

Build all variants from this directory after activating the Pebble SDK:

```sh
mkdir -p build/variants
for mode in 0 1 2 3 4 5 6; do
  UX_FIXTURE_MODE=$mode pebble build
  cp build/ux-fixture.pbw build/variants/ux-fixture-$mode.pbw
done
```

From the app root, run `scripts/emulator-ux.py` with `TIMER_QA_PHASE` set to
`migration`, `ux`, `capacity`, `launcher`, `nearby` and `equal` in turn. Build
the original `tests/legacy-fixture` first for the singleton migration case.
The script copies the final distribution's native payload into a no-JavaScript
QA PBW and records its exact hashes. Its Python AppMessage receiver requests
saved native state and sends receipts; this is explicitly a QA transport,
not evidence of actual phone PKJS/provider delivery.

The `settings` phase checks editor/confirmation cancellation, saved-duration
Restart labels, eight-Ready replacement warnings and the eight-running limit.
For a focused run, set `TIMER_QA_EXPECT_PHASES=settings`; the report records this
scope explicitly. The default required phases remain the original six above.

The `uxpass` phase captures mixed-list position/scroll affordances, identical
timer Options context, invalid seconds and locked 24-hour fields. The `storage`
phase captures actual exhausted-generation states and native completion without changing
the candidate binary. It never claims a real phone or physical-watch test.

The `hourglass` phase verifies Ready/editor sand at the bottom, running sand at
the top, horizontal static pause, partial-sand proportions through resume,
unchanged ownership for other timers and opening an existing run without a
replayed start flip. A synthetic incoming call covers the app during a pause
transition in isolated QEMU; return must settle without replay. Screenshot RPC
timing may miss individual 300ms transitions, so the report records which samples
actually caught intermediate frames. This does not measure battery use or CPU
work underneath the cover.

The `legibility` phase captures replacement hints, editor labels and mixed long
durations. It enters 23:59:59 through the actual native editor to exercise the
longest list context. Review the unmodified native PNGs, not just the phase
completion flag. `TIMER_QA_EXPECT_SHA256` can require an exact distribution PBW
hash before any emulator connection; `TIMER_QA_EXPECT_PHASES` records the intended
suite without implying unperformed historical phases ran again.

The focused 0.11.1 `clock` phase covers the wall-clock header on all six normal
and critical screens, real QEMU 12/24-hour settings, thicker 1Hz falling-sand
pixels and paused body stability. Equal native deadlines exercise Snooze
leading to the next pending alarm, Done acknowledging it, and reopening
without re-alerting acknowledged work. Physical vibration is not measured by
QEMU; portable fake-SDK feedback tests cover vibration calls and cancellation.

The `clock-idle` phase waits for real minute changes while empty, paused and
editing, with no running timers or watch-time resets. Each header must change
while body pixels and paused owner observation/expiry stay unchanged. Screenshot
stability does not measure repaint frequency, CPU use or battery consumption;
the native SECOND-versus-MINUTE subscription policy is reviewed separately.
Use `TIMER_QA_EXPECT_PHASES=clock,clock-idle` for this focused follow-up.

The focused `reset` phase checks Reset from running/paused timers, Ready no-op,
saved repeat after remaining edits and Snooze, Ready-duration editing, process
reopen and explicit Start as a new run. Existing mode3 deadlines prove that a
reset timer no longer wakes Timer at its old deadline while its sibling still
launches the real native alarm. No watch time is changed. The `storage` phase
also attempts Reset against the existing exhausted-generation fixture: the
candidate must retain the old timer/alarm, show failure and publish no unsaved
withdrawal. Use `TIMER_QA_EXPECT_PHASES=reset,storage` for this focused task;
older full-suite phases are not automatically rerun or claimed.
