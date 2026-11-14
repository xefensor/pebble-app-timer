# Timer third-party notices

Timer uses the repository's PolyForm Noncommercial License 1.0.0 and required
attribution, carried in `LICENSE` in the source checkout and packaged PBW.

## Pebble Activities

The app bundles shared native and JavaScript helpers from
[Pebble Activities](https://github.com/xefensor/pebble-lib-activities), pinned
to the reviewed tarball recorded in `vendor/ACTIVITIES.json`. That dependency uses the PolyForm
Noncommercial License 1.0.0 with its required attribution. Its original licence
is retained in `licenses/pebble-activities-LICENSE.txt` in the packaged PBW and
inside the vendored source tarball. See [vendor/README.md](vendor/README.md) for
provenance and the SHA-256 checksum.

## Pebble SDK support code

The Pebble SDK injects `_pkjs_shared_additions.js` into the phone-side JavaScript
bundle. Its notice is preserved in that bundle:

> Copyright 2024 Google LLC

That SDK component is licensed under the Apache License, Version 2.0. The full
licence is carried in `licenses/Apache-2.0.txt` in the PBW and source checkout.
The injected component is unmodified. It adds Pebble event aliases and error
reporting and is supplied by the Pebble SDK.

## Fredoka and Roboto Condensed

Timer's rounded numerals use Fredoka Bold outlines. Supporting text uses Roboto
Condensed Bold. These are the same reviewed source fonts as Dynamic; Timer's
resources and generated outlines keep their source geometry and attribution.

- Fredoka: Copyright 2016 The Fredoka Project Authors
  (<https://github.com/hafontia/Fredoka-One>).
- Roboto Condensed: Copyright 2011 The Roboto Project Authors
  (<https://github.com/googlefonts/roboto-classic>).

Both fonts use the SIL Open Font License, Version 1.1. The original font license
texts accompany the source fonts under `resources/fonts/` and are included in
each standalone PBW as `licenses/OFL-Fredoka.txt` and
`licenses/OFL-RobotoCondensed.txt`. Font source checksums and the reused outline
provenance are recorded in `resources/fonts/font-provenance.json`.

Timer bundles no external image or audio assets. Its hourglass and action icons
are drawn procedurally. No source or assets from `coredevices/pebble-timer` are
included.

## XOCE shared UI

The locally pinned `pebble-xoce-ui` 0.1.0 library supplies shared native drawing
and phone-settings styles. It is authored by Xef and uses the same PolyForm
Noncommercial license as this application. Its sources and license are included
in the committed vendor archive; it is not a separately installed service.
