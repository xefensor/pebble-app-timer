# Timer maintenance contracts

These are current implementation constraints. They apply when changing the
relevant subsystem, not as a requirement to reread every document for every edit.

## Ownership and compatibility

Native state, verified banks, deadlines and Wakeups are authoritative. Preserve eight slots, saved repeat duration, acknowledgement and migration compatibility. A transport ACK is not a saved-state receipt. Paused sharing expires after five minutes without renewing its observation. Zero does not establish completion. Keep one AppMessage queue. Appearance keys are separate from timer/Glance keys; appearance never changes a timer. Back, critical warnings, safe Keep-first Delete and immediate owner operations remain available.

## Current interaction and product boundaries

Watch timer controls stay on the watch. The phone has one Light/Dark appearance selector and confirmed Save-and-stay. The 34 px contrasting action rail spans every screen row; headers and footers stay inside 166 px. Header clock 26, primary utility 24 and secondary 22 use Roboto Condensed Bold; dominant values use Fredoka. Keep two 80 px list rows or one above a complete critical warning, all eight selections visible, rigid 120 ms choices and bounded 144 ms physical feedback. Long elapsed/counter values and warnings stay complete.

## Source ownership

src/c/main.c orchestrates SDK input, persistence and alarms; timer_model/collection own portable state; timer_ui/fonts/hourglass own rendering. src/pkjs and src/plugin own the phone bridge.

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
