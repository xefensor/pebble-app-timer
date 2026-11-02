# Pinned dependencies

Timer 0.4.0 is an unpublished local candidate pinned to
`pebble-activities-0.2.0.tgz`. Its exact provenance is also recorded in
[ACTIVITIES.json](ACTIVITIES.json):

- Source commit: `af6c48f727c33cd16ded003347a7a4639fd99901`
- SHA-256: `2d2b3056bdb7c39d8b3d533928a98bcdc74a8172585b88cb2a19df00430e10ae`
- SDK: 4.33.1, Emery; the archive contains both ACT1 compatibility and the
  bounded ACT2 native view, headers and JavaScript source.
- Licence: PolyForm Noncommercial License 1.0.0, including required attribution.

The tarball includes the native SDK package (`dist.zip`) and JavaScript source.
`npm ci --ignore-scripts` installs it locally without a registry or sibling
checkout dependency. Timer's version is independent of the library version.

Verify the current dependency:

```sh
sha256sum vendor/pebble-activities-0.2.0.tgz
```

The following pin belongs to the published Timer 0.2.0 testing release and is
preserved as historical provenance.

`pebble-activities-0.1.2.tgz` is the immutable published package from
[Pebble Activities v0.1.2](https://github.com/xefensor/pebble-activities/releases/tag/v0.1.2):

- Source commit: `baac66dc88a6be6062dc8fe57156fdb96007b3c7`
- Download: <https://github.com/xefensor/pebble-activities/releases/download/v0.1.2/pebble-activities-0.1.2.tgz>
- SHA-256: `dd49a628fa14c319648bb782eb15502a485cf225c77f5f709e86c74aa57be0f6`
- Licence: PolyForm Noncommercial License 1.0.0, including the package's required
  attribution.

Keep the package, lockfile, provenance, checksum and bundled licence in agreement.
