# Timer UX review — 0.8.0

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
