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
