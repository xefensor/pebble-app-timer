# Timer

A native timer for **stock PebbleOS**, with activity sharing for compatible
watchfaces such as [Dynamic](https://github.com/xefensor/dynamic-watchface).
The first target is Pebble Time 2 (`emery`, 200 × 228).

Download [Timer 0.1.1](https://github.com/xefensor/pebble-app-timer/releases/download/v0.1.1/Timer-0.1.1.pbw)
and verify it against [SHA256SUMS.txt](https://github.com/xefensor/pebble-app-timer/releases/download/v0.1.1/SHA256SUMS.txt).
This is a development prerelease. Real-phone and physical-watch checks remain
separate from portable tests and emulator checks.

## Version 0.1.1

- Move the existing timer into its own repository, build and release pipeline.
- Rename the launcher entry to Timer while retaining its app UUID, saved timer
  format and provider storage identity for updates from Activity Timer 0.1.0.
- Pin Pebble Activities 0.1.0 as a local, checksummed dependency; no sibling
  checkout or separate library build is needed.
- Keep the existing one-timer controls and native alarm behaviour. The proposed
  Pebble-inspired interface and multiple-timer UI are not part of this release.

## What you install

Install **one Timer PBW**. It contains the native watchapp, its PKJS phone bridge
and a read-only provider plugin. Timer owns its saved timer and native Wakeup
alarm. The bridge saves the last reported state on the phone, and the plugin
exposes that state as `activity/session` v1 to compatible consumers.

[Pebble Activities](https://github.com/xefensor/pebble-activities) supplies the
shared library and source convention. It is not another installed app or
background service. Timer's plugin may also appear in the phone app's Plugins
list because the same PBW provides both a watchapp and a data source.

Activity sharing requires a phone app with Pebble plugin support and the
relevant permissions or preview enabled. The native timer and an accepted
Wakeup alarm continue independently of sharing. The consumer receives a
phone-mediated snapshot, not direct access to another watchapp's memory.

## Controls

- **Select:** start the chosen duration, pause, or resume.
- **Up:** add one minute before or during a timer.
- **Down:** reduce the chosen duration before starting, to a minimum of five
  seconds; cancel an active or paused timer.
- **Back:** exit. A successfully scheduled native Wakeup remains active.

The timer respects Quiet Time for vibration. PebbleOS controls missed-Wakeup
notifications. A scheduling failure displays “Alarm not set; keep app open”
instead of claiming a background alarm was secured.

## Sharing and freshness

“Shared with phone” means that PKJS saved and read back the current revision and
returned a matching application receipt. It does not mean Dynamic has received
it. A native AppMessage transport ACK alone is insufficient.

The provider marks its data as **last reported / unconfirmed**. Polling the
snapshot or reconnecting the phone cannot prove the watch timer has not changed
while disconnected. The original observation, deadline and validity are kept;
polling never makes an old snapshot fresh. A locally calculated zero does not
prove the native app completed or sounded its alarm.

State changes are persisted before publishing. Reopening Timer refreshes the
native observation. Revisions and the PKJS handshake prevent queued old states
from replacing a newer run, including after native app data resets. Cancelled
and completed records are terminal state, not new consumer alerts.

## Build and check

Use Node.js, Python 3, a C99 compiler, and a Pebble SDK with Emery support:

```sh
npm ci --ignore-scripts
npm test
npm run build
```

The dependency is installed from `vendor/pebble-activities-0.1.0.tgz`; its
provenance and SHA-256 are recorded in [vendor/README.md](vendor/README.md).

The build writes `dist/Timer-0.1.1.pbw` and its checksum. It bundles the provider
JavaScript and sets `manifestVersion: 2`, including `plugin.js` alongside PKJS
and the watch binary. A plain `pebble build` creates a development PBW without
that extra provider bundle; use `npm run build` for an installable shared-activity
build. The packaged PBW includes the app and library licences and SDK notices.

Portable tests check the native timer model and isolated PKJS/storage/provider
handoff. Artifact checks exercise the packaged scripts. These do not substitute
for the actual phone or watch checks below.

For this extraction, the portable timer model and bridge suites passed, and an
isolated checkout installed the pinned dependency and built the SDK PBW. No
real-phone or physical-watch verification is claimed for version 0.1.1.

## Actual phone and watch checks

These are a verification procedure, **not a claim that hardware testing has
been completed for this release**.

1. Enable plugin support and permission preview where required in the stock
   Pebble phone app. Install Timer and a compatible Dynamic PBW on the same
   Time 2. When upgrading Activity Timer, verify the existing saved timer is
   retained and only one launcher entry remains.
2. Choose a duration and press Select. Wait for “Shared with phone”, then return
   to Dynamic. Its countdown should use the same deadline.
3. Reopen Timer, pause, and return. Dynamic should show a fixed remaining time.
   Resume, extend and cancel in turn; each operation should update or clear the
   same run without replaying a completion.
4. Start another run and immediately exit before the application receipt. Timer
   must remain usable, and an old receipt must not acknowledge the new run.
5. Disconnect Bluetooth while starting or changing a timer. Back must remain
   usable. An accepted native Wakeup should still alert; Dynamic must not invent
   an unsent state or infer that the alarm rang.
6. Reconnect and reopen Timer. The current native state should replace the phone
   snapshot, including after restarting the phone app. Check competing Wakeups
   and the five-second minimum on the actual firmware.

For local alarm diagnosis, run `ACTIVITY_TIMER_DIAGNOSTICS=1 pebble build`.
Opt-in logs contain only public timer state and cover scheduling, Wakeup lookup,
launch reason, callbacks and exit state. Normal release builds omit these logs.
A stored Wakeup or a completed timer shown after manual reopening does not prove
automatic launch.

## Design direction

The next interface is intended to follow the familiar list, editor and button
flow of [Pebble Timer](https://github.com/coredevices/pebble-timer). This extraction
does not import that project's source or implement the proposed redesign.

## Licence

[PolyForm Noncommercial License 1.0.0](LICENSE), with its required attribution.
See [third-party notices](THIRD-PARTY-NOTICES.md) for bundled Pebble Activities and
SDK components.
