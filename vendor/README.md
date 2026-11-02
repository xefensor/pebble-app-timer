# Pinned dependencies

`pebble-activities-0.1.2.tgz` is the immutable published package from
[Pebble Activities v0.1.2](https://github.com/xefensor/pebble-activities/releases/tag/v0.1.2):

- Source commit: `baac66dc88a6be6062dc8fe57156fdb96007b3c7`
- Download: <https://github.com/xefensor/pebble-activities/releases/download/v0.1.2/pebble-activities-0.1.2.tgz>
- SHA-256: `dd49a628fa14c319648bb782eb15502a485cf225c77f5f709e86c74aa57be0f6`
- Licence: PolyForm Noncommercial License 1.0.0, including the package's required
  attribution.

The tarball includes the native SDK package (`dist.zip`) and JavaScript source.
`npm ci --ignore-scripts` installs it locally without a registry or sibling
checkout dependency. Timer's version is independent of the library version.

Verify before replacing a dependency:

```sh
sha256sum vendor/pebble-activities-0.1.2.tgz
```

Keep the package, lockfile, provenance, checksum and bundled licence in agreement.
