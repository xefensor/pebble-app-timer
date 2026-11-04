# Timer 0.11.1 local validation

Candidate PBW SHA-256:
`2b4c9ce770cb5a08b1a22da13c7788e77570158ebd15cd99ab7b87a8c0292652`.

- [Portable checks](checks/portable-tests.log): eight strict C suites and 22 JS
  checks. The new fake-SDK alarm scheduler exercises repeated pulses, multiple
  pending alarms, failed acknowledgement, Quiet Time, cancellation, reopening
  and timer-registration failure without stacking callbacks or first pulses.
- [Python regressions](checks/python-tests.log): 42 release, module and
  optimization checks.
- [SDK build](checks/sdk-build.log): Emery resources 22,802 bytes, RAM footprint
  35,831 bytes and 95,241 bytes available heap. These are SDK build figures,
  not peak-runtime or battery measurements.
- [Font audit](font-audit.json) and [packaged runtime](packaged-runtime.log):
  exact font/license identity and the actual distribution phone scripts pass.
- [Ownership identity](ownership-identity.json): native model/storage/Wakeup
  functions, phone scripts, provider metadata and font payload are retained.
- [Focused native report](native/ux.json): header placement, 12/24-hour switching,
  visible moving sand, paused stability, native alarm acknowledgement/Snooze,
  critical warnings and real idle minute rollovers. Both phases pass with 30
  native captures, including one labelled seed fixture.
- [Header fit](header-font-fit.json) checks all 2,160 clock strings and short
  titles against the packaged font. [Clock visibility](clock-visibility-audit.json)
  verifies clock ink on all 29 Timer screenshots; the labelled fixture is excluded.
- [Overview](overview.json): unchanged native screenshots with captions outside
  their 200×228 pixels.

Native layouts use the exact candidate payload in isolated Emery QEMU with
synthetic saved timers and explicitly labelled transport/receipt fixtures.
Vibration call timing is verified by the fake SDK; actual motor strength,
physical-watch feel, phone delivery and battery use remain untested.

Repeating feedback requires the Timer process. Closing it cancels the callback
and current vibration; saved pending alarms resume feedback on reopening.
Quiet Time is checked before each pulse and on native updates, since the public
SDK has no Quiet Time change subscription.

The broader 0.11.0 native regression evidence and its artifacts remain in
their original versioned locations. This focused follow-up does not claim
that all earlier migration, capacity or launcher phases ran again.
