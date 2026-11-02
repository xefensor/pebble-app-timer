# Timer third-party notices

Timer uses the repository's PolyForm Noncommercial License 1.0.0 and required
attribution, carried in `LICENSE` in the source checkout and packaged PBW.

## Pebble Activities 0.1.0

The app bundles shared native and JavaScript helpers from
[xefensor/pebble-activities](https://github.com/xefensor/pebble-activities), pinned
to the published 0.1.0 tarball in `vendor/`. That dependency uses the PolyForm
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

Timer uses the watch's built-in fonts and bundles no external font, image or
audio assets. No source or assets from `coredevices/pebble-timer` are included.
