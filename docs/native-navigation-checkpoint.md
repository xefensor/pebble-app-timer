# Native navigation checkpoint — 2026-11-08

Local development candidate; no release, push or version bump. Starting Timer commit: `67c4f5bb2a9b167d0e69213da5368623c3b6873a`.

All six screens have app-owned Windows, normal transitions use the SDK stack, and list/Options/confirmation use native MenuLayer motion. Add/Edit uses measured native type and the existing stock-reference field/value feedback. Alarm preempts the ordinary stack. Production bundles no custom font payload.

The shared type table comes from local UI commit `b67370a` and candidate archive `pebble-xoce-ui-0.1.1-native-type-63526c2e66e1.tgz`, SHA-256 `63526c2e66e1d2150aecf0b2671233733060090bd5a6c2fdfb4e1eae748cb36f`. The exact checksum and source identity are in `vendor/UI.json`.

## Automated and packaged checks

`npm test`, `npm run build`, `npm run check:artifact` passed. Native role expectations cover both original firmware and the 4.44 style table. Portable window paths cover every destination and editor return path. Packaged owner/settings compatibility checks passed. SDK 4.33.1 / CLI 5.0.40; actual emulator firmware v4.33.2. Runtime used Node 26.12.0 and Python 3.13.16; exact contributor runtime pins were unavailable.

Candidate: `build/candidate/Timer-0.14.5.pbw`.
SHA-256: `b8be35c6d46f25e13749f2035b0065b5961cf302bf82581f918692fdf2aff64f`.

Exact native payload:

- `emery/app_resources.pbpack`: `42f9180a4dbc22c617346fb11a8609765ac8092709de5032e5d5a971af277e68`
- `emery/manifest.json`: `be3efa618ac6ebb8f02c1ab3bcc1c3fb8d412a67de2a4b00e2cc7bad701b4593`
- `emery/pebble-app.bin`: `9f274a1085bfd19d21a94c94b0077f5800b7bc4a77092ab00610dd732e23048e`

## Emulator checks

The exact candidate passed list motion, rapid reversal, Select during scrolling, identity restoration after sorting, light/dark, 12/24-hour headers, all eight slots, long values, complete warnings and alarm interruption. Independent direct-SDK glyph specimens matched native Header/MenuCellTitle/Footer bitmaps. OS size commands were read back through `preferred_content_size()`; injected font-size overrides were not used.

| OS size | SDK value | Standard row | Complete rows | First row |
| --- | --- | --- | --- | --- |
| small | 1 | 50 px | 3 | 50 px |
| medium | 2 | 56 px | 3 | 50 px |
| large | 3 | 86 px | 2 | 50 px |

The navigation harness passed at all three sizes: Options → confirmation → Keep; reopen on Keep; Back/cancel without bank changes; edit paused remaining from 600 to 660 seconds; reset to saved 600-second repeat; start/pause; explicit Delete; Add 661 seconds; real exhausted-generation storage refusal; alarm interrupting an editor; ten nested navigation cycles. Sixty raw captures are hash-bound to each report.

Separate Czech and German lookup fixtures passed the same navigation sequence and all three real OS size preferences. They are different native binaries, not final-byte or installed OS-language-pack proof. Large editor, confirmation and alarm captures were inspected side by side; native accents, help and complete warnings fit.

The separate heap-log fixture completed 40 open/Back cycles, 80 samples. Minimum free heap: 82560 bytes; last 20 range 82560–82596 bytes. The uninstrumented candidate also survived 20 open/Back cycles. All app allocations were released at exit.

Disposable evidence roots:

- `build/qa/timer-checkpoint/b8be35c6d46f/1794136757843519738` — list checkpoint (`b8be35c6d46f25e13749f2035b0065b5961cf302bf82581f918692fdf2aff64f`)
- `build/qa/native-navigation/27c19570c71c/1794136868027026617` — locale-de (`27c19570c71c26c7a8231e7fb620a3fc5d4becfa0792eb7e7eb3d01dd73a0e4b`)
- `build/qa/native-navigation/3c60570fdeb7/1794136868025544318` — locale-cs (`3c60570fdeb79d8e1be41cf5f02735e1ea2149ea84bf35870d64c7eb4ab27b78`)
- `build/qa/native-navigation/b8be35c6d46f/1794136757833724726` — candidate (`b8be35c6d46f25e13749f2035b0065b5961cf302bf82581f918692fdf2aff64f`)

Each report includes native hashes, capture hashes and its verification scope. `large-locales.png` beside the candidate navigation report is assembled from unmodified raw captures.

## Pending

Phone settings delivery, touch opt-in, later firmware and physical Time 2/phone checks remain pending. The SDK shell omits Settings and the bundled stock Timer; these references are public SDK specimens and pinned upstream stock source. No paired physical device was exposed. On a watch: repeat all three Settings text sizes, inspect all six screens in light/dark, start a timer offline, cover/reopen the app, confirm the alarm at the original deadline, and verify Back/cancel never mutates a timer.
