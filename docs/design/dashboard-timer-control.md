# Dashboard Timer control: feasibility and deferred design

Status: deferred by user choice, 2026-11-04. After reviewing the platform limits,
the user chose to leave Dashboard Pause/Resume out for now. The foreground
handoff described below is **not approved or implemented**.
No activity action, command transport, worker or permission has been added.

## Intended behavior

Select on a running Timer activity should pause that exact timer, retaining its
remaining time. Select on a paused activity should resume it. This is not Cancel,
Restart or an acknowledgement. Other timer slots must remain unchanged.

The native Timer remains the owner of saved state, deadlines and Wakeup alarms.
A changed phone snapshot alone cannot pause a timer: its existing native alarm
would still fire. A successful Resume must secure the native alarm as well as
save the new deadline.

## Verified platform boundaries

These findings come from the public SDK 4.33.1 headers and pinned official source:
PebbleOS `2be9be7806744268d7cc4b57f594b7ba68854f83`, mobile app
`38ab8a09372baaac380bce2f0c73d6b20fb6c393`. They are source inspection, not a
physical-watch or phone integration test, and do not assert that every released
firmware version contains the same implementation.

| Route | Boundary |
|---|---|
| AppMessage | The receiving foreground app compares the target UUID with its own UUID and rejects a mismatch. Dashboard cannot receive on Timer's behalf. [Implementation][appmessage] |
| Background worker | `app_worker_send_message` uses the current process's service identity. The OS rejects foreign UUID subscriptions and excludes recipients with a different UUID. Dashboard cannot address a Timer worker. [Sender][worker-sender], [recipients][worker-isolation], [identity check][worker-identity] |
| Native storage | `persist_*` selects the store using the current process UUID. Dashboard cannot edit Timer's native bank. Phone `localStorage` is a separate store. [Implementation][persist] |
| Wakeup | Scheduling records the caller's UUID; cancellation and query check that UUID against the alarm owner. Dashboard cannot cancel Timer's alarm. [Scheduling and cancellation][wakeup], [query][wakeup-query] |
| Phone plugins | Provider plugins can handle actions and read sources, but have no native AppMessage bridge. Registering a Timer action handler does not make native Timer code run while another app is open. [Plugin host API][plugin-host], [source bridge][source-bridge] |
| Launch | Watch State exposes `launch_app` and `launch_previous_app`. The phone start packet has a UUID and no background flag; the OS performs a foreground app switch. [Watch State][watch-state], [packet][launch-packet], [OS launch][launch-os], [switch][launch-switch] |

The worker SDK also exposes neither AppMessage nor Wakeup. Introducing a Timer
worker therefore does not provide the missing cross-app command route. Workers
are not an unrestricted background service; launching a different app's worker
also involves the OS worker-switch confirmation. [Worker launch][worker-launch]

For separate installed Timer and Dashboard apps, supported stock APIs offer no
verified route for invisible Pause/Resume. A future platform command bridge
could provide one. Combining Timer into Dashboard under one native app identity
would be a different product architecture. Neither change is part of this work.

## Possible foreground handoff

If the user accepts a brief automatic app switch, the following route is viable
in principle and still needs implementation and device validation:

1. Dashboard asks the Timer provider to queue a short-lived, conditional command
   for one exact native timer run. The provider does not change the snapshot.
2. Dashboard uses the official Watch State `launch_app` action to open Timer.
3. Timer PKJS retrieves the pending command and passes it through Timer's native
   AppMessage queue. Native code checks the current model before applying it.
4. Native Timer saves and verifies the bank, updates the Wakeup broker and sends
   an explicit application receipt with its resulting authoritative state.
5. After accepting the matching native publication, Timer PKJS opens Dashboard
   by its explicit UUID, `f76f9fc1-2944-4248-bcc5-3eea2af0183b`. A successful
   launch request alone is not proof that Timer executed the command.

Bind the return to the requesting Dashboard session and a short expiry. Do not
return after the user has independently opened another app. Prefer an explicit
destination over `launch_previous_app`, whose previous-app value can change.
Watch State currently launches on all connected watches, so multi-watch command
and return binding also needs a defined policy before enabling this route.

## Required correctness rules

- Carry a unique request ID, exact run identity, expected revision and state,
  explicit `pause` or `resume`, and expiry. Validate bounded arguments. Permit
  only one outstanding command and make duplicates harmless; never retry an
  ambiguous toggle. A reused slot must not receive a command for its former run.
  Persist deduplication consistently with the model, or recover through exact
  revision guards without repeating an operation whose outcome is unknown.
- Check the command before the normal `ACTIVITY_REQUEST` reconfirmation, which
  advances source revisions. Otherwise opening Timer would invalidate the
  command's expected revision before it could be checked.
- Resolve native run identity across the eight slots. `readSnapshots` does not
  preserve native slot indices; source ordering is not a slot address.
- Save through the existing verified bank and earliest-deadline Wakeup broker.
  Preserve other running timers' alarms. `prv_apply` currently reports a saved
  model even if alarm scheduling fails, so its return value alone cannot be a
  successful Resume receipt. Verify the actual scheduling result.
- Distinguish queue acceptance, AppMessage transport ACK, native execution
  receipt and accepted source publication. Return to Dashboard only for the
  matching successful operation; keep storage/alarm failures visible in Timer.
  A timeout means unknown outcome, not success or an automatic replay.
- Keep the saved native snapshot `unconfirmed`. Dashboard currently suppresses
  actions from unconfirmed projections. Any future conditional native command
  needs an explicit, reviewed host contract rather than falsely confirming the
  snapshot. Polls and receipts must not renew native observation or validity.
- Preserve run/revision monotonicity, paused visibility expiry, offline native
  alarms and Back availability. Do not replace native ownership with a phone
  countdown or add a mobile-app/firmware fork.

Before shipping, exercise stale revisions, completed/reused runs, duplicate and
expired commands, disconnection, storage failure, Wakeup failure, multiple timer
slots, user navigation during handoff and multiple connected watches. Test the
actual packaged provider/PKJS/native round trip on a real phone and watch.

[appmessage]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/applib/app_message/app_message_inbox.c#L69
[worker-sender]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/applib/app_worker.c#L40
[worker-isolation]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/services/event_service/service.c#L223
[worker-identity]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/services/event_service/service.c#L428
[persist]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/applib/persist.c#L20
[wakeup]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/services/wakeup/service.c#L398
[wakeup-query]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/services/wakeup/service.c#L606
[plugin-host]: https://github.com/coredevices/mobileapp/blob/38ab8a09372baaac380bce2f0c73d6b20fb6c393/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/plugin/PluginHostJs.kt#L1
[source-bridge]: https://github.com/coredevices/mobileapp/blob/38ab8a09372baaac380bce2f0c73d6b20fb6c393/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/plugin/PluginSourcesJsBridge.kt#L28
[watch-state]: https://github.com/coredevices/mobileapp/blob/38ab8a09372baaac380bce2f0c73d6b20fb6c393/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/plugin/WatchStatePlugin.kt#L25
[launch-packet]: https://github.com/coredevices/mobileapp/blob/38ab8a09372baaac380bce2f0c73d6b20fb6c393/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/packets/AppRunState.kt#L24
[launch-os]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/process_management/app_run_state.c#L63
[launch-switch]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/process_management/app_manager.c#L707
[worker-launch]: https://github.com/coredevices/PebbleOS/blob/2be9be7806744268d7cc4b57f594b7ba68854f83/fw/syscall/app_worker_syscalls.c#L33
