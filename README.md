# Timer

A native timer for stock PebbleOS with eight saved timers, watch-owned alarms and optional
activity sharing.

## Download

[Version 0.14.5](https://github.com/xefensor/pebble-app-timer/releases/tag/v0.14.5) is a development prerelease.

[PBW](https://github.com/xefensor/pebble-app-timer/releases/download/v0.14.5/Timer-0.14.5.pbw),
[source](https://github.com/xefensor/pebble-app-timer/releases/download/v0.14.5/Timer-0.14.5-source.zip),
[debug files](https://github.com/xefensor/pebble-app-timer/releases/download/v0.14.5/Timer-0.14.5-debug.zip), and
[checksums](https://github.com/xefensor/pebble-app-timer/releases/download/v0.14.5/SHA256SUMS.txt).

Install the PBW through the Pebble phone app. No separate library installation is needed.

Create a timer with the watch buttons. Select pauses/resumes or starts a Ready timer; More
options offers Reset and safe deletion. Native Wakeup owns accepted alarms while the app is
closed. Optional sharing requires phone plugin support. The phone settings page chooses
Light/Dark appearance and confirms Save without closing.

[Usage and controls](docs/USAGE.md) · [Current UI](docs/UI.md).

## Compatibility

The current SDK target is **Pebble Time 2 / Emery**, using **Pebble SDK 4.33.1**. Portable and
emulator evidence is versioned; physical phone/watch checks retain their documented gaps.
Existing screenshots keep their original version and provenance.

## Version 0.14.5

- Adopt shared XOCE headers, readable type roles, full-height action rails, canonical icons and stock Pebble action-bar feedback.
- Keep one timer anchored at the first-row position. Match native duration-editor movement while retaining rigid 120 ms menu selection.
- Preserve a valid later alarm when scheduling its replacement fails. Keep timer ownership, eight slots, persistence and explicit Reset/Start behavior.
- Bundle UI 0.1.1 and the confirmed stay-open appearance settings page.

## Development

Use Node 22, Python 3.12 and a C compiler. SDK builds additionally need Pebble SDK 4.33.1 on
PATH.

```sh
npm ci --ignore-scripts
npm test
npm run build
npm run check:artifact
```

[Contributor guide](CONTRIBUTING.md) · [Release preparation](docs/RELEASING.md) · [History index](docs/history/index.md).
Development outputs belong in ignored `build/`; published archives stay frozen.

## Licence

[PolyForm Noncommercial 1.0.0](LICENSE). Preserve bundled third-party notices and attribution.
