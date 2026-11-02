# Timer maintainer notes

This repository owns the native Timer watchapp and its bundled phone-side
PKJS/provider plugin. It targets stock PebbleOS on Emery (Pebble Time 2).
`pebble-activities` is a pinned shared dependency, not a separate installed
service. Keep timer behaviour independent of Dynamic's presentation.

## Identity and native ownership

- Preserve app UUID `50e9ef12-a5ab-4369-a3b5-3447bfba5151`, native persistence
  keys/model compatibility, plugin UUID and shared storage namespace when
  updating Activity Timer installations.
- The native timer owns saved state, deadlines and Wakeup alarms. Phone receipt
  means accepted shared state, not an alarm or a disk-flush guarantee.
- Preserve source/run identity, revisions, transport handshakes and observation
  time. Polling a saved snapshot must not renew its native observation. Zero is
  not proof of native completion.
- A paused activity is shared for `ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS`
  (five minutes), then hidden from consumers while remaining saved in Timer.
  Reconfirmation advances its revision without renewing observation/expiry.
  Only explicit paused edits/extensions or Resume establish a fresh window.
  Clamp old paused leases from the saved observation when upgrading.
- Back remains available when disconnected, storage fails or sharing is pending.
  Scheduling failure must remain visible; do not claim an alarm was secured.
- Keep a single AppMessage queue. A transport ACK is not an application receipt.

## Dependencies and packaging

- Install with `npm ci --ignore-scripts`. Use the versioned tarball in `vendor/`;
  never rely on a sibling library checkout. Record updated provenance and hashes.
  Version 0.4.0 pins Activities 0.2.0; `vendor/ACTIVITIES.json` records
  its exact reviewed source commit and archive checksum.
- Use `npm run build` for distribution. Its manifestVersion 2 PBW must contain
  native app, PKJS, independently callable `plugin.js`, licences and notices.
  A plain SDK development PBW lacks the additional provider packaging.
- Do not copy upstream Pebble Timer code or assets without checking its actual
  reuse terms. It is currently a design reference, not a bundled dependency.
- Preserve published artifacts and tags. Changed bytes require a new version.

## Verification

Run `npm test`, build the app and run the release artifact checks. Check the
actual packaged PKJS/provider handoff, symbolic and numeric message keys, native
identity and resource integrity. Record emulator results separately from real
phone and physical-watch checks; never describe unperformed checks as passed.
Check affected layouts on an actual emulator when changing native rendering.

The approved hourglass reference is in `docs/design/`; those browser images
remain design evidence, not native captures. `src/c/timer_ui.c` owns the native
166px content/34px action-bar composition. `timer_fonts.c` uses the reviewed
Dynamic Fredoka outlines and native Roboto bitmap fonts; preserve their SIL
notices in both the source and PBW. `scripts/check-timer-fonts.py` verifies
packaged resources and linked outlines against their exact source assets.
Keep digits stable, sand proportional, paused views still and transitions
bounded. Preserve initial launch focus and cancel decorative work under a
covering window. All eight saved timers retain native deadline ownership.
More options owns Restart and confirmed Delete; Keep is the initial choice.
Held buttons must not act again after their first press changes screens.
Normal sharing status belongs in More options; alarm/storage failures stay
visible. Restart creates a new run in the same slot with the configured duration.

The app has eight stable native timer slots and an independent list/editor/detail/
alarm interface. ACTIVITY_SLOT (1021) is an internal bridge key;
ACTIVITY_TOTAL_DURATION (1022) adds immutable configured-duration metadata without
changing the persisted native model. Keep the activity/session schema 1 source
alongside activity/card schema 2. The native publication/receipt path remains
schema 1 plus the optional bridge metadata. Source revisions advance globally
and dirty slots are published in ascending order.
Declare the `activity` source category once, with items `session` and `card`
and the union of both schemas' properties.
Keep the earliest-deadline Wakeup broker: separate nearby Wakeups conflict on
stock PebbleOS. Versioned storage chunks must stay below the SDK’s per-key limit;
write and verify a new bank before committing it or retiring legacy state.

The accompanying PolyForm Noncommercial licence and required attribution apply.
Preserve third-party notices, including the SDK's Apache 2.0 notice.
