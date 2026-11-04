# Timer 0.11.0 local validation

Final candidate PBW SHA-256:
`4fb9ef3ab3581338842ae57bf2610abdef665886d1612f515c45a4eeeddebbbd`.

- [Portable tests](checks/portable-tests.log): seven strict C suites and 22 JS
  checks, including sand conservation, all used rotation scales and finite easing.
- [Python regressions](checks/python-tests.log): 42 release, module and
  optimization checks.
- [SDK build](checks/sdk-build.log): Emery resources 22,802 bytes; static RAM
  footprint 35,243 bytes and 95,829 bytes available heap. These are build figures,
  not runtime peak memory or physical-watch performance measurements.
- [Font audit](font-audit.json): exact bitmap resources, clock outlines and licences.
- [Packaged runtime](packaged-runtime.log): actual distribution PKJS/provider in
  isolated host VMs; no real phone.
- [Ownership identity](ownership-identity.json): retained timer, bank, alarm,
  Glance and phone sources, resources and provider metadata compared with 0.10.
- [Native report](native/ux.json): exact candidate hash, native-only payload,
  synthetic fixture scope, per-phase checks and original 200×228 screenshots.
- [Scheduling error/recovery](native/errors/errors.json): seven captures,
  including two explicitly labelled external-fixture surfaces. Together with
  the six native phases, 83 original screenshot hashes are verified.
- [Overview composition](overview.json): unscaled native screenshots with
  captions outside the watch pixels.

The first candidate's horizontal sand defect remains in
[rejected-candidate evidence](candidate-1-failed/). Other intermediate captures
retain their original candidate hashes and phase scope; they are not final
candidate evidence. The final report names only the phases actually completed.

Dashboard Pause/Resume is deferred by user choice. No action handler, new
permission or automatic Timer app switch is added. See the
[platform investigation](../../design/dashboard-timer-control.md).

No real phone, physical watch, battery drain, vibration feel or peak runtime
memory was tested. Published releases and tags remain unchanged.
