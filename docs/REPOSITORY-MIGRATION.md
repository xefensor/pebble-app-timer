# XOCE repository names

The November 2026 naming change uses `pebble-<type>-<name>` under the XOCE
(Xef's Open Computing Ecosystem) umbrella.

Timer remains [xefensor/pebble-app-timer](https://github.com/xefensor/pebble-app-timer).
Related canonical repositories are:

- [Dynamic: pebble-watchface-dynamic](https://github.com/xefensor/pebble-watchface-dynamic).
- [Activities: pebble-lib-activities](https://github.com/xefensor/pebble-lib-activities).
- [Core: pebble-lib-core](https://github.com/xefensor/pebble-lib-core).

Repository names are independent of package names. Timer keeps the
`pebble-activities` dependency, imports and SDK header paths. The vendored
Activities 0.4.0 tarball, original source commit and checksums are unchanged;
`vendor/ACTIVITIES.json` supplies its current repository location. The package
is included in the source archive and installs without the old repository.

Timer 0.12.0 keeps its tested artifact commit
`2c5a9ef2b2f8b0da3e6643abe87ec07f149442b8`, original PBW/source/debugging
archives, checksums and tag. Later repository documentation does not replace
those assets. App/plugin UUIDs, native storage, alarms and activity APIs retain
their existing identities. This migration adds no timer behaviour.

Frozen source archives, licence notices and historical QA/provenance may retain
their original repository URLs. Current documentation and download links use
the canonical repositories; historical evidence is not represented as a new
test run. Rebuilding an existing version locally does not authorise replacing
its published files.

## Migration checks — 6 November 2026

The portable suite passed, including 31 JavaScript handoff tests and native
model, collection, fonts, hourglass/UI, launcher-glance, alarm-feedback and
UI-state checks. All 35 release-helper tests passed. All 40 tracked
PBW/ZIP/tarball artifacts matched their pre-migration bytes. Release preparation
uses the original tested 0.12.0 artifact commit. No new phone or physical-watch
verification is claimed for this repository-only migration.
