# Using Timer

## What you install

Install **one Timer PBW**. It contains the native watchapp, its PKJS phone bridge
and a read-only provider plugin. Timer owns its saved timer and native Wakeup
alarm. The bridge saves the last reported state on the phone, and the plugin
exposes that state primarily as `activity/summary` v4. The summary contains one
structured countdown and a separate optional destination app. A host such as
Dynamic chooses its primary and one-row secondary presentation. Timer supplies
no coordinates, mandatory display labels, derived elapsed detail or layout
selection.

`activity/live` v3, `activity/session` v1 and compact `activity/card` v2 remain
available for older consumers. One activity category declares all four items and their
properties. Every view keeps the original native identity, revision and
timestamps. Polling cannot renew source observation or validity, and a
five-minute-old paused timer remains saved in Timer after its activity expires.

[Pebble Activities](https://github.com/xefensor/pebble-lib-activities) supplies the
shared library and source convention. It is not another installed app or
background service. Timer's plugin may also appear in the phone app's Plugins
list because the same PBW provides both a watchapp and a data source.

Activity sharing requires a phone app with Pebble plugin support and the
relevant permissions or preview enabled. The native timer and an accepted
Wakeup alarm continue independently of sharing. The consumer receives a
phone-mediated snapshot, not direct access to another watchapp's memory.

## Controls

| Screen | Up | Select | Down | Back |
| --- | --- | --- | --- | --- |
| Timer list | Previous row | Open timer / New timer | Next row | Exit to watchface |
| Set timer | Increase selected field | Next field / start or save | Decrease selected field | Previous field / originating screen |
| Timer detail | Change remaining / saved duration | Pause / resume / repeat Ready | More options | Timer list |
| More options | Previous option | Choose option | Next option | Timer detail |
| Delete confirmation | Keep | Confirm selected choice | Delete | Cancel to Options |
| Time’s up | Snooze one minute | Done | Done (compatibility) | Done / timer list |

The editor sets hours, minutes and seconds, from five seconds to 24 hours.
The native app owns every deadline and alarm. Leaving the app or losing phone
connectivity does not cancel a successfully scheduled Wakeup.

The detail screen shows the remaining countdown, hourglass and Elapsed from one
native time read. Elapsed excludes paused time. Changing the remaining time keeps
the elapsed portion and adjusts the run's progress span; an unchanged save keeps
the deadline. The original saved repeat duration stays separate, including after
a one-minute snooze. Ready timers retain that saved duration for their next run.

Options and the duration editor control timers on the watch. The phone appearance
page selects Light/Dark and keeps the confirmed Save state open. The [UX review](../docs/UX-REVIEW.md) records the changes and retained behavior.

## Countdown in the app list

While a timer is running, its native deadline supplies an
[App Glance](https://developer.repebble.com/guides/user-interfaces/appglance-c/)
subtitle. PebbleOS updates the countdown while Timer is closed, without phone
JavaScript or a background worker. With several running timers, the earliest
deadline appears first and queued slices expire into the next running timer.
Paused or acknowledged saved timers do not leave a stale countdown behind.

The public SDK does not offer a temporary top-of-list position. The current
[launcher ordering implementation](https://github.com/coredevices/PebbleOS/blob/main/fw/process_management/app_menu_data_source.c)
reserves automatic promotion for specific system applications. This app updates
its subtitle and preserves the user's launcher order.
To keep Timer at the top manually, drag it upward in the phone app's
**My Apps** list, as described in
[Pebble's app organisation guide](https://help.repebble.com/en/articles/14166032-adding-watchfaces-apps).

The timer respects Quiet Time for vibration. PebbleOS controls missed-Wakeup
notifications. A scheduling failure displays “Alarm not set; keep open”
instead of claiming a background alarm was secured.

## Sharing and freshness

“Shared with phone” means that PKJS saved and read back the current revision and
returned a matching application receipt. It does not mean Dynamic has received
it. A native AppMessage transport ACK alone is insufficient.
Open More options to see this status; normal sharing messages do not cover the
countdown or hourglass. Storage and alarm failures stay visible while timing.

The provider marks its data as **last reported / unconfirmed**. Polling the
snapshot or reconnecting the phone cannot prove the watch timer has not changed
while disconnected. The original observation, deadline and validity are kept;
polling never makes an old snapshot fresh. A locally calculated zero does not
prove the native app completed or sounded its alarm.

State changes are persisted before publishing. Reopening Timer reconfirms the
native state. An unchanged paused timer keeps its original observation and
five-minute visibility deadline, including after that deadline has passed.
It remains available in Timer; Resume or an explicit duration edit publishes
a fresh visible state. Revisions and the PKJS handshake prevent queued old states
from replacing a newer run, including after native app data resets. Cancelled
and completed records are terminal state, not new consumer alerts. Schema 1
removes both terminal states. Schema 2 can display the native completed result
for its original, bounded validity while unacknowledged. Done publishes a newer,
hidden completed result; the saved timer remains reusable. Reconnection and
reopening never extend terminal validity. Cancellation still clears it. Native alarm
vibration remains owned by Timer, independently of either display source.

## Actual phone and watch checks

These are a verification procedure, **not a claim that hardware testing has
been completed for this release**.

1. Enable plugin support and permission preview where required in the stock
   Pebble phone app. Install Timer and a compatible Dynamic PBW on the same
   Time 2. When upgrading Activity Timer, verify the existing saved timer is
   retained and only one launcher entry remains.
2. Add a timer from the + row, choose a duration and start it. Open More options
   and wait for “Shared with phone”, then return to Dynamic. Its countdown
   should use the same deadline.
3. Reopen Timer, pause, and return. Dynamic should show a fixed remaining time
   for five minutes, then hide it. Reopen Timer and return without editing;
   the paused timer must remain saved there and stay hidden on Dynamic. Resume
   it and verify it reappears. Pause and explicitly edit its duration to verify
   a fresh five-minute window, then delete it; each operation should update or
   clear the same run without replaying a completion.
4. Start another run and immediately exit before the application receipt. Timer
   must remain usable, and an old receipt must not acknowledge the new run.
5. Disconnect Bluetooth while starting or changing a timer. Back must remain
   usable. An accepted native Wakeup should still alert; Dynamic must not invent
   an unsent state or infer that the alarm rang.
6. Reconnect and reopen Timer. The current native state should replace the phone
   snapshot, including after restarting the phone app. Test multiple nearby deadlines and competing Wakeups
   and the five-second minimum on the actual firmware.

For local alarm diagnosis, run `ACTIVITY_TIMER_DIAGNOSTICS=1 pebble build`.
Opt-in logs contain only public timer state and cover scheduling, Wakeup lookup,
launch reason, callbacks and exit state. Normal release builds omit these logs.
A stored Wakeup or a completed timer shown after manual reopening does not prove
automatic launch.
