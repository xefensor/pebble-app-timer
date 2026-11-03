# Timer UX review — 0.9.0

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
