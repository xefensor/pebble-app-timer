# Timer maintenance contracts

These are current implementation constraints. They apply when changing the
relevant subsystem, not as a requirement to reread every document for every edit.

## Ownership and compatibility

Native state, verified banks, deadlines and Wakeups are authoritative. Preserve eight slots, saved repeat duration, acknowledgement and migration compatibility. A transport ACK is not a saved-state receipt. Paused sharing expires after five minutes without renewing its observation. Zero does not establish completion. Keep one AppMessage queue. Appearance keys are separate from timer/Glance keys; appearance never changes a timer. Back, critical warnings, safe Keep-first Delete and immediate owner operations remain available.

## Current interaction and product boundaries

Watch timer controls stay on the watch. The published phone page owns independent Text Size, Appearance and Alarm Sound choices, with saved/pending/applied status. The 34 px contrasting action rail spans every screen row; headers and footers stay inside 166 px. Current rendering uses actual PebbleOS Header, MenuCellTitle, MenuCellSubtitle and Footer profiles from the pinned shared UI candidate. Native LECO number fonts fit dominant values without rescaling. MenuLayer owns measured list, Options and confirmation motion; real Windows own navigation. Preserve the first-row y=50 anchor, identity restoration, all eight timers, complete warnings, safe Keep-first deletion, the stock editor feedback and bounded 144 ms physical-button feedback. App code retains all input, window, timer, alarm and resource lifetime ownership.

## Source ownership

src/c/main.c orchestrates SDK input, persistence and alarms; timer_model/collection own portable state; timer_ui/native_type/hourglass own rendering (legacy custom-font sources remain for historical tooling, excluded from production). src/pkjs and src/plugin own the phone bridge.

## Phone persistence and evidence

For apps with phone settings, keep active-writer selection, separate Android
DOM/native storage views, exact application acknowledgements and bounded handoffs.
Saving stays open where supported; uncertain writes retain the draft and explicit
readback/retry. Discard cannot undo a write already sent. Load errors must not
silently replace saved choices with editable defaults. Do not introduce unsolicited
focus or scroll changes. Historical browser/emulator results do not validate a
new build. Keep portable, packaged, simulated transport and real-device claims distinct.

## Dependencies, identities and licences

Preserve public identifiers, message-key numbers, compatible storage formats,
licences and attribution. Pinned vendor archives/checksums remain independent of
sibling checkouts. Do not repin consumers or change published bytes as cleanup.

## History

[Earlier maintainer notes](history/maintainer-notes-before-rework.md) retain the
full rationale/history. Read them only when investigating an old decision.

## UI preferences protocol

Timer owns `timer-ui-preferences-v1` on the phone and verified native key 2106.
The older `timer-appearance-settings-v1`, native key 2105 and message keys 1024/1025
remain compatible. New request/result keys are 1026/1027; no timer, Glance, bank or
publication key changes. The new record also preserves the earlier colour-only
phone mirror and native fallback. An older appearance page changes appearance
without discarding the manual text size.

The fixed 20-byte little-endian frame contains version 1, field count 3, status,
hello flag, revision, text-size choice, appearance choice and last explicit
appearance. Each of the last four fields occupies four bytes. Requests require a
positive revision, status zero and no hello flag. Native validation rejects unknown
fields, lengths, flags, modes and inconsistent explicit fallbacks. An applied reply
follows exact persistent readback and applies the same verified choices to layout
and drawing. A failed save sends failure, never an applied receipt. Startup and
reconnect report the currently loaded choices with the hello flag.

The phone stores desired choices before delivery. Only a receipt matching revision
and every choice establishes Applied on watch. A fresh mismatched watch hello
invalidates the old receipt and reconciles the desired choices. Delivery uses the
existing single app queue, with three bounded attempts and a separate wait for an
application receipt. Native UI replies share the existing outbox without consuming
or acknowledging an owner publication ticket. The UI library owns no controller,
persistence, queue, event subscription or timer.

A runtime resync, explicit Retry or mismatched fresh watch hello rotates the
positive delivery revision before sending. A delayed receipt from before that
boundary cannot mark the new delivery applied. If the new revision cannot be
verified in phone storage, keep the choice pending and suppress old receipts.


## Alarm sound preference

A native alarm keeps the existing repeating double vibration every three seconds.
On Time 2 (Emery), sound defaults on, including an upgrade with no saved sound
preference: two 150 ms square-wave notes at MIDI 93 (1760 Hz), separated by a
100 ms rest, at volume 70. Round 2 (Gabbro) has no speaker and retains vibration.
Quiet Time and speaker mute are respected. Busy/failed sound retries at the next
normal cadence; it never delays timer operations. Multiple pending timers share
one feedback scheduler. Only owned sound is cancelled on disable, final verified
Done/Snooze, Quiet Time or exit. Failed acknowledgement leaves the pending mask
and feedback active.

TIMER_ALARM_PREFERENCES / RESULT append keys 1028/1029. Their independent 12-byte
v1 frame contains revision and a sound boolean; reply flags include hello and
speaker capability. Persistence key 2107 is separate from appearance keys and
native timer banks. Readback must confirm before applying and returning success.
An absent record enables sound; a damaged existing record reports failure and
keeps sound off rather than silently restoring it. Phone storage uses
`timer-alarm-preferences-v1`, its own settings form, delivery revision and receipt.
Transport ACK does not establish application. Speaker capability does not change
timer duration, state, Wakeups, Glance or publisher ownership.
