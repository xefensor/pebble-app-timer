# Timer preferences checkpoint — 2026-11-08

The latest candidate is recorded in the [receipt freshness checkpoint](ui-preferences-revision-checkpoint.md). The evidence below describes the original preference candidate.

Local candidate only; no version change, push or publication. Earlier native
navigation evidence is in [its checkpoint](native-navigation-checkpoint.md).

## Result

Timer has independent Text Size (Follow Pebble / Small / Medium / Large) and
Appearance (Follow Pebble / Light / Dark). All manual text choices use the same
native roles as the corresponding actual Emery preference. Saved choices survive
relaunch. Existing explicit Light/Dark settings migrate without changing timers.

The supported SDK 4.33.1 has no public system appearance API. The phone explains
that Follow Pebble Appearance is unavailable, retains that choice and uses its
last explicit Light/Dark fallback. System appearance/scheduled switching remains
pending, including on firmware 4.45.0; the candidate uses only its supported SDK
surface. Fresh appearance remains Timer's previous Light default.

The phone distinguishes Saved on phone / Pending on watch from Applied on watch.
Only the exact native application receipt after persistence readback permits the
latter. Transport success, old revisions, other choices and failed saves cannot
establish it. Pending choices survive reopening; runtime start or a fresh watch
hello reconciles them through Timer's one transport queue. Status updates preserve
unsaved drafts. Existing Android live-writer handoffs remain bounded.

## Candidate and evidence

`build/candidate/Timer-0.14.5.pbw` SHA-256:
`92cce7f3e41eceed8b1b86a1fa5cb615547e6a56227215b7b8496545ad261d82`.
SDK 4.33.1 / CLI 5.0.40; local validation used Node 26.12.0 and Python 3.13.16.
The documented Node 22.23.4 / Python 3.12.15 pair was unavailable locally.

- `npm test`, `npm run build`, `npm run check:artifact` and `npm run check:config`
  pass. Portable checks include native codec/persistence failures and eight focused
  phone-controller cases: exact receipt, stale receipt, reopen/retry, bounded
  delivery, failed watch save, legacy migration, corrupt storage and revision
  continuity. The normal owner/source, localization and workflow checks remain.
- `node scripts/check-settings.cjs` executes the exact optimized packaged HTML,
  PKJS and plugin in Chromium with simulated phone APIs and independent Android
  stores. It checks both selectors, 320/360px Light/Dark layouts, 44px targets,
  pending/applied status, a fresh watch mismatch, writer handoff, lost responses,
  corrupt-store repair, draft preservation and Czech enlarged-text layout.
  Evidence: `build/qa/settings/report.json` and eight raw captures.
- `python3 scripts/emulator-preferences-bridge.py` runs the exact optimized phone
  bundles over a simulated SDK bridge against the unmodified native payload in
  Emery. A choice saved while the watch app is closed starts pending. Actual
  AppMessage transport ACKs and the native save/readback receipt then establish
  Applied on watch with the exact desired choices.
  Evidence: `build/qa/preferences-bridge/1794161157685086674/report.json`.
- `python3 scripts/emulator-ui-preferences.py` passes on actual OS sizes 1/2/3
  in Emery firmware 4.33.2 and 4.45.0. For each size, the manual override with a
  conflicting OS size matches the corresponding Follow Pebble content pixels,
  survives relaunch, and renders Light/Dark correctly. The saved unsupported
  Follow Appearance choice retains Dark as fallback. A separate read-only probe
  confirms exact persisted preference bytes. Timer bank checksum stays
  **3008147237**, before and after all preference updates.
  Evidence: `build/qa/ui-preferences/1794161157689406732/report.json` (4.33.2)
  and `build/qa/ui-preferences/1794161157687506998/report.json` (4.45.0), each
  with 15 raw captures and `manual-sizes.png` arranged without rescaling pixels.

The native matrix uses emulator-only V1 data seeds and a read-only persistence
probe sharing Timer's UUID. They are separate from the exact candidate native
payload. Never install these helpers on a physical watch. Build them with
`python3 scripts/build-fixtures.py` after the candidate build, which cleans
`build/`. For 4.45.0 set `PEBBLE_QA_MICRO_FLASH` and
`PEBBLE_QA_SPI_FLASH_TEMPLATE` to the verified SDK-shell images documented in
`../recovery-validation/firmware-4.45.0/source.json` in the workspace.

## Pending physical checks

No physical phone or Time 2 was available. The phone SDK/storage bridge above is
simulated; native rendering, preference persistence and receipts run in QEMU.
On hardware, disconnect the watch, save a manual size and Dark, reopen settings,
then reconnect and open Timer. Check pending becomes applied, relaunch retains
both choices, and switching back to Follow Pebble text uses the launch-time watch
size. Keep a running timer through those steps and check its original deadline.
Published-PBW upgrade coverage belongs to the remaining cross-project upgrade
stage; this checkpoint does not claim it.
