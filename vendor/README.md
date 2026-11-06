# Pinned dependencies

Timer 0.7.0 uses the snapshot summary API and is pinned to
`pebble-activities-0.4.0.tgz`. Its exact source commit, SHA-256 and SHA-512
integrity are recorded in
[ACTIVITIES.json](ACTIVITIES.json):

- SDK: 4.33.1, Emery; the archive contains both ACT1 compatibility and the
  bounded ACT2/ACT3 native transports, schema 4 summary API, legacy source APIs,
  headers and JavaScript source.
- Licence: PolyForm Noncommercial License 1.0.0, including required attribution.

The tarball includes the native SDK package (`dist.zip`) and JavaScript source.
`npm ci --ignore-scripts` installs it locally without a registry or sibling
checkout dependency. Timer's version is independent of the library version.

Verify the current dependency:

```sh
sha256sum vendor/pebble-activities-0.4.0.tgz
```

The published Timer 0.6.0 pin remains `pebble-activities-0.3.0.tgz`, source
`dad422a3384864966b60acf6dc3e52ddec296a09`, SHA-256
`882df4cf462d19a14baf7aebacb06c7f0c807a86b183614a6ba95d6a776cc92c`.

The following pin belongs to the published Timer 0.2.0 testing release and is
preserved as historical provenance.

`pebble-activities-0.1.2.tgz` is the immutable published package from
[Pebble Activities v0.1.2](https://github.com/xefensor/pebble-lib-activities/releases/tag/v0.1.2):

- Source commit: `baac66dc88a6be6062dc8fe57156fdb96007b3c7`
- Download: <https://github.com/xefensor/pebble-lib-activities/releases/download/v0.1.2/pebble-activities-0.1.2.tgz>
- SHA-256: `dd49a628fa14c319648bb782eb15502a485cf225c77f5f709e86c74aa57be0f6`
- Licence: PolyForm Noncommercial License 1.0.0, including the package's required
  attribution.

Keep the package, lockfile, provenance, checksum and bundled licence in agreement.
