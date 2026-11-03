# Timer 0.10.0 optimization review

This local candidate preserves the approved Timer UI and activity API. It cuts
unnecessary rendering, removes unused font data and reduces installed phone
scripts. Published 0.9.0 bytes and tags remain unchanged.

## Resulting work and size

| Measurement | 0.9.0 | 0.10.0 |
|---|---:|---:|
| Distribution PBW | 104,142 B | 74,892 B |
| SDK static RAM footprint | 36,501 B | 34,393 B |
| Packaged native image | 31,556 B | 29,436 B |
| Native resources | 26,719 B | 22,802 B |
| Installed PKJS | 55,220 B | 38,415 B |
| Installed provider script | 86,585 B | 64,087 B |

These are actual packaged/SDK measurements, not live-heap or battery estimates.
The debugging ZIP is a separate developer download containing the original
scripts, original SDK map and chained minified maps. Normal installs retain
all required notices but carry no source map or dangling map URL.

The earlier read-only estimate of the unused 24pt font used an intermediate
`.reso` size. The actual packaged bitmap is **3,917 bytes**, and the resource
pack drops by exactly that amount. Removing unused outlines saves **2,496 linked
data bytes** before the additional small state/window helpers; net SDK footprint
falls by **2,108 bytes**.

## Rendering and native ownership

Sharing transport ACKs, retries, application receipts and connection changes now
compare their actually displayed status with the last drawn screen. Only Options
shows normal sharing status; storage/alarm errors remain visible everywhere.
Native model changes, screen transitions, focus restoration and visible clock
ticks retain their ordinary redraw path.

List rendering and tick decisions share one portable row-window calculation,
including the two-row critical-warning layout. An offscreen running timer still
reconciles at its native deadline but does not repaint a list of static rows.
The eight-slot stable order is calculated once per list pass. Completion checks
test whether any timer is actually due before copying the 436-byte collection
into existing static transaction scratch.

Verified persistence banks/readback, publication tickets, eight-slot complete
handoffs and tombstones, original observations, Wakeup ownership, App Glance
projection/checkpoint suppression and bounded motion are preserved. No new
polling, scrolling, continuous animation, worker or phone settings is added.

The exact reviewed Fredoka outlines are regenerated locally for the eleven
characters the digit renderer accepts: `0123456789:`. All retained points,
contours and glyph records are identical to the previous corpus prefix. Six
used Roboto body fonts remain; the unused 24pt font is no longer loaded.

## Phone scripts and release identity

Distribution packaging uses the locked Terser 5.46.0 build dependency, with
compression disabled and copyright/license comments retained. Development SDK
bundles remain untouched. The debug ZIP supplies original SDK/source inputs and
the exact chained maps. Committed-HEAD release preparation checks the original
module graph against pinned source, independently reproduces the optimized code
and maps, then exercises the actual optimized PKJS/provider handoff.

Regression checks reject changed runtime code or maps even when their debug
hashes are recomputed, as well as maps accidentally embedded in the PBW. The
release workflow installs the locked build tool in both validation jobs.

## Validation and remaining measurements

- 22 JavaScript tests, seven portable C suites and 42 Python regressions pass.
- SDK 4.33.1 Emery build, exact font/resource/license identity and packaged
  optimized-runtime cold handoff pass.
- Six actual QEMU phases pass: UX pass, normal UX, real exhausted-generation
  storage refusal, nearby deadlines, equal deadlines and launcher behavior.
  The 59 captured frames include the separately labelled storage fixture and
  OS launcher screens. They all refer to the final candidate's exact native
  payloads; no phone JavaScript was attached to this emulator run.
- 24 complete native frames match approved 0.9.0 RGB pixels exactly. Five other
  matching scenarios have independently advancing countdown/elapsed values.
  Representative list, paused, editor, hourglass, alarm, critical-warning and
  launcher captures were opened for visual inspection.
- Native assertions cover offline background alarms, Done/Snooze, no-op and
  elapsed-preserving edits, saved repeat duration, storage-failure recovery,
  nearby/equal deadline dismissal, OS app-list ticking and same-second fast
  pause/exit clearing.

The [host raster profile](qa/0.10.0/raster-profile.json) counts edge tests and
intersections at all four UI clock sizes while hashing coverage. It confirms
the bounded scanline work but is not native draw timing. Keep the existing
allocation-free scratch after eliminating unnecessary frames: a coverage cache
would require extra RAM and careful handling of fractional glyph positions and
invalidation. Actual watch timing must justify that extra complexity first.

No physical watch or real phone was available. This pass does not establish
physical battery savings, vibration/Bluetooth delivery, phone cold-start time,
minimum live heap or peak stack. The existing device procedure in the README
still applies. Evidence and source/PBW hashes are indexed by
[asset provenance](asset-provenance.json) and
[the native report](qa/0.10.0/native/ux.json).
