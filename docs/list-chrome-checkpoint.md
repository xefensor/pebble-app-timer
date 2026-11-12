# Timer list appearance checkpoint — 2026-11-12

The New/Replace position now highlights the full 166×32 px header, including
the wall clock. Light mode uses black with white text, and dark mode uses white
with black text. While that position owns focus, the native MenuLayer's remembered
row uses normal colors; Down restores ordinary header colors and the row highlight.

The list starts at `XOCE_UI_HEADER_HEIGHT` (32 px), removing the old 18 px gap.
Its normal viewport grows from 178 to 196 px. Warning panels retain their full
height at the bottom; the Replace notice still starts at y=194, giving that list
a 162 px viewport. Single and multiple timers share the same top anchor.

Per-timer blue/yellow progress tracks are removed. Row text positions, measured
inline/stacked fits, row heights, complete numeric values, fonts and action rail
are unchanged. Editor, details, options, confirmation, alarms, timer banks,
deadlines, Wakeups, protocol keys and action bindings retain their existing rules.

## Verification

Candidate: `build/candidate/Timer-0.14.5.pbw`.

SHA-256: `c5bef4df45f57ebca2540dfe89f9e17a0631d59626f87f54b51683766720ae44`.

`npm test`, `npm run build`, `npm run check:artifact` and
`npm run check:config` passed. The portable production-paint regression verifies
header colors/background bounds in both themes, normal rows under header focus,
and absence of progress-track/fill drawing. Existing row-fit boundaries, owner,
Wakeup, persistence, localization, packaged transport/settings and release tests
remain passing.

The new focused native check fails on the previous candidate
(`c7265a5378ad8c5b894ff98c1e5bd75bdcef818d780534ab3b9bdc101879799f`)
at the missing header highlight, and passes against this candidate:

- 60 exact-candidate focus captures cover Small/Medium/Large in light/dark,
  empty New, first-row selection, header-only focus, the upper boundary,
  Add cancellation/Back, row focus restoration, Replace/its complete notice and
  a full busy collection. Timer-bank checksums remain unchanged during navigation
  and focusing Replace does not replace anything.
- The full checkpoint passes with 227 captures. It covers all eight rows,
  single/multiple anchors, scrolling/holds, Select during motion, state-driven
  sorting, long values, complete critical warnings, alarm interruption and native
  type/rail comparisons. This count includes explicitly separate starting-build,
  direct-SDK reference, diagnostic and Czech/German lookup-fixture frames.
- Independent SDK bitmap comparisons verify unchanged title, header and footer
  fonts. Forty diagnostic open/Back cycles provide 80 bounded heap samples;
  another 20 cycles use the exact candidate without diagnostic instrumentation.
- 60 exact-candidate navigation captures verify Details, Add/Edit, Options,
  safe Keep-first confirmation, repeat-duration behavior, deletion, alarm
  preemption, storage refusal and ten nested navigation cycles.

The English paused-timer fixture measures rows at 50/56/60 px for Small/Medium/
Large. Its 196 px viewport now fits three complete rows at all sizes. Longer
labels/values can still require taller rows; complete selected rows remain visible.
The old check that Large must always fit fewer rows was replaced with measured
font-height/count ordering, while retaining exact font bitmap comparisons.

Evidence is bound to PBW/native hashes in:

- `build/qa/timer-checkpoint/c5bef4df45f5/1794483216846793823/list-chrome-report.json`
- `build/qa/timer-checkpoint/c5bef4df45f5/1794483502326733070/report.json`
- `build/qa/native-navigation/c5bef4df45f5/1794483216817073189/report.json`

The consolidated report and logs are under `build/qa/list-chrome-checkpoint/`.
Historical QA is retained. Fresh reference/diagnostic builds use
`PEBBLE_QA_OUTPUT_ROOT=build/qa/list-chrome-checkpoint` to avoid replacing earlier
reference files. Reproduce header checks with
`python3 scripts/emulator-timer-checkpoint.py --list-chrome-only`; the ordinary
checkpoint and `scripts/emulator-timer-navigation.py` cover the broader flows.

SDK 4.33.1 / CLI 5.0.40 and Emery firmware v4.33.2 were used. The available
Node 26.12.0 / Python 3.13.16 differ from the contributor runtime pins. Local
validation sourced `recovery-validation/env.sh` with `PEBBLE_RECOVERY_SKIP_NPM=1`
and `PEBBLE_RECOVERY_TIMESTAMP=1794470400`, reusing installed verified dependencies
without npm lifecycle scripts. All data-writing helpers remained emulator-only.
No physical watch, real paired phone/provider, release or publication is claimed.
