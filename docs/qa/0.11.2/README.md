# Timer 0.11.2 activity reliability

Candidate PBW SHA-256:
`597429bab50353d13508f9edd2abf3915f6b99b2c725d587365e7451d6fd982a`.

- [Portable checks](checks/portable-tests.log): 28 JS checks and eight C suites.
- [Python regressions](checks/python-tests.log): 42 release/module/optimization checks.
- [SDK build](checks/sdk-build.log), [font audit](font-audit.json) and
  [actual packaged runtime](packaged-runtime.log) pass.
- [Packaged transport comparison](transport-work.json) executes the old and new
  distribution scripts in isolated VMs. It measures duplicate startup requests,
  recovery after a thrown bridge call and single ownership despite late callbacks.
- [Source/payload identity](ownership-identity.json) preserves native timer state,
  alarms, UI, clocks, provider schema and the pinned publisher dependency.

Bootstrap retries follow failed sends only: at most three total requests with
500/1,000ms backoff. Publisher acceptance cancels pending retries; a stale or
rejected snapshot cannot falsely claim success. Native receipt and retry behavior
remains authoritative. Repeated connection events do not start another handoff.

The UI is unchanged from 0.11.1; its original native captures are historical
evidence linked by source identity, not new 0.11.2 screenshots. This pass does
not measure physical battery savings, radio delivery or real-phone behavior.
Published artifacts and earlier local 0.11.1 files are preserved.
