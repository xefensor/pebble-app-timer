# Native deadline fixture

This emulator-only app seeds a valid versioned collection with two running
timers: both expire after 20 seconds, or the second expires five seconds later.
It cancels old Wakeups, preserves publication ticket key 2102, and does not
schedule an alarm itself. The actual Timer distribution's native payload must
then load the collection and schedule its own earliest-deadline Wakeup.

`src/c/timer_model.*` and `timer_collection.*` are exact copies of the app's
current model sources; the deadline probe checks that identity. Update the
copies when those app sources change. Fixture `main.c` only writes checked
native test state and displays setup status. It never belongs in the release PBW.

From this directory, build the two setups before running the native probe:

```sh
TIMER_DEADLINE_OFFSET=0 pebble build
cp build/deadline-fixture.pbw ../../build/deadline-simultaneous-fixture.pbw
pebble clean
TIMER_DEADLINE_OFFSET=5 pebble build
cp build/deadline-fixture.pbw ../../build/deadline-nearby-fixture.pbw
```

From the repository root, run `scripts/emulator-smoke.py` with the SDK's Python
environment first. It produces the native-only copy of the current distribution.
Then run `scripts/emulator-deadlines.py`. Both select Emery explicitly and replace
its Timer test data. The deadline probe checks background launch, both pending
alarms, eight-slot cleanup and an isolated five-second Wakeup. It rejects a
launcher screen in place of an alarm. No watch-clock changes occur while timers
are scheduled. Evidence stays under `docs/qa/0.2.0/local-recovery/`.
