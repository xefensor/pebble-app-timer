# Timer UI/UX alignment handoff

Reviewed baseline: Timer 0.12.0; repository HEAD
`a21cc63877c9ba3fd4f14ffb671df6768a275486` on 2026-11-06.

## Canonical design and status

The agreed standard is maintained in the local `xoce` website checkout at
`src/pages/docs/design/pebble-ui-system.md`. When that checkout's preview is
running, read [the unified Pebble UI/UX standard](http://127.0.0.1:4321/docs/design/pebble-ui-system/).
The website has no public repository or deployment yet; this is a local reference.

This handoff records **approved design targets, with implementation pending**.
It does not claim that a released app follows the complete standard. Shared
visual roles use neutral light/dark surfaces and a restrained accent per app.
Exact accent colours and final pixel metrics still need reviewed native and
phone previews. Website colours and web fonts are a separate identity.

## Current implementation

- Timer already uses shared Fredoka number outlines and Roboto interface
  fonts, a short title-left/local-time-right header and a 34px contextual
  action bar beside 166px of content on Emery.
- The countdown stays fixed while the hourglass rotates for state changes.
  Ready has sand below, Running has remaining sand above, and Paused lies
  horizontally. Motion is bounded; covered and paused views do no decorative
  work. Running grains reuse the existing second tick.
- Normal screens are drawn white and alarms black. This is not a saved
  light/dark preference shared with Dashboard.
- Delete starts on Keep, but its confirmation currently says Delete timer?
  without showing the duration that identifies the target.
- Timer creation, editing and options live on the watch. There is no phone
  configuration form to restyle. Native storage, deadlines and Wakeup remain
  the timer owner's responsibility.

See [UX review](UX-REVIEW.md), `src/c/timer_ui.c`,
`src/c/timer_fonts.c`, and `src/c/timer_hourglass.h`.

## Pending work

- Apply shared neutral light/dark roles and a restrained app accent while
  preserving countdown legibility, proportional sand and warning meanings.
  The location of a new theme preference is unresolved: this handoff does
  not add a phone form or choose an on-watch settings flow.
- Align header spacing, type roles, menu selection, icon weight and feedback
  layouts with the common system. Keep critical storage/alarm warnings and
  Done/Snooze actions visible. Do not shrink essential content repeatedly to
  force it into a shared template.
- Name the exact timer on Delete using useful identifying context, such as
  its duration, while keeping Keep as the initial choice and Back returning
  to the same Options row. Review duplicate-duration cases as well.
- Preserve explicit Reset-to-Ready and a separate Select to start, the saved
  repeat duration, ordinary Back, independent sibling timers, five-minute
  paused-activity visibility, and acknowledgement hiding completed results.
- No phone timer editor, remote native-alarm control or Dashboard Pause/Resume
  is introduced by visual unification. If meaningful phone preferences are
  added in a later agreed scope, use the canonical stay-open/confirmed-Save
  pattern then; do not duplicate timer state merely to create a settings page.

## Acceptance checks for the implementation

- Capture list, editor, running/paused/Ready, Options, Delete and alarm states
  in the selected appearances, including long durations, duplicate durations,
  full capacity and invalid/locked fields. Check title/time and sidebar fitting
  on the actual 200 × 228 target.
- Cancel an edit or deletion and confirm the original screen, selected row
  and timer survive. A held button must not act again after changing screens.
- Reset without starting, close and reopen, then restart Timer; Ready and the
  saved duration remain. Verify other timers and native alarms independently.
- Exercise save and alarm-scheduling failure. The previous native state stays
  authoritative and actionable; decorative state must not falsely report a
  successful operation. Keep Done/Snooze reachable during warnings.
- Reopen and cover running/paused timers without replaying transitions or
  adding perpetual animation. Preserve Quiet Time and the existing open-app
  repeating-alarm behaviour; no closed-process vibration claim is implied.
- Run the relevant model, font/package, native and artifact checks when code
  changes. Record emulator fitting separately from real-watch readability,
  physical vibration and battery checks. Preserve published artifacts/tags.
