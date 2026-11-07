# Historical UX-REVIEW.md

Retained before the2026-11-07 rework. Use current UI/maintenance/development
documentation for work on main; older evidence retains its original scope.

# Timer UX review

## 0.11.1 clock, falling sand and alarm feedback

Every screen now reserves the existing top band for a short context title on
the left and the current local time on the right, following Dashboard's
composition. The clock respects the watch's 12/24-hour setting. Compact editor
and Options titles retain their specific context below the header. Delete
confirmation warnings move beneath the header, leaving the clock and both
choices visible. The countdown and normal hourglass keep their existing size.

The falling stream and individual grains are intentionally larger so running
progress is visible on the watch. Grain positions use the existing one-second
tick. Proportional chamber fill and the 300ms rotations remain independent;
paused and Ready glass do not gain continuous animation.

Pending alarms repeat a double vibration every three seconds while Timer is
open, checking Quiet Time before each pulse. Acknowledgement or Snooze clears
the corresponding pending alarm after the native save succeeds. If another
alarm remains, feedback continues. Failed acknowledgement does not silence a
still-pending result. Saved unacknowledged alarms resume feedback on reopening;
exiting the process stops its feedback timer. Physical vibration feel remains
a watch check.

## 0.11.0 hourglass and readability

The countdown keeps a fixed position while the hourglass communicates state.
Setup and acknowledged Ready have their sand at the bottom. Starting rotates
the glass to put the remaining sand above; Pause rotates it onto its side;
Resume returns it upright. A finished alarm has its sand below without an
extra flip. Proportional sand is still derived from the native model.

Only the glass rotates, for at most 300ms. Rapid Pause/Resume starts from the
current decorative angle. Opening an existing timer, ordinary ticks and phone
receipts do not replay the animation. Covering the app stops its decorative
timer; the countdown and native Wakeup continue independently.

Labels and guidance now primarily use 18, 19 and 23px body fonts across the
list, duration editor, detail, Options, confirmation and alarm. The native
right-side action bar and established navigation remain. Critical error
compositions reserve space for the warnings and available alarm actions.
Long state/duration subtitles use 16 or 15px only when necessary to preserve
the complete value on one line beside the progress bar. Shorter replacement
guidance and HRS/MIN/SEC labels fit at the normal larger size. Invalid setup
values keep the same bottom-sand pose while disabling the Start action.

Dashboard Pause/Resume is deferred by user choice. The
[platform investigation and proposed handoff](design/dashboard-timer-control.md)
record why an invisible control cannot safely operate a separately installed
Timer through the supported SDK. No new control is advertised by the provider.

## Earlier 0.9.0 review

This pass follows the complete watch journey: empty and mixed lists, capacity,
duration entry, running/paused/Ready detail, Options, safe deletion, native
completion/Snooze/Done and failure recovery. The hourglass and normal detail
composition are retained. The improvements focus on where the user is, which
actions are available and what remains safe when a native operation fails.

| Area | Improvement |
| --- | --- |
| List orientation | Larger Timer N of M footer, static scroll thumb and a selected row kept in the visible window |
| Capacity | Amber Full heading and focus on an existing timer; Select opens that timer while the unavailable Add row is skipped |
| Empty/list boundaries | Only the available create/open/up/down actions have action-bar icons |
| Options context | Selected timer position, saved repeat duration and state remain above Change/Restart/Delete |
| Invalid duration | An amber seconds field and explicit five-second minimum replace an actionable-looking commit check |
| Maximum duration | Locked minutes/seconds at 24 hours use a gray selection and omit ineffective plus/minus icons |
| Critical errors | A short title and Keep Timer open remain readable; list rows and Delete choices retain their space |
| Alarm with a write failure | A compact hourglass reserves space for both Done/Snooze hints and the critical warning |

The earlier saved-count label was numerically correct: the two 8 glyphs in the
full-list frame have identical ink. Its small type was difficult to read, and
it did not explain position among off-screen timers. The new footer addresses
readability and orientation rather than claiming a count-model bug.

New native evidence uses the exact candidate binary and resources. Mixed-state,
equal-duration, invalid/locked-field and Full journeys are exercised alongside
the existing navigation, acknowledgement and repeat tests. The scheduling-error
fixture owns a separate conflicting Wakeup and is explicitly cleared. A valid
V1 bank at the maximum generation exercises Timer's actual refusal to commit
another bank: failed Done stays pending, and Back remains available. Fixture
screens are labelled separately from actual Timer screens.

The model, collection, Glance, phone publisher/provider, library pin and leases
are unchanged. No extra timer, background polling or automatic text motion is
introduced. The existing 180ms transition remains bounded and is cancelled when
the app is covered. Physical button feel, phone transport and battery remain
device checks. See [validation](VALIDATION.md) and versioned QA for exact scope.

## Earlier 0.8.0 review

Timer's settings belong in its on-watch Options and duration editor. Its phone
code only publishes snapshots, so a new phone form would duplicate state with
no useful setting to control. Native timers, alarms and saved slots keep their
existing ownership.

The list now calls its add action **New timer**. Eight saved timers can include
acknowledged Ready entries that may be reused. Selecting New timer in that state
shows **Replaces oldest ready** before entering the editor; **Replace saved**
and its warning remain visible before starting. All eight ineligible slots
still show Full. Running, paused and unacknowledged alarms are never reclaimed.

Cancel now follows the navigation path. An editor opened from Options returns
to Options; a direct detail edit returns to detail; a new edit returns to the
list. Delete Keep and Back restore Options with the same row selected. Saving
an edit still returns to the timer detail so its result is visible. Back hints
explain the previous field or cancellation for Hours, Minutes and Seconds.

Restart names the saved repeat duration. This makes its result clear after
changing remaining time or snoozing, without adding another confirmation step.
Delete retains a separate confirmation with Keep selected first. The model,
bank formats and timer operations are unchanged.

Native review found the alarm's Snooze hint touching the last display row.
Both hints move up eight pixels, preserving their spacing and the hourglass
while leaving eight pixels below the final descender. Alarm actions are unchanged.

The review also covered empty, running, paused, Ready and alarm states,
button holds, failed storage/alarm scheduling, sharing status and Back exit.
Critical failures remain visible; routine sharing status remains in Options.
Held buttons cannot act again after opening another screen. Alarm Up snoozes,
Select and Back acknowledge, and the existing Down acknowledgement remains
available for compatibility. An acknowledged result stays saved for explicit
repeat without remaining on activity consumers.

Actual emulator navigation/capacity checks and packaged-source checks are
recorded in [validation](VALIDATION.md). Emulator data and simulated receipts
do not establish physical-watch latency, live phone delivery or battery cost.
