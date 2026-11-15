# Release 1.0.3 · 2026-11-14

Timer 1.0.3 is published in [Pebble Store](https://apps.repebble.com/2f8f8cfd4e7e442fb724dac7) and as [GitHub v1.0.3](https://github.com/xefensor/pebble-app-timer/releases/tag/v1.0.3). Public Store/GitHub identity and exact PBW bytes were reverified on 2026-11-15 by the XOCE website deployment gate. The matching PBW SHA-256 is `33bc8ee06bbcc084a70d133a6f126bc4bcb0b90f3e46f5823dc78ea5b38171f6`. Publication does not authorize overwriting this release or its archives.

The final source tests, fresh builds, package checks and native emulator scope are recorded in [release validation](qa/release-1.0.3/validation.json). Historical galleries retain their actual capture versions and final-binary reuse proof. Physical phone/watch and live-provider results are not claimed. Xef's stable-release acceptance removed the earlier additional physical-device approval prerequisite; it did not create unobserved test results.

## Historical preparation notes

The dated records below describe the earlier preparation sequence. Their old version numbers and then-pending publication steps do not override the completed publication above. Frozen artifacts and QA records remain unchanged.

# Stable release preparation — 1.0.2

Xef accepted the reviewed Round 2 implementation on 2026-11-13 and selected a
stable, listed public release. This acceptance removes the earlier additional
physical-device review prerequisite; it does not create unobserved test results.

This release retains Time 2 and Round 2, the public UUID, saved state, permissions
and provider protocols. Duo is excluded. Core/Activities/UI archives remain
pinned to their exact reviewed sources. Older release files and versioned QA
records stay frozen.

The current source/package/emulator coverage is recorded in
[the release review](qa/1.0.0/release-review.json). Physical phone/watch
coverage remains explicitly separate. Earlier versioned reviews retain their
original decisions and do not override Xef's current acceptance.

Prepare/check the Store kit, then freeze the exact candidate with
`python3 scripts/freeze-release.py`. Commit its source, metadata and archives
locally before running this repository's GitHub release preparer. The release
configuration marks this version stable. No upload, tag or publication is
implied by preparation.

Publication uses the existing GitHub release workflow and the exact same frozen
PBW in the Developer Dashboard. Verify listing ownership, the unchanged UUID,
an unused version and live app-owned GitHub support/privacy URLs before upload.
After all six public releases match the checked hashes, activate the redesigned
website and then update Store metadata to its hosted product/support/privacy
pages. The website's live release gate remains mandatory.


## Feedback development candidate — 2026-11-14

Local review adds independent alarm sound preference and Time 2 speaker feedback. The app version and UUID are unchanged;
this is a development candidate, outside the earlier stable acceptance. Portable
source checks, SDK 4.44 builds and native Emery/Gabbro runs are recorded in
`Watch/Feedback-Development-2026-11-14` in the workspace. Chromium runs use the
actual generated settings HTML with a simulated Pebble bridge. Physical watch
and Android integration are unverified; Timer acoustics and battery impact need
a physical Time 2. Frozen release PBWs, source/Store archives and their proof
remain unchanged. No commit is pushed and no artifact is submitted or published.


## Store update — 1.0.2 (2026-11-14)

Xef requested a new numeric version for the reviewed current implementation and Store handoff. Public Store history already contains 1.0.0; 1.0.2 was absent from the checked public Store history and GitHub tags. The public UUID, saved-state compatibility, pinned dependencies and reviewed native interfaces are retained. New source/artifact/Store verification is recorded with this release preparation; prior emulator captures are reused only after native renderer/resource equality is verified. No new physical-device result is claimed. GitHub branch pushes are authorized; app submission and website deployment remain separate.
