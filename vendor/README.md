# Pinned dependencies

`pebble-activities-0.1.0.tgz` is the immutable published package from
[Pebble Activities v0.1.0](https://github.com/xefensor/pebble-activities/releases/tag/v0.1.0):

- Source commit: `c6eb79d31cb31425b00933b64c23f0152e943457`
- Download: <https://github.com/xefensor/pebble-activities/releases/download/v0.1.0/pebble-activities-0.1.0.tgz>
- SHA-256: `46c69a333b81f475d7e6c38f5aae5b82d1d519b743d4875675f44d07d3bf781e`
- Licence: PolyForm Noncommercial License 1.0.0, including the package's required
  attribution.

The tarball includes the native SDK package (`dist.zip`) and JavaScript source.
`npm ci --ignore-scripts` installs it locally without a registry or sibling
checkout dependency. Timer's version is independent of the library version.

Verify before replacing a dependency:

```sh
sha256sum vendor/pebble-activities-0.1.0.tgz
```

Keep the package, lockfile, provenance, checksum and bundled licence in agreement.
